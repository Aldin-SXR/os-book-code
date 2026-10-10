/* overcommit.c -- request far more memory than the machine has and measure
 * what the request actually costs. Runs on the host, not on spark.
 *
 *     cc -O0 -o overcommit overcommit.c && ./overcommit
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#define RSS_TO_BYTES 1            /* macOS reports ru_maxrss in bytes */
#else
#include <unistd.h>
#define RSS_TO_BYTES 1024         /* Linux reports it in kilobytes */
#endif

static unsigned long long physical_memory(void)
{
#if defined(__APPLE__)
    unsigned long long bytes = 0;
    size_t len = sizeof bytes;
    int mib[2] = { CTL_HW, HW_MEMSIZE };
    if (sysctl(mib, 2, &bytes, &len, NULL, 0) != 0) return 0;
    return bytes;
#else
    long pages = sysconf(_SC_PHYS_PAGES), size = sysconf(_SC_PAGE_SIZE);
    if (pages < 0 || size < 0) return 0;
    return (unsigned long long) pages * (unsigned long long) size;
#endif
}

static unsigned long long resident(void)
{
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return (unsigned long long) usage.ru_maxrss * RSS_TO_BYTES;
}

int main(void)
{
    const size_t wanted = 64ULL * 1024 * 1024 * 1024;     /* 64 GiB */
    unsigned long long before = resident();

    printf("this machine has   %8llu MiB of RAM\n", physical_memory() >> 20);

    char *block = malloc(wanted);
    if (block == NULL) {
        printf("malloc(%zu) failed\n", wanted);
        return 1;
    }

    /* Touch both ends, so nobody can say the memory was never real. */
    block[0] = 'a';
    block[wanted - 1] = 'z';

    unsigned long long after = resident();
    printf("asked for          %8zu MiB\n", wanted >> 20);
    printf("wrote '%c' at       %10p\n", block[0], (void *) block);
    printf("wrote '%c' at       %10p\n", block[wanted - 1],
           (void *) (block + wanted - 1));
    printf("and it all cost    %8llu KiB of real memory\n",
           (after - before) >> 10);

    return 0;
}
