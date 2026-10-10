# spark -- build and run
#
# Three tools, none of them configured the way the host's own toolchain is.
# clang is told to produce 32-bit code for a machine with no operating system,
# nasm assembles the boot code, and ld.lld places the pieces where link.ld
# says they go.

CC := clang
AS := nasm
LD := ld.lld

# -target i386-none-elf : 32-bit x86, no OS, ELF object files
# -ffreestanding        : there is no standard library, so do not assume one
# -fno-stack-protector  : its guard calls a runtime function that is absent
# -fno-pic              : the load address is fixed, so no indirection
# -mno-sse              : no vector instructions; they fault until enabled
CFLAGS  := -target i386-none-elf -ffreestanding \
           -fno-stack-protector -fno-pic -mno-sse \
           -std=c11 -Wall -Wextra -O2 -g
ASFLAGS := -f elf32 -g
LDFLAGS := -m elf_i386 -T link.ld

CSRC := $(wildcard kernel/*.c)
OBJS := boot.o $(CSRC:.c=.o)

all: spark.bin

spark.bin: $(OBJS) link.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

run: spark.bin
	qemu-system-i386 -kernel spark.bin

clean:
	rm -f $(OBJS) spark.bin

.PHONY: all run clean
