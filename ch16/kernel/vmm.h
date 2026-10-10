/* spark -- kernel/vmm.h
 *
 * Virtual memory: the page tables, and the translation the processor performs
 * once they are installed.
 *
 * Every address in the kernel up to this point went straight to memory. From
 * here on the processor reads two tables of the kernel's own making before
 * any address reaches the bus, which is what lets one address space be laid
 * out differently from the memory behind it.
 *
 * spark keeps two regions and they are meant to be read as a contrast. The
 * direct map covers every byte of physical memory at its own address, so the
 * kernel can still reach a frame by the number the frame allocator handed it.
 * The heap lives above the direct map at an address nothing is fitted to, and
 * its pages are mapped one at a time to frames from anywhere.
 */
#ifndef SPARK_VMM_H
#define SPARK_VMM_H

#include "types.h"
#include "isr.h"

/* A page is a frame seen from the other side: the same 4 KiB, named by a
 * virtual address rather than by a physical one. */
#define PAGE_SIZE 4096u

/* One table holds 1024 entries of 4 bytes, so it fills a frame exactly and
 * describes 4 MiB. Both facts are consequences of the entry being a word and
 * the page being 4 KiB, which is why the numbers are not arbitrary.
 *
 * TABLE_SPAN is that 4 MiB. Nothing here needs it, because vmm_init walks a
 * page at a time; it is the step a walk over the directory takes, which is
 * the last exercise in the chapter. */
#define ENTRIES_PER_TABLE 1024u
#define TABLE_SPAN        (ENTRIES_PER_TABLE * PAGE_SIZE)

/* The flag bits of a directory or table entry. The processor defines more;
 * these are the three the book names. Bit 0 clear means the entry describes
 * nothing, and every other bit in it is then the kernel's to use.
 *
 * PAGE_USER is declared and never passed. It marks a page reachable from
 * ring 3, and spark has no ring 3 until Chapter 14, which is the chapter that
 * starts setting it. */
#define PAGE_PRESENT (1u << 0)
#define PAGE_WRITE   (1u << 1)
#define PAGE_USER    (1u << 2)

/* The three fields a virtual address is cut into. The processor does exactly
 * this, in hardware, on every single memory reference. */
#define PAGE_DIR_INDEX(v)   ((uint32_t) (v) >> 22)
#define PAGE_TABLE_INDEX(v) (((uint32_t) (v) >> 12) & 0x3FFu)
#define PAGE_OFFSET(v)      ((uint32_t) (v) & 0xFFFu)

/* The other half of an entry: the address, with the flags masked off. A frame
 * address always ends in twelve zeros, which is what leaves room for them. */
#define PAGE_FRAME(e)       ((uint32_t) (e) & ~0xFFFu)

/* The bits of the error code the processor pushes for a page fault. Bit 0 is
 * the one that separates the two kinds of failure: clear means there was no
 * translation at all, set means there was one and it forbade this access. */
#define PF_PROTECTION (1u << 0)
#define PF_WRITE      (1u << 1)
#define PF_USER       (1u << 2)

/* The two bits of cr0 this chapter is about. PG turns translation on. WP
 * makes the processor honour PAGE_WRITE in ring 0, and it is clear out of
 * reset, so a page marked read-only is not read-only for the kernel until
 * something sets it. spark leaves it clear; the chapter's last hands-on box
 * is what setting it does. */
#define CR0_PG (1u << 31)
#define CR0_WP (1u << 16)

/* Where the kernel heap lives from this chapter on: 3 GiB, which is far above
 * any memory this machine has, so an address in it can only ever be one the
 * page tables made. Chapter 10's heap was at a physical address and had to
 * grow into whatever frame happened to sit above it. */
#define HEAP_VIRTUAL_BASE 0xC0000000u

/* Build the direct map over all of physical memory, install the directory,
 * and set CR0.PG. Must run after pmm_init, because every page table is a
 * frame from the frame allocator. */
void vmm_init(void);

/* Point one page at one frame. Both addresses are rounded down to a page
 * boundary. Returns false only when a new page table was needed and there
 * was no frame left to make one from. */
bool vmm_map(uint32_t virt, uint32_t phys, uint32_t flags);

/* Remove one page. The frame is not freed: the caller owns it and may have
 * other names for it. Unmapping a page nothing maps does nothing, so a
 * caller tidying up after a failure need not check first. */
void vmm_unmap(uint32_t virt);

/* Walk the tables in software and report what the hardware would do with this
 * address. False means nothing maps it, and *out is untouched. */
bool vmm_translate(uint32_t virt, uint32_t *out);

/* Make faults inside this region allocate a frame and carry on, rather than
 * panicking. A zero length turns it off again. */
void vmm_demand_region(uint32_t base, uint32_t bytes);

/* The physical address of the page directory, which is what CR3 holds. */
uint32_t vmm_directory(void);

/* How many page tables exist, and the bytes they occupy. */
uint32_t vmm_table_count(void);

/* Page faults taken so far, and the address in CR2 at the last one. */
uint32_t vmm_fault_count(void);
uint32_t vmm_last_fault(void);

#endif
