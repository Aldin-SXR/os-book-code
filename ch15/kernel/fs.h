/* spark -- kernel/fs.h
 *
 * sparkfs: a file system small enough to read in an afternoon.
 *
 * A disk gives you numbered blocks and nothing else. It has no notion of a
 * file, a name, or where one piece of data ends and the next begins. A file
 * system is the agreement that turns those blocks into something a program
 * can ask for by name, and like every agreement it is only worth what both
 * sides spell out. Everything below is that spelling.
 *
 * sparkfs is inode-based, which is how nearly every Unix file system works.
 * The idea is that a file and its name are separate objects: an inode holds
 * the size, the block numbers and nothing else, and a directory is a file
 * whose contents are pairs of name and inode number. That separation is
 * what lets one file have two names, and it is why deleting a name does not
 * always delete a file.
 *
 * The layout is deliberately flat and deliberately small, and mkfs.py reads
 * these same constants out of this file so that the image and the driver
 * cannot disagree about where anything is.
 */
#ifndef SPARK_FS_H
#define SPARK_FS_H

#include "types.h"

/* One block is one sector. A real file system uses several sectors per
 * block, because the arithmetic is the same and the disk is faster in
 * larger pieces, and it costs wasted space in every file that does not
 * fill one. One is the version with no arithmetic in the way. */
#define FS_BLOCK_BYTES 512

/* 'SPRK' read as a little-endian word, which is what a hex dump of the
 * first four bytes of the image shows. A magic number is the cheapest
 * possible check that the thing you have mounted is the thing you think. */
#define FS_MAGIC 0x4B525053u

/* The fixed shape of the disk. Everything after the inode table is data. */
#define FS_SUPER_BLOCK  0
#define FS_BITMAP_BLOCK 1
#define FS_INODE_BLOCK  2
#define FS_INODE_BLOCKS 8
#define FS_DATA_BLOCK   (FS_INODE_BLOCK + FS_INODE_BLOCKS)

/* Eight inodes to a block, 64 of them in the eight blocks above. Inode 0 is
 * never used, so that a zero in a directory entry can mean "empty slot",
 * and inode 1 is always the root directory. */
#define FS_INODES_PER_BLOCK (FS_BLOCK_BYTES / 64)
#define FS_INODES (FS_INODE_BLOCKS * FS_INODES_PER_BLOCK)
#define FS_ROOT_INODE 1

/* What an inode can point at. Eight block numbers is a 4 KiB maximum file,
 * which is small and honest: the next step is an indirect block, and
 * Exercise 15.5 is that step. */
#define FS_DIRECT 8
#define FS_MAX_BYTES (FS_DIRECT * FS_BLOCK_BYTES)

/* A name is 27 characters and a terminator, which makes a directory entry
 * exactly 32 bytes and 16 of them fit in a block. Real file systems use
 * variable-length entries and pay for it in code. */
#define FS_NAME_MAX 28
#define FS_ENTRIES_PER_BLOCK (FS_BLOCK_BYTES / 32)

/* What an inode is. */
#define FS_FREE 0
#define FS_FILE 1
#define FS_DIR  2

/* Block 0, read once at mount and then believed. */
struct fs_super {
    uint32_t magic;
    uint32_t blocks;      /* the whole disk, in blocks      */
    uint32_t inodes;      /* how many the table holds       */
    uint32_t data_block;  /* the first block that holds data*/
    uint32_t root;        /* always FS_ROOT_INODE           */
};

/* Sixty-four bytes, of which spark uses forty-four.
 *
 * There is no name here, and that absence is the whole design. A name lives
 * in a directory, an inode lives in the table, and the only thing joining
 * them is the number. */
struct fs_inode {
    uint32_t type;           /* FS_FREE, FS_FILE or FS_DIR */
    uint32_t size;           /* bytes, not blocks          */
    uint32_t links;          /* how many names point here  */
    uint32_t direct[FS_DIRECT];
    uint32_t reserved[5];
};

_Static_assert(sizeof(struct fs_inode) == 64, "8 a block");

/* Thirty-two bytes, and a directory is a file made of these. */
struct fs_entry {
    uint32_t inode;          /* 0 means the slot is empty */
    char name[FS_NAME_MAX];
};

_Static_assert(sizeof(struct fs_entry) == 32, "16 a block");

/* Read the superblock and check it. False if the disk holds something
 * else, which is the only protection against mounting a stranger. */
bool fs_mount(void);

/* Turn a path into an inode number, or 0 if no such name exists. Absolute
 * paths only: everything starts at the root, because spark has no idea of a
 * current directory until Chapter 16. */
uint32_t fs_lookup(const char *path);

/* Read an inode out of the table. False if the number is out of range. */
bool fs_stat(uint32_t inum, struct fs_inode *out);

/* Copy bytes out of a file. Returns how many were copied, which is less
 * than len at the end of the file and zero past it. */
uint32_t fs_read(uint32_t inum, uint32_t offset,
                 void *buf, uint32_t len);

/* Walk a directory. index counts entries that are in use, so a caller can
 * count up from 0 until this returns false. */
bool fs_list(uint32_t dir, uint32_t index,
             char *name_out, uint32_t *inum_out);

/* Make a new file in the root directory, with these contents. Returns the
 * inode number, or 0 if the disk or the table is full. */
uint32_t fs_create(const char *name, const void *data, uint32_t len);

/* Give an existing file a second name in the root directory. Nothing is
 * copied: the new entry holds the same inode number and the inode's link
 * count goes up by one. Returns false if the name is taken or the target
 * does not exist. */
bool fs_link(const char *path, const char *newname);

/* Remove a name from the root directory. The file itself is freed only when
 * the last name pointing at it is gone. */
bool fs_unlink(const char *name);

/* What the superblock says about the disk, which is not the same question
 * as how big the disk is: a file system may be smaller than the device it
 * sits on, and usually is. */
uint32_t fs_blocks(void);

/* How many blocks the bitmap says are in use, for the chapter to print. */
uint32_t fs_blocks_used(void);

#endif
