/* spark -- kernel/ata.c
 *
 * The primary ATA channel, in about a hundred lines.
 */
#include "ata.h"
#include "io.h"
#include "string.h"

/* The eight registers of the primary channel, and the control port that
 * sits on its own. These addresses have been fixed since the IBM PC/AT in
 * 1984 and no machine has moved them since. */
#define ATA_DATA     0x1F0   /* 16 bits wide */
#define ATA_ERROR    0x1F1   /* why it failed */
#define ATA_COUNT    0x1F2   /* how many sectors */
#define ATA_LBA_LOW  0x1F3   /* sector bits 7..0 */
#define ATA_LBA_MID  0x1F4   /* bits 15..8 */
#define ATA_LBA_HIGH 0x1F5   /* bits 23..16 */
#define ATA_DRIVE    0x1F6   /* drive, and bits 27..24 */
#define ATA_STATUS   0x1F7   /* status in, command out */
#define ATA_CONTROL  0x3F6   /* reset, and irq disable */

/* Bits of the status register. */
#define ST_ERR  (1u << 0)    /* the last command failed       */
#define ST_DRQ  (1u << 3)    /* a sector is ready to transfer */
#define ST_DF   (1u << 5)    /* drive fault                   */
#define ST_RDY  (1u << 6)    /* spun up and willing           */
#define ST_BSY  (1u << 7)    /* busy; every other bit is junk */

#define CMD_READ     0x20
#define CMD_WRITE    0x30
#define CMD_FLUSH    0xE7
#define CMD_IDENTIFY 0xEC

/* spark's file system is on the primary slave, which is index 1 under QEMU.
 * The boot disk, when there is one, is the master, and keeping the two apart
 * means the same driver works whether the kernel was started from a disk or
 * handed straight to the processor by a Multiboot loader. */
#define DRIVE_SLAVE 0x10

static uint32_t sectors;
static uint32_t reads;
static uint32_t writes;

/* Four reads of the status port, thrown away.
 *
 * The specification asks for 400 nanoseconds between selecting a drive and
 * believing what the status register says, because the controller needs that
 * long to put the newly selected drive's status there. A port read takes
 * about 100 ns whatever the processor's clock is doing, so four of them is
 * the conventional way to wait. It looks like nothing and it is the
 * difference between reading this drive's status and the other one's. */
static void settle(void)
{
    for (int i = 0; i < 4; i++) {
        (void) inb(ATA_STATUS);
    }
}

/* Wait for BSY to clear, then report whether the drive is unhappy.
 *
 * Every other bit of the status register is meaningless while BSY is set,
 * which is why this cannot be one test. */
static bool wait_ready(void)
{
    uint8_t status;

    do {
        status = inb(ATA_STATUS);
    } while (status & ST_BSY);

    return (status & (ST_ERR | ST_DF)) == 0;
}

/* Wait for the drive to have data for us, or to give up. */
static bool wait_drq(void)
{
    uint8_t status;

    for (;;) {
        status = inb(ATA_STATUS);

        if (status & ST_BSY) {
            continue;
        }
        if (status & (ST_ERR | ST_DF)) {
            return false;
        }
        if (status & ST_DRQ) {
            return true;
        }
    }
}

/* Point the drive at one sector. Shared by read and write, because the
 * first six registers are set identically and only the command differs. */
static void select_sector(uint32_t lba)
{
    outb(ATA_DRIVE,
         0xE0 | DRIVE_SLAVE | ((lba >> 24) & 0x0F));
    settle();

    outb(ATA_COUNT, 1);
    outb(ATA_LBA_LOW, (uint8_t) (lba & 0xFF));
    outb(ATA_LBA_MID, (uint8_t) ((lba >> 8) & 0xFF));
    outb(ATA_LBA_HIGH, (uint8_t) ((lba >> 16) & 0xFF));
}

bool ata_init(void)
{
    uint16_t id[256];

    sectors = 0;
    reads = 0;
    writes = 0;

    /* Bit 1 of the control port masks the drive's interrupt. spark polls,
     * so an interrupt here would arrive at a vector with no handler. */
    outb(ATA_CONTROL, 0x02);

    outb(ATA_DRIVE, 0xE0 | DRIVE_SLAVE);
    settle();

    /* A channel with nothing on it reads back 0xFF, the bus floating high,
     * and there is no drive to answer the command below. */
    if (inb(ATA_STATUS) == 0xFF) {
        return false;
    }

    outb(ATA_COUNT, 0);
    outb(ATA_LBA_LOW, 0);
    outb(ATA_LBA_MID, 0);
    outb(ATA_LBA_HIGH, 0);
    outb(ATA_STATUS, CMD_IDENTIFY);

    if (inb(ATA_STATUS) == 0) {
        return false;      /* nothing there */
    }

    if (!wait_drq()) {
        return false;
    }

    insw(ATA_DATA, id, 256);

    /* Words 60 and 61 are the sector count as a 32-bit number, low half
     * first. The drive is describing itself; nothing else in the machine
     * knows how big it is. */
    sectors = (uint32_t) id[60] | ((uint32_t) id[61] << 16);
    return sectors > 0;
}

uint32_t ata_sectors(void) { return sectors; }
uint32_t ata_reads(void)   { return reads; }
uint32_t ata_writes(void)  { return writes; }

bool ata_read(uint32_t lba, void *buf)
{
    if (lba >= sectors) {
        return false;
    }

    if (!wait_ready()) {
        return false;
    }

    select_sector(lba);
    outb(ATA_STATUS, CMD_READ);

    if (!wait_drq()) {
        return false;
    }

    insw(ATA_DATA, buf, ATA_SECTOR_BYTES / 2);
    reads++;
    return true;
}

bool ata_write(uint32_t lba, const void *buf)
{
    if (lba >= sectors) {
        return false;
    }

    if (!wait_ready()) {
        return false;
    }

    select_sector(lba);
    outb(ATA_STATUS, CMD_WRITE);

    if (!wait_drq()) {
        return false;
    }

    outsw(ATA_DATA, buf, ATA_SECTOR_BYTES / 2);

    /* The drive is allowed to acknowledge a write before the data is on the
     * platter, so a flush is what makes the write durable. Leave it out and
     * everything works until the machine loses power at the wrong moment,
     * which is the worst kind of bug to have shipped. */
    outb(ATA_STATUS, CMD_FLUSH);

    if (!wait_ready()) {
        return false;
    }

    writes++;
    return true;
}
