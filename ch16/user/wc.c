/* spark -- user/wc.c
 *
 * Count lines, words and bytes, which is the program every shell chapter
 * needs because it turns a file into a number.
 */
#include "ulib.h"

int main(int argc, char **argv)
{
    char buf[512];

    if (argc < 2) {
        puts("wc: give me a path\n");
        return 1;
    }

    u32 got = read_file(argv[1], buf, sizeof buf);

    if (got == SYS_ERROR) {
        puts("wc: no such file\n");
        return 1;
    }

    u32 lines = 0;
    u32 words = 0;
    u32 in_word = 0;

    for (u32 i = 0; i < got; i++) {
        char c = buf[i];

        if (c == '\n') {
            lines++;
        }

        if (c == ' ' || c == '\n' || c == '\t') {
            in_word = 0;
        } else if (!in_word) {
            in_word = 1;
            words++;
        }
    }

    putn(lines);
    puts(" ");
    putn(words);
    puts(" ");
    putn(got);
    puts("\n");
    return 0;
}
