/* spark -- kernel/ata.h
 *
 * A disk, one sector at a time.
 *
 * Chapter 4 set out the shape every device driver in this book has: a
 * command register, a status register, a data register, and a protocol for
 * using the three in the right order. The screen was the first instance and
 * the keyboard was the second. A disk is the third, and it is the one where
 * the shape pays for itself, because the protocol has more steps and none of
 * them is guessable.
 *
 * This driver polls. Chapter 8 measured what polling costs and concluded
 * that an interrupt is usually better, and a real disk driver does use one,
 * because a read takes milliseconds and the processor has better things to
 * do. spark polls because the file system above it is synchronous: every
 * caller wants its block before it can continue, so there is nothing else to
 * run while the disk works. Exercise 15.7 is the interrupt-driven version.
 */
#ifndef SPARK_ATA_H
#define SPARK_ATA_H

#include "types.h"

/* One sector, which is the smallest thing a disk will move. Every read and
 * write in this chapter is a whole one of these. */
#define ATA_SECTOR_BYTES 512

/* Find the disk and read its size out of it. Returns false if nothing
 * answers, which under QEMU means no second drive was attached. */
bool ata_init(void);

/* How many sectors the disk says it has, from its own IDENTIFY data rather
 * than from the size of the file backing it. */
uint32_t ata_sectors(void);

/* One sector in, one sector out. buf is ATA_SECTOR_BYTES long. Both return
 * false if the drive reported an error. */
bool ata_read(uint32_t lba, void *buf);
bool ata_write(uint32_t lba, const void *buf);

/* How many of each spark has done, so that the chapter can show that a
 * cache would have something to do. */
uint32_t ata_reads(void);
uint32_t ata_writes(void);

#endif
