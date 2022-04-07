#ifndef NGSCOPE_DEF_H
#define NGSCOPE_DEF_H

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

#include "srsran/srsran.h"

#define MAX_NOF_DCI_DECODER 4
#define MAX_NOF_RF_DEV 4
#define NOF_LOG_SF 32
#define NOF_LOG_SUBF NOF_LOG_SF * 10
#define MAX_MSG_PER_SUBF 10
#define MAX_DCI_BUFFER 10

typedef struct{
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    int             nof_dci;
    int             header;   
}dci_ready_t;

typedef struct {
    ngscope_dci_per_sub_t   dci_per_sub;
    uint32_t                tti;
    uint16_t                cell_idx;
}ngscope_status_buffer_t;

#ifdef __cplusplus
}
#endif
#endif
