/* layout.c -- where the compiler places the fields of a struct, and what the
 * packed attribute changes.
 *
 * It matters more in a kernel than in an ordinary program: a structure handed
 * to the processor has to match the layout the hardware expects, byte for
 * byte.
 *
 *     cc -o layout layout.c && ./layout
 */
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

struct loose {
    uint8_t  type;
    uint32_t base;
    uint16_t limit;
};

struct tight {
    uint8_t  type;
    uint32_t base;
    uint16_t limit;
} __attribute__((packed));

int main(void)
{
    printf("struct loose: %zu bytes\n", sizeof(struct loose));
    printf("  type  at %zu\n", offsetof(struct loose, type));
    printf("  base  at %zu\n", offsetof(struct loose, base));
    printf("  limit at %zu\n", offsetof(struct loose, limit));
    printf("  the fields add up to %zu, so %zu bytes are padding\n\n",
           sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint16_t),
           sizeof(struct loose)
           - (sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint16_t)));

    printf("struct tight: %zu bytes\n", sizeof(struct tight));
    printf("  type  at %zu\n", offsetof(struct tight, type));
    printf("  base  at %zu\n", offsetof(struct tight, base));
    printf("  limit at %zu\n", offsetof(struct tight, limit));
    printf("  no padding, because we asked for none\n");
    return 0;
}
