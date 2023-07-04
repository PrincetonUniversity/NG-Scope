#ifndef NGSCOPE_skip_tti_H
#define NGSCOPE_skip_tti_H

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

#ifdef __cplusplus
extern "C" {
#endif

#include "ngscope_def.h"

#define MAX_NOF_SKIP_TTI 200
typedef struct{
	uint32_t sf[MAX_NOF_SKIP_TTI];
	uint32_t sf_idx[MAX_NOF_SKIP_TTI];
	int 	nof_tti;
}task_skip_tti_t;

int skip_tti_init(task_skip_tti_t* q);
int skip_tti_put(task_skip_tti_t* q, uint32_t sf, uint32_t sf_idx);
uint32_t skip_tti_get(task_skip_tti_t* q);
int skip_tti_empty(task_skip_tti_t* q);

#endif
