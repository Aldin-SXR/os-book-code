# spark -- build and run
#
# Two images from one kernel. spark.elf keeps its ELF headers, which is what a
# Multiboot loader reads. spark.img is a disk image: the boot sector followed
# by the same kernel with the headers stripped.

CC := clang
AS := nasm
LD := ld.lld

CFLAGS  := -target i386-none-elf -ffreestanding \
           -fno-stack-protector -fno-pic -mno-sse \
           -std=c11 -Wall -Wextra -O2 -g
ASFLAGS := -f elf32 -g
LDFLAGS := -m elf_i386 -T link.ld

KASM := boot.asm gdt_flush.asm isr.asm switch.asm \
        tss_flush.asm user_enter.asm
CSRC := $(wildcard kernel/*.c)
OBJS := $(KASM:.asm=.o) $(CSRC:.c=.o)

all: spark.elf spark.img hello.img fs.img

# The programs that live on the disk.
#
# A separate link, to a separate address, with no kernel code in it. These
# are flat binaries rather than ELF: kernel/exec.c copies the file to one
# address and jumps at its first byte, which is the simplest loader there
# is and the one that shows what a loader does.
UPROGS := echo cat ls wc
UBIN   := $(addprefix user/,$(addsuffix .bin,$(UPROGS)))

user/%.bin: user/start.o user/ulib.o user/%.o user/link.ld
	$(LD) -m elf_i386 -T user/link.ld --oformat=binary -o $@ \
	    user/start.o user/ulib.o user/$*.o

programs: $(UBIN)

# The file system image, built by a host program from the same constants the
# kernel reads. It is a second disk rather than part of spark.img, so that
# the kernel finds it in the same place whichever way it was started.
fs.img: mkfs.py kernel/fs.h $(UBIN)
	python3 mkfs.py $@

spark.elf: $(OBJS) link.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

# The same link without headers. The boot sector has no ELF parser: it copies
# the image into memory and jumps at its first byte.
spark.bin: $(OBJS) link.ld
	$(LD) $(LDFLAGS) --oformat=binary -o $@ $(OBJS)

# The boot sector cannot discover the size of the kernel, so it is measured
# here and assembled in as a constant.
SECTORS := $$(( ($$(wc -c < spark.bin) + 511) / 512 ))

bootsect.bin: bootsect.asm spark.bin
	$(AS) -f bin -DKERNEL_SECTORS=$(SECTORS) $< -o $@

spark.img: bootsect.bin spark.bin
	cat bootsect.bin spark.bin > $@

# The minimal boot sector, built on its own.
hello.img: hello.asm
	$(AS) -f bin $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

# The file system disk goes at index 1, the primary slave, in both cases.
# Under -kernel there is nothing at index 0; under -drive index 0 is the
# boot disk. Either way the file system is in the same place.
FSDISK := -drive format=raw,index=1,media=disk,file=fs.img

run: spark.elf fs.img             ## boot through QEMU's own Multiboot loader
	qemu-system-i386 -kernel spark.elf $(FSDISK)

run-disk: spark.img fs.img        ## boot from the disk image, through the boot sector
	qemu-system-i386 -drive format=raw,file=spark.img $(FSDISK)

run-hello: hello.img             ## boot the minimal boot sector on its own
	qemu-system-i386 -drive format=raw,file=hello.img

clean:
	rm -f $(OBJS) spark.elf spark.bin spark.img bootsect.bin hello.img
	rm -f fs.img user/*.o user/*.bin

.PHONY: all programs run run-disk run-hello clean
