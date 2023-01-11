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
#include "ngscope/hdr/dciLib/thread_exit.h"
#include "ngscope/hdr/dciLib/ue_list.h"

extern bool go_exit;

//  Status-Tracker <--- DCI buffer ---> (DCI-Ring-Buffer -- Cell-Status-Tracker)
extern ngscope_status_buffer_t 	cell_stat_buffer[MAX_DCI_BUFFER];
extern dci_ready_t          	cell_stat_ready;

// DCI status container for Cell-Status-Tracker
//extern ngscope_cell_dci_ring_buffer_t 		cell_status[MAX_NOF_RF_DEV];
//extern CA_status_t   		ca_status;
//extern pthread_mutex_t 		cell_status_mutex;

extern ngscope_ue_list_t 	ue_list[MAX_NOF_RF_DEV];

extern bool task_scheduler_closed[MAX_NOF_RF_DEV];

///* init the ca status */
//int init_CA_status(CA_status_t* q, uint16_t targetRNTI, int nof_cell, int cell_prb[MAX_NOF_RF_DEV]){
//	q->targetRNTI 	= targetRNTI;
//	q->nof_cell 	= nof_cell;
//	q->header 		= 0;
//	q->all_cell_ready 		= false;
//	
//	memcpy(&q->cell_prb, cell_prb, MAX_NOF_RF_DEV *sizeof(int));
//	return 0;
//}
///* Operator */
//bool a_larger_than_b(int a, int b){
//    if( (a-b) >= 0){
//       return true;
//    }else{
//        /* b --> 320 --> a 
//           b 310   a 1 (a is larger then b in this case) */
//        if( (abs(NOF_LOG_SUBF - b) < NOF_LOG_SUBF/8) && 
//            (a < NOF_LOG_SUBF/8)){
//            return true;
//        }
//    } 
//    return false;
//}
//
//
//int find_header(int cell_header[MAX_NOF_RF_DEV], int nof_cell){
//	int min_header = cell_header[0];
//	for(int i=1; i<nof_cell; i++){
//		if(a_larger_than_b(min_header, cell_header[i])){
//			min_header = cell_header[i];
//		}
//	}
//	return min_header;
//}
//
//void update_CA_status_header(CA_status_t* q, ngscope_cell_dci_ring_buffer_t 	p[MAX_NOF_RF_DEV]){
//	int nof_cell = q->nof_cell; 
//	int cell_header[MAX_NOF_RF_DEV] = {0};
//
//	bool cell_ready = true;
//	if(q->all_cell_ready == false){
//		for(int i=0; i<nof_cell; i++){
//			if(p[i].cell_ready == false){
//				cell_ready = false;
//			}
//			cell_header[i] = p[i].cell_header;
//		}
//
//		if(cell_ready){
//			q->all_cell_ready = true;
//			q->header = find_header(cell_header, nof_cell);
//			printf("\n\n\n ALL THREE CELL ARE READY!!! \n\n\n");
//		}
//	}	
//	return;
//}
//
//
///* Init the cell status */
//int init_cell_status(ngscope_cell_dci_ring_buffer_t* q, uint16_t targetRNTI, int cell_prb, int cell_idx){
//	q->targetRNTI 	= targetRNTI;
//	q->cell_prb	 	= cell_prb;
//	q->cell_ready 	= false;
//	q->cell_header	= 0;
//	q->cell_idx 	= cell_idx;
//
//	q->nof_logged_dci 	= 0;
//	q->most_recent_sf 	= 0;
//
//	for(int i=0; i<NOF_LOG_SUBF; i++){
//		memset(&(q->sub_stat[i]), 0, sizeof(sf_status_t));
//	}
//	return 0;
//}
//
///* fill the msg to one subframe */
//void enqueue_dci_sf(sf_status_t*q, uint16_t targetRNTI, ngscope_status_buffer_t* dci_buffer){
//	memcpy(q->dl_msg, &(dci_buffer->dci_per_sub.dl_msg), dci_buffer->dci_per_sub.nof_dl_dci);
//	memcpy(q->ul_msg, &(dci_buffer->dci_per_sub.ul_msg), dci_buffer->dci_per_sub.nof_ul_dci);
//
//	q->tti 				= dci_buffer->tti;
//	q->nof_dl_msg 		= dci_buffer->dci_per_sub.nof_dl_dci;
//	q->nof_ul_msg 		= dci_buffer->dci_per_sub.nof_ul_dci;
//
//	//printf("log tti:%d\n", q->tti);
//    // Set the logging timestamp
//    q->timestamp_us 	= timestamp_us();
//
//	/* copy downlink and uplink messages 
//	 * NOTE: we need to copy all MAX_DCI_PER_SUB dci message */
//	memcpy(&(q->dl_msg), &(dci_buffer->dci_per_sub.dl_msg), 
//				MAX_DCI_PER_SUB * sizeof(ngscope_dci_msg_t));
//
//	memcpy(&(q->ul_msg), &(dci_buffer->dci_per_sub.ul_msg), 
//				MAX_DCI_PER_SUB * sizeof(ngscope_dci_msg_t));
//
//	q->ue_dl_prb 	= 0;	
//	q->ue_ul_prb 	= 0;	
//
//	int cell_prb = 0;
//
//	if(q->nof_dl_msg > 0){
//		// handle the downlink 
//		for(int i=0; i<q->nof_dl_msg; i++){
//			cell_prb += q->dl_msg[i].prb;	
//			if(q->dl_msg[i].rnti == targetRNTI){
//				q->ue_dl_prb 	= q->dl_msg[i].prb;	
//			}
//		}
//		q->cell_dl_prb = cell_prb;
//	}
//
//	if(q->nof_ul_msg > 0){
//		cell_prb = 0;
//		// handle the uplink 
//		for(int i=0; i<q->nof_ul_msg; i++){
//			cell_prb += q->ul_msg[i].prb;	
//			if(q->ul_msg[i].rnti == targetRNTI){
//				q->ue_ul_prb 	= q->ul_msg[i].prb;	
//			}
//		}
//		q->cell_ul_prb = cell_prb;
//	}
//
//	/* Mark this subframe as filed*/
//	q->filled = true;
//	return;
//}
//
//
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
//
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
//
//// check if the dci message decoding has timed out
//// This is to prevent that in some cases the dci decoding of a specific 
//// subframe has been missed
//void check_dci_decoding_timeout(ngscope_cell_dci_ring_buffer_t* q){
//	int header 		= TTI_TO_IDX( (q->cell_header + 1)); 
//	int recent_sf 	= q->most_recent_sf;
//	while(header != recent_sf){
//		//printf("header:%d\n",header);
//		if(q->sub_stat[header].filled == false){
//			int time_lag = (recent_sf - header + NOF_LOG_SUBF) % NOF_LOG_SUBF;
//			if(time_lag > DCI_DECODE_TIMEOUT){
//				q->sub_stat[header].filled = true;
//			}
//		}
//		header = TTI_TO_IDX((header + 1));
//	}
//}
//
//void update_cell_status_header(ngscope_cell_dci_ring_buffer_t* q, int remote_sock, FILE* fd, uint32_t tti){
//	if(q->nof_logged_dci < 50){
//		return;
//	}
//	check_dci_decoding_timeout(q);
//
//	int index = TTI_TO_IDX( (q->cell_header + 1));
//	while(q->sub_stat[index].filled){
//		q->cell_header 				= index;
//		q->sub_stat[index].filled 	= false;
//		push_dci_to_remote(&q->sub_stat[index], q->cell_idx, q->targetRNTI, remote_sock);
//		fprintf(fd,"%d\t%d\t%d\t%d\t\n", q->sub_stat[index].tti, q->cell_header, index, tti);
//		//printf("push tti:%d index:%d\n", q->sub_stat[index].tti, index);
//		// update the index
//		index = TTI_TO_IDX( (index + 1));
//	}
//	return;
//}
//
//void update_most_recent_sf(ngscope_cell_dci_ring_buffer_t* q, int index){
//	int recent_sf 	= q->most_recent_sf;
//	//if( abs(index - recent_sf)> 100 ){
//	//	printf("The most recent sf and the new sf seems jump too much! Take care!\n");
//	//}
//
//	if(index > recent_sf){
//		// only valid if index and recent_sf doesn't cross boundry
//		// |-----------------------recent_sf  index --------------|
//		if( abs(index - recent_sf)< 30 ){
//			q->most_recent_sf = index;
//		}
//	}else if(index < recent_sf){
//		// only valid if the index and recent sf across the boundry
//		// |--index -----------------------------------recent_sf---|
//		if( abs(index - recent_sf)> 250 ){
//			q->most_recent_sf = index;
//		}
//	}
//	
//	return;
//}
//
///* insert the dci into the cell status */
//void enqueue_dci_cell(ngscope_cell_dci_ring_buffer_t* q, 
//						ngscope_status_buffer_t* dci_buffer,
//						int remote_sock,
//						FILE* fd1,
//						FILE* fd2){
//
//	uint32_t tti 	= dci_buffer->tti;
//	int 	 index 	= TTI_TO_IDX(tti);
//
//	//printf("enqueue dci_buffer tti:%d index:%d\n", tti, index);
//
//	/* if the cell is not ready (the first enqueued dci)
//	* set the header to (current index - 1) */
//	if(q->cell_ready == false){
//		q->cell_ready = true;
//		// in case index-1 is negative
//		q->cell_header = TTI_TO_IDX(index - 1 + NOF_LOG_SUBF);
//		q->most_recent_sf 	= q->cell_header;
//	}
//
//	/* Enqueue the dci into the cell */
//	enqueue_dci_sf(&(q->sub_stat[index]), q->targetRNTI, dci_buffer);
//
//	// increase the number logged dci (we enqueue only 1 dci)
//	if(q->nof_logged_dci < NOF_LOG_SUBF){
//		q->nof_logged_dci++;
//	}
//
//	update_most_recent_sf(q, index);
//
//	/* update the header */
//	uint16_t last_header = q->cell_header;
//
//	if(q->cell_idx == 0){
//		fprintf(fd1, "%d\t%d\t%d\t%d\t", tti, index, last_header, q->most_recent_sf);
//		for(int i=0; i< NOF_LOG_SUBF; i++){
//			fprintf(fd1, "%d", q->sub_stat[i].filled);
//			if(i % 40 == 0){
//				fprintf(fd1, " ");
//			}
//		}
//		fprintf(fd1, "\n");
//	}
//
//	update_cell_status_header(q, remote_sock, fd2, tti);
//
//	if(last_header != q->cell_header){
//		/* Header has been updated */
//	}
//	return;
//}

void* cell_status_thread(void* arg){
	// get the info 
	cell_status_info_t info;
	info = *(cell_status_info_t *)arg;
	int remote_sock 	= info.remote_sock;
	
	int buf_size = CELL_STATUS_RING_BUF_SIZE; 
	int nof_dci;

	ngscope_cell_dci_ring_buffer_t 		cell_status[MAX_NOF_RF_DEV];
	CA_status_t   						ca_status;

	ngscope_status_buffer_t dci_buf[MAX_DCI_BUFFER];

	/* We now init two status buffer CA status and cell status */
	// --> init the CA status
	CA_status_init(&ca_status, buf_size, info.targetRNTI, info.nof_cell, info.cell_prb);

	// --> init the cell status
	for(int i=0; i<info.nof_cell; i++){
		dci_ring_buffer_init(&cell_status[i], info.targetRNTI, info.cell_prb[i], i, buf_size);
	}

	FILE* fd = fopen("cell_status.txt","w+");
	//FILE* fd_log = fopen("dci_log.txt","w+");
	//FILE* fd_tti = fopen("tti_log.txt","w+");

	while(true){
        if(go_exit) break;

        pthread_mutex_lock(&cell_stat_ready.mutex);
        // Use while in case some corner case conditional wake up
        while(cell_stat_ready.nof_dci <=0){
            pthread_cond_wait(&cell_stat_ready.cond, &cell_stat_ready.mutex);
        }
		nof_dci 		= cell_stat_ready.nof_dci;

		//fprintf(fd, "%d\t%d\n", cell_stat_buffer[0].tti, nof_dci);
		memcpy(dci_buf, cell_stat_buffer, nof_dci * sizeof(ngscope_status_buffer_t));

        // clean the dci buffer 
        memset(cell_stat_buffer, 0, nof_dci * sizeof(ngscope_status_buffer_t));

        // reset the dci buffer
        cell_stat_ready.header    = 0;
        cell_stat_ready.nof_dci   = 0;
        pthread_mutex_unlock(&cell_stat_ready.mutex);

		//printf("cell_stat: tti:%d cell_idx:%d \n", dci_buf[0].tti, dci_buf[0].cell_idx);
//		//printf("cell ready:%d \n", cell_status[0].cell_ready);
        //pthread_mutex_lock(&cell_status_mutex);
		for(int i=0; i<nof_dci; i++){
			int cell_idx = dci_buf[i].cell_idx;	
  			fprintf(fd, "%d\t%d\t%d\t\n", dci_buf[i].tti, nof_dci, cell_status[cell_idx].cell_header);
			//fprintf(fd, "%d\t%d\n", dci_buf[i].tti, nof_dci);
			//enqueue the dci to the according cell status buffer
			dci_ring_buffer_put_dci(&(cell_status[cell_idx]), &(dci_buf[i]), remote_sock);
			//int header = cell_status[cell_idx].cell_header;
			//printf("cell status header:%d ready:%d\n", header, cell_status[cell_idx].cell_ready);
		}
		CA_status_update_header(&ca_status, cell_status);
        //pthread_mutex_unlock(&cell_status_mutex);

		// update ue list
		for(int i=0; i<nof_dci; i++){

			ngscope_ue_list_enqueue_rnti_per_sf_per_cell(ue_list, &dci_buf[i]);

			//int cell_idx = dci_buf[i].cell_idx;	
			//for(int j=0; j<dci_buf[i].dci_per_sub.nof_dl_dci; j++){
			//	enqueue_ue_list_rnti(&(ue_list[cell_idx]), dci_buf[i].tti, \
			//			dci_buf[i].dci_per_sub.dl_msg[j].rnti, true);
			//}
			//for(int j=0; j<dci_buf[i].dci_per_sub.nof_ul_dci; j++){
			//	enqueue_ue_list_rnti(&(ue_list[cell_idx]), dci_buf[i].tti, \
			//			dci_buf[i].dci_per_sub.ul_msg[j].rnti, false);
			//}
		}
    } 

 	wait_for_ALL_RF_DEV_close();        
	for(int i=0; i<info.nof_cell; i++){

		ngscope_ue_list_print_ue_freq(&(ue_list[i]));

		// delete the ring buffer
		dci_ring_buffer_delete(&(cell_status[i]));
	}
	
	fclose(fd);
	//fclose(fd_log);
	//fclose(fd_tti);
	printf("CELL STATUS TRACK CLOSED!\n");

	return NULL;
}
