/* pointers.c -- what a pointer holds, and how far adding one moves it for
 * each pointed-to type.
 *
 *     cc -o pointers pointers.c && ./pointers
 */
#include <stdio.h>
#include <stdint.h>

int main(void)
{
    uint8_t  bytes[4]  = { 10, 20, 30, 40 };
    uint32_t words[4]  = { 10, 20, 30, 40 };

    uint8_t  *b = bytes;
    uint32_t *w = words;

    printf("a pointer is just a number:\n");
    printf("  b     = %p\n", (void *) b);
    printf("  b + 1 = %p   (moved %td byte)\n",
           (void *) (b + 1), (char *) (b + 1) - (char *) b);
    printf("  w     = %p\n", (void *) w);
    printf("  w + 1 = %p   (moved %td bytes)\n",
           (void *) (w + 1), (char *) (w + 1) - (char *) w);
    printf("\n");
    printf("adding 1 moves by the size of what is pointed at, not by 1.\n");
    printf("\n");

    /* The same four bytes, read two ways. */
    uint32_t value = 0x44332211;
    uint8_t *first = (uint8_t *) &value;
    printf("0x%08X stored at %p reads back byte by byte as:\n",
           value, (void *) &value);
    printf("  ");
    for (int i = 0; i < 4; i++) {
        printf("%02X ", first[i]);
    }
    printf("\n  which is %s.\n",
           first[0] == 0x11 ? "little-endian: the low byte first"
                            : "big-endian: the high byte first");
    return 0;
}
