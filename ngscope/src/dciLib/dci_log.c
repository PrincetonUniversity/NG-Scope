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
extern bool go_exit;
extern cell_status_t 	cell_status[MAX_NOF_RF_DEV];
extern CA_status_t   	ca_status;
extern pthread_mutex_t 	cell_status_mutex;

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
			fprintf(fd_dl, "\n");
		}
	}else{
		// We must fill the TTI even if there is no dci message inside this subframe
		fprintf(fd_dl, "%d\t", q->tti);
		for(int i=0; i<10;i++){
			fprintf(fd_dl, "%d\t", 0);
		}
		fprintf(fd_dl, "%ld\n", q->timestamp_us);
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
void log_per_subframe(sf_status_t* q,
					prog_args_t* prog_args,
					FILE* fd_dl, 
					FILE* fd_ul)
{
    bool log_dl    = prog_args->log_dl;
    bool log_ul    = prog_args->log_ul;

    //printf("TTI:%d idx:%d nof_dl_msg:%d nof_ul_msg:%d\n", q.tti[buf_idx], buf_idx, nof_dl_msg, nof_ul_msg);

    if(log_dl){
		log_dl_subframe(q, fd_dl);
    }

    if(log_ul){
		log_ul_subframe(q, fd_ul);
    }

    return;
}

int  find_ue_dl_dci(ngscope_cell_status_t* q, uint16_t rnti, int sf_idx){
    // RNTI cannot be zero
    if(rnti <= 0){
        return -1;
    }

    for(int i=0; i<MAX_DCI_PER_SUB; i++){
        if(q->dl_msg[i][sf_idx].rnti == rnti){
            return i;
        }
    } 
    return -1;
}


void auto_dci_logging(cell_status_t* q,
						prog_args_t* prog_args,
						FILE* fd_dl, 
						FILE* fd_ul,
						int   last_header)
{
	int start_idx = last_header;
	int end_idx   = q->cell_header;

	if(q->cell_ready){
		// we are not logging single dci 
		if(start_idx == end_idx){
			return;
		}else if(start_idx > end_idx){
			end_idx += NOF_LOG_SUBF;
		}
		//printf("start_idx:%d end_idx:%d\n", start_idx, end_idx);
		for(int i=start_idx+1; i<=end_idx; i++){
			// moving forward until we reach the current header
			int idx = TTI_TO_IDX(i);
			// log the dci
			log_per_subframe(&q->sub_stat[idx], prog_args, fd_dl, fd_ul);
		}
	}
    return;
}

void fill_file_descriptor(FILE* fd_dl[MAX_NOF_RF_DEV],
                            FILE* fd_ul[MAX_NOF_RF_DEV],
                            bool log_dl, bool log_ul, int nof_rf_dev,
                            long long* rf_freq)
{
    // create the folder
    system("mkdir ./dci_log");

    char fileName[128];
    for(int i=0; i<nof_rf_dev; i++){
        if(log_dl){
            sprintf(fileName, "./dci_log/dci_raw_log_dl_freq_%lld.dciLog", rf_freq[i]);
            fd_dl[i] = fopen(fileName, "w+");
             if(fd_dl[i] == NULL){
                printf("ERROR: fail to open log file!\n");
                exit(0);
            }
        }

        if(log_ul){
            sprintf(fileName, "./dci_log/dci_raw_log_ul_freq_%lld.dciLog", rf_freq[i]);
            fd_ul[i] = fopen(fileName, "w+");
             if(fd_ul[i]== NULL){
                printf("ERROR: fail to open log file!\n");
                exit(0);
            }
        }
    }
}

void* dci_log_thread(void* p){
  	prog_args_t* prog_args = (prog_args_t*)p; 
    FILE* fd_dl[MAX_NOF_RF_DEV];
    FILE* fd_ul[MAX_NOF_RF_DEV];
    int nof_dev = prog_args->nof_rf_dev;
    // Logging reated  --> fill no matter we log or not
    fill_file_descriptor(fd_dl, fd_ul, prog_args->log_dl, prog_args->log_ul, nof_dev, prog_args->rf_freq_vec);

	int curr_header[MAX_NOF_RF_DEV];
	bool cell_ready[MAX_NOF_RF_DEV] = {false};
	while(!go_exit){
        pthread_mutex_lock(&cell_status_mutex);
		for(int i=0; i<nof_dev; i++){
			if(cell_status[i].cell_ready){
				if(cell_ready[i] == false){
					// The first time we log the dci messages
					curr_header[i] = cell_status[i].cell_header;
					cell_ready[i] = true;
				}else{
					auto_dci_logging(&cell_status[i], prog_args, fd_dl[i], fd_ul[i], curr_header[i]);
					if(curr_header[i] !=  cell_status[i].cell_header){
						curr_header[i] =  cell_status[i].cell_header;
					}
				}
			}		
		}
        pthread_mutex_unlock(&cell_status_mutex);
		// we may not want to always hold the lock
		usleep(100);
	}
	return NULL;
}
