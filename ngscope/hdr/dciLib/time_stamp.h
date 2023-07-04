#ifndef NGSCOPE_TIME_S_H
#define NGSCOPE_TIME_S_H

#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/socket.h>


#ifdef __cplusplus
extern "C" {
#endif

#include "srsran/srsran.h"

/* Timestamp related function */
int64_t timestamp_ns();
int64_t timestamp_us();
int64_t timestamp_ms();

#endif
