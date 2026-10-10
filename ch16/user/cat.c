/* spark -- user/cat.c
 *
 * Print a file. Two system calls and a buffer.
 */
#include "ulib.h"

int main(int argc, char **argv)
{
    char buf[512];

    if (argc < 2) {
        puts("cat: give me a path\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        u32 got = read_file(argv[i], buf, sizeof buf);

        if (got == SYS_ERROR) {
            puts("cat: no such file: ");
            puts(argv[i]);
            puts("\n");
            return 1;
        }

        write(buf, got);
    }
    return 0;
}
