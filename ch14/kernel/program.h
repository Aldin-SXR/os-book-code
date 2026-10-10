/* spark -- kernel/program.h
 *
 * The programs that run in ring 3.
 *
 * Everything below this line is compiled into the kernel image like the rest
 * of spark, and then linked into a section of its own so that its pages can
 * be handed to ring 3 and no others. That is a shortcut: a real kernel reads
 * a program off a disk, which is Chapter 15, and starts it with the machinery
 * of Chapter 16. What matters here is the boundary rather than where the
 * bytes came from, and this way there is no file system in the way of it.
 *
 * None of these functions may be called from the kernel. They would run
 * perfectly, in ring 0, with every privilege they are meant not to have,
 * which is exactly the mistake the chapter is about.
 */
#ifndef SPARK_PROGRAM_H
#define SPARK_PROGRAM_H

#include "types.h"

/* Prints two lines, asks its own pid, is refused a kernel pointer, and
 * exits with its pid as the status. */
void user_hello(void);

/* Three programs that each break one rule, to see who stops them. */
void user_cli(void);        /* executes a privileged instruction */
void user_poke(void);       /* writes to a page of the kernel    */
void user_fake_fault(void); /* raises a vector it may not raise  */

#endif
