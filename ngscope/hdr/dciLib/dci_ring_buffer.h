#ifndef NGSCOPE_DCI_RING_BUFFER
#define NGSCOPE_DCI_RING_BUFFER

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

	int 		cell_idx;
	bool 	 	cell_ready; //If the cell is ready

	int 		cell_prb; 	//Total PRB the cell has
	int 		nof_logged_dci;
	int 		most_recent_sf;

	int 		buf_size;

	//sf_status_t sub_stat[NOF_LOG_SUBF];
	sf_status_t* sub_stat;

	FILE* 		fd_log;

}ngscope_cell_dci_ring_buffer_t;

typedef struct{
    uint16_t    targetRNTI;
	int 		buf_size;
	int         nof_cell;
    int         header;
    int         cell_prb[MAX_NOF_RF_DEV];
    bool        all_cell_ready;
}CA_status_t;

/*Carrer Aggregation related */
int CA_status_init(CA_status_t* q, int buf_size, uint16_t targetRNTI, int nof_cell, int cell_prb[MAX_NOF_RF_DEV]);
void CA_status_update_header(CA_status_t* q, ngscope_cell_dci_ring_buffer_t  p[MAX_NOF_RF_DEV]);


/*Single Cell related */
int dci_ring_buffer_init(ngscope_cell_dci_ring_buffer_t* q, uint16_t targetRNTI, int cell_prb, int cell_idx, int buf_size);
int dci_ring_buffer_delete(ngscope_cell_dci_ring_buffer_t* q);
void dci_ring_buffer_put_dci(ngscope_cell_dci_ring_buffer_t* q, ngscope_status_buffer_t* dci_buffer, int remote_sock);
void dci_ring_buffer_clear_cell_fill_flag(ngscope_cell_dci_ring_buffer_t* q, int cell_idx);
#endif
