/* spark -- kernel/fs.c
 *
 * Turning numbered blocks into names and contents.
 */
#include "fs.h"
#include "ata.h"
#include "string.h"

static struct fs_super super;
static bool mounted;

/* The bitmap, kept in memory and written back whenever it changes.
 *
 * One block covers 4096 blocks at a bit each, which is four times the disk
 * sparkfs uses, so it never needs a second one. A real file system has many
 * bitmap blocks and reads only the one it needs, which is the same idea
 * with an index in front of it. */
static uint8_t bitmap[FS_BLOCK_BYTES];

/* One block of scratch, because every operation here reads a block, looks
 * at part of it, and moves on. A kernel that cared about speed would keep
 * the last few blocks instead, which is Exercise 15.6. */
static uint8_t scratch[FS_BLOCK_BYTES];

static bool read_inode(uint32_t inum, struct fs_inode *out)
{
    if (inum == 0 || inum >= super.inodes) {
        return false;
    }

    uint32_t block = FS_INODE_BLOCK +
                     inum / FS_INODES_PER_BLOCK;
    uint32_t slot = inum % FS_INODES_PER_BLOCK;

    if (!ata_read(block, scratch)) {
        return false;
    }

    memcpy(out, scratch + slot * sizeof(struct fs_inode),
           sizeof(struct fs_inode));
    return true;
}

/* Read, change one inode, write the whole block back.
 *
 * A disk moves whole sectors, so there is no such thing as writing 64
 * bytes. Every inode update is a read of its seven neighbours, a change,
 * and a write of all eight, which is one of the reasons a real kernel
 * keeps inodes in memory and writes them back in batches. */
static bool write_inode(uint32_t inum,
                        const struct fs_inode *in)
{
    if (inum == 0 || inum >= super.inodes) {
        return false;
    }

    uint32_t block = FS_INODE_BLOCK +
                     inum / FS_INODES_PER_BLOCK;
    uint32_t slot = inum % FS_INODES_PER_BLOCK;

    if (!ata_read(block, scratch)) {
        return false;
    }

    memcpy(scratch + slot * sizeof(struct fs_inode), in,
           sizeof(struct fs_inode));
    return ata_write(block, scratch);
}

static bool bit_test(uint32_t block)
{
    return (bitmap[block / 8] & (1u << (block % 8))) != 0;
}

static void bit_set(uint32_t block, bool used)
{
    if (used) {
        bitmap[block / 8] |= (uint8_t) (1u << (block % 8));
    } else {
        bitmap[block / 8] &= (uint8_t) ~(1u << (block % 8));
    }
}

/* Find a free block, mark it, and put the bitmap back on the disk.
 *
 * Writing the bitmap immediately is the slow, safe order. The fast order is
 * to keep it in memory and write it later, and the price of that is a
 * machine that loses power between allocating a block and recording it,
 * leaving a block that belongs to a file and is also free. Every real file
 * system has an answer to this and none of them is short. */
static uint32_t alloc_block(void)
{
    for (uint32_t b = super.data_block;
         b < super.blocks; b++) {
        if (!bit_test(b)) {
            bit_set(b, true);

            if (!ata_write(FS_BITMAP_BLOCK, bitmap)) {
                bit_set(b, false);
                return 0;
            }
            return b;
        }
    }
    return 0;
}

static void free_block(uint32_t block)
{
    if (block < super.data_block || block >= super.blocks) {
        return;
    }

    bit_set(block, false);
    ata_write(FS_BITMAP_BLOCK, bitmap);
}

static uint32_t alloc_inode(void)
{
    struct fs_inode node;

    for (uint32_t i = FS_ROOT_INODE + 1;
         i < super.inodes; i++) {
        if (read_inode(i, &node) && node.type == FS_FREE) {
            return i;
        }
    }
    return 0;
}

bool fs_mount(void)
{
    mounted = false;

    if (!ata_read(FS_SUPER_BLOCK, scratch)) {
        return false;
    }

    memcpy(&super, scratch, sizeof super);

    /* The only thing standing between us and mounting a stranger. A disk
     * with something else on it has a different word here, and reading its
     * blocks as though they were inodes would give a file system full of
     * plausible nonsense. */
    if (super.magic != FS_MAGIC) {
        return false;
    }

    if (!ata_read(FS_BITMAP_BLOCK, bitmap)) {
        return false;
    }

    mounted = true;
    return true;
}

/* Find one name in one directory. Returns the inode number, or 0. */
static uint32_t find_in(uint32_t dir, const char *name,
                        uint32_t len)
{
    struct fs_inode node;

    if (len == 0 || len >= FS_NAME_MAX) {
        return 0;
    }

    if (!read_inode(dir, &node) || node.type != FS_DIR) {
        return 0;
    }

    for (uint32_t d = 0; d < FS_DIRECT; d++) {
        if (node.direct[d] == 0) {
            continue;
        }
        if (!ata_read(node.direct[d], scratch)) {
            return 0;
        }

        const struct fs_entry *e =
            (const struct fs_entry *) scratch;

        for (uint32_t i = 0; i < FS_ENTRIES_PER_BLOCK; i++) {
            if (e[i].inode == 0) {
                continue;
            }
            if (strlen(e[i].name) == len &&
                memcmp(e[i].name, name, len) == 0) {
                return e[i].inode;
            }
        }
    }
    return 0;
}

uint32_t fs_lookup(const char *path)
{
    if (!mounted || path == NULL || path[0] != '/') {
        return 0;
    }

    uint32_t here = super.root;
    const char *at = path + 1;

    while (*at != '\0') {
        const char *slash = at;

        while (*slash != '\0' && *slash != '/') {
            slash++;
        }

        uint32_t len = (uint32_t) (slash - at);

        if (len > 0) {
            here = find_in(here, at, len);

            if (here == 0) {
                return 0;
            }
        }

        at = (*slash == '/') ? slash + 1 : slash;
    }
    return here;
}

bool fs_stat(uint32_t inum, struct fs_inode *out)
{
    return mounted && read_inode(inum, out);
}

uint32_t fs_read(uint32_t inum, uint32_t offset,
                 void *buf, uint32_t len)
{
    struct fs_inode node;

    if (!mounted || !read_inode(inum, &node) || offset >= node.size) {
        return 0;
    }

    if (offset + len > node.size) {
        len = node.size - offset;
    }

    uint8_t *out = buf;
    uint32_t done = 0;

    while (done < len) {
        uint32_t at = offset + done;
        uint32_t index = at / FS_BLOCK_BYTES;
        uint32_t within = at % FS_BLOCK_BYTES;
        uint32_t take = FS_BLOCK_BYTES - within;

        if (take > len - done) {
            take = len - done;
        }

        if (index >= FS_DIRECT || node.direct[index] == 0) {
            break;
        }

        if (!ata_read(node.direct[index], scratch)) {
            break;
        }

        memcpy(out + done, scratch + within, take);
        done += take;
    }
    return done;
}

bool fs_list(uint32_t dir, uint32_t index,
             char *name_out, uint32_t *inum_out)
{
    struct fs_inode node;
    uint32_t seen = 0;

    if (!mounted || !read_inode(dir, &node) || node.type != FS_DIR) {
        return false;
    }

    for (uint32_t d = 0; d < FS_DIRECT; d++) {
        if (node.direct[d] == 0) {
            continue;
        }
        if (!ata_read(node.direct[d], scratch)) {
            return false;
        }

        const struct fs_entry *e =
            (const struct fs_entry *) scratch;

        for (uint32_t i = 0; i < FS_ENTRIES_PER_BLOCK; i++) {
            if (e[i].inode == 0) {
                continue;
            }
            if (seen == index) {
                memcpy(name_out, e[i].name, FS_NAME_MAX);
                *inum_out = e[i].inode;
                return true;
            }
            seen++;
        }
    }
    return false;
}

/* Put one name in the root directory. The caller has already decided that
 * the name is free and that the inode exists. */
static bool add_entry(const char *name, uint32_t inum)
{
    struct fs_inode root;

    if (!read_inode(super.root, &root)) {
        return false;
    }

    uint32_t len = strlen(name);

    if (len == 0 || len >= FS_NAME_MAX) {
        return false;
    }

    for (uint32_t d = 0; d < FS_DIRECT; d++) {
        if (root.direct[d] == 0) {
            continue;
        }
        if (!ata_read(root.direct[d], scratch)) {
            return false;
        }

        struct fs_entry *e = (struct fs_entry *) scratch;

        for (uint32_t i = 0; i < FS_ENTRIES_PER_BLOCK; i++) {
            if (e[i].inode != 0) {
                continue;
            }

            memset(&e[i], 0, sizeof e[i]);
            e[i].inode = inum;
            memcpy(e[i].name, name, len);
            return ata_write(root.direct[d], scratch);
        }
    }
    return false;
}

uint32_t fs_create(const char *name, const void *data, uint32_t len)
{
    struct fs_inode node;

    if (!mounted || len > FS_MAX_BYTES) {
        return 0;
    }

    if (find_in(super.root, name, strlen(name)) != 0) {
        return 0;
    }

    uint32_t inum = alloc_inode();

    if (inum == 0) {
        return 0;
    }

    memset(&node, 0, sizeof node);
    node.type = FS_FILE;
    node.size = len;
    node.links = 1;

    const uint8_t *from = data;
    uint32_t done = 0;

    for (uint32_t d = 0; d < FS_DIRECT && done < len; d++) {
        uint32_t block = alloc_block();

        if (block == 0) {
            return 0;
        }

        uint32_t take = len - done;

        if (take > FS_BLOCK_BYTES) {
            take = FS_BLOCK_BYTES;
        }

        memset(scratch, 0, sizeof scratch);
        memcpy(scratch, from + done, take);

        if (!ata_write(block, scratch)) {
            return 0;
        }

        node.direct[d] = block;
        done += take;
    }

    if (!write_inode(inum, &node) || !add_entry(name, inum)) {
        return 0;
    }
    return inum;
}

uint32_t fs_append(uint32_t inum, const void *data, uint32_t len)
{
    struct fs_inode node;

    if (!mounted || !read_inode(inum, &node) || node.type != FS_FILE) {
        return 0;
    }

    const uint8_t *from = data;
    uint32_t done = 0;

    while (done < len) {
        uint32_t at = node.size + done;
        uint32_t index = at / FS_BLOCK_BYTES;
        uint32_t within = at % FS_BLOCK_BYTES;

        if (index >= FS_DIRECT) {
            break;
        }

        /* A file grows a block at a time, and only when the byte about to
         * be written is the first one of a block nobody has allocated. */
        if (node.direct[index] == 0) {
            node.direct[index] = alloc_block();

            if (node.direct[index] == 0) {
                break;
            }

            memset(scratch, 0, sizeof scratch);
        } else if (!ata_read(node.direct[index], scratch)) {
            break;
        }

        uint32_t take = FS_BLOCK_BYTES - within;

        if (take > len - done) {
            take = len - done;
        }

        memcpy(scratch + within, from + done, take);

        if (!ata_write(node.direct[index], scratch)) {
            break;
        }

        done += take;
    }

    node.size += done;
    write_inode(inum, &node);
    return done;
}

bool fs_link(const char *path, const char *newname)
{
    struct fs_inode node;
    uint32_t inum = fs_lookup(path);

    if (inum == 0 || inum == super.root) {
        return false;
    }

    if (find_in(super.root, newname, strlen(newname)) != 0) {
        return false;
    }

    if (!read_inode(inum, &node)) {
        return false;
    }

    /* Nothing is copied. The new name holds the same number, and the count
     * of names goes up by one. This is the entire operation, and it is why
     * a link costs 32 bytes rather than the size of the file. */
    node.links++;

    if (!write_inode(inum, &node)) {
        return false;
    }
    return add_entry(newname, inum);
}

bool fs_unlink(const char *name)
{
    struct fs_inode root;
    struct fs_inode node;

    if (!mounted || !read_inode(super.root, &root)) {
        return false;
    }

    uint32_t len = strlen(name);

    for (uint32_t d = 0; d < FS_DIRECT; d++) {
        if (root.direct[d] == 0) {
            continue;
        }
        if (!ata_read(root.direct[d], scratch)) {
            return false;
        }

        struct fs_entry *e = (struct fs_entry *) scratch;

        for (uint32_t i = 0; i < FS_ENTRIES_PER_BLOCK; i++) {
            if (e[i].inode == 0 || strlen(e[i].name) != len ||
                memcmp(e[i].name, name, len) != 0) {
                continue;
            }

            uint32_t inum = e[i].inode;

            memset(&e[i], 0, sizeof e[i]);

            if (!ata_write(root.direct[d], scratch)) {
                return false;
            }

            if (!read_inode(inum, &node)) {
                return false;
            }

            /* The name is gone either way. Whether the file is gone
             * depends on whether anybody else was pointing at it. */
            if (node.links > 0) {
                node.links--;
            }

            if (node.links == 0) {
                for (uint32_t b = 0; b < FS_DIRECT; b++) {
                    if (node.direct[b] != 0) {
                        free_block(node.direct[b]);
                    }
                }
                memset(&node, 0, sizeof node);
            }

            return write_inode(inum, &node);
        }
    }
    return false;
}

uint32_t fs_blocks(void)
{
    return mounted ? super.blocks : 0;
}

uint32_t fs_blocks_used(void)
{
    uint32_t used = 0;

    for (uint32_t b = 0; b < super.blocks; b++) {
        if (bit_test(b)) {
            used++;
        }
    }
    return used;
}
