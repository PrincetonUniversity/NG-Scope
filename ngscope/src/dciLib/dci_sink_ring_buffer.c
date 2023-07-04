#include <unistd.h>
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



#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "ngscope/hdr/dciLib/dci_sink_ring_buffer.h"
#include "ngscope/hdr/dciLib/ngscope_util.h"

void ngscope_dci_sink_cell_init(ngscope_dci_sink_cell_t* q){
	q->header 	= 0;
	q->cell_prb = 50; //default 10MHz
	q->nof_logged_dci 		= 0;
	q->recent_dl_reTx_t_us 	= 0;
	q->recent_ul_reTx_t_us 	= 0;
	q->recent_dl_reTx_tti 	= 0;
	q->recent_ul_reTx_tti 	= 0;
	
	for(int i=0; i<NOF_LOG_DCI; i++){
		memset(&q->dci[i], 0, sizeof(ue_dci_t));
	}
	return;
}


uint16_t find_dci_cell_tail(ngscope_dci_sink_cell_t* q){
	int nof_dci = q->nof_logged_dci;
	int header  = q->header;
	int tail_idx = (header - nof_dci + NOF_LOG_DCI) % NOF_LOG_DCI;
	//printf("nof_dci:%d header:%d | %d tail:%d | %d ", nof_dci, header, q->dci[header].tti, tail_idx, q->dci[tail_idx].tti);
	return q->dci[tail_idx].tti;
}

void ca_update_header(ngscope_dci_sink_CA_t* q){
	int 	nof_cell = q->nof_cell;
	uint16_t header_vec[MAX_NOF_CELL];
	uint16_t tail_vec[MAX_NOF_CELL];
	uint64_t curr_time[MAX_NOF_CELL];
	bool 	ca_ready = true;

	// NOTE::: here the header is the tii, no the index inside the array
	for(int i=0; i<nof_cell; i++){
		int cell_header = (q->cell_dci[i].header - 1) % NOF_LOG_DCI;
		header_vec[i] 	= q->cell_dci[i].dci[cell_header].tti;
		tail_vec[i] 	= find_dci_cell_tail(&q->cell_dci[i]);
		curr_time[i] 	= q->cell_dci[i].dci[cell_header].time_stamp;
		if(q->cell_dci[i].header == 0){
			ca_ready = false;
		}
		//printf("cell header:%d tail:%d ", header_vec[i], tail_vec[i]);
	}
	//printf("\n");

	
	if(q->ca_ready){
		// unwrap tti so that we can compare
		unwrap_tti_array(header_vec, nof_cell);

		uint16_t min_tti = min_in_array_uint16_v(header_vec, nof_cell);
		q->header 		= wrap_tti(min_tti);

		uint16_t max_tti = max_in_array_uint16_v(tail_vec, nof_cell);
		q->tail 		= wrap_tti(max_tti + 1);

		q->curr_time 	= mean_array_uint64(curr_time, nof_cell);

	}else{
		if(ca_ready){
			q->ca_ready = true; 
		}
	}
	//printf("CA Header:%d Tail:%d\n", q->header, q->tail);
	return;
}

void insert_single_dci(ngscope_dci_sink_CA_t* q, ue_dci_t* ue_dci){
	// get the pointer to the dci cell 
	uint8_t cell_idx = ue_dci->cell_idx;

	ngscope_dci_sink_cell_t* dci_cell = &q->cell_dci[cell_idx];

	int prev_header = (dci_cell->header - 1 + NOF_LOG_DCI) % NOF_LOG_DCI;
	int prev_tti 	= dci_cell->dci[prev_header].tti; 
	int curr_tti 	= ue_dci->tti;

	//printf("prev:%d curr:%d header:%d prev_header:%d\n", prev_tti, curr_tti, dci_cell->header, prev_header);
	if( ((prev_tti + 1) % 10240) != curr_tti){
		printf("We missing one DCI message! prev:%d curr:%d ", prev_tti, curr_tti);
		printf("header:%d prev_header:%d\n",  dci_cell->header, prev_header);
	}

	// print the received ue_dci
	//print_non_empty_ue_dci(ue_dci, cell_idx);

	// print cell related information
	//if(cell_idx == 0){
	//	//print_single_cell_info(dci_cell);
	//	print_single_cell_dci(dci_cell);
	//}

	// copy the dci messages
	memcpy(&dci_cell->dci[dci_cell->header], ue_dci, sizeof(ue_dci_t));

	// update the header
	dci_cell->header++;
    dci_cell->header =  dci_cell->header % NOF_LOG_DCI;

	// update the length	
	if(dci_cell->nof_logged_dci < NOF_LOG_DCI){
		dci_cell->nof_logged_dci++;
	}
	// check if current dci indicates retransmission
	if(ue_dci->dl_reTx == 1){
		dci_cell->recent_dl_reTx_t_us = ue_dci->time_stamp;
		dci_cell->recent_dl_reTx_tti  = ue_dci->tti;
	}

	if(ue_dci->ul_reTx == 1){
		dci_cell->recent_ul_reTx_t_us = ue_dci->time_stamp;
		dci_cell->recent_ul_reTx_tti  = ue_dci->tti;
	}

	ca_update_header(q);

	return;
}

/******  Now the interface of the ring buffer *******/
// init the ring buffer
void ngscope_dciSink_ringBuf_init(ngscope_dci_sink_CA_t* q){
	q->nof_cell 	= 1;
	q->header 		= 0;
	q->tail 		= 0;
	q->curr_time 	= 0;
	q->ca_ready 	= false;

	pthread_mutex_init(&q->mutex, NULL);

	for(int i=0; i<MAX_NOF_CELL; i++){
		ngscope_dci_sink_cell_init(&q->cell_dci[i]);
	}
	return;
}

// update the config of the ring buffer
int ngscope_dciSink_ringBuf_update_config(ngscope_dci_sink_CA_t* q, cell_config_t* cell_config){
	pthread_mutex_lock(&q->mutex);
	q->nof_cell = cell_config->nof_cell;
	q->rnti 	= cell_config->rnti;
	for(int i=0; i<q->nof_cell; i++){
		q->cell_dci[i].cell_prb = cell_config->cell_prb[i];
	}
	pthread_mutex_unlock(&q->mutex);
	return 1;
}


// insert one dci inside the ring buffer
int ngscope_dciSink_ringBuf_insert_dci(ngscope_dci_sink_CA_t* q, ue_dci_t* ue_dci){
	// insert dci to the ring buffer
    pthread_mutex_lock(&q->mutex);
	insert_single_dci(q, ue_dci);
    pthread_mutex_unlock(&q->mutex);
	return 1;
}

