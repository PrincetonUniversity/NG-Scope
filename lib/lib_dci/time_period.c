#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <pthread.h>

uint64_t timestamp_ns(){
    struct timespec tvs;
    uint64_t curr_time;
    clock_gettime(CLOCK_MONOTONIC, &tvs);
    curr_time = tvs.tv_sec * 1e9 + tvs.tv_nsec;
    return curr_time;

}

uint32_t timestamp_us(){
    struct timespec tvs;
    uint64_t curr_time;
    uint32_t curr_time_us;
    clock_gettime(CLOCK_MONOTONIC, &tvs);
    curr_time = tvs.tv_sec * 1e9 + tvs.tv_nsec;
    curr_time_us = (uint32_t) (curr_time / 1e3);
    return curr_time_us;
}

uint32_t timestamp_ms(){
    struct timespec tvs;
    uint64_t curr_time;
    uint32_t curr_time_ms;
    clock_gettime(CLOCK_MONOTONIC, &tvs);
    curr_time = tvs.tv_sec * 1e9 + tvs.tv_nsec;

    curr_time_ms = (uint32_t) (curr_time / 1e6);

    return curr_time_ms;
}


