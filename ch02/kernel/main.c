/* spark -- kernel/main.c
 *
 * The entry point, and the order in which the kernel brings itself up.
 *
 * The first thing it does is test its own memory functions. Everything above
 * them depends on them, and they are short enough to be subtly wrong.
 */
#include "types.h"
#include "string.h"
#include "vga.h"

static int row;
static uint8_t plain;

static void report(const char *name, bool passed)
{
    vga_write(row, 0, name, plain);
    vga_write(row, 10, passed ? "ok" : "FAILED",
              passed ? plain : vga_colour(VGA_WHITE, 4));
    row++;
}

static bool check_memset(void)
{
    uint8_t buffer[8];
    memset(buffer, 0xAB, sizeof buffer);
    for (size_t i = 0; i < sizeof buffer; i++) {
        if (buffer[i] != 0xAB) {
            return false;
        }
    }
    return true;
}

static bool check_memcpy(void)
{
    const char source[] = "spark";
    char copy[sizeof source];
    memcpy(copy, source, sizeof source);
    return memcmp(copy, source, sizeof source) == 0;
}

/* The case memcpy is undefined for: overlapping regions with the destination
 * above the source, where a forward copy would read bytes it has already
 * overwritten. */
static bool check_memmove(void)
{
    char buffer[8] = "abcdefg";
    memmove(buffer + 2, buffer, 5);
    return memcmp(buffer, "ababcde", 8) == 0;
}

static bool check_memcmp(void)
{
    return memcmp("ab", "ab", 2) == 0
        && memcmp("ab", "ac", 2) < 0
        && memcmp("ac", "ab", 2) > 0;
}

static bool check_strlen(void)
{
    return strlen("") == 0 && strlen("hello") == 5;
}

void kmain(void)
{
    plain = vga_colour(VGA_WHITE, VGA_BLACK);
    vga_clear(plain);

    vga_write(0, 0, "spark, now in pieces.", plain);
    row = 2;

    report("memset",  check_memset());
    report("memcpy",  check_memcpy());
    report("memmove", check_memmove());
    report("memcmp",  check_memcmp());
    report("strlen",  check_strlen());
}
