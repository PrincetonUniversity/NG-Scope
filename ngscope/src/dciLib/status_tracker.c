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
#define TTI_TO_IDX(i) (i%NOF_LOG_SUBF)

extern bool go_exit;
extern pthread_mutex_t     cell_mutex;
extern srsran_cell_t       cell_vec[MAX_NOF_RF_DEV];

extern dci_ready_t         dci_ready;
extern ngscope_status_buffer_t    dci_buffer[MAX_DCI_BUFFER];

void wait_for_radio(ngscope_status_tracker_t* q, int nof_dev){
    bool radio_ready = true;
    while(true){
        if(go_exit) break;

        radio_ready = true;
        pthread_mutex_lock(&cell_mutex);
        for(int i=0; i<nof_dev; i++){
            if(cell_vec[i].nof_prb == 0){
                radio_ready = false;
                break;
            }
        }
        pthread_mutex_unlock(&cell_mutex);
        if(radio_ready){
            break;
        }else{
            usleep(10000);
        }
    }     
    pthread_mutex_lock(&cell_mutex);
    for(int i=0; i<nof_dev; i++){
        q->ngscope_CA_status.cell_prb[i] = cell_vec[i].nof_prb;
    }
    pthread_mutex_unlock(&cell_mutex);

    return;
}

// --> the most recent subframe between the current header,
//     and the most recent touched subframe, that has not been
//     touched!
uint16_t most_recent_untouched_sub(ngscope_cell_status_t* q){
    uint16_t start  = q->header;
    uint16_t end    = q->dci_touched;
    uint16_t untouched_idx;

    // init the untouched index
    untouched_idx = start;

    // No change return
    if( end == start)
        return start;

    // unwrapping
    if( end < start){ end += NOF_LOG_SUBF;}

    for(int i=start+1;i<=end;i++){
        uint16_t index = TTI_TO_IDX(i);
        untouched_idx = i;
        if(q->token[index] == true){
            // return when we encounter a token that has been taken
            untouched_idx -= 1;
            return  TTI_TO_IDX(untouched_idx);
        }
    }
    return  TTI_TO_IDX(untouched_idx);
}
int update_cell_header(ngscope_cell_status_t* q){
    uint16_t new_header     = most_recent_untouched_sub(q);
    uint16_t last_header    = q->header; 
    if( new_header != last_header){
        q->header = new_header; 
        if(new_header < last_header){ new_header += NOF_LOG_SUBF;}
        //printf("now Update header: ->>>>> last header:%d new header:%d\n", last_header, new_header);
        for(int i = last_header; i<= new_header; i++){
            uint16_t index = TTI_TO_IDX(i);
            q->token[index] = 0;
        }
    } 
    return 0;
}

int reset_dci_msg_buffer(ngscope_cell_status_t* q, int buf_idx){
    for(int i=0; i<MAX_DCI_PER_SUB; i++){
        memset(&(q->dl_msg[i][buf_idx]), 0, sizeof(ngscope_dci_msg_t));
        memset(&(q->ul_msg[i][buf_idx]), 0, sizeof(ngscope_dci_msg_t));
    }
    return 0;
}

int reset_message(ngscope_cell_status_t* q, int buf_idx){
    q->tti[buf_idx] = 0;

    q->cell_dl_prb[buf_idx] = 0;
    q->cell_ul_prb[buf_idx] = 0;

    q->ue_dl_prb[buf_idx]   = 0;
    q->ue_ul_prb[buf_idx]   = 0;

    q->nof_dl_msg[buf_idx]  = 0;
    q->nof_ul_msg[buf_idx]  = 0;

    reset_dci_msg_buffer(q, buf_idx);
    
    return 0;
}

int status_tracker_update_cell_status(ngscope_CA_status_t* q, 
                                        ngscope_status_buffer_t* dci_buffer){
    int cell_idx = dci_buffer->cell_idx;
    uint16_t   targetRNTI = q->targetRNTI;
    uint32_t   tti = dci_buffer->tti;
    int nof_dl_dci  = dci_buffer->dci_per_sub.nof_dl_dci;
    int nof_ul_dci  = dci_buffer->dci_per_sub.nof_ul_dci;
    ngscope_dci_msg_t* dl_msg = dci_buffer->dci_per_sub.dl_msg;
    ngscope_dci_msg_t* ul_msg = dci_buffer->dci_per_sub.ul_msg;
    
    int buf_idx = TTI_TO_IDX(tti);    
    ngscope_cell_status_t* cell_status;
    cell_status = &(q->cell_status[cell_idx]);

    // reset all the dci messages to zeros
    reset_message(cell_status, buf_idx);

    // fill in the tti
    cell_status->tti[buf_idx] = tti;
    cell_status->dci_touched = buf_idx;

    // show that we have fill this buffer
    cell_status->token[buf_idx] = 1;

    // The message account
    cell_status->nof_dl_msg[buf_idx] = nof_dl_dci;
    cell_status->nof_ul_msg[buf_idx] = nof_ul_dci;

    // handle the uplink messages
    if(nof_dl_dci > 0){
        int cell_prb =0;
        int ue_prb =0;
        for(int i=0; i<nof_dl_dci; i++){
            cell_status->dl_msg[i][buf_idx] = dl_msg[i]; 
            cell_prb += dl_msg[i].prb; 
            if(dl_msg[i].rnti == targetRNTI){
                ue_prb = dl_msg[i].prb;
            }
        }
        cell_status->cell_dl_prb[buf_idx]    = cell_prb;
        cell_status->ue_dl_prb[buf_idx]      = ue_prb;
    }

    // handle the uplink messages
    if(nof_ul_dci > 0){
        int cell_prb =0;
        int ue_prb =0;
        for(int i=0; i<nof_ul_dci; i++){
            cell_status->ul_msg[i][buf_idx] = ul_msg[i]; 
            cell_prb += ul_msg[i].prb; 
            if(ul_msg[i].rnti == targetRNTI){
                ue_prb = ul_msg[i].prb;
            }
        }
        cell_status->cell_ul_prb[buf_idx]    = cell_prb;
        cell_status->ue_ul_prb[buf_idx]      = ue_prb;
    }
    // update the cell status header
    update_cell_header(cell_status);

    return 0;
}
void enqueue_rnti(ngscope_ue_list_t* q, uint32_t tti, uint16_t rnti, bool dl){
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


void update_max_freq_ue(ngscope_ue_list_t* q){
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

int status_tracker_update_ue_list(ngscope_ue_list_t* q, 
                                        ngscope_status_buffer_t* dci_buffer){
    uint32_t   tti = dci_buffer->tti;
    int nof_dl_dci  = dci_buffer->dci_per_sub.nof_dl_dci;
    int nof_ul_dci  = dci_buffer->dci_per_sub.nof_ul_dci;
    ngscope_dci_msg_t* dl_msg = dci_buffer->dci_per_sub.dl_msg;
    ngscope_dci_msg_t* ul_msg = dci_buffer->dci_per_sub.ul_msg;

    //downlink message
    if(nof_dl_dci > 0){
        for(int i=0; i<nof_dl_dci; i++){
            uint16_t rnti = dl_msg[i].rnti;
            enqueue_rnti(q, tti, rnti, true);  
        }
    }

    // uplink message
    if(nof_ul_dci > 0){
        for(int i=0; i<nof_ul_dci; i++){
            uint16_t rnti = ul_msg[i].rnti;
            enqueue_rnti(q, tti, rnti, false);  
        }
    }
    update_max_freq_ue(q);
    return 0;
}
int status_tracker_print_ue_freq(ngscope_status_tracker_t* q){
    ngscope_ue_list_t* ue_list = &(q->ue_list);
    printf("High Freq UE: DL rnti:%d freq:%d | UL rnti:%d freq:%d | Total rnti:%d freq: %d\n",
        ue_list->max_dl_freq_ue, ue_list->ue_dl_cnt[ue_list->max_dl_freq_ue], 
        ue_list->max_ul_freq_ue, ue_list->ue_ul_cnt[ue_list->max_ul_freq_ue], 
        ue_list->max_freq_ue, ue_list->ue_cnt[ue_list->max_freq_ue]);
    return 0;
}


int status_tracker_handle_dci_buffer(ngscope_status_tracker_t* q, 
                                        ngscope_status_buffer_t* dci_buffer){

    /* Update the cell status */
    ngscope_CA_status_t* cell_status = &(q->ngscope_CA_status);
    status_tracker_update_cell_status(cell_status, dci_buffer);

    /* Update ue list ***/
    ngscope_ue_list_t* ue_list = &(q->ue_list);
    status_tracker_update_ue_list(ue_list, dci_buffer);
 
    return 0;
}

void* status_tracker_thread(void* p){
    // Number of RF devices
    int nof_dev = *(int *)p;
    int nof_dci = 0;

    printf("\n\nstart status tracker nof_dev :%d\n\n", nof_dev);
    /* Init the status tracker */
    ngscope_status_tracker_t status_tracker;
    memset(&status_tracker, 0, sizeof(ngscope_status_tracker_t));
   
    // Container for store obtained csi  
    ngscope_status_buffer_t    dci_queue[MAX_DCI_BUFFER];

    /* Wait the Radio to be ready */
    wait_for_radio(&status_tracker, nof_dev);
   
    printf("\n\n\n Radio is ready! \n\n"); 

    while(true){
        if(go_exit) break;
        // reset the dci queue 
        memset(dci_queue, 0, MAX_DCI_BUFFER * sizeof(ngscope_status_buffer_t));

        pthread_mutex_lock(&dci_ready.mutex);
        // Use while in case some cornal case conditional wake up
        while(dci_ready.nof_dci <=0){
            pthread_cond_wait(&dci_ready.cond, &dci_ready.mutex);
        }
        // copy data from the dci buffer
        memcpy(dci_queue, dci_buffer, MAX_DCI_BUFFER * sizeof(ngscope_status_buffer_t));
        nof_dci     = dci_ready.nof_dci;
        // reset the dci buffer
        dci_ready.header    = 0;
        dci_ready.nof_dci   = 0;
        pthread_mutex_unlock(&dci_ready.mutex);
        printf("Copy %d dci tti:%d ul dci:%d dl_dci:%d\n\n", nof_dci, dci_queue[0].tti, 
            dci_queue[0].dci_per_sub.nof_ul_dci, dci_queue[0].dci_per_sub.nof_dl_dci);
        for(int i=0; i<nof_dci; i++){
            status_tracker_handle_dci_buffer(&status_tracker, &dci_queue[i]);
            //int header = status_tracker.ngscope_CA_status.cell_status[0].header;
            //int dl_prb = status_tracker.ngscope_CA_status.cell_status[0].cell_dl_prb[header];
            //int ul_prb = status_tracker.ngscope_CA_status.cell_status[0].cell_ul_prb[header];
            //printf("CELL header:%d prb:%d %d \n", header, dl_prb, ul_prb); 
        }
    }
    
    status_tracker_print_ue_freq(&status_tracker);
    return NULL;
}
