/* spark -- kernel/heap.c
 *
 * A first-fit allocator over a run of frames, with a header in front of every
 * block and immediate coalescing on free.
 *
 * The blocks tile the region: every byte of the heap belongs to exactly one
 * block, free or used, and the blocks are linked in address order. That is
 * what makes coalescing possible at all, because two blocks can only be
 * merged if the kernel knows they are neighbours.
 */
#include "heap.h"
#include "pmm.h"
#include "vmm.h"
#include "panic.h"

/* The state word is the first field of every header and holds one of exactly
 * two values, so it doubles as a check: an address that was never a block is
 * very unlikely to have either of them lying in front of it. A header that is
 * absorbed by a merge has its state cleared, so a pointer into the middle of
 * a coalesced block fails the same test. */
#define BLOCK_FREE 0x46524545u   /* 'FREE' */
#define BLOCK_USED 0x55534544u   /* 'USED' */

struct block {
    uint32_t state;
    uint32_t size;       /* payload bytes, not the header */
    struct block *prev;  /* address order, NULL at the base */
    struct block *next;  /* address order, NULL at the top */
};

/* The header has to be a whole number of alignment units, or the payload
 * after it lands on an odd boundary and every block after the first is
 * misaligned. Adding a field without adding padding is the way that breaks,
 * so the compiler is asked to check. */
_Static_assert(sizeof(struct block) % HEAP_ALIGN == 0,
               "the header must be a multiple of HEAP_ALIGN");

#define HEADER ((uint32_t) sizeof(struct block))

/* The smallest payload worth splitting a block for. A remainder below
 * HEADER + this is left inside the block that was handed out, because a free
 * block too small to ever be used still costs a header to describe. */
#define MIN_PAYLOAD HEAP_ALIGN

static struct block *head;    /* lowest block in the region */
static uint32_t region_base;  /* first byte of the heap */
static uint32_t region_top;   /* one past the last byte */
static uint32_t region_frames;

static struct block *header_of(void *payload)
{
    return (struct block *) ((uint8_t *) payload - HEADER);
}

static void *payload_of(struct block *b)
{
    return (void *) ((uint8_t *) b + HEADER);
}

static uint32_t round_up(uint32_t bytes)
{
    return (bytes + HEAP_ALIGN - 1u) & ~(HEAP_ALIGN - 1u);
}

/* Take frames from the physical allocator and add them to the top of the
 * region.
 *
 * Chapter 10 required each frame to come back immediately above the one
 * before it, because the block list is a tiling of one contiguous range and
 * a gap in it would let a merge run over memory the heap does not own. That
 * requirement is gone. region_top is a virtual address now, so the page
 * always goes exactly where the tiling needs it and the frame behind it can
 * be anywhere in the machine.
 *
 * Returns the number of frames actually added, which may be fewer than
 * asked for and may be zero. */
static uint32_t take_frames(uint32_t frames)
{
    uint32_t added = 0;

    for (uint32_t i = 0; i < frames; i++) {
        void *frame = pmm_alloc_frame();

        if (frame == NULL) {
            break;
        }

        if (!vmm_map(region_top, (uint32_t) frame,
                     PAGE_PRESENT | PAGE_WRITE)) {
            pmm_free_frame(frame);
            break;
        }

        region_top += FRAME_SIZE;
        added++;
    }

    region_frames += added;
    return added;
}

/* Absorb the block above this one, if it is free. The absorbed header stops
 * being a header: its state word is cleared so that a pointer still held into
 * that block fails the check in kfree rather than being accepted. */
static void merge_with_next(struct block *b)
{
    struct block *n = b->next;

    if (n == NULL || n->state != BLOCK_FREE) {
        return;
    }

    b->size += HEADER + n->size;
    b->next = n->next;

    if (n->next != NULL) {
        n->next->prev = b;
    }

    n->state = 0;
}

/* Cut a block down to size, leaving the remainder as a free block behind it.
 * A remainder too small to hold a header and a usable payload is left where
 * it is: the caller gets a few bytes more than it asked for, which is waste
 * inside the block rather than a free block nobody can ever use. */
static void split(struct block *b, uint32_t size)
{
    if (b->size < size + HEADER + MIN_PAYLOAD) {
        return;
    }

    struct block *rest =
        (struct block *) ((uint8_t *) b + HEADER + size);

    rest->state = BLOCK_FREE;
    rest->size  = b->size - size - HEADER;
    rest->prev  = b;
    rest->next  = b->next;

    if (b->next != NULL) {
        b->next->prev = rest;
    }

    b->next = rest;
    b->size = size;
}

/* First fit: the lowest block large enough. Best fit would search the whole
 * list for the tightest one; Chapter 10's measurements say what the choice
 * is worth here. */
static struct block *find_fit(uint32_t size)
{
    for (struct block *b = head; b != NULL; b = b->next) {
        if (b->state == BLOCK_FREE && b->size >= size) {
            return b;
        }
    }

    return NULL;
}

/* Lay a free block over frames just added at the top, and merge it with the
 * block below if that one is free. */
static void extend(uint32_t old_top, uint32_t added_frames)
{
    struct block *b = (struct block *) old_top;

    b->state = BLOCK_FREE;
    b->size  = added_frames * FRAME_SIZE - HEADER;
    b->next  = NULL;
    b->prev  = NULL;

    if (head == NULL) {
        head = b;
        return;
    }

    struct block *last = head;

    while (last->next != NULL) {
        last = last->next;
    }

    last->next = b;
    b->prev = last;

    if (last->state == BLOCK_FREE) {
        merge_with_next(last);
    }
}

void heap_init(void)
{
    head = NULL;
    region_base = HEAP_VIRTUAL_BASE;
    region_top = region_base;
    region_frames = 0;

    uint32_t added = take_frames(HEAP_INITIAL_FRAMES);

    if (added != HEAP_INITIAL_FRAMES) {
        panic("heap_init: could not map a heap");
    }

    extend(region_base, added);
}

void *kmalloc(uint32_t bytes)
{
    /* Rounding an enormous request up would wrap to a small one, and the
     * caller would be handed a block far shorter than it asked for. */
    if (bytes > 0xFFFFFFFFu - HEAP_ALIGN) {
        return NULL;
    }

    uint32_t size = round_up(bytes);

    /* malloc(0) is allowed to return a distinct pointer that may be freed,
     * and a block of no payload could not be told apart from the one above
     * it, so the smallest block is one alignment unit. */
    if (size == 0) {
        size = HEAP_ALIGN;
    }

    struct block *b = find_fit(size);

    if (b == NULL) {
        uint32_t need = size + HEADER + FRAME_SIZE - 1;
        uint32_t want = need / FRAME_SIZE;
        uint32_t frames = want > HEAP_GROW_FRAMES
                        ? want : HEAP_GROW_FRAMES;
        uint32_t old_top = region_top;
        uint32_t added = take_frames(frames);

        if (added == 0) {
            return NULL;
        }

        extend(old_top, added);
        b = find_fit(size);

        if (b == NULL) {
            return NULL;
        }
    }

    split(b, size);
    b->state = BLOCK_USED;

    return payload_of(b);
}

void kfree(void *pointer)
{
    /* Freeing nothing is not an error, so that a caller cleaning up after a
     * failed allocation does not have to test first. */
    if (pointer == NULL) {
        return;
    }

    uint32_t address = (uint32_t) pointer;

    if (address < region_base + HEADER ||
        address >= region_top) {
        panic("kfree: that address is not in the heap");
    }

    if (address % HEAP_ALIGN != 0) {
        panic("kfree: that address was never handed out");
    }

    struct block *b = header_of(pointer);

    if (b->state == BLOCK_FREE) {
        panic("kfree: that block is already free");
    }

    if (b->state != BLOCK_USED) {
        panic("kfree: that address was never handed out");
    }

    b->state = BLOCK_FREE;

    /* Forwards first, then backwards, so that three free blocks in a row
     * become one and not two. */
    merge_with_next(b);

    if (b->prev != NULL && b->prev->state == BLOCK_FREE) {
        merge_with_next(b->prev);
    }
}

uint32_t heap_header_size(void)
{
    return HEADER;
}

uint32_t heap_block_size(void *pointer)
{
    if (pointer == NULL) {
        return 0;
    }

    return header_of(pointer)->size;
}

uint32_t heap_base(void)
{
    return region_base;
}

void heap_get_stats(struct heap_stats *out)
{
    out->bytes = region_top - region_base;
    out->frames = region_frames;
    out->used = 0;
    out->free = 0;
    out->overhead = 0;
    out->blocks = 0;
    out->free_blocks = 0;
    out->largest_free = 0;

    for (struct block *b = head; b != NULL; b = b->next) {
        out->blocks++;
        out->overhead += HEADER;

        if (b->state == BLOCK_FREE) {
            out->free += b->size;
            out->free_blocks++;

            if (b->size > out->largest_free) {
                out->largest_free = b->size;
            }
        } else {
            out->used += b->size;
        }
    }
}
