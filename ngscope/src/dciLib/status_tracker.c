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

//#define TTI_TO_IDX(i) (i%NOF_LOG_SUBF)

extern bool go_exit;
extern pthread_mutex_t     cell_mutex;
extern srsran_cell_t       cell_vec[MAX_NOF_RF_DEV];

extern dci_ready_t                  dci_ready;
extern ngscope_status_buffer_t      dci_buffer[MAX_DCI_BUFFER];

pthread_cond_t      plot_cond  = PTHREAD_COND_INITIALIZER;
pthread_mutex_t     plot_mutex = PTHREAD_MUTEX_INITIALIZER;
ngscope_plot_t      plot_data;

/* Operator */

bool a_le_than_b(int a, int b){
    if( (a-b) >= 0){
       return true;
    }else{
        /* b --> 320 --> a 
           b 310   a 1 (a is larger then b in this case) */
        if( (abs(NOF_LOG_SUBF - b) < NOF_LOG_SUBF/8) && 
            (a < NOF_LOG_SUBF/8)){
            return true;
        }
    } 
    return false;
}
void init_plot_data(ngscope_CA_status_t* q, int nof_dev){
    pthread_mutex_lock(&plot_mutex);
    plot_data.nof_cell = nof_dev;

    //pthread_mutex_lock(&cell_mutex);
    for(int i=0; i<nof_dev; i++){
        plot_data.cell_prb[i] = q->cell_prb[i];
    }
    //pthread_mutex_unlock(&cell_mutex);

    pthread_mutex_unlock(&plot_mutex);
}

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

        
    //printf("most untouched sub end:%d start:%d\n",end, start);
    // No change return
    if( end == start){
        printf("most untouched sub end:%d start:%d\n",end, start);
        return start;
    }
    // unwrapping
    if( end < start){ end += NOF_LOG_SUBF;}

    for(int i=start+1;i<=end;i++){
        uint16_t index = TTI_TO_IDX(i);
        untouched_idx = i;
        //printf("idx:%d token:%d\n", index, q->token[index]);
        //if(q->token[index] == true){
        if(q->token[index] == 0){
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
    //printf("update_cell_header: last header:%d new header:%d touched:%d", 
    //        last_header, new_header, q->dci_touched);

    /* If we are the first time logging it */
    if(q->ready == false){
        printf("First time Logging\n");
        q->ready = true;
        q->header = q->dci_touched;
        return 0;
    }
    if( new_header != last_header){
        q->header = new_header; 
        if(new_header < last_header){ new_header += NOF_LOG_SUBF;}
        //printf("now Update header: ->>>>> last header:%d new header:%d\n", last_header, new_header);
        for(int i = last_header; i<= new_header; i++){
            uint16_t index = TTI_TO_IDX(i);
            q->token[index] = 0;
        }
        //for(int i = last_header; i<= new_header+4; i++){
        //    uint16_t index = TTI_TO_IDX(i);
        //    printf("idx:%d token:%d \n", index, q->token[index]);
        //}
        //for(int i = 0; i< 320; i++){
        //    printf("%d | ", q->token[i]);
        //}
        //printf("\n");
    } 
    //printf("\n");
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

/* Update the cell status */
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

    // Set the logging timestamp
    cell_status->timestamp_us[buf_idx] = timestamp_us();

    // fill in the tti
    cell_status->tti[buf_idx] = tti;

    if(a_le_than_b(buf_idx, cell_status->dci_touched)){
        cell_status->dci_touched = buf_idx;
    }

    // show that we have fill this buffer
    cell_status->token[buf_idx] = 1;

    // The message account
    cell_status->nof_dl_msg[buf_idx] = nof_dl_dci;
    cell_status->nof_ul_msg[buf_idx] = nof_ul_dci;

    //printf("nof_dl_msg:%d ul_msg:%d buf_idx:%d\n",cell_status->nof_dl_msg[buf_idx], cell_status->nof_ul_msg[buf_idx], buf_idx);
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

// --> find the max tti in the array
static int min_tti_list(int* array, int num){
    int value = MAX_TTI;
    for(int i=0;i<num;i++){
        if(array[i] < value){
            value = array[i];
        }
    }
    return value;
}

// --> find the min tti in the array
static int max_tti_list(int* array, int num){
    int value = 0;
    for(int i=0;i<num;i++){
        if(array[i] > value){
            value = array[i];
        }
    }
    return value;
}
// --> unwrapping tti
static int unwrapping_tti(int* tti_array, int num){
    int max_tti = max_tti_list(tti_array, num);
    for(int i=0;i<num;i++){
        if(tti_array[i] == max_tti){continue;}
        if(max_tti > tti_array[i] + MAX_TTI/2){
            tti_array[i] += MAX_TTI;
        }
    }
    return 0;
}

/* All the cells are synchronized if the header TTI are the same and are not zero*/
static bool check_synchronization(ngscope_CA_status_t* q){
    uint16_t nof_cell = q->nof_cell;

    /* the first array is always not empty, we use it as the anchor
     * if the tti of other cells (the same sf) are different from the anchor
     * we know the cells are not synchronized */
    uint32_t anchor_TTI = q->cell_status[0].tti[q->header];
    uint32_t target_TTI;

    if(anchor_TTI == 0){
    return false;   // TTI cannot be zero
    }

    if(nof_cell == 1){
        if(anchor_TTI <= 0){
            return false;   // If there is only one cell and the anchor_TTI is zero
        }
    }else if(nof_cell > 1){
        for(int i=1;i<nof_cell;i++){
            target_TTI = q->cell_status[i].tti[q->header];
            if(target_TTI != anchor_TTI){
                return false;
            }
        }
    }else{
        printf("\n\n\n ERROR: nof_cell must be a postive integer! \n\n\n");
        return false;
    }
    return true;
}

/* Update the header of the whole status structure */
void update_status_header(ngscope_CA_status_t* q){
    int nof_cell = q->nof_cell;
    int tti_array[nof_cell];
    
    bool all_cell_ready = true;  
     
    /* find the minimum TTI we have in all the cells*/
    for(int i=0; i<nof_cell; i++){
        tti_array[i] = q->cell_status[i].tti[q->cell_status[i].header];
        if(q->cell_status[i].ready == false){
            all_cell_ready = false;
        } 
    }
    if(all_cell_ready == false){
        //printf("cell not ready! nof_cell:%d\n", nof_cell);
        return;
    }
    unwrapping_tti(tti_array, nof_cell); // unwrapping tti index
    uint16_t targetTTI      = min_tti_list(tti_array, nof_cell); // find the smallest tti
    uint16_t targetIndex    = TTI_TO_IDX(targetTTI);         // translate the TTI

    /* set the header to the minimum */
    // If the index equals the header, we should not move the header of the structure
    if( targetIndex == q->header){
        return;
    }

    // Warning user if we move too fast  
    if( targetIndex < q->header){
        targetIndex += NOF_LOG_SUBF;
    }
    if( targetIndex - q->header > 100){
        printf("WARNNING: the header is moved for interval of %d subframes!\n", (targetIndex - q->header));
    }

    // update the header
    q->header = TTI_TO_IDX(targetIndex);

    if(q->all_cell_synced == false){
        q->all_cell_synced = check_synchronization(q);
    }
    return;
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

int sum_per_sf_prb_dl(ngscope_dci_per_sub_t* q){
    int nof_dl_prb = 0;
    if(q->nof_dl_dci > 0){
        for(int i=0; i< q->nof_dl_dci; i++){
            nof_dl_prb += q->dl_msg[i].prb; 
        }
    }
    return nof_dl_prb;
}

int sum_per_sf_prb_ul(ngscope_dci_per_sub_t* q){
    int nof_ul_prb = 0;
    if(q->nof_ul_dci > 0){
        for(int i=0; i< q->nof_ul_dci; i++){
            nof_ul_prb += q->ul_msg[i].prb; 
        }
    }
    return nof_ul_prb;
}

int status_tracker_handle_plot(ngscope_status_buffer_t* dci_buffer){
    int      idx, max_prb;
    uint32_t tti = dci_buffer->tti;
    int cell_idx = dci_buffer->cell_idx;

    idx = tti % PLOT_SF;
    pthread_mutex_lock(&plot_mutex);
    max_prb = plot_data.cell_prb[cell_idx];
        
    /* Enqueue CSI */
    for(int i=0; i< max_prb * 12; i++){
        plot_data.plot_data_cell[cell_idx].plot_data_sf[idx].csi_amp[i] = 
            dci_buffer->csi_amp[i];
    }

    /* Enqueue TTI */
    plot_data.plot_data_cell[cell_idx].plot_data_sf[idx].tti = tti;

    /* Enqueue Cell downlink PRB */
    plot_data.plot_data_cell[cell_idx].plot_data_sf[idx].cell_dl_prb = 
        sum_per_sf_prb_dl(&dci_buffer->dci_per_sub);

    /* Enqueue Cell uplink PRB */
    plot_data.plot_data_cell[cell_idx].plot_data_sf[idx].cell_ul_prb = 
        sum_per_sf_prb_ul(&dci_buffer->dci_per_sub);

    /* touch the buffer and set the token */ 
    plot_data.plot_data_cell[cell_idx].dci_touched = idx;
    plot_data.plot_data_cell[cell_idx].token[idx]  = 1;

    pthread_cond_signal(&plot_cond);
    pthread_mutex_unlock(&plot_mutex);

    return 0;
}


/* Handle each task */
int status_tracker_handle_dci_buffer(ngscope_status_tracker_t* q, 
                                        ngscope_status_buffer_t* dci_buffer){
    /* Update the cell status */
    ngscope_CA_status_t* cell_status = &(q->ngscope_CA_status);
    status_tracker_update_cell_status(cell_status, dci_buffer);

    /* Update ue list ***/
    ngscope_ue_list_t* ue_list = &(q->ue_list);
    status_tracker_update_ue_list(ue_list, dci_buffer);

    /* Handle the plotting */ 
    status_tracker_handle_plot(dci_buffer);
    return 0;
}


void* status_tracker_thread(void* p){
    /* TODO log target status */
    prog_args_t* prog_args = (prog_args_t*)p; 

    // Number of RF devices
    int nof_dev = prog_args->nof_rf_dev;
    int nof_dci = 0;
    int nof_prb = 0;
    int dis_plot = prog_args->disable_plots;
    uint16_t targetRNTI = prog_args->rnti;
    int remote_enable   =  prog_args->remote_enable;

    FILE* fd_dl[MAX_NOF_RF_DEV];
    FILE* fd_ul[MAX_NOF_RF_DEV];

    int last_header[MAX_NOF_RF_DEV] = {0};
    int curr_header[MAX_NOF_RF_DEV] = {0};
    bool cell_ready[MAX_NOF_RF_DEV] = {0};

    printf("DIS_PLOT:%d nof_RF_DEV:%d \n", dis_plot, nof_dev);

    /* Init the status tracker */
    ngscope_status_tracker_t status_tracker;
    memset(&status_tracker, 0, sizeof(ngscope_status_tracker_t));

    /* INIT status_tracker */
    status_tracker.ngscope_CA_status.targetRNTI = targetRNTI;
    status_tracker.ngscope_CA_status.nof_cell   = nof_dev;

    printf("\n\nstart status tracker nof_dev :%d %d \n\n", nof_dev, status_tracker.ngscope_CA_status.cell_status[0].ready);
    // Logging reated 
    fill_file_descirptor(fd_dl, fd_ul, prog_args->log_dl, prog_args->log_ul, nof_dev, prog_args->rf_freq_vec);
    
    // Container for store obtained csi  
    ngscope_status_buffer_t    dci_queue[MAX_DCI_BUFFER];

    /* Wait the Radio to be ready */
    wait_for_radio(&status_tracker, nof_dev);
    
    /*start plot thread assuming only 1 cell*/ 
    pthread_t plot_thread;
    if(dis_plot == 0){
        nof_prb = status_tracker.ngscope_CA_status.cell_prb[0];
        // Init the plot data structure
        init_plot_data(&status_tracker.ngscope_CA_status, nof_dev);
        plot_init_thread(&plot_thread);
    }else{
        printf("displot:%d\n", dis_plot);
    }
    printf("\n\n\n Radio is ready! \n\n"); 

    if(remote_enable){    
        status_tracker.remote_sock = connectServer();
        if( status_tracker.remote_sock > 0){
            printf("\n\n\n Remote Socket Connected :%d \n\n", status_tracker.remote_sock); 
        }else{
            printf("\n\n\n Remote Socket Connection faliled!  \n\n"); 
        }
    }
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
        nof_dci     = dci_ready.nof_dci;
        memcpy(dci_queue, dci_buffer, nof_dci * sizeof(ngscope_status_buffer_t));

        // clean the dci buffer 
        memset(dci_buffer, 0, nof_dci * sizeof(ngscope_status_buffer_t));

        // reset the dci buffer
        dci_ready.header    = 0;
        dci_ready.nof_dci   = 0;
        pthread_mutex_unlock(&dci_ready.mutex);
        
        //printf("Copy %d dci->", nof_dci); 
        //for(int i=0; i<nof_dci; i++){
        //    printf(" %d-th dci: ul dci:%d dl_dci:%d  tti:%d IDX:%d\n", i, dci_queue[i].dci_per_sub.nof_ul_dci, 
        //    dci_queue[i].dci_per_sub.nof_dl_dci, dci_queue[i].tti, TTI_TO_IDX(dci_queue[i].tti));
        //}printf("\n");

        for(int i=0; i<nof_dci; i++){
            status_tracker_handle_dci_buffer(&status_tracker, &dci_queue[i]);
            //printf("status header:%d cell header:%d\n", status_tracker.ngscope_CA_status.header, 
            //        status_tracker.ngscope_CA_status.cell_status[0].header);
            //int header = status_tracker.ngscope_CA_status.cell_status[0].header;
            //int dl_prb = status_tracker.ngscope_CA_status.cell_status[0].cell_dl_prb[header];
            //int ul_prb = status_tracker.ngscope_CA_status.cell_status[0].cell_ul_prb[header];
            //printf("CELL header:%d prb:%d %d \n", header, dl_prb, ul_prb); 
        }
            
        /*   Logging the DCI */
        // Update the current header
        for(int i=0;i<nof_dev;i++){
            curr_header[i] = status_tracker.ngscope_CA_status.cell_status[i].header;
        }
        // log dci and if remote socket connect, send the data
        auto_dci_logging(&status_tracker.ngscope_CA_status, fd_dl, fd_ul, \
                    cell_ready, curr_header, last_header, nof_dev, status_tracker.remote_sock, remote_enable); 
        for(int i=0;i<nof_dev;i++){
            last_header[i] = curr_header[i];
        }
        //printf("end status\n"); 
        update_status_header(&status_tracker.ngscope_CA_status);
        //printf("status header:%d\n", status_tracker.ngscope_CA_status.header);
    }
    printf("Close Status Tracker!\n");
    
    if(dis_plot == 0){
        if (!pthread_kill(plot_thread, 0)) {
          pthread_kill(plot_thread, SIGHUP);
          pthread_join(plot_thread, NULL);
        }
    }
    // close the remote socket
    if(status_tracker.remote_sock > 0){
        close(status_tracker.remote_sock);
    }

    status_tracker_print_ue_freq(&status_tracker);
    return NULL;
}
