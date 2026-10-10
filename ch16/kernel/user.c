/* spark -- kernel/user.c
 *
 * Opening a hole in the wall, exactly as wide as it has to be.
 */
#include "user.h"
#include "vmm.h"
#include "pmm.h"
#include "panic.h"

/* Placed by link.ld at the two ends of the .user section, both on page
 * boundaries. They are addresses, not variables, which is why they are
 * declared as arrays and used without taking a value out of them. */
extern char _user_start[];
extern char _user_end[];

static uint32_t stacks_given;

/* The one extra region, registered by exec_init. */
static uint32_t extra_base;
static uint32_t extra_end;

void user_init(void)
{
    uint32_t base = (uint32_t) (uintptr_t) _user_start;
    uint32_t end  = (uint32_t) (uintptr_t) _user_end;

    /* The pages are already mapped, by Chapter 11's direct map, which is why
     * this remaps the same frame at the same address rather than allocating
     * anything. What changes is one bit of the entry.
     *
     * No PAGE_WRITE. A user program may read its own code and the strings
     * in it, and may not modify either, which is what stops it rewriting an
     * instruction the kernel is about to check. */
    for (uint32_t at = base; at < end; at += PAGE_SIZE) {
        if (!vmm_map(at, at, PAGE_PRESENT | PAGE_USER)) {
            panic("user_init: cannot map the user section");
        }
    }
}

uint32_t user_stack_alloc(void)
{
    if (stacks_given >= USER_STACKS) {
        return 0;
    }

    uint32_t base = USER_STACK_BASE +
                    stacks_given * USER_STACK_BYTES;
    void *frame = pmm_alloc_frame();

    if (frame == NULL) {
        return 0;
    }

    if (!vmm_map(base, (uint32_t) frame,
                 PAGE_PRESENT | PAGE_WRITE | PAGE_USER)) {
        return 0;
    }

    stacks_given++;

    /* One past the end. A stack grows downwards, so the first push lands on
     * the last word of the page. */
    return base + USER_STACK_BYTES;
}

void user_allow(uint32_t base, uint32_t bytes)
{
    extra_base = base;
    extra_end = base + bytes;
}

bool user_may_read(uint32_t addr, uint32_t len)
{
    if (len == 0) {
        return true;
    }

    /* Computed this way rather than as addr + len, which wraps to something
     * small when a program passes a length near 0xFFFFFFFF and then passes
     * the test it should have failed. */
    uint32_t last = addr + (len - 1);

    if (last < addr) {
        return false;
    }

    uint32_t text_base = (uint32_t) (uintptr_t) _user_start;
    uint32_t text_end  = (uint32_t) (uintptr_t) _user_end;

    if (addr >= text_base && last < text_end) {
        return true;
    }

    uint32_t stack_end = USER_STACK_BASE +
                         stacks_given * USER_STACK_BYTES;

    if (addr >= USER_STACK_BASE && last < stack_end) {
        return true;
    }

    if (extra_end > extra_base &&
        addr >= extra_base && last < extra_end) {
        return true;
    }

    return false;
}

uint32_t user_text_base(void)
{
    return (uint32_t) (uintptr_t) _user_start;
}

uint32_t user_text_end(void)
{
    return (uint32_t) (uintptr_t) _user_end;
}
