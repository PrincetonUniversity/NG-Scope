#ifndef TIME_PERIOD_H
#define TIME_PERIOD_H

#include <stdio.h>
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
#include <semaphore.h>
#include <stdint.h>
uint64_t timestamp_ns();
uint32_t timestamp_us();
uint32_t timestamp_ms();

#endif
