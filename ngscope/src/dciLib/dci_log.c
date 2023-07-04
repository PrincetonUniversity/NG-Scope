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

#include "ngscope/hdr/dciLib/dci_log.h"
#include "ngscope/hdr/dciLib/cell_status.h"
#include "ngscope/hdr/dciLib/socket.h"
#include "ngscope/hdr/dciLib/parse_args.h"
#include "ngscope/hdr/dciLib/thread_exit.h"

extern bool go_exit;
//extern ngscope_cell_dci_ring_buffer_t 	cell_status[MAX_NOF_RF_DEV];
//extern CA_status_t   	ca_status;
//extern pthread_mutex_t 	cell_status_mutex;

// DCI status container for DCI-Logger 
//extern ngscope_cell_dci_ring_buffer_t 		log_cell_status[MAX_NOF_RF_DEV];
//extern CA_status_t   						log_ca_status;

//  Status-Tracker <--- DCI buffer ---> (DCI-Ring-Buffer -- DCI-Logger)
extern ngscope_status_buffer_t      log_stat_buffer[MAX_DCI_BUFFER];
extern dci_ready_t               	log_stat_ready;


void log_dl_subframe(sf_status_t* q,
					FILE* fd_dl)
{
	int nof_dl_msg = q->nof_dl_msg;
	/* Logging downlink messages */
	if(nof_dl_msg > 0){
		for(int i=0; i<nof_dl_msg; i++){
			// TTI RNTI
			fprintf(fd_dl, "%d\t%d\t", q->tti, q->dl_msg[i].rnti);
			//printf("DL rnti:%d \n",  q.dl_msg[i].rnti);

			// CELL_PRB UE_PRB
			fprintf(fd_dl, "%d\t%d\t", q->cell_dl_prb, q->dl_msg[i].prb);

			// TB1 related information
			fprintf(fd_dl, "%d\t%d\t%d\t", q->dl_msg[i].tb[0].mcs, q->dl_msg[i].tb[0].rv,
								q->dl_msg[i].tb[0].tbs);
			// TB2 related information
			if(q->dl_msg[i].nof_tb > 1){
				fprintf(fd_dl, "%d\t%d\t%d\t", q->dl_msg[i].tb[1].mcs, q->dl_msg[i].tb[1].rv,
								q->dl_msg[i].tb[1].tbs);
			}else{
				fprintf(fd_dl, "%d\t%d\t%d\t", 0, 0, 0);
			}

			fprintf(fd_dl, "%d\t%ld\t",  q->dl_msg[i].harq, q->timestamp_us);

			fprintf(fd_dl, "%d\t",  q->dl_msg[i].tb[0].ndi);
			if(q->dl_msg[i].nof_tb > 1){
				fprintf(fd_dl, "%d\t",  q->dl_msg[i].tb[1].ndi);
			}else{
				fprintf(fd_dl, "%d\t",  0);
			}

			fprintf(fd_dl, "\n");
		}
	}else{
		// We must fill the TTI even if there is no dci message inside this subframe
		fprintf(fd_dl, "%d\t", q->tti);
		for(int i=0; i<10;i++){
			fprintf(fd_dl, "%d\t", 0);
		}
		fprintf(fd_dl, "%ld\t", q->timestamp_us);
		for(int i=0; i<2;i++){
			fprintf(fd_dl, "%d\t", 0);
		}
		fprintf(fd_dl, "\n");
	}
	return;
}

void log_ul_subframe(sf_status_t* q,
					FILE* fd_ul)
{
    int nof_ul_msg = q->nof_ul_msg;

	/* Logging uplink messages */
	if(nof_ul_msg > 0){
		for(int i=0; i<nof_ul_msg; i++){
			// TTI RNTI
			fprintf(fd_ul, "%d\t%d\t", q->tti, q->ul_msg[i].rnti);
			// CELL_PRB UE_PRB
			fprintf(fd_ul, "%d\t%d\t", q->cell_ul_prb, q->ul_msg[i].prb);
			// TB1 related information
			fprintf(fd_ul, "%d\t%d\t%d\t", q->ul_msg[i].tb[0].mcs, q->ul_msg[i].tb[0].rv,
								q->ul_msg[i].tb[0].tbs);

			// TB2 related information --> UPlink has no second TB yet
			if(q->ul_msg[i].nof_tb > 1){
				fprintf(fd_ul, "%d\t%d\t%d\t", q->ul_msg[i].tb[1].mcs, q->ul_msg[i].tb[1].rv,
								q->ul_msg[i].tb[1].tbs);
			}else{
				fprintf(fd_ul, "%d\t%d\t%d\t", 0, 0, 0);
			}
			fprintf(fd_ul, "%d\t%ld\t",  0, q->timestamp_us);
			fprintf(fd_ul, "\n");
		}
	}else{
		// We must fill the TTI even if there is no dci message inside this subframe
		fprintf(fd_ul, "%d\t", q->tti);
		for(int i=0; i<10;i++){
			fprintf(fd_ul, "%d\t", 0);
		}
		fprintf(fd_ul, "%ld\n", q->timestamp_us);
	}

	return;
}

/* Logging related function */
void log_per_subframe(sf_status_t* q, ngscope_dci_log_config_t* config, int cell_idx)
{
    //printf("TTI:%d idx:%d nof_dl_msg:%d nof_ul_msg:%d\n", q.tti[buf_idx], buf_idx, nof_dl_msg, nof_ul_msg);

    if(config->log_dl[cell_idx]){
		log_dl_subframe(q, config->fd_dl[cell_idx]);
    }

    if(config->log_ul[cell_idx]){
		log_ul_subframe(q, config->fd_ul[cell_idx]);
    }

    return;
}

void log_per_cell(ngscope_cell_dci_ring_buffer_t* q, ngscope_dci_log_config_t* config, int cell_idx)
{
	int start_idx = config->curr_header[cell_idx];
	int end_idx   = q->cell_header;

#ifdef LOG_DCI_LOGGER
	fprintf(config->fd_log_cell, "%d\t%d\t\n", start_idx, end_idx);
#endif

	if(q->cell_ready){
		// we are not logging single dci 
		if(start_idx == end_idx){
			return;
		}else if(start_idx > end_idx){
			end_idx += q->buf_size;
		}
		for(int i=start_idx+1; i<=end_idx; i++){
			if(go_exit) break;
			// moving forward until we reach the current header
			int idx = i % q->buf_size;
			// log the dci
			log_per_subframe(&q->sub_stat[idx], config, cell_idx);
		}
	}
	//fclose(fd);
    return;
}

void log_multi_cell(ngscope_cell_dci_ring_buffer_t* 	q, 
					ngscope_dci_log_config_t* 			config,
					ngscope_cell_dci_ring_buffer_t 		cell_status[MAX_NOF_RF_DEV],
					CA_status_t*   						ca_status)
{
	if(ca_status->all_cell_ready){
		for(int i=0; i<config->nof_cell; i++){
			if(go_exit) break;
			if(cell_status[i].cell_ready){
				if(config->cell_ready[i] == false){
					// The first time we log the dci messages
					config->curr_header[i] = cell_status[i].cell_header;
					config->cell_ready[i] = true;
					dci_ring_buffer_clear_cell_fill_flag(q, i);
				}else{
					//int cur_head = curr_header[i];
					//int cell_header = cell_status[i].cell_header;
					log_per_cell(&cell_status[i], config, i);
					if(config->curr_header[i] !=  cell_status[i].cell_header){
						config->curr_header[i] =  cell_status[i].cell_header;
					}
					//fprintf(fd, "%d\t%d\t%d\t%d\t\n", cur_head, cell_header, curr_header[i], 
								//cell_status[i].cell_header);
				}
			}		
		}
	}
	return;
}
void fill_file_descriptor(FILE* fd_dl[MAX_NOF_RF_DEV],
                          FILE* fd_ul[MAX_NOF_RF_DEV],
						  ngscope_config_t* config)
{

	int nof_rf_dev = config->nof_rf_dev;
    // create the folder
    system("mkdir ./dci_log");
    char fileName[128];
    for(int i=0; i<nof_rf_dev; i++){
        if(config->rf_config[i].log_dl){
            sprintf(fileName, "./dci_log/dci_raw_log_dl_freq_%lld.dciLog", config->rf_config[i].rf_freq);
            fd_dl[i] = fopen(fileName, "w+");
             if(fd_dl[i] == NULL){
                printf("ERROR: fail to open log file!\n");
                exit(0);
            }
        }

        if(config->rf_config[i].log_ul){
            sprintf(fileName, "./dci_log/dci_raw_log_ul_freq_%lld.dciLog", config->rf_config[i].rf_freq);
            fd_ul[i] = fopen(fileName, "w+");
             if(fd_ul[i]== NULL){
                printf("ERROR: fail to open log file!\n");
                exit(0);
            }
        }
    }
	return;
}

void fill_dci_log_config(ngscope_dci_log_config_t* q, ngscope_config_t* config){
	q->nof_cell 	= config->nof_rf_dev; 
	q->targetRNTI 	= config->rnti; 
	for(int i=0; i<q->nof_cell; i++){
		//q->cell_prb[i] 	= config.rf_config[i].
		q->log_dl[i] 	= config->rf_config[i].log_dl;
		q->log_ul[i] 	= config->rf_config[i].log_ul;

		q->curr_header[i] 	= 0;
		q->cell_ready[i] 	= false;
	}

	fill_file_descriptor(q->fd_dl,  q->fd_ul, config);

#ifdef LOG_DCI_LOGGER
	q->fd_log_cell = fopen("dci_log_cell.txt", "w+");
#endif

	return;
}

void clear_dci_log_config(ngscope_dci_log_config_t* q){

    for(int i=0; i<q->nof_cell; i++){
		if(q->log_dl[i]){
			fclose(q->fd_dl[i]);
		}

		if(q->log_ul[i]){
			fclose(q->fd_ul[i]);
		}
	}
#ifdef LOG_DCI_LOGGER
	fclose(q->fd_log_cell);
#endif
	return;
}
void* dci_log_thread(void* p){
	log_config_t* log_config  = (log_config_t*)p;

	ngscope_dci_log_config_t dci_log_config;

	int buf_size = DCI_LOGGER_RING_BUF_SIZE;

	ngscope_cell_dci_ring_buffer_t 		cell_status[MAX_NOF_RF_DEV];
	CA_status_t   						ca_status;

	ngscope_status_buffer_t dci_buf[MAX_DCI_BUFFER];

	// fill the corresponding field of the dci log
	fill_dci_log_config(&dci_log_config, &(log_config->config));

	/* We now init two status buffer CA status and cell status */
	// --> init the CA status
	CA_status_init(&ca_status, buf_size, dci_log_config.targetRNTI, dci_log_config.nof_cell, log_config->cell_prb);

	FILE* fd = fopen("dci_log.txt", "w+");

	printf("\n\n\n nof_cell:%d targetRNTI:%d \n\n\n", dci_log_config.nof_cell, dci_log_config.targetRNTI);

	// --> init the cell status
	for(int i=0; i<dci_log_config.nof_cell; i++){
		dci_ring_buffer_init(&cell_status[i], dci_log_config.targetRNTI, log_config->cell_prb[i], i, buf_size);
	}

	while(true){
        if(go_exit) break;

        pthread_mutex_lock(&log_stat_ready.mutex);
        // Use while in case some corner case conditional wake up
        while(log_stat_ready.nof_dci <=0){
            pthread_cond_wait(&log_stat_ready.cond, &log_stat_ready.mutex);
        }
		int nof_dci 		= log_stat_ready.nof_dci;
		//fprintf(fd, "%d\t%d\n", cell_stat_buffer[0].tti, nof_dci);
		memcpy(dci_buf, log_stat_buffer, nof_dci * sizeof(ngscope_status_buffer_t));

        // clean the dci buffer 
        memset(log_stat_buffer, 0, nof_dci * sizeof(ngscope_status_buffer_t));

     	// reset the dci buffer
        log_stat_ready.header    = 0;
        log_stat_ready.nof_dci   = 0;
        pthread_mutex_unlock(&log_stat_ready.mutex);

		//printf("Logger get %d dci messages! buf_size:%d %d\n", nof_dci, cell_status[0].buf_size, buf_size);
		for(int i=0; i<nof_dci; i++){
			int cell_idx = dci_buf[i].cell_idx;	
			//enqueue the dci to the according cell status buffer
			//printf("LOGGER put dci!cell_idx:%d header:%d tti:%d\n", cell_idx, cell_status[cell_idx].cell_header, dci_buf[i].tti);
			dci_ring_buffer_put_dci(&cell_status[cell_idx], &dci_buf[i], 0);
		}
		CA_status_update_header(&ca_status, cell_status);
		log_multi_cell(cell_status,  &dci_log_config, cell_status, &ca_status);
		fprintf(fd, "%d\t%d\t\n", cell_status[0].cell_header, dci_log_config.curr_header[0]);
	} 
	fclose(fd);

	clear_dci_log_config(&dci_log_config);

 	wait_for_ALL_RF_DEV_close();        

	for(int i=0; i<dci_log_config.nof_cell; i++){
		// delete the ring buffer
		dci_ring_buffer_delete(&(cell_status[i]));
	}
	
	printf("DCI-LOGGER IS CLOSED!\n");
	return NULL;
}

//void auto_dci_logging(ngscope_cell_dci_ring_buffer_t* q,
//						//prog_args_t* prog_args,
//						ngscope_config_t* config,
//						FILE* fd_dl, 
//						FILE* fd_ul,
//						int   last_header,
//						int   cell_idx,
//						FILE* fd)
//{
//	int start_idx = last_header;
//	int end_idx   = q->cell_header;
//
//	//FILE* fd = fopen("./auto_dci_log.txt","a+");
//
//	if(q->cell_ready){
//		// we are not logging single dci 
//		if(start_idx == end_idx){
//			return;
//		}else if(start_idx > end_idx){
//			end_idx += q->buf_size;
//		}
//		fprintf(fd,"%d\t%d\t%d\n", start_idx, end_idx, q->sub_stat[start_idx].tti);
//		//printf("start_idx:%d end_idx:%d\n", start_idx, end_idx);
//		for(int i=start_idx+1; i<=end_idx; i++){
//			if(go_exit) break;
//			// moving forward until we reach the current header
//			int idx = i % q->buf_size;
//			// log the dci
//			log_per_subframe(&q->sub_stat[idx], config, fd_dl, fd_ul, cell_idx);
//		}
//	}
//	//fclose(fd);
//    return;
//}


//void* dci_log_thread(void* p){
//  	//prog_args_t* prog_args = (prog_args_t*)p; 
//	ngscope_config_t* config = (ngscope_config_t*)p;
//    FILE* fd_dl[MAX_NOF_RF_DEV];
//    FILE* fd_ul[MAX_NOF_RF_DEV];
//    int nof_dev = config->nof_rf_dev;
//    // Logging reated  --> fill no matter we log or not
//    fill_file_descriptor(fd_dl, fd_ul, config);
//	//prog_args->log_dl, prog_args->log_ul, nof_dev, prog_args->rf_freq_vec);
//
//	int curr_header[MAX_NOF_RF_DEV];
//	bool cell_ready[MAX_NOF_RF_DEV] = {false};
//
//	FILE* fd = fopen("./auto_dci_log.txt","w+");
//
//	while(!go_exit){
//        pthread_mutex_lock(&cell_status_mutex);
//		for(int i=0; i<nof_dev; i++){
//			if(go_exit) break;
//			if(cell_status[i].cell_ready){
//				if(cell_ready[i] == false){
//					// The first time we log the dci messages
//					curr_header[i] = cell_status[i].cell_header;
//					cell_ready[i] = true;
//				}else{
//					//int cur_head = curr_header[i];
//					//int cell_header = cell_status[i].cell_header;
//					auto_dci_logging(&cell_status[i], config, fd_dl[i], fd_ul[i], curr_header[i], i, fd);
//					if(curr_header[i] !=  cell_status[i].cell_header){
//						curr_header[i] =  cell_status[i].cell_header;
//					}
//					//fprintf(fd, "%d\t%d\t%d\t%d\t\n", cur_head, cell_header, curr_header[i], 
//								//cell_status[i].cell_header);
//				}
//			}		
//		}
//        pthread_mutex_unlock(&cell_status_mutex);
//		// we may not want to always hold the lock
//		usleep(100);
//	}
//	fclose(fd);
//	printf("DCI LOGGING THREAD closed!\n");
//	return NULL;
//}
