/* spark -- kernel/exec.h
 *
 * Turning a file into a running program.
 *
 * Chapter 14 ran programs that were compiled into the kernel and linked into
 * a section of their own, which was honest about the boundary and dishonest
 * about where programs come from. Chapter 15 gave spark a disk. This is the
 * piece between them: read a file, put it in memory the program is allowed
 * to reach, build the stack it expects to start on, and hand it the
 * processor.
 *
 * The file is a flat binary rather than an ELF image. A real loader reads
 * headers, maps several segments with different permissions, and relocates
 * or refuses to; this copies the whole file to one address and jumps at its
 * first byte, because that is the part of loading a program that is actually
 * about loading a program.
 */
#ifndef SPARK_EXEC_H
#define SPARK_EXEC_H

#include "types.h"
#include "proc.h"

/* Where every program is loaded. One address, because spark's processes
 * share an address space and a flat binary cannot be moved: user/link.ld
 * fixes every address in the program at this base. So spark runs one loaded
 * program at a time, which is what the shell does anyway. */
#define EXEC_BASE  0x50000000u
#define EXEC_PAGES 8

/* The most arguments a command line can carry into a program. */
#define EXEC_ARGS 8

/* Map the load region and the stack that goes with it. After vmm_init and
 * user_init, before the first spawn. */
void exec_init(void);

/* Load path, give it these arguments, and start it. out_inode is where its
 * SYS_WRITE goes: 0 for the screen, or the inode of a file to append to,
 * which is how the shell redirects. Returns the new process, or NULL. */
struct process *exec_spawn(const char *path,
                           char **argv, uint32_t argc,
                           uint32_t out_inode);

/* How many bytes the last program loaded, for the chapter to print. */
uint32_t exec_last_size(void);

#endif
