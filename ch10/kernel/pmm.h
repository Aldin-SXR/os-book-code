/* spark -- kernel/pmm.h
 *
 * The physical memory manager: ownership of every 4 KiB frame of RAM.
 *
 * Every address the kernel has used so far was fixed in advance, by the
 * hardware, by the linker script or by the boot code. Allocation begins where
 * that stops working, which is the first time the kernel needs room for
 * something whose size is not known until it runs.
 *
 * The allocator is a bitmap: one bit per frame, clear for free and set for
 * taken, with a single frame in and out. It cannot satisfy a request for two
 * adjacent frames or for a fraction of one; both belong to the layers built
 * on top of it.
 */
#ifndef SPARK_PMM_H
#define SPARK_PMM_H

#include "types.h"

/* The allocation unit, fixed by the processor rather than chosen here: 4 KiB
 * is the page size the paging hardware works in, and allocating in any other
 * unit would have to be undone before paging could use it. */
#define FRAME_SIZE 4096u

/* One bit per frame of the whole 32-bit address space: 1,048,576 frames,
 * which is 128 KiB of bitmap.
 *
 * The count is written out rather than computed from 4 GiB divided by
 * FRAME_SIZE, because 4 GiB does not fit in the uint32_t that division would
 * be performed in and the expression evaluates to zero.
 *
 * Sizing the table for the address space rather than for the RAM present is
 * what lets it live in .bss, where it needs no allocator of its own. */
#define PMM_MAX_FRAMES (1024u * 1024u)   /* 4 GiB of frames */

/* Read the loader's memory map, mark everything it does not vouch for as
 * taken, then reserve the first megabyte and the kernel's own frames.
 *
 * magic and info are the two values the loader passes to kmain. */
void pmm_init(uint32_t magic, uint32_t info);

/* One frame, 4 KiB, aligned, or NULL when there are none left.
 * The contents are whatever the last owner wrote; this is not
 * calloc and the frame is not cleared. */
void *pmm_alloc_frame(void);

/* Return one frame. A double free panics rather than being absorbed: the
 * second free is evidence that something is still using a frame it has
 * already given up. */
void pmm_free_frame(void *frame);

/* Usable frames according to the memory map: a property of the machine, and
 * fixed for the life of the kernel. */
uint32_t pmm_total_frames(void);

/* How many of those are free at this moment. */
uint32_t pmm_free_frames(void);

/* True if the loader supplied a memory map, false if the figures above rest
 * on the assumed size instead. */
bool pmm_map_was_measured(void);

/* Print the loader's map, one row per region, uninterpreted. */
void pmm_dump_map(void);

/* The bounds of the kernel image, from the symbols link.ld defines. Both are
 * rounded outwards to frame boundaries. */
uint32_t pmm_kernel_start(void);
uint32_t pmm_kernel_end(void);

#endif
