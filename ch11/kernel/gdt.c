/* spark -- kernel/gdt.c
 *
 * Building the descriptor table, loading it, and reading back the one the
 * processor is actually using.
 */
#include "gdt.h"
#include "string.h"

/* One descriptor, in the layout the processor reads: eight bytes, with the
 * base and the limit each split across several of them.
 *
 * The split is inherited. The 80286 descriptor was six bytes around a 24-bit
 * base and a 16-bit limit, and the 80386 added eight more bits of base, four
 * more of limit and four flag bits without moving any existing field. Every
 * field that looks misplaced occupies the two bytes the 286 left unused. */
struct gdt_entry {
    uint16_t limit_low;    /* limit, bits 15..0            */
    uint16_t base_low;     /* base,  bits 15..0            */
    uint8_t  base_mid;     /* base,  bits 23..16           */
    uint8_t  access;       /* present, ring, type          */
    uint8_t  flags_limit;  /* flags 7..4, limit 19..16     */
    uint8_t  base_high;    /* base,  bits 31..24           */
} __attribute__((packed));

_Static_assert(sizeof(struct gdt_entry) == 8, "eight bytes");

/* Fields of the access byte. */
#define ACC_PRESENT    (1 << 7)  /* 0 faults when used      */
#define ACC_DPL(n)     ((n) << 5)/* the ring that may use it*/
#define ACC_CODE_DATA  (1 << 4)  /* 0 is a system entry     */
#define ACC_EXECUTABLE (1 << 3)  /* code if set, else data  */
#define ACC_DIRECTION  (1 << 2)  /* conforming or downward  */
#define ACC_RW         (1 << 1)  /* readable, or writable   */
#define ACC_ACCESSED   (1 << 0)  /* the processor sets this */

/* Fields of the flags nibble, the upper half of byte 6. */
#define FLG_GRANULAR   (1 << 3)  /* limit counts 4 KiB pages*/
#define FLG_32BIT      (1 << 2)  /* 32-bit code and data    */

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr   gdt_ptr;

/* Defined in gdt_flush.asm: a far jump has no expression in C. */
extern void gdt_flush(const struct gdt_ptr *ptr);

/* Fill in one entry from the four values that vary between them.
 *
 * limit is the address of the last byte inside the segment, so a segment
 * covering the whole address space is written as 0xFFFFFFFF rather than as a
 * length. */
static void set_entry(int index,
                      uint32_t base, uint32_t limit,
                      uint8_t access, uint8_t flags)
{
    /* The limit field is twenty bits, which cannot express 0xFFFFFFFF. The
     * granularity bit makes it count 4 KiB units instead: 0xFFFFF of them is
     * 4 GiB, and the low twelve bits of the effective limit are then all
     * ones. Every segment here ends one byte short of a page boundary, so
     * the conversion is exact. */
    if (flags & FLG_GRANULAR) {
        limit >>= 12;
    }

    gdt[index].limit_low   = (uint16_t) (limit & 0xFFFF);
    gdt[index].base_low    = (uint16_t) (base & 0xFFFF);
    gdt[index].base_mid    = (uint8_t) (base >> 16);
    gdt[index].access      = access;
    gdt[index].flags_limit =
        (uint8_t) (((limit >> 16) & 0x0F) |
                   ((flags & 0x0F) << 4));
    gdt[index].base_high   = (uint8_t) (base >> 24);
}

void gdt_init(void)
{
    const uint8_t flags = FLG_GRANULAR | FLG_32BIT;
    const uint8_t code  = ACC_PRESENT | ACC_CODE_DATA |
                          ACC_EXECUTABLE | ACC_RW;
    const uint8_t data  = ACC_PRESENT | ACC_CODE_DATA |
                          ACC_RW;

    /* Entry 0 must be all zeros and cannot be used. A segment register that
     * was never initialised holds 0, and the processor faults as soon as one
     * is used to reach memory; without the null entry the same mistake would
     * silently address the bottom of memory. */
    memset(&gdt[0], 0, sizeof gdt[0]);

    set_entry(1, 0, 0xFFFFFFFF, code | ACC_DPL(0), flags);
    set_entry(2, 0, 0xFFFFFFFF, data | ACC_DPL(0), flags);

    /* The same two segments at ring 3, for user code. They cost sixteen
     * bytes and save rebuilding the table when ring 3 is first used. */
    set_entry(3, 0, 0xFFFFFFFF, code | ACC_DPL(3), flags);
    set_entry(4, 0, 0xFFFFFFFF, data | ACC_DPL(3), flags);

    gdt_ptr.limit = (uint16_t) (sizeof gdt - 1);
    gdt_ptr.base  = (uint32_t) (uintptr_t) gdt;

    gdt_flush(&gdt_ptr);
}

void gdt_current(struct gdt_ptr *out)
{
    __asm__ volatile ("sgdt %0" : "=m"(*out));
}

uint16_t gdt_read_cs(void)
{
    uint16_t value;
    __asm__ volatile ("mov %%cs, %0" : "=r"(value));
    return value;
}

uint16_t gdt_read_ds(void)
{
    uint16_t value;
    __asm__ volatile ("mov %%ds, %0" : "=r"(value));
    return value;
}

/* Reassemble one entry of whichever table the processor is using.
 *
 * This is set_entry inverted, and it reads through GDTR rather than through
 * the array above, so it describes an inherited table as readily as spark's
 * own: what the machine is running under, not what was intended. */
bool gdt_describe(unsigned index,
                  uint32_t *base, uint32_t *limit,
                  unsigned *ring, const char **kind)
{
    struct gdt_ptr current;
    gdt_current(&current);

    unsigned end = (index + 1) * sizeof(struct gdt_entry);
    if (end > (unsigned) current.limit + 1) {
        return false;
    }

    const struct gdt_entry *e =
        (const struct gdt_entry *) (uintptr_t) current.base;
    e += index;

    *base = (uint32_t) e->base_low |
            (uint32_t) e->base_mid << 16 |
            (uint32_t) e->base_high << 24;

    *limit = (uint32_t) e->limit_low |
             ((uint32_t) (e->flags_limit & 0x0F) << 16);
    if (e->flags_limit & (FLG_GRANULAR << 4)) {
        *limit = *limit << 12 | 0xFFF;
    }

    *ring = (e->access >> 5) & 3;

    if (!(e->access & ACC_PRESENT)) {
        *kind = "null";
    } else if (!(e->access & ACC_CODE_DATA)) {
        *kind = "system";
    } else if (e->access & ACC_EXECUTABLE) {
        *kind = "code";
    } else {
        *kind = "data";
    }
    return true;
}
