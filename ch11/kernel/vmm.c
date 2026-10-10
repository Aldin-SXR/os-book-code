/* spark -- kernel/vmm.c
 *
 * The two-level page table, the code that fills it in, and the page fault
 * handler that runs when the processor consults it and finds nothing.
 */
#include "vmm.h"
#include "pmm.h"
#include "panic.h"
#include "printf.h"
#include "string.h"

/* The top-level table. It is a static array rather than a frame from the
 * allocator so that it exists before any allocation does, and it is aligned
 * because the processor keeps only the top 20 bits of the address in CR3:
 * the low 12 bits of that register are flags, so a directory that is not
 * frame-aligned is loaded at the wrong address with some of its address
 * bits read as settings. */
static uint32_t directory[ENTRIES_PER_TABLE]
    __attribute__((aligned(PAGE_SIZE)));

_Static_assert(sizeof directory == PAGE_SIZE,
               "a page directory is exactly one frame");

static uint32_t tables_made;
static uint32_t faults;
static uint32_t last_fault;
static uint32_t demand_base;
static uint32_t demand_end;

static uint32_t read_cr2(void)
{
    uint32_t value;
    __asm__ volatile ("mov %%cr2, %0" : "=r" (value));
    return value;
}

/* Throw away any cached translation for one page. The processor keeps
 * recently used translations in the TLB and does not notice the kernel
 * editing a table underneath it, so every change to a live mapping has to be
 * followed by this. */
static void flush(uint32_t virt)
{
    __asm__ volatile ("invlpg (%0)" :: "r" (virt) : "memory");
}

/* The table an address falls in, made if it is not there yet.
 * Returns NULL only when a frame was needed and none was free. */
static uint32_t *table_for(uint32_t virt, bool make)
{
    uint32_t index = PAGE_DIR_INDEX(virt);
    uint32_t entry = directory[index];

    if (entry & PAGE_PRESENT) {
        /* The table's own address is physical, and the direct map is what
         * makes it reachable by that same number once paging is on. */
        return (uint32_t *) PAGE_FRAME(entry);
    }

    if (!make) {
        return NULL;
    }

    void *frame = pmm_alloc_frame();

    if (frame == NULL) {
        return NULL;
    }

    uint32_t *table = (uint32_t *) frame;

    /* A fresh frame holds whatever its last owner left. Every entry has to
     * read as not present before the directory points at it, or the
     * processor will follow a stale word as though it were a mapping. */
    memset(table, 0, PAGE_SIZE);

    directory[index] =
        (uint32_t) frame | PAGE_PRESENT | PAGE_WRITE;
    tables_made++;

    return table;
}

bool vmm_map(uint32_t virt, uint32_t phys, uint32_t flags)
{
    uint32_t *table = table_for(virt, true);

    if (table == NULL) {
        return false;
    }

    table[PAGE_TABLE_INDEX(virt)] =
        PAGE_FRAME(phys) | PAGE_OFFSET(flags) | PAGE_PRESENT;

    flush(virt);
    return true;
}

void vmm_unmap(uint32_t virt)
{
    uint32_t *table = table_for(virt, false);

    if (table == NULL) {
        return;
    }

    table[PAGE_TABLE_INDEX(virt)] = 0;
    flush(virt);
}

bool vmm_translate(uint32_t virt, uint32_t *out)
{
    /* The same first step the processor takes, and the same one vmm_map
     * takes, which is why it is a function rather than written out twice. */
    uint32_t *table = table_for(virt, false);

    if (table == NULL) {
        return false;
    }

    uint32_t page = table[PAGE_TABLE_INDEX(virt)];

    if (!(page & PAGE_PRESENT)) {
        return false;
    }

    *out = PAGE_FRAME(page) | PAGE_OFFSET(virt);
    return true;
}

void vmm_demand_region(uint32_t base, uint32_t bytes)
{
    demand_base = base;
    demand_end = base + bytes;
}

/* Vector 14. CR2 holds the address that could not be translated, and the
 * error code says why, which is the only place any of it is recorded: the
 * faulting instruction is unchanged and will be run again the moment this
 * returns. */
static void page_fault(struct registers *regs)
{
    uint32_t address = read_cr2();

    faults++;
    last_fault = address;

    if (demand_end > demand_base &&
        address >= demand_base && address < demand_end) {
        void *frame = pmm_alloc_frame();

        if (frame != NULL &&
            vmm_map(address, (uint32_t) frame,
                    PAGE_PRESENT | PAGE_WRITE)) {
            /* Return without touching eip. The instruction runs again, and
             * this time the translation is there. */
            return;
        }
    }

    const char *kind = (regs->error & PF_PROTECTION)
                     ? "protection" : "not present";
    const char *access = (regs->error & PF_WRITE)
                       ? "on a write" : "on a read";
    const char *origin = (regs->error & PF_USER)
                       ? "user code" : "the kernel";

    kprintf("\npage fault at 0x%08x, %s, %s, %s\n",
            address, kind, access, origin);

    panic_with("page fault", regs);
}

void vmm_init(void)
{
    memset(directory, 0, sizeof directory);
    tables_made = 0;

    /* The direct map: every byte of physical memory at its own address.
     *
     * It is what keeps the kernel working across the instruction that turns
     * paging on, since every address it is using stays valid, and it is what
     * lets the code above reach a page table by the physical address the
     * frame allocator returned. */
    for (uint32_t at = 0; at < pmm_top(); at += PAGE_SIZE) {
        if (!vmm_map(at, at, PAGE_PRESENT | PAGE_WRITE)) {
            panic("vmm_init: no frame for a page table");
        }
    }

    isr_install(14, page_fault);

    __asm__ volatile ("mov %0, %%cr3" :: "r" (directory));

    /* Read, set one bit, write back. cr0 also holds the protected-mode bit
     * Chapter 5 set, so writing a constant here would clear it. */
    uint32_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r" (cr0));
    cr0 |= CR0_PG;
    __asm__ volatile ("mov %0, %%cr0" :: "r" (cr0));
}

uint32_t vmm_directory(void)
{
    return (uint32_t) directory;
}

uint32_t vmm_table_count(void)
{
    return tables_made;
}

uint32_t vmm_fault_count(void)
{
    return faults;
}

uint32_t vmm_last_fault(void)
{
    return last_fault;
}
