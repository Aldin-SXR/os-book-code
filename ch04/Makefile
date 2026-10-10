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

CSRC := $(wildcard kernel/*.c)
OBJS := boot.o $(CSRC:.c=.o)

all: spark.elf spark.img hello.img

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

run: spark.elf                    ## boot through QEMU's own Multiboot loader
	qemu-system-i386 -kernel spark.elf

run-disk: spark.img               ## boot from the disk image, through the boot sector
	qemu-system-i386 -drive format=raw,file=spark.img

run-hello: hello.img             ## boot the minimal boot sector on its own
	qemu-system-i386 -drive format=raw,file=hello.img

clean:
	rm -f $(OBJS) spark.elf spark.bin spark.img bootsect.bin hello.img

.PHONY: all run run-disk run-hello clean
