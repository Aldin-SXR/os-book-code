/* spark -- kernel/pmm.c
 *
 * The frame bitmap, the four rules that set its initial state, and the
 * allocator built on it.
 */
#include "pmm.h"
#include "multiboot.h"
#include "printf.h"
#include "panic.h"
#include "string.h"

/* Defined by link.ld at the two ends of the image. The value wanted is the
 * address of each symbol, never its contents; declaring them as arrays makes
 * that the natural reading, since an array decays to its address. */
extern uint8_t _kernel_start[];
extern uint8_t _kernel_end[];

/* The first megabyte holds the real-mode interrupt table, the BIOS data
 * area, the video memory and firmware structures. Parts of it are reported as
 * available in every map, so a kernel that trusts the map here will hand out
 * the frames it is writing characters into. */
#define LOW_MEMORY (1024u * 1024u)

/* The fallback when no map is supplied. 32 MiB is QEMU's default without -m,
 * and low enough to be safe on any machine worth booting. Every figure
 * derived from it is an assumption, which is what pmm_map_was_measured
 * reports. */
#define ASSUMED_RAM (32u * 1024u * 1024u)

#define BITS_PER_WORD 32u
#define BITMAP_WORDS  (PMM_MAX_FRAMES / BITS_PER_WORD)

static uint32_t bitmap[BITMAP_WORDS];
static uint32_t total_frames;
static uint32_t free_frames;
static uint32_t hint;            /* where a search starts */
static bool measured;
static const struct multiboot_info *mb;

const char *multiboot_type_name(uint32_t type)
{
    switch (type) {
    case 1:  return "available";
    case 2:  return "reserved";
    case 3:  return "ACPI, reclaimable";
    case 4:  return "ACPI, must keep";
    case 5:  return "bad memory";
    default: return "unknown";
    }
}

static bool is_used(uint32_t frame)
{
    return (bitmap[frame / BITS_PER_WORD] &
            (1u << (frame % BITS_PER_WORD))) != 0;
}

/* Both are idempotent and both maintain free_frames. That is what keeps the
 * count correct across overlapping ranges: the first megabyte and the kernel
 * image are reserved twice over in the common case. */
static void mark_used(uint32_t frame)
{
    if (!is_used(frame)) {
        bitmap[frame / BITS_PER_WORD] |=
            1u << (frame % BITS_PER_WORD);
        free_frames--;
    }
}

static void mark_free(uint32_t frame)
{
    if (is_used(frame)) {
        bitmap[frame / BITS_PER_WORD] &=
            ~(1u << (frame % BITS_PER_WORD));
        free_frames++;
    }
}

/* Frames needed to cover a byte count, rounding up. Plain division rounds
 * down, and the two range functions below need one of each. */
static uint64_t frames_up(uint64_t bytes)
{
    return (bytes + FRAME_SIZE - 1) / FRAME_SIZE;
}

static uint64_t clamp(uint64_t frame)
{
    return frame > PMM_MAX_FRAMES ? PMM_MAX_FRAMES : frame;
}

/* The rounding is deliberately asymmetric.
 *
 * Releasing rounds the start up and the end down, so a frame is freed only if
 * it lies entirely inside the region. Reserving rounds the start down and the
 * end up, so a frame is withheld if any part of it lies inside. Both err
 * towards handing out less memory, where the cost of an error is waste rather
 * than corruption. */
static void release_range(uint64_t base, uint64_t length)
{
    uint64_t start = frames_up(base);
    uint64_t end   = clamp((base + length) / FRAME_SIZE);

    for (uint64_t f = start; f < end; f++) {
        mark_free((uint32_t) f);
    }
}

static void reserve_range(uint64_t base, uint64_t length)
{
    uint64_t start = base / FRAME_SIZE;
    uint64_t end   = clamp(frames_up(base + length));

    for (uint64_t f = start; f < end; f++) {
        mark_used((uint32_t) f);
    }
}

void pmm_init(uint32_t magic, uint32_t info)
{
    /* Rule one: nothing is free until the map says so. Starting from the
     * opposite assumption would treat every address the map omits as RAM,
     * and most of the 4 GiB this bitmap covers is not memory at all. */
    memset(bitmap, 0xFF, sizeof bitmap);
    free_frames = 0;

    mb = NULL;
    measured = false;

    if (magic == MULTIBOOT_MAGIC && info != 0) {
        mb = (const struct multiboot_info *) info;
        measured = (mb->flags & MB_FLAG_MMAP) != 0;
    }

    if (measured) {
        /* Rule two: what the map vouches for is free. */
        uint32_t at = mb->mmap_addr;
        uint32_t end = mb->mmap_addr + mb->mmap_length;

        while (at < end) {
            const struct multiboot_mmap_entry *e =
                (const struct multiboot_mmap_entry *) at;

            if (e->type == MB_MEMORY_AVAILABLE) {
                release_range(e->base, e->length);
            }

            at += e->size + sizeof e->size;
        }
    } else {
        release_range(0, ASSUMED_RAM);
    }

    total_frames = free_frames;

    /* Rule three: the first megabyte is never allocatable, whatever the
     * map reports about it. */
    reserve_range(0, LOW_MEMORY);

    /* Rule four: an allocator may not hand out the memory it is
     * standing in. The bitmap above is inside this range, so
     * this one line protects the kernel and the table both. */
    reserve_range((uint32_t) _kernel_start,
                  (uint32_t) (_kernel_end - _kernel_start));

    hint = 0;
}

void *pmm_alloc_frame(void)
{
    for (uint32_t w = hint; w < BITMAP_WORDS; w++) {
        /* A word of all ones is 32 taken frames. Skipping it whole is what
         * keeps the scan cheap: most of the address space is not memory, so
         * most words stay saturated for the life of the kernel. */
        if (bitmap[w] == 0xFFFFFFFFu) {
            continue;
        }

        for (uint32_t b = 0; b < BITS_PER_WORD; b++) {
            uint32_t frame = w * BITS_PER_WORD + b;

            if (!is_used(frame)) {
                mark_used(frame);
                hint = w;
                return (void *) (frame * FRAME_SIZE);
            }
        }
    }

    return NULL;
}

void pmm_free_frame(void *frame)
{
    uint32_t address = (uint32_t) frame;

    if (address % FRAME_SIZE != 0) {
        panic("pmm_free_frame: not a frame address");
    }

    uint32_t index = address / FRAME_SIZE;

    if (!is_used(index)) {
        panic("pmm_free_frame: that frame is already free");
    }

    mark_free(index);

    /* The hint must never be above the lowest free frame. */
    if (index / BITS_PER_WORD < hint) {
        hint = index / BITS_PER_WORD;
    }
}

uint32_t pmm_total_frames(void) { return total_frames; }
uint32_t pmm_free_frames(void)  { return free_frames;  }
bool pmm_map_was_measured(void) { return measured;     }

uint32_t pmm_kernel_start(void)
{
    return (uint32_t) _kernel_start;
}

uint32_t pmm_kernel_end(void)
{
    return (uint32_t) _kernel_end;
}

void pmm_dump_map(void)
{
    if (!measured) {
        kprintf("  nothing: no map, so %u MiB is assumed\n",
                ASSUMED_RAM / (1024u * 1024u));
        return;
    }

    kprintf("  base        length      kind\n");

    uint32_t at = mb->mmap_addr;
    uint32_t end = mb->mmap_addr + mb->mmap_length;

    while (at < end) {
        const struct multiboot_mmap_entry *e =
            (const struct multiboot_mmap_entry *) at;

        /* The map uses 64-bit quantities, because the format outlived the
         * 32-bit machine. A region above 4 GiB cannot be addressed here, so
         * the upper halves are tested and the region reported as skipped
         * rather than truncated into nonsense. */
        if ((e->base >> 32) != 0 || (e->length >> 32) != 0) {
            kprintf("  (a region above 4 GiB, skipped)\n");
        } else {
            kprintf("  0x%08x  0x%08x  %s\n",
                    (uint32_t) e->base, (uint32_t) e->length,
                    multiboot_type_name(e->type));
        }

        at += e->size + sizeof e->size;
    }
}
