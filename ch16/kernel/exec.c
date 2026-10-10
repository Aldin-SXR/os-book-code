/* spark -- kernel/exec.c
 *
 * Four steps: find it, put it somewhere, build a stack, run it.
 */
#include "exec.h"
#include "fs.h"
#include "vmm.h"
#include "pmm.h"
#include "user.h"
#include "string.h"
#include "panic.h"

static bool ready;
static uint32_t stack_top;
static uint32_t last_size;

void exec_init(void)
{
    /* The load region is mapped once and reused, because only one program
     * runs at a time. A kernel whose processes had address spaces of their
     * own would map this per process and could then run several. */
    for (uint32_t i = 0; i < EXEC_PAGES; i++) {
        void *frame = pmm_alloc_frame();

        if (frame == NULL) {
            panic("exec_init: no frame for the load region");
        }

        uint32_t at = EXEC_BASE + i * PAGE_SIZE;

        if (!vmm_map(at, (uint32_t) frame,
                     PAGE_PRESENT | PAGE_WRITE | PAGE_USER)) {
            panic("exec_init: cannot map the load region");
        }
    }

    user_allow(EXEC_BASE, EXEC_PAGES * PAGE_SIZE);

    stack_top = user_stack_alloc();

    if (stack_top == 0) {
        panic("exec_init: no stack for programs");
    }

    ready = true;
}

/* Put the arguments where the program expects to find them.
 *
 * A program starts with its stack already holding argc and a pointer to an
 * array of pointers to its arguments, and every one of those addresses has
 * to be an address the program can read. So the strings are copied into the
 * program's own stack first, and the array is built out of where they
 * landed. Returns the stack pointer the program should start on. */
static uint32_t build_stack(char **argv, uint32_t argc)
{
    uint32_t placed[EXEC_ARGS];
    uint32_t sp = stack_top;

    for (uint32_t i = argc; i > 0; i--) {
        uint32_t len = strlen(argv[i - 1]) + 1;

        sp -= len;
        sp &= ~3u;
        memcpy((void *) (uintptr_t) sp, argv[i - 1], len);
        placed[i - 1] = sp;
    }

    sp -= (argc + 1) * sizeof(uint32_t);
    sp &= ~3u;

    uint32_t *array = (uint32_t *) (uintptr_t) sp;

    for (uint32_t i = 0; i < argc; i++) {
        array[i] = placed[i];
    }

    array[argc] = 0;        /* the NULL argv ends with */

    uint32_t argv_at = sp;

    /* What _start reads: argc at the top, then the array. */
    sp -= 2 * sizeof(uint32_t);

    uint32_t *frame = (uint32_t *) (uintptr_t) sp;

    frame[0] = argc;
    frame[1] = argv_at;
    return sp;
}

struct process *exec_spawn(const char *path,
                           char **argv, uint32_t argc,
                           uint32_t out_inode)
{
    struct fs_inode node;

    if (!ready || argc == 0 || argc > EXEC_ARGS) {
        return NULL;
    }

    uint32_t inum = fs_lookup(path);

    if (inum == 0 || !fs_stat(inum, &node) ||
        node.type != FS_FILE) {
        return NULL;
    }

    if (node.size == 0 ||
        node.size > EXEC_PAGES * PAGE_SIZE) {
        return NULL;
    }

    /* Zero the whole region before reading the file into the front of it.
     * The part past the end of the file is the program's .bss, which
     * user/link.ld puts last and --oformat=binary leaves out of the file,
     * so the only thing making it zero is this. */
    memset((void *) EXEC_BASE, 0, EXEC_PAGES * PAGE_SIZE);

    if (fs_read(inum, 0, (void *) EXEC_BASE,
                node.size) != node.size) {
        return NULL;
    }

    last_size = node.size;

    uint32_t sp = build_stack(argv, argc);
    struct process *p =
        proc_create_program(argv[0], EXEC_BASE, sp);

    if (p == NULL) {
        return NULL;
    }

    p->out_inode = out_inode;
    return p;
}

uint32_t exec_last_size(void)
{
    return last_size;
}
