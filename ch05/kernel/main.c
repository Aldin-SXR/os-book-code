/* spark -- kernel/main.c
 *
 * Report the descriptor table the kernel was handed, install one of its own,
 * and report that.
 */
#include "types.h"
#include "string.h"
#include "vga.h"
#include "printf.h"
#include "gdt.h"

#define MULTIBOOT_MAGIC 0x2BADB002
#define SPARK_BOOT_MAGIC 0xB007AC05

static const char *loader_name(uint32_t magic)
{
    switch (magic) {
    case MULTIBOOT_MAGIC: return "a Multiboot loader";
    case SPARK_BOOT_MAGIC: return "our own boot sector";
    default:              return "somebody who did not say";
    }
}

#define RULE_WIDTH 62

static void rule(void)
{
    for (int i = 0; i < RULE_WIDTH; i++) {
        vga_putchar('-');
    }
    vga_putchar('\n');
}

/* The processor's current segmentation state, read back from it. Called
 * before and after gdt_init so that the handover is visible. */
static void report(const char *when)
{
    struct gdt_ptr current;
    gdt_current(&current);

    uint16_t cs = gdt_read_cs();

    kprintf("%s\n", when);
    kprintf("  gdtr  base %p  limit %04X  (%u entries)\n",
            (void *) current.base, current.limit,
            (unsigned) ((current.limit + 1u) / 8u));
    kprintf("  cs %04X  ds %04X  running in ring %u\n",
            cs, gdt_read_ds(), (unsigned) (cs & 3));
}

/* Print the table the processor is using, entry by entry, as gdt_describe
 * decodes it. */
static void print_table(void)
{
    kprintf("  #  sel   base        limit       ring  what\n");

    for (unsigned i = 0; i < 8; i++) {
        uint32_t base, limit;
        unsigned ring;
        const char *kind;

        if (!gdt_describe(i, &base, &limit, &ring, &kind)) {
            break;
        }
        kprintf("  %u  0x%02X  0x%08X  0x%08X  %u     %s\n",
                i, i * 8, base, limit, ring, kind);
    }
}

void kmain(uint32_t magic, uint32_t boot_info)
{
    (void) boot_info;

    vga_init();
    vga_set_colour(vga_colour(VGA_WHITE, VGA_BLACK));
    kprintf("spark, chapter 5: protected mode and the gdt\n");
    vga_set_colour(vga_colour(VGA_LIGHT_GREY, VGA_BLACK));
    rule();

    report("the table we were handed");
    kprintf("\n");

    gdt_init();

    report("the table we installed");
    rule();

    print_table();
    rule();

    kprintf("started by %s, and the far jump worked\n",
            loader_name(magic));
}
