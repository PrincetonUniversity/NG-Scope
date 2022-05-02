#ifndef NGSCOPE_TASK_SCHE_H
#define NGSCOPE_TASK_SCHE_H

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
#include "radio.h"

#define MAX_TMP_BUFFER 10

typedef struct{
    uint32_t        sf_idx; // subframe index 0-9
    uint32_t        sfn;    // system frame index 0-1020
    cf_t*           IQ_buffer[SRSRAN_MAX_PORTS]; //IQ buffer that stores the IQ sample
}task_tmp_sf_buffer_t;


typedef struct{
    uint32_t        sf_idx; // subframe index 0-9
    uint32_t        sfn;    // system frame index 0-1020
    cf_t*           IQ_buffer[SRSRAN_MAX_PORTS]; //IQ buffer that stores the IQ sample

    pthread_mutex_t         sf_mutex;
    pthread_cond_t          sf_cond;
}ngscope_sf_buffer_t;

typedef struct{
    srsran_rf_t         rf;
    srsran_cell_t       cell;
    srsran_ue_sync_t    ue_sync;
    prog_args_t         prog_args;
}ngscope_task_scheduler_t;

typedef struct{
    task_tmp_sf_buffer_t           sf_buf[MAX_TMP_BUFFER];
    int             header;
    int             tail;
}task_tmp_buffer_t;

int task_scheduler_init(ngscope_task_scheduler_t* task_scheduler,
                            prog_args_t prog_args);
//                            srsran_rf_t* rf, 
//                            srsran_cell_t* cell,
//                            srsran_ue_sync_t* ue_sync);
//
int task_scheduler_start(ngscope_task_scheduler_t* task_scheduler);
                            //prog_args_t prog_args);
                            //srsran_cell_t* cell,
                            //srsran_ue_sync_t* ue_sync);

void* task_scheduler_thread(void* p);
 
#ifdef __cplusplus
}
#endif

#endif
