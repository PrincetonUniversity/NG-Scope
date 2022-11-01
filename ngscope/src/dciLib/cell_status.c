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

#include "ngscope/hdr/dciLib/status_tracker.h"
#include "ngscope/hdr/dciLib/status_plot.h"
#include "ngscope/hdr/dciLib/ngscope_def.h"
#include "ngscope/hdr/dciLib/parse_args.h"
#include "ngscope/hdr/dciLib/dci_log.h"
#include "ngscope/hdr/dciLib/time_stamp.h"
#include "ngscope/hdr/dciLib/socket.h"
#include "ngscope/hdr/dciLib/sync_dci_remote.h"
#include "ngscope/hdr/dciLib/cell_status.h"

extern bool go_exit;

extern cell_status_buffer_t      cell_stat_buffer[MAX_DCI_BUFFER];
extern dci_ready_t               cell_stat_ready;

extern ngscope_ue_list_t 	ue_list[MAX_NOF_RF_DEV];


extern cell_status_t 	cell_status[MAX_NOF_RF_DEV];
extern CA_status_t   	ca_status;
extern pthread_mutex_t 	cell_status_mutex;

/* init the ca status */
int init_CA_status(uint16_t targetRNTI, int nof_cell, int cell_prb[MAX_NOF_RF_DEV]){
	ca_status.targetRNTI 	= targetRNTI;
	ca_status.nof_cell 		= nof_cell;
	ca_status.header 		= 0;
	ca_status.all_cell_synced 		= false;
	
	memcpy(ca_status.cell_prb, cell_prb, MAX_NOF_RF_DEV *sizeof(int));
	return 0;
}

/* Init the cell status */
int init_cell_status(uint16_t targetRNTI, int cell_prb, int cell_idx){
	cell_status[cell_idx].targetRNTI = targetRNTI;
	cell_status[cell_idx].cell_prb	 = cell_prb;
	cell_status[cell_idx].cell_ready = false;
	cell_status[cell_idx].cell_header= 0;
	cell_status[cell_idx].cell_idx 	 = cell_idx;

	for(int i=0; i<NOF_LOG_SUBF; i++){
		memset(&(cell_status[cell_idx].sub_stat[i]), 0, sizeof(sf_status_t));
	}
	return 0;
}

/* fill the msg to one subframe */
void enqueue_dci_sf(sf_status_t*q, 
					uint16_t targetRNTI,
					cell_status_buffer_t* dci_buffer){
	memcpy(q->dl_msg, &(dci_buffer->dci_per_sub.dl_msg), dci_buffer->dci_per_sub.nof_dl_dci);
	memcpy(q->ul_msg, &(dci_buffer->dci_per_sub.ul_msg), dci_buffer->dci_per_sub.nof_ul_dci);

	q->tti 				= dci_buffer->tti;
	q->nof_dl_msg 		= dci_buffer->dci_per_sub.nof_dl_dci;
	q->nof_ul_msg 		= dci_buffer->dci_per_sub.nof_ul_dci;

	//printf("log tti:%d\n", q->tti);
    // Set the logging timestamp
    q->timestamp_us 	= timestamp_us();

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

int push_dci_to_remote(sf_status_t* q, int cell_idx, uint16_t targetRNTI, int remote_sock){
	if(remote_sock <= 0){
		//printf("ERROR: sock not set!\n\n");
		return -1;
	}
	ngscope_dci_sync_t dci_sync;
	dci_sync_init(&dci_sync);

	// container for the dci to be pushed
	/* We push one dci per subframe, 
	   no matter we have the dci of the target UE or not*/
	dci_sync.time_stamp = q->timestamp_us;
	dci_sync.tti        = q->tti;

	dci_sync.cell_idx   = cell_idx;

	// We set to default protocol version
	// TODO add more protocol and enable configuing it
	dci_sync.proto_v 	= 0;

	int 	nof_dl_msg  = q->nof_dl_msg;
	int 	nof_ul_msg  = q->nof_ul_msg;

	for(int i=0; i<nof_dl_msg;i++){
		if(q->dl_msg[i].rnti == targetRNTI){
			//printf("found rnti:%d tbs:%d \n", targetRNTI, q->dl_msg[i].tb[0].tbs);
			dci_sync.downlink = true;
			memcpy(&dci_sync.dl_dci, &q->dl_msg[i], sizeof(ngscope_dci_msg_t));
		}
	}
	for(int i=0; i<nof_ul_msg;i++){
		if(q->ul_msg[i].rnti == targetRNTI){
			dci_sync.uplink = true;
			memcpy(&dci_sync.ul_dci, &q->ul_msg[i], sizeof(ngscope_dci_msg_t));
		}
	}

	ngscope_sync_dci_remote(remote_sock, dci_sync);

	return 0;
}


void update_cell_status_header(cell_status_t* q, int remote_sock){
	int index = TTI_TO_IDX( (q->cell_header + 1));
	while(q->sub_stat[index].filled){
		q->cell_header 				= index;
		q->sub_stat[index].filled 	= false;

		push_dci_to_remote(&q->sub_stat[index], q->cell_idx, q->targetRNTI, remote_sock);
		//printf("push tti:%d index:%d\n", q->sub_stat[index].tti, index);
		// update the index
		index = TTI_TO_IDX( (index + 1));
	}
	return;
}

/* insert the dci into the cell status */
void enqueue_dci_cell(cell_status_t*q, 
						cell_status_buffer_t* dci_buffer,
						int remote_sock){

	uint32_t tti 	= dci_buffer->tti;
	int 	 index 	= TTI_TO_IDX(tti);

	//printf("enqueue dci_buffer tti:%d index:%d\n", tti, index);

	/* if the cell is not ready (the first enqueued dci)
	* set the header to (current index - 1) */
	if(q->cell_ready == false){
		q->cell_ready = true;
		// in case index-1 is negative
		q->cell_header = TTI_TO_IDX(index - 1 + NOF_LOG_SUBF);
	}

	/* Enqueue the dci into the cell */
	enqueue_dci_sf(&(q->sub_stat[index]), q->targetRNTI, dci_buffer);

	/* update the header */
	uint16_t last_header = q->cell_header;
	update_cell_status_header(q, remote_sock);
	if(last_header != q->cell_header){
		/* Header has been updated */
	}
	return;
}

void enqueue_dci_rnti(ngscope_ue_list_t* q, uint32_t tti, uint16_t rnti, bool dl){
    if(q->ue_cnt[rnti] == 0){
        q->ue_enter_time[rnti] = tti;
    }    
    q->ue_cnt[rnti]++;
    q->ue_last_active[rnti] = tti; 
    if(dl){
        q->ue_dl_cnt[rnti]++;
    }else{
        q->ue_ul_cnt[rnti]++;
    }
    return;
}

void find_max_freq_ue(ngscope_ue_list_t* q){
    // we only count the C-RNTI
    int max_cnt = 0, idx = 0; 
    int max_dl_cnt =0, dl_idx = 0;
    int max_ul_cnt =0, ul_idx = 0;
    for(int i=10; i<65524; i++){
        if(q->ue_cnt[i] > max_cnt){
            max_cnt = q->ue_cnt[i];
            idx     = i;
        } 
        if(q->ue_dl_cnt[i] > max_dl_cnt){
            max_dl_cnt = q->ue_dl_cnt[i];
            dl_idx     = i;
        }
        if(q->ue_ul_cnt[i] > max_ul_cnt){
            max_ul_cnt = q->ue_ul_cnt[i];
            ul_idx     = i;
        }
    }
    q->max_freq_ue = idx;
    q->max_dl_freq_ue = dl_idx;
    q->max_ul_freq_ue = ul_idx;
    return;
}

int cell_status_print_ue_freq(ngscope_ue_list_t* q){
	// find the max frequency ue in the ue list
	find_max_freq_ue(q);

	// print the max frequency ue in the ue list
    printf("High Freq UE: DL rnti:%d freq:%d | UL rnti:%d freq:%d | Total rnti:%d freq: %d\n",
        q->max_dl_freq_ue, q->ue_dl_cnt[q->max_dl_freq_ue], 
        q->max_ul_freq_ue, q->ue_ul_cnt[q->max_ul_freq_ue], 
        q->max_freq_ue, q->ue_cnt[q->max_freq_ue]);
    return 0;
}


void* cell_status_thread(void* arg){
	// get the info 
	cell_status_info_t info;
	info = *(cell_status_info_t *)arg;
	int remote_sock 	= info.remote_sock;
	
	int nof_dci;
	cell_status_buffer_t dci_buf[MAX_DCI_BUFFER];

	/* We now init two status buffer CA status and cell status */
	// --> init the CA status
	init_CA_status(info.targetRNTI, info.nof_cell, info.cell_prb);

	// --> init the cell status
	for(int i=0; i<info.nof_cell; i++){
		init_cell_status(info.targetRNTI, info.cell_prb[i], i);
	}

	FILE* fd = fopen("cell_status.txt","w+");
	while(true){
        if(go_exit) break;

        pthread_mutex_lock(&cell_stat_ready.mutex);
        // Use while in case some corner case conditional wake up
        while(cell_stat_ready.nof_dci <=0){
            pthread_cond_wait(&cell_stat_ready.cond, &cell_stat_ready.mutex);
        }
		nof_dci 		= cell_stat_ready.nof_dci;
		//fprintf(fd, "%d\t%d\n", cell_stat_buffer[0].tti, nof_dci);
		memcpy(dci_buf, cell_stat_buffer, nof_dci * sizeof(cell_status_buffer_t));

        // clean the dci buffer 
        memset(cell_stat_buffer, 0, nof_dci * sizeof(cell_status_buffer_t));

        // reset the dci buffer
        cell_stat_ready.header    = 0;
        cell_stat_ready.nof_dci   = 0;
        pthread_mutex_unlock(&cell_stat_ready.mutex);

		//printf("cell_stat: tti:%d cell_idx:%d \n", dci_buf[0].tti, dci_buf[0].cell_idx);
//		//printf("cell ready:%d \n", cell_status[0].cell_ready);
        pthread_mutex_lock(&cell_status_mutex);
		for(int i=0; i<nof_dci; i++){
			int cell_idx = dci_buf[i].cell_idx;	
  			fprintf(fd, "%d\t%d\t%d\t\n", dci_buf[i].tti, nof_dci, cell_status[cell_idx].cell_header);
			//fprintf(fd, "%d\t%d\n", dci_buf[i].tti, nof_dci);
			//enqueue the dci to the according cell status buffer
			enqueue_dci_cell(&(cell_status[cell_idx]), &(dci_buf[i]), remote_sock);

			//int header = cell_status[cell_idx].cell_header;
			//printf("cell status header:%d ready:%d\n", header, cell_status[cell_idx].cell_ready);
		}
        pthread_mutex_unlock(&cell_status_mutex);

		// update ue list
		for(int i=0; i<nof_dci; i++){
			int cell_idx = dci_buf[i].cell_idx;	
			for(int j=0; j<dci_buf[i].dci_per_sub.nof_dl_dci; j++){
				enqueue_dci_rnti(&(ue_list[cell_idx]), dci_buf[i].tti, \
						dci_buf[i].dci_per_sub.dl_msg[j].rnti, true);
			}
			for(int j=0; j<dci_buf[i].dci_per_sub.nof_ul_dci; j++){
				enqueue_dci_rnti(&(ue_list[cell_idx]), dci_buf[i].tti, \
						dci_buf[i].dci_per_sub.ul_msg[j].rnti, false);
			}
		}
    } 

	sleep(1);
	for(int i=0; i<info.nof_cell; i++){
		cell_status_print_ue_freq(&(ue_list[i]));
	}
	fclose(fd);
	return NULL;
}
