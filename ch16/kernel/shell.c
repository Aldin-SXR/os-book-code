/* spark -- kernel/shell.c
 *
 * Read a line, split it, find the file, run it, wait.
 */
#include "shell.h"
#include "exec.h"
#include "fs.h"
#include "proc.h"
#include "sched.h"
#include "keyboard.h"
#include "vga.h"
#include "pmm.h"
#include "heap.h"
#include "timer.h"
#include "printf.h"
#include "string.h"

static uint32_t commands;

/* Whole-word comparison, because memcmp alone would make "helpme" match
 * "help" and run the wrong thing. */
static bool word_is(const char *word, const char *name)
{
    uint32_t n = strlen(name);

    return strlen(word) == n && memcmp(word, name, n) == 0;
}

static bool is_space(char c)
{
    return c == ' ' || c == '\t';
}

/* Split a line into words in place.
 *
 * The words are the line, with the spaces between them replaced by
 * terminators, so nothing is copied and every pointer points into the
 * caller's buffer. That is how a real shell does it too, and it is why
 * argv[0] and the command line are the same memory. */
static uint32_t split(char *line, char **argv, uint32_t max)
{
    uint32_t argc = 0;
    char *at = line;

    while (*at != '\0' && argc < max) {
        while (is_space(*at)) {
            *at++ = '\0';
        }

        if (*at == '\0') {
            break;
        }

        argv[argc++] = at;

        while (*at != '\0' && !is_space(*at)) {
            at++;
        }
    }
    return argc;
}

/* Take a trailing "> path" off the word list and return the inode to write
 * to, creating the file if it is not there.
 *
 * This runs before the program starts and the program is never told. That
 * is the whole of redirection, and Section 16.5 is about why it can be. */
static uint32_t take_redirect(char **argv, uint32_t *argc)
{
    if (*argc < 2 || strlen(argv[*argc - 2]) != 1 ||
        argv[*argc - 2][0] != '>') {
        return 0;
    }

    const char *path = argv[*argc - 1];

    *argc -= 2;

    uint32_t inum = fs_lookup(path);

    if (inum == 0) {
        /* fs_create takes a name in the root rather than a path, which is
         * as far as Chapter 15 went. */
        const char *bare = path[0] == '/' ? path + 1 : path;

        inum = fs_create(bare, "", 0);
    }
    return inum;
}

/* The commands that are the shell rather than a program in /bin.
 *
 * Two different things make a command a built-in, and spark has one of
 * each kind. A command that has to change the shell itself cannot be a
 * program, because a program would be changing its own copy and then
 * exiting; cd is the classic case, and it is why the section below exists.
 * A command that has to read state the kernel never hands out cannot be a
 * program either: sysnum.h has no call that returns a process table, a
 * frame count or a tick count, so a ring-3 program has no way to ask for
 * any of them. That is what ps, mem and uptime are.
 *
 * clear is the second kind too, for a smaller reason. A program can write
 * two thousand spaces, but it cannot put the cursor back afterwards,
 * because SYS_WRITE is the only thing it has and the cursor is not part of
 * what it writes.
 *
 * Returns true if it handled the line. */
static bool builtin(char **argv, uint32_t argc)
{
    if (word_is(argv[0], "help")) {
        kprintf("built in: help ps mem uptime clear\n");
        kprintf("the rest is in /bin\n");
        return true;
    }

    if (word_is(argv[0], "clear")) {
        vga_clear();
        return true;
    }

    if (word_is(argv[0], "uptime")) {
        /* The same counter twice: the ticks the handler has counted, and
         * what they come to in milliseconds at the rate the chip really
         * runs at rather than the rate Chapter 7 asked for. */
        kprintf("  up %u ms, over %u ticks\n",
                timer_uptime_ms(), timer_ticks());
        return true;
    }

    if (word_is(argv[0], "mem")) {
        struct heap_stats h;

        heap_get_stats(&h);

        kprintf("  frames  %u free of %u\n",
                pmm_free_frames(), pmm_total_frames());
        kprintf("  heap    %u used, %u free, %u in headers\n",
                h.used, h.free, h.overhead);
        return true;
    }

    if (word_is(argv[0], "ps")) {
        struct process *p = proc_first();

        do {
            kprintf("  %u %s %s\n", p->pid, p->name,
                    proc_state_name(p->state));
            p = p->next;
        } while (p != proc_first());

        return true;
    }

    (void) argc;
    return false;
}

void shell_run(const char *line, bool echo)
{
    char buf[SHELL_LINE];
    char *argv[EXEC_ARGS];
    char path[SHELL_WORD + 8];

    uint32_t n = 0;

    while (n + 1 < sizeof buf && line[n] != '\0') {
        buf[n] = line[n];
        n++;
    }

    buf[n] = '\0';

    if (echo) {
        kprintf("$ %s\n", buf);
    }

    uint32_t argc = split(buf, argv, EXEC_ARGS);

    if (argc == 0) {
        return;
    }

    commands++;

    if (builtin(argv, argc)) {
        return;
    }

    uint32_t out = take_redirect(argv, &argc);

    if (argc == 0) {
        return;
    }

    /* A bare name means a program in /bin, and a name with a slash in it is
     * a path. That is the whole of what a search path is, with one
     * directory in it. */
    const char *target = argv[0];

    if (argv[0][0] != '/') {
        uint32_t i = 0;

        memcpy(path, "/bin/", 5);

        while (i < SHELL_WORD && argv[0][i] != '\0') {
            path[5 + i] = argv[0][i];
            i++;
        }

        path[5 + i] = '\0';
        target = path;
    }

    struct process *p = exec_spawn(target, argv, argc, out);

    if (p == NULL) {
        kprintf("%s: not found\n", argv[0]);
        return;
    }

    /* Wait. The shell has nothing to do until the program finishes, which
     * is why an interactive shell is idle almost all of the time. */
    while (p->state != PROC_DONE) {
        proc_yield();
    }
}

void shell_loop(void)
{
    char line[SHELL_LINE];

    for (;;) {
        kprintf("$ ");

        size_t got = kbd_readline(line, sizeof line);

        line[got] = '\0';
        shell_run(line, false);
    }
}

uint32_t shell_commands(void)
{
    return commands;
}
