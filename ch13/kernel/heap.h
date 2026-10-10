/* spark -- kernel/heap.h
 *
 * The kernel heap: memory in the sizes the kernel actually asks for.
 *
 * The frame allocator of Chapter 9 hands out 4 KiB and nothing else, which is
 * the wrong unit for almost everything a kernel allocates. A process control
 * block is a hundred bytes and a file name is a dozen, and a frame for each
 * would waste more than it used.
 *
 * kmalloc divides frames into blocks of any size, and kfree puts them back.
 * Neither moves memory: an address handed out stays valid until it is freed,
 * because the kernel keeps pointers to what it allocates.
 */
#ifndef SPARK_HEAP_H
#define SPARK_HEAP_H

#include "types.h"

/* Every payload starts on an 8-byte boundary, and every requested size is
 * rounded up to a multiple of this. x86 will load a misaligned word without
 * complaining, but a structure the caller lays over the block may hold a
 * 64-bit field, and 8 is the widest alignment anything in spark needs. */
#define HEAP_ALIGN 8u

/* What the heap takes from the frame allocator at start-up, and what it adds
 * each time it has to grow. Both are frames, not bytes. */
#define HEAP_INITIAL_FRAMES 16u
#define HEAP_GROW_FRAMES     8u

/* A running total of what the heap holds. Every field is bytes except the
 * four counts, and used + free + overhead == bytes always. */
struct heap_stats {
    uint32_t bytes;         /* the whole region        */
    uint32_t frames;        /* the same, in frames     */
    uint32_t used;          /* payload bytes given out */
    uint32_t free;          /* payload bytes still free */
    uint32_t overhead;      /* bytes spent on headers  */
    uint32_t blocks;        /* blocks in the list      */
    uint32_t free_blocks;   /* how many of those are free */
    uint32_t largest_free;  /* the biggest free payload   */
};

/* Take HEAP_INITIAL_FRAMES from the frame allocator and lay one free block
 * over them. Must be called after pmm_init and before any kmalloc. */
void heap_init(void);

/* Bytes, not frames. Returns an 8-aligned address, or NULL when the heap
 * could not be grown to hold the request. The contents are whatever the
 * previous owner left; this is not calloc. */
void *kmalloc(uint32_t bytes);

/* Return a block. The pointer must be one kmalloc returned and not yet
 * freed; anything else panics rather than being absorbed, because a heap
 * that quietly forgives a bad free fails later and somewhere else. */
void kfree(void *pointer);

/* The size of one block header, which is what a single byte of payload
 * really costs before rounding. */
uint32_t heap_header_size(void);

/* The payload a block actually holds, which is the request rounded up and
 * sometimes a little more: a remainder too small to split off is left inside
 * the block. The standard C library calls this malloc_usable_size. */
uint32_t heap_block_size(void *pointer);

/* Where the heap region begins, for printing. */
uint32_t heap_base(void);

/* Fill in the totals by walking the block list. This is a walk rather than a
 * set of running counters so that the figures come from the list itself: a
 * counter that disagrees with the list would report the heap as healthy. */
void heap_get_stats(struct heap_stats *out);

#endif
