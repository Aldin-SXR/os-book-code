/* spark -- kernel/main.c
 *
 * The entry point. The kernel can be started by a Multiboot loader or by
 * spark's own boot sector, and it reports which of the two started it.
 */
#include "types.h"
#include "string.h"
#include "vga.h"

/* A loader leaves a number in eax identifying itself. The first is fixed by
 * the Multiboot specification; the second is spark's own, chosen in
 * bootsect.asm, and means nothing outside this kernel. */
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

/* The memory-function self-tests, reduced to a single answer. They run at
 * every boot because the rest of the kernel depends on them. */
static bool strings_work(void)
{
    uint8_t filled[8];
    static const char word[] = "spark";
    char copied[sizeof word];
    char moved[8] = "abcdefg";

    memset(filled, 0xAB, sizeof filled);
    for (size_t i = 0; i < sizeof filled; i++) {
        if (filled[i] != 0xAB) {
            return false;
        }
    }

    memcpy(copied, word, sizeof word);
    memmove(moved + 2, moved, 5);

    return memcmp(copied, word, sizeof word) == 0
        && memcmp(moved, "ababcde", 8) == 0
        && memcmp("ab", "ac", 2) < 0
        && strlen("hello") == 5;
}

void kmain(uint32_t magic, uint32_t boot_info)
{
    const uint8_t plain = vga_colour(VGA_WHITE, VGA_BLACK);
    const uint8_t bad = vga_colour(VGA_WHITE, 4);

    vga_clear(plain);
    vga_write(0, 0, "spark, off a disk of its own.", plain);

    vga_write(2, 0, "started by", plain);
    vga_write(2, 16, loader_name(magic), plain);

    vga_write(3, 0, "magic in eax", plain);
    vga_write_hex(3, 16, magic, plain);

    vga_write(4, 0, "ebx", plain);
    vga_write_hex(4, 16, boot_info, plain);

    const bool ok = strings_work();
    vga_write(6, 0, "string.c", plain);
    vga_write(6, 16, ok ? "self-test ok" : "SELF-TEST FAILED", ok ? plain : bad);
}
