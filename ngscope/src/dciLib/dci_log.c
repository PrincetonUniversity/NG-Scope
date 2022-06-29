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
#include "ngscope/hdr/dciLib/socket.h"

/* Logging related function */
void log_per_dci(ngscope_CA_status_t* status,
                    FILE* fd_dl[MAX_NOF_RF_DEV], FILE* fd_ul[MAX_NOF_RF_DEV],
                    int cell_idx,
                    int buf_idx)
{
    ngscope_cell_status_t q= status->cell_status[cell_idx];
    int nof_dl_msg = q.nof_dl_msg[buf_idx];
    int nof_ul_msg = q.nof_ul_msg[buf_idx];
    //printf("TTI:%d idx:%d nof_dl_msg:%d nof_ul_msg:%d\n", q.tti[buf_idx], buf_idx, nof_dl_msg, nof_ul_msg);

    /* Logging downlink messages */
    if(nof_dl_msg > 0){
        for(int i=0; i<nof_dl_msg; i++){
            // TTI RNTI
            fprintf(fd_dl[cell_idx], "%d\t%d\t", q.tti[buf_idx], q.dl_msg[i][buf_idx].rnti);
            //printf("DL rnti:%d \n",  q.dl_msg[i][buf_idx].rnti);

            // CELL_PRB UE_PRB
            fprintf(fd_dl[cell_idx], "%d\t%d\t", q.cell_dl_prb[buf_idx], q.dl_msg[i][buf_idx].prb);

            // TB1 related information
            fprintf(fd_dl[cell_idx], "%d\t%d\t%d\t", q.dl_msg[i][buf_idx].tb[0].mcs, q.dl_msg[i][buf_idx].tb[0].rv,
                                q.dl_msg[i][buf_idx].tb[0].tbs);
            // TB2 related information
            if(q.dl_msg[i][buf_idx].nof_tb > 1){
                fprintf(fd_dl[cell_idx], "%d\t%d\t%d\t", q.dl_msg[i][buf_idx].tb[1].mcs, q.dl_msg[i][buf_idx].tb[1].rv,
                                q.dl_msg[i][buf_idx].tb[1].tbs);
            }else{
                fprintf(fd_dl[cell_idx], "%d\t%d\t%d\t", 0, 0, 0);
            }

            fprintf(fd_dl[cell_idx], "%d\t%ld\t",  q.dl_msg[i][buf_idx].harq, q.timestamp_us[buf_idx]);
            fprintf(fd_dl[cell_idx], "\n");
        }
    }else{
        // We must fill the TTI even if there is no dci message inside this subframe
        fprintf(fd_dl[cell_idx], "%d\t", q.tti[buf_idx]);
        for(int i=0; i<10;i++){
            fprintf(fd_dl[cell_idx], "%d\t", 0);
        }
        fprintf(fd_dl[cell_idx], "%ld\n", q.timestamp_us[buf_idx]);
    }
    /* Logging uplink messages */
    if(nof_ul_msg > 0){
        for(int i=0; i<nof_ul_msg; i++){
            // TTI RNTI
            fprintf(fd_ul[cell_idx], "%d\t%d\t", q.tti[buf_idx], q.ul_msg[i][buf_idx].rnti);
            // CELL_PRB UE_PRB
            fprintf(fd_ul[cell_idx], "%d\t%d\t", q.cell_ul_prb[buf_idx], q.ul_msg[i][buf_idx].prb);
            // TB1 related information
            fprintf(fd_ul[cell_idx], "%d\t%d\t%d\t", q.ul_msg[i][buf_idx].tb[0].mcs, q.ul_msg[i][buf_idx].tb[0].rv,
                                q.ul_msg[i][buf_idx].tb[0].tbs);

            // TB2 related information --> UPlink has no second TB yet
            if(q.ul_msg[i][buf_idx].nof_tb > 1){
                fprintf(fd_ul[cell_idx], "%d\t%d\t%d\t", q.ul_msg[i][buf_idx].tb[1].mcs, q.ul_msg[i][buf_idx].tb[1].rv,
                                q.ul_msg[i][buf_idx].tb[1].tbs);
            }else{
                fprintf(fd_ul[cell_idx], "%d\t%d\t%d\t", 0, 0, 0);
            }
            fprintf(fd_ul[cell_idx], "%d\t%ld\t",  0, q.timestamp_us[buf_idx]);
            fprintf(fd_ul[cell_idx], "\n");
        }
    }else{
        // We must fill the TTI even if there is no dci message inside this subframe
        fprintf(fd_ul[cell_idx], "%d\t", q.tti[buf_idx]);
        for(int i=0; i<10;i++){
            fprintf(fd_ul[cell_idx], "%d\t", 0);
        }
        fprintf(fd_ul[cell_idx], "%ld\n", q.timestamp_us[buf_idx]);
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
void auto_dci_logging(ngscope_CA_status_t* q,
                        FILE* fd_dl[MAX_NOF_RF_DEV], FILE* fd_ul[MAX_NOF_RF_DEV],
                        bool cell_ready[MAX_NOF_RF_DEV],
                        int* curr_header, int* last_header,
                        int  nof_dev, int remote_sock, int remote_enable)
{
    uint16_t targetRNTI = q->targetRNTI;

    for(int cell_idx=0; cell_idx<nof_dev; cell_idx++){
        int start_idx = last_header[cell_idx];
        int end_idx   = curr_header[cell_idx];
    
        if(q->cell_status[cell_idx].ready){
            if(cell_ready[cell_idx]){
                if(start_idx == end_idx){
                    continue;
                }else if(start_idx > end_idx){
                    end_idx += NOF_LOG_SUBF;
                }
                //printf("start_idx:%d end_idx:%d\n", start_idx, end_idx);
                for(int i=start_idx+1; i<=end_idx; i++){
                    // moving forward until we reach the current header
                    int idx = TTI_TO_IDX(i);
                    //printf("Logging -->buf_idx:%d tti:%d ", idx, q->cell_status[cell_idx].tti[idx]);
                    // log the dci
                    log_per_dci(q, fd_dl, fd_ul, cell_idx, idx);

                    ngscope_dci_msg_t dci;
                    memset(&dci, 0, sizeof(ngscope_dci_msg_t));

                    printf("rnti:%d\n", targetRNTI);

                    //printf("tbs:%d %d\n", dci.tb[0].tbs, dci.tb[1].tbs);
                    uint64_t time_stamp = q->cell_status[cell_idx].timestamp_us[idx];
                    uint16_t tti        = q->cell_status[cell_idx].tti[idx];
                    int ue_dci_idx = find_ue_dl_dci(&q->cell_status[cell_idx], targetRNTI, idx);
                    if( ue_dci_idx >=0){
                        memcpy(&dci, &q->cell_status[cell_idx].dl_msg[ue_dci_idx][idx], sizeof(ngscope_dci_msg_t));
                        printf("FIND DCI%d tti:%d tbs:%d %d nof_tb:%d\n", ue_dci_idx, tti, dci.tb[0].tbs, dci.tb[1].tbs, dci.nof_tb);
                    }
                    if( (remote_sock > 0) && remote_enable ){
                        ngscope_update_dci(remote_sock, time_stamp, tti, dci);
                    }
                }
                //printf("\n");
            }else{
                /* The first time we log the cell */
                int idx = TTI_TO_IDX(end_idx);
                log_per_dci(q, fd_dl, fd_ul, cell_idx, idx);
                cell_ready[cell_idx] = true;
            }
        }
    }
    return;
}

void fill_file_descirptor(FILE* fd_dl[MAX_NOF_RF_DEV],
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


