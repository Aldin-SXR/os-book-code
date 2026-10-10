/* spark -- kernel/timer.c
 *
 * Programming the PIT, counting its ticks, and the two things the count is
 * used for: uptime and sleeping.
 */
#include "timer.h"
#include "io.h"
#include "isr.h"
#include "pic.h"

/* Written by the interrupt handler and read by ordinary code. volatile is
 * required: without it the compiler may hoist the read out of a wait loop and
 * spin on a value that can no longer change. */
static volatile uint32_t ticks;

/* The requested rate, kept because the arithmetic below needs a whole number
 * of ticks per second and the delivered rate is not one. */
static uint32_t rate_hz = 1;
static uint16_t divisor;

/* Fields of the command byte.
 *
 *   bits 7..6  counter to program; 0 is the one wired to IRQ 0
 *   bits 5..4  access mode; 11 sends the divisor low byte then high
 *   bits 3..1  counting mode; 2 is rate generator, which reloads itself
 *   bit  0     0 selects binary counting, 1 selects BCD
 */
#define PIT_SELECT_CH0  (0 << 6)
#define PIT_ACCESS_BOTH (3 << 4)
#define PIT_MODE_RATE   (2 << 1)

_Static_assert(PIT_FREQUENCY * 1000ull < 0xFFFFFFFFull,
               "the millihertz calculation stays in 32 bits");

static void on_tick(struct registers *regs)
{
    (void) regs;
    ticks++;
}

uint32_t timer_init(uint32_t hz)
{
    /* Round to nearest rather than truncating: the same cost, half the
     * error. */
    uint32_t d = (PIT_FREQUENCY + hz / 2) / hz;

    /* The counter is sixteen bits, and a written zero means 65536: the
     * slowest the chip runs, about 18.2 Hz, which is where the firmware
     * leaves it. */
    if (d > 65536) {
        d = 65536;
    }
    if (d < 1) {
        d = 1;
    }

    divisor = (uint16_t) (d & 0xFFFF);
    rate_hz = hz;
    ticks = 0;

    outb(PIT_COMMAND, PIT_SELECT_CH0 | PIT_ACCESS_BOTH |
                      PIT_MODE_RATE);
    outb(PIT_CHANNEL0, (uint8_t) (divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t) (divisor >> 8));

    isr_install(IRQ(0), on_tick);
    pic_unmask(0);

    /* The delivered rate in millihertz. A whole number of hertz would
     * misreport it: the divisor is an integer and the crystal is not a
     * multiple of the rates anyone asks for.
     *
     * The product stays inside 32 bits, which the assertion above enforces.
     * A 64-bit division here would emit a call to __udivdi3, which is in a
     * runtime library this kernel does not link. */
    return (PIT_FREQUENCY * 1000u) / d;
}

uint32_t timer_ticks(void)
{
    return ticks;
}

uint16_t timer_divisor(void)
{
    return divisor;
}

/* Whole seconds first, then the remainder.
 *
 * The direct ticks * 1000 / rate overflows 32 bits after about 4.3 million
 * ticks, which is twelve hours at 100 Hz. Splitting the division keeps every
 * intermediate value in range. */
uint32_t timer_uptime_ms(void)
{
    uint32_t t = ticks;

    return (t / rate_hz) * 1000u
           + (t % rate_hz) * 1000u / rate_hz;
}

void timer_sleep(uint32_t ms)
{
    uint32_t wanted = (ms * rate_hz + 999u) / 1000u;
    uint32_t target = ticks + wanted;

    /* hlt stops the processor until an interrupt arrives, and one arrives
     * every tick, so the wait costs no execution time. It depends on the
     * timer: with IRQ 0 masked this would halt the machine permanently. */
    while (ticks < target) {
        __asm__ volatile ("hlt");
    }
}
