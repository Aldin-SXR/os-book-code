/* spark -- kernel/pic.h
 *
 * The pair of 8259A interrupt controllers: sixteen device lines, cascaded,
 * each mapped to a vector in the IDT.
 *
 * The firmware leaves lines 0 to 7 on vectors 8 to 15, which the processor
 * has already reserved for its own exceptions: vector 8 is the double fault
 * and 13 the general protection fault. A timer tick would then arrive through
 * the double fault's gate, with nothing in the frame to say which it was.
 * Remapping both chips clear of that range is this module's first job.
 */
#ifndef SPARK_PIC_H
#define SPARK_PIC_H

#include "types.h"

/* Each chip answers on two ports. The data port's meaning depends on where
 * the chip is in its initialisation sequence. */
#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

/* Remap the sixteen lines onto the two vector ranges given and mask them all.
 * Each driver unmasks the line it services. */
void pic_init(uint8_t offset1, uint8_t offset2);

/* Let one line through, or stop it. */
void pic_unmask(uint8_t irq);
void pic_mask(uint8_t irq);

/* The masks as they stand, chip 1 in the low byte. */
uint16_t pic_masks(void);

/* Acknowledge an interrupt. Until this is sent the chip holds the line in
 * service and raises neither it nor any lower-numbered line again: a handler
 * that omits it runs exactly once, and the device then looks dead. */
void pic_eoi(uint8_t irq);

#endif
