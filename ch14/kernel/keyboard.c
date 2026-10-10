/* spark -- kernel/keyboard.c
 *
 * Scancode translation, the buffer between the handler and ordinary code,
 * and line input built on top of both.
 */
#include "keyboard.h"
#include "io.h"
#include "isr.h"
#include "pic.h"
#include "vga.h"

/* Scancode set 1, which the controller still produces whatever the keyboard
 * is, because everything downstream of it expects that set.
 *
 * Indexed by scancode; the value is the character the key produces. Zero
 * marks a key with no character of its own: a modifier, a function key, or a
 * code this driver does not handle. The layout is US; another layout is a
 * different table and no other change. */
static const char plain[128] = {
    0,   27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', '0', '-', '=','\b','\t',
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
  'o', 'p', '[', ']','\n',   0, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
 '\'', '`',   0,'\\', 'z', 'x', 'c', 'v',
  'b', 'n', 'm', ',', '.', '/',   0, '*',
    0, ' ',
};

/* The same keys with shift held. A second table rather than arithmetic on
 * the first: the shifted digits follow no rule that could be computed. */
static const char shifted[128] = {
    0,   27, '!', '@', '#', '$', '%', '^',
  '&', '*', '(', ')', '_', '+','\b','\t',
  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
  'O', 'P', '{', '}','\n',   0, 'A', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
  '"', '~',   0, '|', 'Z', 'X', 'C', 'V',
  'B', 'N', 'M', '<', '>', '?',   0, '*',
    0, ' ',
};

/* The scancodes that carry no character. A release repeats the press code
 * with bit 7 set, so every code here is below 0x80 and a release is one bit
 * test away. */
#define SC_LSHIFT   0x2A
#define SC_RSHIFT   0x36
#define SC_CAPSLOCK 0x3A
#define SC_RELEASE  0x80
#define SC_EXTENDED 0xE0

/* A ring buffer written by the handler and read by ordinary code. Both
 * indices are volatile: each side changes one behind the other's back.
 *
 * The size is a power of two so that wrapping is cheap. */
#define RING 64
static volatile char ring[RING];
static volatile uint32_t head;   /* the handler writes here */
static volatile uint32_t tail;   /* readers take from here  */

static volatile uint32_t scancodes;

static bool shift;
static bool caps;
static bool extended;

static void push(char c)
{
    uint32_t next = (head + 1) % RING;

    /* A full buffer means the reader has fallen behind. The new character
     * is dropped rather than overwriting one the reader has not taken:
     * losing the end of the input is recoverable, silently rewriting it is
     * not. */
    if (next != tail) {
        ring[head] = c;
        head = next;
    }
}

/* Translate one scancode into at most one character, maintaining the
 * modifier state as it goes. */
static void translate(uint8_t code)
{
    if (code == SC_EXTENDED) {
        extended = true;
        return;
    }

    /* Extended keys arrive as 0xE0 followed by a code that means something
     * else in the plain table. The driver has no use for them, so the pair
     * is discarded rather than mistranslated. */
    if (extended) {
        extended = false;
        return;
    }

    bool released = (code & SC_RELEASE) != 0;
    uint8_t key = code & 0x7F;

    if (key == SC_LSHIFT || key == SC_RSHIFT) {
        shift = !released;
        return;
    }
    if (key == SC_CAPSLOCK) {
        if (!released) {
            caps = !caps;
        }
        return;
    }
    if (released) {
        return;
    }

    char c = shift ? shifted[key] : plain[key];

    /* Caps lock is not a second shift: it applies to letters only, so it
     * cannot be folded into the table lookup above. */
    if (caps && c >= 'a' && c <= 'z') {
        c = (char) (c - 'a' + 'A');
    } else if (caps && shift && c >= 'A' && c <= 'Z') {
        c = (char) (c - 'A' + 'a');
    }

    if (c) {
        push(c);
    }
}

static void on_key(struct registers *regs)
{
    (void) regs;

    /* One byte per interrupt, and the read is what empties the
     * controller's output buffer. Without it the buffer stays full, the
     * empty-to-full transition never recurs, and no further interrupt is
     * raised. */
    scancodes++;
    translate(inb(KBD_DATA));
}

void kbd_init(void)
{
    /* The firmware may have left a byte in the output buffer. While it sits
     * there the controller raises no interrupt, so the driver would appear
     * dead until something else drained it. */
    while (inb(KBD_STATUS) & KBD_OUTPUT_FULL) {
        (void) inb(KBD_DATA);
    }

    head = tail = 0;
    scancodes = 0;

    isr_install(IRQ(1), on_key);
    pic_unmask(1);
}

bool kbd_haschar(void)
{
    return head != tail;
}

char kbd_getchar(void)
{
    while (!kbd_haschar()) {
        __asm__ volatile ("hlt");
    }

    char c = ring[tail];
    tail = (tail + 1) % RING;
    return c;
}

uint32_t kbd_scancodes(void)
{
    return scancodes;
}

size_t kbd_readline(char *buf, size_t size)
{
    size_t n = 0;

    for (;;) {
        char c = kbd_getchar();

        if (c == '\n') {
            vga_putchar('\n');
            break;
        }
        if (c == '\b') {
            if (n > 0) {
                n--;
                vga_backspace();
            }
            continue;
        }
        if (n + 1 < size) {
            buf[n++] = c;
            vga_putchar(c);
        }
    }

    buf[n] = '\0';
    return n;
}

char kbd_poll_char(uint32_t *asked)
{
    uint32_t n = 0;

    /* Each iteration is one port read that answers "nothing yet"; the count
     * is what makes the cost of polling visible. */
    while (!(inb(KBD_STATUS) & KBD_OUTPUT_FULL)) {
        n++;
    }

    uint8_t code = inb(KBD_DATA);

    if (asked) {
        *asked += n;
    }

    bool released = (code & SC_RELEASE) != 0;
    return released ? 0 : plain[code & 0x7F];
}
