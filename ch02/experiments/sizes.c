/* sizes.c -- the width of each built-in type on the host machine.
 *
 *     cc -o sizes sizes.c && ./sizes
 *
 * The figures are the host's, not spark's: the kernel is a 32-bit target and
 * the host is almost certainly 64-bit, which is what makes the fixed-width
 * types necessary.
 */
#include <stdio.h>
#include <stdint.h>

int main(void)
{
    printf("type            bytes   bits\n");
    printf("char            %5zu   %4zu\n", sizeof(char),      sizeof(char) * 8);
    printf("short           %5zu   %4zu\n", sizeof(short),     sizeof(short) * 8);
    printf("int             %5zu   %4zu\n", sizeof(int),       sizeof(int) * 8);
    printf("long            %5zu   %4zu\n", sizeof(long),      sizeof(long) * 8);
    printf("long long       %5zu   %4zu\n", sizeof(long long), sizeof(long long) * 8);
    printf("void *          %5zu   %4zu\n", sizeof(void *),    sizeof(void *) * 8);
    printf("size_t          %5zu   %4zu\n", sizeof(size_t),    sizeof(size_t) * 8);
    printf("\n");
    printf("uint8_t         %5zu   %4zu\n", sizeof(uint8_t),   sizeof(uint8_t) * 8);
    printf("uint16_t        %5zu   %4zu\n", sizeof(uint16_t),  sizeof(uint16_t) * 8);
    printf("uint32_t        %5zu   %4zu\n", sizeof(uint32_t),  sizeof(uint32_t) * 8);
    printf("uint64_t        %5zu   %4zu\n", sizeof(uint64_t),  sizeof(uint64_t) * 8);
    return 0;
}
