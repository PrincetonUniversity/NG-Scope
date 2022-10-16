#ifndef NGSCOPE_CELL_STATUS
#define NGSCOPE_CELL_STATUS
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

#ifdef __cplusplus
extern "C" {
#endif

/* status of one subframe
 * Each sbframe includes multiple control messages */
typedef struct{
	uint16_t 	tti;

	bool 		filled;
    uint8_t     cell_dl_prb; // total cell downlink prb
    uint8_t     cell_ul_prb; // total cell uplink prb

    uint8_t     ue_dl_prb; 	 // total ue downlink prb
    uint8_t     ue_ul_prb;   // total ue uplink prb

    uint8_t     nof_dl_msg; 
    uint8_t     nof_ul_msg;

    uint64_t    timestamp_us; // time stamp of the decode msg

	ngscope_dci_msg_t dl_msg[MAX_DCI_PER_SUB];
    ngscope_dci_msg_t ul_msg[MAX_DCI_PER_SUB];

}sf_status_t;

typedef struct{
	uint16_t 	cell_header;
    uint16_t    targetRNTI;

	bool 	 	cell_ready; //If the cell is ready

	int 		cell_prb; 	//Total PRB the cell has

	sf_status_t sub_stat[NOF_LOG_SUBF];
}cell_status_t;

typedef struct{
    uint16_t    targetRNTI;
	int         nof_cell;
    int         header;
    int         cell_prb[MAX_NOF_RF_DEV];
    bool        all_cell_synced;
}CA_status_t;

typedef struct{
    uint16_t    targetRNTI;
    int         nof_cell;
    int         remote_sock;
	bool 		remote_enable;
    int         cell_prb[MAX_NOF_RF_DEV];
}cell_status_info_t;

typedef struct {
    ngscope_dci_per_sub_t   dci_per_sub;
    uint32_t                tti;
    uint16_t                cell_idx;
}cell_status_buffer_t;

void* cell_status_thread(void* arg);
#endif
