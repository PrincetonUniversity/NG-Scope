#ifndef NGSCOPE_STATUS_TRACKER_H
#define NGSCOPE_STATUS_TRACKER_H

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

#include "srsran/srsran.h"
#include "ngscope_def.h"
#include "load_config.h"

#ifdef __cplusplus
extern "C" {
#endif


// status of one cell
typedef struct{
    uint16_t    header;

    bool        ready;
    //handle multi-thread synchronization
    uint16_t    dci_touched;
    uint16_t    token[CELL_STATUS_RING_BUF_SIZE];

    uint16_t    tti[CELL_STATUS_RING_BUF_SIZE];

    uint8_t     cell_dl_prb[CELL_STATUS_RING_BUF_SIZE];
    uint8_t     cell_ul_prb[CELL_STATUS_RING_BUF_SIZE];

    uint8_t     ue_dl_prb[CELL_STATUS_RING_BUF_SIZE];
    uint8_t     ue_ul_prb[CELL_STATUS_RING_BUF_SIZE];

    uint8_t     nof_dl_msg[CELL_STATUS_RING_BUF_SIZE];
    uint8_t     nof_ul_msg[CELL_STATUS_RING_BUF_SIZE];

    uint64_t    timestamp_us[CELL_STATUS_RING_BUF_SIZE];

    ngscope_dci_msg_t dl_msg[MAX_DCI_PER_SUB][CELL_STATUS_RING_BUF_SIZE];
    ngscope_dci_msg_t ul_msg[MAX_DCI_PER_SUB][CELL_STATUS_RING_BUF_SIZE];

}ngscope_cell_status_t;

typedef struct{
    uint16_t    targetRNTI;
    int         nof_cell;
    int         header;
    int         cell_prb[MAX_NOF_RF_DEV];
    bool        all_cell_synced;
    ngscope_cell_status_t cell_status[MAX_NOF_RF_DEV];
}ngscope_CA_status_t;

typedef struct{
    //ngscope_CA_status_t ngscope_CA_status;
    //ngscope_ue_list_t   ue_list;
    int 		cell_prb[MAX_NOF_RF_DEV];
    int     	remote_sock;
	uint16_t    targetRNTI;
    int         nof_cell;
    bool        all_cell_synced;
}ngscope_status_tracker_t;

void* status_tracker_thread(void* p);
#endif
