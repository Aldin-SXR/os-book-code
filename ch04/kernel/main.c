/* spark -- kernel/main.c
 *
 * Exercise the screen driver: colours, wrapping, scrolling and the
 * conversions kprintf supports.
 */
#include "types.h"
#include "string.h"
#include "vga.h"
#include "printf.h"

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

#define RULE_WIDTH 44

static void rule(void)
{
    for (int i = 0; i < RULE_WIDTH; i++) {
        vga_putchar('-');
    }
    vga_putchar('\n');
}

void kmain(uint32_t magic, uint32_t boot_info)
{
    vga_init();

    /* Thirty lines onto a twenty-five line screen, which forces a scroll.
     * Doing this first leaves the evidence at the top of the finished screen
     * and the interesting part below it. */
    for (int i = 1; i <= 30; i++) {
        kprintf("line %d of 30, printed to make the screen scroll\n", i);
    }

    vga_set_colour(vga_colour(VGA_WHITE, VGA_BLACK));
    kprintf("spark, chapter 4: the screen\n");
    vga_set_colour(vga_colour(VGA_LIGHT_GREY, VGA_BLACK));
    rule();

    kprintf("%%d   %d and %d\n", -42, 42);
    kprintf("%%u   %u\n", 4294967254u);
    kprintf("%%x   %x, and %%08X gives %08X\n", 0xDEADBEEF, 0xBEEF);
    kprintf("%%c   %c%s\n", 'a', " string, via %s");
    kprintf("%%p   %p\n", (void *) 0x100000);
    kprintf("%%5d  [%5d], padded to five\n", 42);
    rule();

    kprintf("started by %s\n", loader_name(magic));
    kprintf("magic %p, boot info %p\n", (void *) magic, (void *) boot_info);
}
