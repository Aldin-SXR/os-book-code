/* spark -- kernel/printf.c
 *
 * Conversion of a format string and its arguments into characters, written
 * out through the screen driver.
 *
 * stdarg.h is available in a freestanding build because variable argument
 * lists are part of the language rather than the library.
 */
#include <stdarg.h>

#include "printf.h"
#include "vga.h"

static void pad(int count, char with)
{
    while (count-- > 0) {
        vga_putchar(with);
    }
}

/* Print value in the given base, right-aligned to at least width characters.
 * Digits are produced least significant first, so they are buffered and
 * emitted in reverse; the buffer is sized for the longest case, which is
 * thirty-two binary digits. */
static void print_unsigned(uint32_t value, unsigned base,
                           bool upper, int width, bool zero)
{
    const char *digits = upper ? "0123456789ABCDEF"
                               : "0123456789abcdef";
    char buffer[32];
    int length = 0;

    do {
        buffer[length++] = digits[value % base];
        value /= base;
    } while (value != 0);

    pad(width - length, zero ? '0' : ' ');
    while (length > 0) {
        vga_putchar(buffer[--length]);
    }
}

/* The negation avoids the one value it would be undefined for: INT32_MIN has
 * no positive counterpart, so -value overflows there. Negating value + 1 and
 * adding one back in unsigned arithmetic is defined for every input. */
static void print_signed(int32_t value, int width, bool zero)
{
    if (value < 0) {
        vga_putchar('-');
        print_unsigned((uint32_t) -(value + 1) + 1, 10, false,
                       width - 1, zero);
    } else {
        print_unsigned((uint32_t) value, 10, false, width, zero);
    }
}

void kprintf(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    for (size_t i = 0; format[i] != '\0'; i++) {
        if (format[i] != '%') {
            vga_putchar(format[i]);
            continue;
        }

        i++;
        bool zero = false;
        int width = 0;

        if (format[i] == '0') {
            zero = true;
            i++;
        }
        while (format[i] >= '0' && format[i] <= '9') {
            width = width * 10 + (format[i] - '0');
            i++;
        }

        switch (format[i]) {
        case 'd':
            print_signed(va_arg(args, int32_t), width, zero);
            break;
        case 'u':
            print_unsigned(va_arg(args, uint32_t), 10, false, width, zero);
            break;
        case 'x':
            print_unsigned(va_arg(args, uint32_t), 16, false, width, zero);
            break;
        case 'X':
            print_unsigned(va_arg(args, uint32_t), 16, true, width, zero);
            break;
        case 'p':
            vga_write("0x");
            print_unsigned((uint32_t) (uintptr_t) va_arg(args, void *),
                           16, false, 8, true);
            break;
        /* Retrieved as int: types narrower than int are promoted on their
         * way into a variable argument list, and asking va_arg for a char
         * reads the wrong width. */
        case 'c':
            vga_putchar((char) va_arg(args, int));
            break;
        case 's':
            vga_write(va_arg(args, const char *));
            break;
        case '%':
            vga_putchar('%');
            break;
        case '\0':
            i--;                 /* a % at the very end; do not run off it */
            break;
        default:
            vga_putchar('%');
            vga_putchar(format[i]);
            break;
        }
    }

    va_end(args);
}
