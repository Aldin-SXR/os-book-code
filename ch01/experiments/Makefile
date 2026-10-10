# Host-side experiments. These are ordinary programs, built with the host
# compiler and run under the host operating system, because what they measure
# is the system the kernel is about to replace.
CFLAGS := -O0 -Wall -Wextra -std=c11

# cc where there is one. A fresh WSL2 or Debian install that followed the
# setup steps has clang and no cc, and make's default would fail there.
CC := $(shell command -v cc >/dev/null 2>&1 && echo cc || echo clang)

all: overcommit counter

overcommit: overcommit.c
	$(CC) $(CFLAGS) -o $@ $<

counter: counter.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f overcommit counter

.PHONY: all clean
