/* spark -- kernel/pic.c
 *
 * Initialisation, masking and acknowledgement for the two 8259A controllers.
 */
#include "pic.h"
#include "io.h"

/* The initialisation control words. Once ICW1 reaches the command port the
 * chip reads the next three bytes written to its data port as ICW2, ICW3 and
 * ICW4 in that order, whatever their values. */
#define ICW1_INIT  0x10   /* this is an initialisation sequence */
#define ICW1_ICW4  0x01   /* a fourth word follows              */
#define ICW4_8086  0x01   /* 8086 mode, rather than the 8080's  */

#define PIC_EOI    0x20   /* end of interrupt                   */

/* Back-to-back port accesses outrun the original hardware. The conventional
 * delay is a write to an unused port, which is slow enough and has no other
 * effect. QEMU needs no delay and is unaffected by one. */
static inline void io_wait(void)
{
    outb(0x80, 0);
}

void pic_init(uint8_t offset1, uint8_t offset2)
{
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    /* ICW2: the vector each chip's first line is delivered on. */
    outb(PIC1_DATA, offset1);
    io_wait();
    outb(PIC2_DATA, offset2);
    io_wait();

    /* ICW3: the cascade wiring. The second chip has no line to the
     * processor and raises line 2 of the first instead. The master is given
     * that line as a bit mask and the slave as a line number. */
    outb(PIC1_DATA, 1 << 2);
    io_wait();
    outb(PIC2_DATA, 2);
    io_wait();

    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    /* A set bit masks a line. All sixteen start masked: a device with no
     * handler that keeps raising its line would leave the processor with no
     * time for anything else. */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

void pic_unmask(uint8_t irq)
{
    uint16_t port = irq < 8 ? PIC1_DATA : PIC2_DATA;
    uint8_t  bit  = (uint8_t) (1u << (irq & 7));

    outb(port, (uint8_t) (inb(port) & ~bit));
}

void pic_mask(uint8_t irq)
{
    uint16_t port = irq < 8 ? PIC1_DATA : PIC2_DATA;
    uint8_t  bit  = (uint8_t) (1u << (irq & 7));

    outb(port, (uint8_t) (inb(port) | bit));
}

uint16_t pic_masks(void)
{
    return (uint16_t) (inb(PIC1_DATA) |
                       (uint16_t) inb(PIC2_DATA) << 8);
}

void pic_eoi(uint8_t irq)
{
    /* A line on the slave arrived through the master, so both are waiting.
     * The slave is acknowledged first: acknowledging the master first would
     * let it re-raise the cascade line before the slave has finished. */
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}
