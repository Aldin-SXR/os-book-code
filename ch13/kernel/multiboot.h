/* spark -- kernel/multiboot.h
 *
 * The information structure a Multiboot loader leaves for the kernel.
 *
 * The loader passes two values to kmain: a magic number identifying itself,
 * and the address of the structure below, which records what it learned about
 * the machine before the kernel ran. Only the memory map is used here.
 */
#ifndef SPARK_MULTIBOOT_H
#define SPARK_MULTIBOOT_H

#include "types.h"

/* The value a Multiboot loader leaves in eax. spark's own boot sector leaves
 * 0xB007AC05 instead, which is how the kernel tells its two entry paths
 * apart. */
#define MULTIBOOT_MAGIC 0x2BADB002u

/* Flag bits. A loader sets a bit to declare that the fields belonging to it
 * are filled in; the fields are meaningless until the bit is checked. */
#define MB_FLAG_MEM  (1u << 0)   /* mem_lower and mem_upper  */
#define MB_FLAG_MMAP (1u << 6)   /* mmap_length and mmap_addr */

/* The structure as far as the memory map. The order is fixed by the
 * specification: unused fields are declared and skipped rather than omitted,
 * because the offsets of the ones that follow depend on them.
 *
 * MB_FLAG_MEM covers the two size fields and MB_FLAG_MMAP the two at the
 * end. Neither pair is valid without its flag. */
struct multiboot_info {
    uint32_t flags;

    uint32_t mem_lower;   /* KiB below 1 MiB */
    uint32_t mem_upper;   /* KiB above 1 MiB */

    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];

    uint32_t mmap_length; /* bytes of map    */
    uint32_t mmap_addr;   /* where it starts */
};

/* One region of the map. Type 1 is the only kind a kernel may hand out; the
 * rest is device memory, firmware tables, or memory reported as faulty.
 *
 * size counts the bytes of the entry that follow it, so advancing to the next
 * entry adds size plus the four bytes of size itself. The indirection lets a
 * later revision lengthen the entries without breaking existing readers. */
struct multiboot_mmap_entry {
    uint32_t size;
    uint64_t base;
    uint64_t length;
    uint32_t type;
} __attribute__((packed));

#define MB_MEMORY_AVAILABLE 1

/* The region types, in words, for reporting. */
const char *multiboot_type_name(uint32_t type);

#endif
