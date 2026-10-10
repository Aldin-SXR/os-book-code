/* spark -- kernel/shell.h
 *
 * The shell, which is the point of the whole book and is not part of the
 * kernel at all.
 *
 * Everything spark has built exists so that a person can type a word and
 * have a program run. The shell is the program that turns the first into
 * the second, and the striking thing about it is how little it is allowed
 * to do: it reads a line, splits it on spaces, finds a file, asks the
 * kernel to run it, and waits. Every one of those is something an ordinary
 * program can do.
 *
 * spark's shell runs in ring 0, for one reason and it is not privilege: the
 * loader of Chapter 14 puts a program at one fixed address, so only one
 * loaded program can exist at a time, and the shell has to be the thing
 * that is not it. A reader who skipped Chapter 14 can run the loaded
 * programs in ring 0 as well, and nothing else in this chapter changes.
 */
#ifndef SPARK_SHELL_H
#define SPARK_SHELL_H

#include "types.h"

/* The longest command line, and the longest word in one. */
#define SHELL_LINE 128
#define SHELL_WORD 32

/* Run one command line and wait for it to finish. echo says whether to
 * print the line first, which the scripted demonstration wants and an
 * interactive prompt does not, because the keyboard driver already did. */
void shell_run(const char *line, bool echo);

/* Read lines and run them until the machine stops. Does not return. */
void shell_loop(void) __attribute__((noreturn));

/* How many commands have been run, for the chapter to print. */
uint32_t shell_commands(void);

#endif
