/* counter.c -- run two copies side by side: each reports the same address
 * for its counter and neither can see the other's.
 *
 *     cc -O0 -o counter counter.c
 *     ./counter & ./counter & wait
 */
#include <stdio.h>
#include <unistd.h>

int counter = 0;

int main(void)
{
    printf("pid %d: counter lives at %p\n", getpid(), (void *) &counter);
    for (int i = 0; i < 5; i++) {
        printf("pid %d: counter = %d\n", getpid(), counter);
        counter++;
        sleep(1);
    }
    return 0;
}
