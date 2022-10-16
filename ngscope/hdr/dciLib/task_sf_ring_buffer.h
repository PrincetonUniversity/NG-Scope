#ifndef NGSCOPE_TASK_RING_BUF_H
#define NGSCOPE_TASK_RING_BUF_H

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

#define MAX_TMP_BUFFER 15

typedef struct{
    uint32_t        sf_idx; // subframe index 0-9
    uint32_t        sfn;    // system frame index 0-1020
    cf_t*           IQ_buffer[SRSRAN_MAX_PORTS]; //IQ buffer that stores the IQ sample
}task_tmp_sf_buffer_t;

typedef struct{
    task_tmp_sf_buffer_t           sf_buf[MAX_TMP_BUFFER];
    int   	header;
    int   	tail;
	int 	len;			
	bool 	full;
}task_tmp_buffer_t;

int task_sf_ring_buffer_init(task_tmp_buffer_t* q,
								int max_num_samples);
int task_sf_ring_buffer_free(task_tmp_buffer_t* q);
int task_sf_ring_buffer_put(task_tmp_buffer_t* q,
								cf_t* buffers[SRSRAN_MAX_CHANNELS],
								uint32_t sfn,
								uint32_t sf_idx,
								int rf_nof_rx_ant,
								int max_num_samples);

int task_sf_ring_buffer_get(task_tmp_buffer_t* q);
bool task_sf_ring_buffer_full(task_tmp_buffer_t* q);
bool task_sf_ring_buffer_empty(task_tmp_buffer_t* q);
int task_sf_ring_buffer_len(task_tmp_buffer_t* q);

#endif
