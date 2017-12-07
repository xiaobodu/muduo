#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <unistd.h>
#include <errno.h>
#include <numa.h>
#include <sched.h>
#include <sys/stat.h>
#include <assert.h>

#include "cpu.h"

#if  __GNUC_PREREQ (4,6)
#pragma GCC diagnostic push
#endif
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"


/*----------------------------------------------------------------------------*/
int GetNumCPUs()
{
    return sysconf(_SC_NPROCESSORS_ONLN);
}
/*----------------------------------------------------------------------------*/

int
CoreAffinitize(int cpu)
{
    cpu_set_t *cmask;
    struct bitmask *bmask;
    size_t n;
    int ret;

    n = GetNumCPUs();

    if (cpu < 0 || cpu >= (int) n) {
            errno = -EINVAL;
            return -1;
        }

    cmask = CPU_ALLOC(n);
    if (cmask == NULL)
        return -1;

    CPU_ZERO_S(n, cmask);
    CPU_SET_S(cpu, n, cmask);

    ret = sched_setaffinity(0, n, cmask);

    CPU_FREE(cmask);

    if (numa_max_node() == 0)
        return ret;

    bmask = numa_bitmask_alloc(16);
    assert(bmask);

    numa_bitmask_setbit(bmask, cpu %2);
    numa_set_membind(bmask);
    numa_bitmask_free(bmask);

    return ret;
}

#if  __GNUC_PREREQ (4,6)
#pragma GCC diagnostic pop
#else
#pragma GCC diagnostic warning "-Wconversion"
#pragma GCC diagnostic warning "-Wold-style-cast"
#endif
