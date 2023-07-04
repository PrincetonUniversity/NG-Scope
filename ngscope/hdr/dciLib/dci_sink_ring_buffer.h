#ifndef DCI_RING_BUF_HH 
#define DCI_RING_BUF_HH

#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>

#include "dci_sink_def.h"


// the ring buffer that stores the dci of a single cell
typedef struct{
	int 		cell_prb;
	int 		header;
	int 		nof_logged_dci;

	uint64_t	recent_dl_reTx_t_us;	
	uint64_t	recent_ul_reTx_t_us;	

	uint16_t 	recent_dl_reTx_tti;
	uint16_t 	recent_ul_reTx_tti;

	// the dci 
	ue_dci_t 	dci[NOF_LOG_DCI];	
}ngscope_dci_sink_cell_t;

/* The combination of multiple ring buffers that
 * store the dci with Carrier Aggregation Implemented */
typedef struct{
	bool ca_ready;
	int header;
	int tail;
	int nof_cell;
	uint64_t curr_time;
	uint16_t rnti;

	ngscope_dci_sink_cell_t cell_dci[MAX_NOF_CELL];
	pthread_mutex_t mutex;  
}ngscope_dci_sink_CA_t;



void ngscope_dciSink_ringBuf_init(ngscope_dci_sink_CA_t* q);

int ngscope_dciSink_ringBuf_update_config(ngscope_dci_sink_CA_t* q, cell_config_t* cell_config);

int ngscope_dciSink_ringBuf_insert_dci(ngscope_dci_sink_CA_t* q, ue_dci_t* ue_dci);

#endif
