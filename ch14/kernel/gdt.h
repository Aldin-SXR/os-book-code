/* spark -- kernel/gdt.h
 *
 * The global descriptor table, which the processor consults on every memory
 * access.
 *
 * The table spark inherits from its loader sits in memory the kernel will
 * later hand out as free, so this module builds a table of its own inside
 * the kernel image and points the processor at that.
 */
#ifndef SPARK_GDT_H
#define SPARK_GDT_H

#include "types.h"

/* A selector is what a segment register holds: the byte offset of an entry
 * into the table, which is why these are multiples of eight.
 *
 * The low three bits are not part of the offset. Bit 2 selects the local
 * descriptor table, which spark never uses, and bits 1 and 0 are the ring the
 * access is made on behalf of. Both user selectors are named at ring 0 here;
 * a ring-3 access uses them with the low two bits set, as 0x1B and 0x23. */
#define GDT_NULL        0x00
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE   0x18
#define GDT_USER_DATA   0x20
#define GDT_TSS         0x28

#define GDT_ENTRIES 6

/* What lgdt is handed and what sgdt reads back: the table's size, one less
 * than its length in bytes, followed by its address.
 *
 * Six bytes with nothing between them. Without the packed attribute the
 * compiler aligns base on a four-byte boundary, leaving a two-byte hole, and
 * lgdt then reads the address from the wrong offset. */
struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

_Static_assert(sizeof(struct gdt_ptr) == 6, "six bytes");

/* Build spark's own table and start using it. */
void gdt_init(void);

/* Write entry 5, describing the task state segment. Separate from gdt_init
 * because the segment it describes belongs to another module, and because
 * the table has to exist before there is anywhere to put the descriptor. */
void gdt_set_tss(uint32_t base, uint32_t size);

/* The state the processor is actually using, read back from it rather than
 * assumed from what was written. */
void gdt_current(struct gdt_ptr *out);
uint16_t gdt_read_cs(void);
uint16_t gdt_read_ds(void);

/* Pull one entry apart again, for printing. False if the index
 * is past the end of the table the processor is using. */
bool gdt_describe(unsigned index,
                  uint32_t *base, uint32_t *limit,
                  unsigned *ring, const char **kind);

#endif
