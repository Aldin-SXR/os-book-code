/* spark -- kernel/timer.h
 *
 * The 8253 programmable interval timer, channel 0.
 *
 * The counter is loaded with a divisor, counts down to zero, raises IRQ 0 and
 * reloads. It is the only mechanism by which the kernel regains the processor
 * from code that does not yield it, so every form of scheduling rests on it.
 */
#ifndef SPARK_TIMER_H
#define SPARK_TIMER_H

#include "types.h"

/* The input frequency, in hertz: the NTSC colour subcarrier crystal already
 * present in the original machine, divided by three. It is not a round
 * number, which is why a requested rate is rarely the rate delivered. */
#define PIT_FREQUENCY 1193182u

/* Channel 0 drives IRQ 0. Channels 1 and 2 served memory refresh and the
 * speaker; only the speaker outlived the original hardware. */
#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43

/* Start the timer at the requested rate and let IRQ 0 through.
 * The rate actually achieved is returned in millihertz, because
 * it is almost never the one asked for: the divisor is a whole
 * number and the crystal is not a round one. */
uint32_t timer_init(uint32_t hz);

/* Ticks since timer_init. The counter is written by the interrupt handler
 * and read by ordinary code, so every read of it is a read of shared
 * state. */
uint32_t timer_ticks(void);

/* Milliseconds since timer_init, computed from the rate the chip actually
 * runs at rather than the rate requested. */
uint32_t timer_uptime_ms(void);

/* Wait without spinning: the processor is halted between ticks and woken by
 * each one. */
void timer_sleep(uint32_t ms);

/* The divisor the chip was given, for printing. */
uint16_t timer_divisor(void);

/* Called on every tick, after the count is updated, from inside the
 * interrupt handler. This is the whole of the connection between Chapter 7's
 * timer and Chapter 13's scheduler: one function pointer. NULL removes it.
 *
 * Whatever is installed here runs with interrupts off and may switch to
 * another process and not return for a long time. */
void timer_hook(void (*fn)(void));

#endif
