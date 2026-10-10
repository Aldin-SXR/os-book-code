#!/usr/bin/env python3
"""Build a sparkfs image for the kernel to mount.

Every constant this needs is read out of kernel/fs.h rather than written
here, because an image and a driver that disagree about where the inode
table starts produce a mount that succeeds and a file system that is
nonsense. There is one copy of the layout and this is not it.

    python3 mkfs.py fs.img

Writes a 512 KiB image holding the four files the chapter reads.
"""
import re
import struct
import sys
import pathlib

BLOCKS = 1024                     # 512 KiB, which is plenty and is tiny


def constants(path):
    """Pull the #define values out of the header, resolving the ones that
    are written in terms of each other."""
    text = pathlib.Path(path).read_text()
    raw = dict(re.findall(r"^#define\s+(FS_[A-Z_]+)\s+(.+?)\s*(?:/\*.*)?$",
                          text, re.M))
    out = {}
    for _ in range(len(raw)):
        for name, expr in raw.items():
            if name in out:
                continue
            body = expr.strip().rstrip("u").replace("u)", ")")
            try:
                out[name] = int(eval(body, {"__builtins__": {}}, out))
            except (NameError, SyntaxError, TypeError):
                pass
    missing = set(raw) - set(out)
    assert not missing, "could not resolve %s" % sorted(missing)
    return out


C = constants("kernel/fs.h")
BS = C["FS_BLOCK_BYTES"]
INODE_SIZE = 64
ENTRY_SIZE = 32

# What goes on the disk. A directory's contents are built from its children
# rather than written out, so adding a file here needs no other change.
def program(name):
    """One of the flat binaries from user/, read off the host's disk."""
    return pathlib.Path("user/%s.bin" % name).read_bytes()


def uprogs(path="Makefile"):
    """The programs that go in /bin, read out of the Makefile's UPROGS.

    The Makefile already has to know this list in order to build the
    binaries, and a second copy here is the mistake this file exists to
    avoid everywhere else: adding a program to UPROGS would build it,
    leave it out of the image, and the shell would say `not found` with
    the binary sitting in user/. Order is kept, because it decides the
    inode numbers the chapters quote.
    """
    text = pathlib.Path(path).read_text()
    m = re.search(r"^UPROGS\s*:?=\s*(.+)$", text, re.M)
    assert m, "no UPROGS in %s" % path
    return m.group(1).split()


TREE = {
    "readme.txt":
        b"sparkfs keeps files in blocks of 512 bytes and\n"
        b"names in directories, which is all a file system is.\n",
    "notes.txt":
        b"a name is not a file. see /docs/hello.txt\n",
    "docs": {
        "hello.txt": b"hello from the disk.\n",
    },
    "bin": {name: program(name) for name in uprogs()},
}


class Image:
    def __init__(self, blocks):
        self.data = bytearray(blocks * BS)
        self.blocks = blocks
        self.inodes = [None] * C["FS_INODES"]
        self.next_free = C["FS_DATA_BLOCK"]
        self.used = set(range(C["FS_DATA_BLOCK"]))

    def block(self, n, payload):
        assert len(payload) <= BS, "a block is one block"
        self.data[n * BS:n * BS + len(payload)] = payload

    def alloc_block(self):
        n = self.next_free
        self.next_free += 1
        assert n < self.blocks, "image too small"
        self.used.add(n)
        return n

    def alloc_inode(self):
        for i in range(1, len(self.inodes)):
            if self.inodes[i] is None:
                self.inodes[i] = {"type": 0, "size": 0, "links": 0,
                                  "direct": []}
                return i
        raise AssertionError("out of inodes")

    def write_file(self, inum, payload):
        assert len(payload) <= C["FS_MAX_BYTES"], "file too big for sparkfs"
        direct = []
        for off in range(0, len(payload), BS):
            n = self.alloc_block()
            self.block(n, payload[off:off + BS])
            direct.append(n)
        self.inodes[inum].update(type=C["FS_FILE"], size=len(payload),
                                 direct=direct)

    def write_dir(self, inum, entries):
        """entries is a list of (name, inode). One block, so sixteen."""
        per = C["FS_ENTRIES_PER_BLOCK"]
        assert len(entries) <= per, "a directory is one block here"
        payload = bytearray(BS)
        for i, (name, target) in enumerate(entries):
            raw = name.encode("ascii")
            assert len(raw) < C["FS_NAME_MAX"], "name too long: " + name
            struct.pack_into("<I", payload, i * ENTRY_SIZE, target)
            payload[i * ENTRY_SIZE + 4:
                    i * ENTRY_SIZE + 4 + len(raw)] = raw
        n = self.alloc_block()
        self.block(n, bytes(payload))
        self.inodes[inum].update(type=C["FS_DIR"], size=BS, direct=[n])

    def build(self, tree, inum):
        """Fill in one directory and everything under it."""
        entries = []
        for name, content in tree.items():
            child = self.alloc_inode()
            self.inodes[child]["links"] = 1
            if isinstance(content, dict):
                self.build(content, child)
            else:
                self.write_file(child, content)
            entries.append((name, child))
        self.write_dir(inum, entries)

    def finish(self):
        # The superblock, in the order struct fs_super declares.
        self.block(C["FS_SUPER_BLOCK"],
                   struct.pack("<5I", C["FS_MAGIC"], self.blocks,
                               C["FS_INODES"], C["FS_DATA_BLOCK"],
                               C["FS_ROOT_INODE"]))

        # The bitmap: one bit per block, lowest block in the lowest bit of
        # the first byte. The metadata blocks are marked in use too, so
        # that nothing can ever be handed out on top of the inode table.
        bitmap = bytearray(BS)
        for n in self.used:
            bitmap[n // 8] |= 1 << (n % 8)
        self.block(C["FS_BITMAP_BLOCK"], bytes(bitmap))

        # The inode table.
        table = bytearray(C["FS_INODE_BLOCKS"] * BS)
        for i, node in enumerate(self.inodes):
            if node is None:
                continue
            direct = node["direct"] + [0] * (C["FS_DIRECT"] -
                                             len(node["direct"]))
            struct.pack_into("<3I", table, i * INODE_SIZE,
                             node["type"], node["size"], node["links"])
            struct.pack_into("<%dI" % C["FS_DIRECT"], table,
                             i * INODE_SIZE + 12, *direct)
        self.data[C["FS_INODE_BLOCK"] * BS:
                  C["FS_INODE_BLOCK"] * BS + len(table)] = table
        return bytes(self.data)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "fs.img"
    img = Image(BLOCKS)

    root = img.alloc_inode()
    assert root == C["FS_ROOT_INODE"], "the root has to be inode 1"
    img.inodes[root]["links"] = 1
    img.build(TREE, root)

    pathlib.Path(out).write_bytes(img.finish())
    print("wrote %s: %d blocks, %d in use, %d inodes used of %d"
          % (out, BLOCKS, len(img.used),
             sum(1 for n in img.inodes if n is not None), C["FS_INODES"]))


if __name__ == "__main__":
    main()
