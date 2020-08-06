#ifndef LTESCOPE_MAIN_H
#define LTESCOPE_MAIN_H

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

int lteScope_init();
int lteScope_start();
int lteScope_wait_to_close();

void sig_int_handler(int signo);
#endif

