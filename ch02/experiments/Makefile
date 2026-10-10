# Host-side experiments. Ordinary C programs, built with the host compiler,
# because what they report is the behaviour of the host's compiler and
# machine rather than the kernel's.
CFLAGS := -O0 -Wall -Wextra -std=c11

all: sizes pointers layout

sizes: sizes.c
	$(CC) $(CFLAGS) -o $@ $<
pointers: pointers.c
	$(CC) $(CFLAGS) -o $@ $<
layout: layout.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f sizes pointers layout

.PHONY: all clean
