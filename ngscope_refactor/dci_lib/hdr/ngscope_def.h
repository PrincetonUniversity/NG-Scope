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

#define MAX_NOF_DCI_DECODER 6
#define MAX_NOF_RF_DEV 4
#define NOF_LOG_SF 32

//#define NOF_LOG_SUBF (NOF_LOG_SF * 10)

// the buffer size of the cell status tracker 32-SFN 
#define CELL_STATUS_RING_BUF_SIZE 320 

// the buffer size of the DCI-logger 32-SFN 
#define DCI_LOGGER_RING_BUF_SIZE 320 

#define MAX_MSG_PER_SUBF 10

#define MAX_DCI_BUFFER 30

#define PLOT_SF 10

#define MAX_TTI 10240

//#define TTI_TO_IDX(i) (i%NOF_LOG_SUBF)

#define DCI_DECODE_TIMEOUT 30

/*     LOGGING Related  */
#define LOG_DCI_RING_BUFFER
#define LOG_DCI_LOGGER

typedef struct{
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    int             nof_dci;
    int             header;   
}dci_ready_t;

typedef struct {
    ngscope_dci_per_sub_t   dci_per_sub;
    //float                   csi_amp[100 * 12];
    uint32_t                tti;
    uint16_t                cell_idx;
}ngscope_status_buffer_t;

#ifdef __cplusplus
}
#endif
#endif
