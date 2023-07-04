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

#include "ngscope/hdr/dciLib/ngscope_def.h"
#include "ngscope/hdr/dciLib/parse_args.h"
#include "ngscope/hdr/dciLib/time_stamp.h"
#include "ngscope/hdr/dciLib/sync_dci_remote.h"
#include "ngscope/hdr/dciLib/dci_ring_buffer.h"
#include "ngscope/hdr/dciLib/dci_sink_def.h"
#include "ngscope/hdr/dciLib/dci_sink_sock.h"

extern ngscope_dci_sink_serv_t dci_sink_serv;

/* Operator */
bool a_larger_than_b(int a, int b, int buf_size){
    if( (a-b) >= 0){
       return true;
    }else{
        /* b --> 320 --> a 
           b 310   a 1 (a is larger then b in this case) */
        if( (abs(buf_size - b) < buf_size/8) && 
            (a < buf_size/8)){
            return true;
        }
    } 
    return false;
}

/* Carrier Aggregation Related Functions */
/* init the ca status */
int CA_status_init(CA_status_t* q, int buf_size, uint16_t targetRNTI, int nof_cell, int cell_prb[MAX_NOF_RF_DEV]){
	q->targetRNTI 	= targetRNTI;
	q->buf_size 	= buf_size;
	q->nof_cell 	= nof_cell;
	q->header 		= 0;
	q->all_cell_ready 		= false;
	
	memcpy(&q->cell_prb, cell_prb, MAX_NOF_RF_DEV *sizeof(int));
	return 0;
}

int find_CA_header(int cell_header[MAX_NOF_RF_DEV], int nof_cell, int buf_size){
	int min_header = cell_header[0];
	for(int i=1; i<nof_cell; i++){
		if(a_larger_than_b(min_header, cell_header[i], buf_size)){
			min_header = cell_header[i];
		}
	}
	return min_header;
}

void CA_status_update_header(CA_status_t* q, ngscope_cell_dci_ring_buffer_t 	p[MAX_NOF_RF_DEV]){
	int nof_cell = q->nof_cell; 
	int cell_header[MAX_NOF_RF_DEV] = {0};

	bool cell_ready = true;
	if(q->all_cell_ready == false){
		for(int i=0; i<nof_cell; i++){
			if(p[i].cell_ready == false){
				cell_ready = false;
			}
			cell_header[i] = p[i].cell_header;
		}

		if(cell_ready){
			q->all_cell_ready = true;
			q->header = find_CA_header(cell_header, nof_cell, q->buf_size);
			printf("\n\n\n ALL THREE CELL ARE READY!!! \n\n\n");
		}
	}	
	return;
}


/* Single Cell Related Functions */

/* fill the msg to one subframe */
void enqueue_dci_sf(sf_status_t*q, uint16_t targetRNTI, ngscope_status_buffer_t* dci_buffer){
	memcpy(q->dl_msg, &(dci_buffer->dci_per_sub.dl_msg), dci_buffer->dci_per_sub.nof_dl_dci);
	memcpy(q->ul_msg, &(dci_buffer->dci_per_sub.ul_msg), dci_buffer->dci_per_sub.nof_ul_dci);

	q->tti 				= dci_buffer->tti;
	q->nof_dl_msg 		= dci_buffer->dci_per_sub.nof_dl_dci;
	q->nof_ul_msg 		= dci_buffer->dci_per_sub.nof_ul_dci;

	//printf("log tti:%d\n", q->tti);
    // Set the logging timestamp
    //q->timestamp_us 	= timestamp_us();
    q->timestamp_us 	= dci_buffer->dci_per_sub.timestamp;

	/* copy downlink and uplink messages 
	 * NOTE: we need to copy all MAX_DCI_PER_SUB dci message */
	memcpy(&(q->dl_msg), &(dci_buffer->dci_per_sub.dl_msg), 
				MAX_DCI_PER_SUB * sizeof(ngscope_dci_msg_t));

	memcpy(&(q->ul_msg), &(dci_buffer->dci_per_sub.ul_msg), 
				MAX_DCI_PER_SUB * sizeof(ngscope_dci_msg_t));

	q->ue_dl_prb 	= 0;	
	q->ue_ul_prb 	= 0;	

	int cell_prb = 0;

	if(q->nof_dl_msg > 0){
		// handle the downlink 
		for(int i=0; i<q->nof_dl_msg; i++){
			cell_prb += q->dl_msg[i].prb;	
			if(q->dl_msg[i].rnti == targetRNTI){
				q->ue_dl_prb 	= q->dl_msg[i].prb;	
			}
		}
		q->cell_dl_prb = cell_prb;
	}

	if(q->nof_ul_msg > 0){
		cell_prb = 0;
		// handle the uplink 
		for(int i=0; i<q->nof_ul_msg; i++){
			cell_prb += q->ul_msg[i].prb;	
			if(q->ul_msg[i].rnti == targetRNTI){
				q->ue_ul_prb 	= q->ul_msg[i].prb;	
			}
		}
		q->cell_ul_prb = cell_prb;
	}

	/* Mark this subframe as filed*/
	q->filled = true;
	return;
}

//TODO change it later to remove the remote_sock
int push_dci_to_remote(sf_status_t* q, int cell_idx, uint16_t targetRNTI, int remote_sock){
	if(remote_sock <= 0){
		//printf("ERROR: sock not set!\n\n");
		return -1;
	}
	ue_dci_t ue_dci;	
	ue_dci.cell_idx  = cell_idx;
	ue_dci.time_stamp 	= q->timestamp_us;
	ue_dci.tti 		= 	q->tti;
	ue_dci.rnti 	= targetRNTI;

	ue_dci.dl_rv_flag 	= false;
	ue_dci.ul_rv_flag 	= false;

	int 	nof_dl_msg  = q->nof_dl_msg;
	int 	nof_ul_msg  = q->nof_ul_msg;

	for(int i=0; i<nof_dl_msg;i++){
		if(q->dl_msg[i].rnti == targetRNTI){
			ngscope_dci_msg_t dci_msg = q->dl_msg[i];
			ue_dci.dl_tbs 	= dci_msg.tb[0].tbs + dci_msg.tb[1].tbs;
			if(dci_msg.tb[0].rv > 0 || dci_msg.tb[1].rv > 0){
				ue_dci.dl_reTx = 1;
			}else{
				ue_dci.dl_reTx = 0;
			}
		}
	}

	for(int i=0; i<nof_ul_msg;i++){
		if(q->ul_msg[i].rnti == targetRNTI){
			ngscope_dci_msg_t dci_msg = q->ul_msg[i];
			ue_dci.ul_tbs 	= dci_msg.tb[0].tbs + dci_msg.tb[1].tbs;
			if(dci_msg.tb[0].rv > 0 || dci_msg.tb[1].rv > 0){
				ue_dci.ul_reTx = 1;
			}else{
				ue_dci.ul_reTx = 0;
			}
		}
	}

	sock_send_single_dci(&dci_sink_serv, &ue_dci, 0);

	return 1;
}

///* push to recorded dci to remote */
//int push_dci_to_remote(sf_status_t* q, int cell_idx, uint16_t targetRNTI, int remote_sock){
//	if(remote_sock <= 0){
//		//printf("ERROR: sock not set!\n\n");
//		return -1;
//	}
//	ngscope_dci_sync_t dci_sync;
//	dci_sync_init(&dci_sync);
//
//	// container for the dci to be pushed
//	/* We push one dci per subframe, 
//	   no matter we have the dci of the target UE or not*/
//	dci_sync.time_stamp = q->timestamp_us;
//	dci_sync.tti        = q->tti;
//	dci_sync.cell_idx   = cell_idx;
//
//	//printf("cell_idx:%d\n",cell_idx);
//	// We set to default protocol version
//	// TODO add more protocol and enable configuing it
//	dci_sync.proto_v 	= 0;
//
//	int 	nof_dl_msg  = q->nof_dl_msg;
//	int 	nof_ul_msg  = q->nof_ul_msg;
//
//	for(int i=0; i<nof_dl_msg;i++){
//		if(q->dl_msg[i].rnti == targetRNTI){
//			//printf("found rnti:%d tbs:%d \n", targetRNTI, q->dl_msg[i].tb[0].tbs);
//			dci_sync.downlink = true;
//			memcpy(&dci_sync.dl_dci, &q->dl_msg[i], sizeof(ngscope_dci_msg_t));
//		}
//	}
//	for(int i=0; i<nof_ul_msg;i++){
//		if(q->ul_msg[i].rnti == targetRNTI){
//			dci_sync.uplink = true;
//			memcpy(&dci_sync.ul_dci, &q->ul_msg[i], sizeof(ngscope_dci_msg_t));
//		}
//	}
//
//	ngscope_sync_dci_remote(remote_sock, dci_sync);
//	return 0;
//}

// check if the dci message decoding has timed out
// This is to prevent that in some cases the dci decoding of a specific 
// subframe has been missed
void check_dci_decoding_timeout(ngscope_cell_dci_ring_buffer_t* q){
	int header 		=  (q->cell_header + 1) % q->buf_size; 
	int recent_sf 	= q->most_recent_sf;
	while(header != recent_sf){
		//printf("header:%d\n",header);
		if(q->sub_stat[header].filled == false){
			int time_lag = (recent_sf - header + q->buf_size) % q->buf_size;
			if(time_lag > DCI_DECODE_TIMEOUT){
				q->sub_stat[header].filled = true;
			}
		}
		header = (header + 1) % q->buf_size;
	}
}

void update_cell_status_header(ngscope_cell_dci_ring_buffer_t* q, int remote_sock){
	if(q->nof_logged_dci < (q->buf_size / 8)){
		return;
	}
	check_dci_decoding_timeout(q);

	int index =  (q->cell_header + 1) % q->buf_size;
	while(q->sub_stat[index].filled){
		q->cell_header 				= index;
		q->sub_stat[index].filled 	= false;
		push_dci_to_remote(&q->sub_stat[index], q->cell_idx, q->targetRNTI, remote_sock);
		//fprintf(fd,"%d\t%d\t%d\t%d\t\n", q->sub_stat[index].tti, q->cell_header, index, tti);
		//printf("push tti:%d index:%d\n", q->sub_stat[index].tti, index);
		// update the index
		index =  (index + 1) % q->buf_size;
	}
	return;
}

void update_most_recent_sf(ngscope_cell_dci_ring_buffer_t* q, int index){
	int recent_sf 	= q->most_recent_sf;
	//if( abs(index - recent_sf)> 100 ){
	//	printf("The most recent sf and the new sf seems jump too much! Take care!\n");
	//}

	if(index > recent_sf){
		// only valid if index and recent_sf doesn't cross boundry
		// |-----------------------recent_sf  index --------------|
		if( abs(index - recent_sf)< (q->buf_size / 8) ){
			q->most_recent_sf = index;
		}
	}else if(index < recent_sf){
		// only valid if the index and recent sf across the boundry
		// |--index -----------------------------------recent_sf---|
		if( abs(index - recent_sf)> ( (6 * q->buf_size) / 8) ){
			q->most_recent_sf = index;
		}
	}
	
	return;
}
/* Init the cell status */
int dci_ring_buffer_init(ngscope_cell_dci_ring_buffer_t* q, uint16_t targetRNTI, int cell_prb, int cell_idx, int buf_size){
	q->targetRNTI 	= targetRNTI;
	q->cell_prb	 	= cell_prb;
	q->cell_ready 	= false;
	q->cell_header	= 0;
	q->cell_idx 	= cell_idx;

	q->nof_logged_dci 	= 0;
	q->most_recent_sf 	= 0;
	q->buf_size 		= buf_size;

	q->sub_stat 		= (sf_status_t *)calloc(buf_size, sizeof(sf_status_t));

#ifdef LOG_DCI_RING_BUFFER
	q->fd_log 	= fopen("dci_ring_buffer.txt","w+");
#endif
	return 0;
}

/* Delete cell status */

int dci_ring_buffer_delete(ngscope_cell_dci_ring_buffer_t* q){
	free(q->sub_stat);

#ifdef LOG_DCI_RING_BUFFER
	fclose(q->fd_log);
#endif
	return 0;
}

int dci_ring_buffer_log(ngscope_cell_dci_ring_buffer_t* q, int cell_idx, uint16_t tti){
	if(q->cell_idx == cell_idx){
		fprintf(q->fd_log, "%d\t%d\t%d\t", tti, q->cell_header, q->most_recent_sf);
		for(int i=0; i< q->buf_size; i++){
			fprintf(q->fd_log, "%d", q->sub_stat[i].filled);
			if(i % 40 == 0){
				fprintf(q->fd_log, " ");
			}
		}
		fprintf(q->fd_log, "\n");
	}
	return 0;
}

void dci_ring_buffer_clear_cell_fill_flag(ngscope_cell_dci_ring_buffer_t* q, int cell_idx){
	for(int i=0; i<q->buf_size; i++){
		q->sub_stat[cell_idx].filled = false;
	}
	return;
}

/* insert the dci into the cell status */
void dci_ring_buffer_put_dci(ngscope_cell_dci_ring_buffer_t* q, 
						ngscope_status_buffer_t* dci_buffer,
						int remote_sock)
{
	uint32_t tti 	= dci_buffer->tti;
	int 	 index 	= tti % q->buf_size;

	//printf("enqueue dci_buffer tti:%d index:%d\n", tti, index);

	/* if the cell is not ready (the first enqueued dci)
	* set the header to (current index - 1) */
	if(q->cell_ready == false){
		q->cell_ready = true;
		// in case index-1 is negative
		q->cell_header = (index - 1 + q->buf_size) % q->buf_size;
		q->most_recent_sf 	= q->cell_header;
	}

	/* Enqueue the dci into the cell */
	enqueue_dci_sf(&(q->sub_stat[index]), q->targetRNTI, dci_buffer);

	// increase the number logged dci (we enqueue only 1 dci)
	if(q->nof_logged_dci < q->buf_size){
		q->nof_logged_dci++;
	}

	update_most_recent_sf(q, index);

#ifdef LOG_DCI_RING_BUFFER
	dci_ring_buffer_log(q, 0, tti);
#endif
	/* update the header */
	uint16_t last_header = q->cell_header;
	update_cell_status_header(q, remote_sock);

	if(last_header != q->cell_header){
		/* Header has been updated */
	}
	return;
}



