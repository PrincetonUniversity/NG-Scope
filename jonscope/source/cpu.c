#include <pthread.h>
#include <stdarg.h>
#include "jonscope/headers/cpu.h"


int _pin_thread(int num_cpus, ...)
{
    va_list ap;
    cpu_set_t cpus;

    /* Clear CPU set */
    CPU_ZERO(&cpus);

    /* Assemble CPU set based on arguments */
    va_start(ap, num_cpus);
    while (num_cpus--)
        CPU_SET(va_arg(ap, int), &cpus);
    va_end(ap);

    /* Pin current thread to CPU set */
    return sched_setaffinity(0, sizeof(cpu_set_t), &cpus);
}