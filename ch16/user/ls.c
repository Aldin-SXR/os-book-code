/* spark -- user/ls.c
 *
 * List the root directory. The kernel walks the blocks; this asks for one
 * entry at a time and stops when it is told there are no more.
 */
#include "ulib.h"

int main(int argc, char **argv)
{
    char name[28];
    u32 n = 0;

    (void) argc;
    (void) argv;

    while (list(n, name) != 0) {
        puts(name);
        puts("\n");
        n++;
    }

    putn(n);
    puts(" entries\n");
    return 0;
}
