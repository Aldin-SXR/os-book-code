# Code for Operating Systems: Build Your Own

This is the code for the book *Operating Systems: Build Your Own* by Aldin
Kovačević. Over its chapters the reader writes **spark**, a small x86 kernel
that boots on a bare machine, handles interrupts, manages its own memory, runs
several programs at once, and stops at a shell prompt.

Every listing in the book is copied from a file in this repository, and every
path the book names, such as `code/ch02/kernel/vga.c`, is the same file here
without the `code/` prefix: `ch02/kernel/vga.c`.

## One folder per chapter

`chNN/` is the whole kernel as it stands at the end of chapter NN, not a patch.
You can build any of them on its own, and the difference between two of them
is exactly what a chapter added:

```sh
cd ch07
make run
diff -ru ch06 ch07
```

`ch01/experiments/` and `ch02/experiments/` hold small programs that you run on
your own machine rather than on spark. Chapter 17 builds nothing, so there is
no `ch17/`.

## What you need

```sh
brew install lld nasm qemu                                  # macOS
sudo apt install clang lld nasm qemu-system-x86 make        # Debian, Ubuntu, WSL2
```

Then `make run` in a chapter folder opens a QEMU window with that chapter's
kernel. Chapter 1's says `spark is awake.`
