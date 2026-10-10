/* spark -- user/echo.c
 *
 * The smallest useful program there is, and it is a real one: loaded from
 * the disk, running in ring 3, reaching the screen only by asking.
 */
#include "ulib.h"

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (i > 1) {
            puts(" ");
        }
        puts(argv[i]);
    }

    puts("\n");
    return 0;
}
