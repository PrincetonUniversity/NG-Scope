#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>

#include "srslte/phy/ue/ue_list.h"

// Find the rnti with highest frequency and calculate the number of active UE
static uint16_t max_freq_rnti_in_list(srslte_ue_list_t* q, int* ue_count){
    uint16_t rnti_v = 0;
    uint16_t rnti_f = 0;
    int count = 0;
    for(int i=10;i<65533;i++){
        if(q->active_ue_list[i] > 0){
            count++;
        }
        if(q->ue_cnt[i] > rnti_f){
            rnti_v = i;
            rnti_f = q->ue_cnt[i];
        }
    }
    *ue_count = count;
    return rnti_v;
}

// Init the ue list
void srslte_init_ue_list(srslte_ue_list_t* q){
    q->max_freq_ue      = 0;
    q->nof_active_ue    = 0;
    bzero(q->active_ue_list, 65536*sizeof(uint8_t));
    bzero(q->ue_cnt, 65536*sizeof(uint16_t));
    bzero(q->ue_ul_cnt, 65536*sizeof(uint16_t));
    bzero(q->ue_dl_cnt, 65536*sizeof(uint16_t));
    bzero(q->ue_last_active, 65536*sizeof(uint32_t));
    bzero(q->ue_enter_time, 65536*sizeof(uint32_t));
    return;
}

static void kill_ue(srslte_ue_list_t*q, int rnti){
    q->active_ue_list[rnti] = 0;
    q->ue_cnt[rnti]         = 0;
    q->ue_last_active[rnti] = 0;
    q->ue_enter_time[rnti]  = 0;
    return;
}

void srslte_update_ue_list_every_subframe(srslte_ue_list_t* q, uint32_t tti){
    for(int i=10;i<65533;i++){
        if (q->active_ue_list[i] == 1){
            int inactive_time = tti - q->ue_last_active[i];
            // kill an UE when the UE has been inactive for long time
            if( inactive_time > UE_INACTIVE_TIME_LIMIT){
                kill_ue(q, i);
            }
        }else{
            if (q->ue_cnt[i] > 0){
                int ue_enter_time = tti - q->ue_enter_time[i];
                if( ue_enter_time > UE_REAPPEAR_TIME_LIMIT){
                    //printf("%d time: not long enough kill %d\n",i, ue_enter_time);
                    kill_ue(q, i);
                }
            }
        }
    }
    // update the statistics of active UE
    int count = 0;
    q->max_freq_ue = max_freq_rnti_in_list(q, &count);
    q->nof_active_ue = count;
    return;
}
static void move_ue_to_activeList(srslte_ue_list_t* q, uint16_t rnti){
    q->active_ue_list[rnti] = 1;
    return;
}

// Enqueu one rnti into the list
void srslte_enqueue_ue(srslte_ue_list_t* q, uint16_t rnti, uint32_t tti, bool downlink){
    // reset the time and count for UE (last active time and count)
    if(downlink){ 
	q->ue_dl_cnt[rnti]++;
    }else{
	q->ue_ul_cnt[rnti]++;
    } 
    q->ue_last_active[rnti] = tti;
    q->ue_cnt[rnti]++;

    // if RNTI is not in the active UE list
    if(q->active_ue_list[rnti] == 0){
        if(q->ue_enter_time[rnti] == 0){
            // if UE hasn't been detected before
            q->ue_enter_time[rnti]  = tti;
        }else{
            // move UE to the active UE list if the requirement is satisfied
            int ue_enter_time = q->ue_last_active[rnti] - q->ue_enter_time[rnti];
            int ue_repeat_No  = q->ue_cnt[rnti];
            if( (ue_enter_time < UE_REAPPEAR_TIME_LIMIT) &&
                                    (ue_repeat_No > UE_REAPPEAR_NO_LIMIT)){
                //printf("ACTIVE UE FOUND!\n");
                move_ue_to_activeList(q, rnti);
            }
        }
    }
//    printf("UE-%d enter time:%d last_active:%d cnt:%d\n",rnti, q->ue_enter_time[rnti],
//		    q->ue_last_active[rnti], q->ue_cnt[rnti]);

    // update the statistics of active UE
    int count = 0;
    q->max_freq_ue = max_freq_rnti_in_list(q, &count);
    q->nof_active_ue = count;
}

// check whether the rnti is in the list or not
uint8_t srslte_check_ue_inqueue(srslte_ue_list_t* q, uint16_t rnti){
    return q->active_ue_list[rnti];
}


void srslte_copy_active_ue_list(srslte_ue_list_t* q, srslte_active_ue_list_t* active_ue_list){
    memcpy(active_ue_list->active_ue_list, q->active_ue_list, 65536 * sizeof(uint8_t));
    active_ue_list->max_freq_ue = q->max_freq_ue;
    active_ue_list->nof_active_ue = q->nof_active_ue;
    return;
}

void fill_ue_statistics(srslte_ue_list_t* q, srslte_dci_msg_paws* dci_msg)
{
    dci_msg->max_freq_ue    = q->max_freq_ue;
    dci_msg->nof_active_ue  = q->nof_active_ue;
    dci_msg->active	    = q->active_ue_list[dci_msg->rnti];
    dci_msg->max_freq_ue_cnt= q->ue_cnt[q->max_freq_ue];
    dci_msg->my_cnt	    = q->ue_cnt[dci_msg->rnti];
}

void srslte_enqueue_subframe_msg(srslte_dci_subframe_t* q, srslte_ue_list_t* ue_list, uint32_t tti)
{
    if( q->dl_msg_cnt > 0){
	for(int i=0;i<q->dl_msg_cnt;i++){
	    srslte_enqueue_ue(ue_list, q->downlink_msg[i].rnti, tti, q->downlink_msg[i].downlink); 
	    fill_ue_statistics(ue_list, &q->downlink_msg[i]);
	}
    }
    if( q->ul_msg_cnt > 0){
	for(int i=0;i<q->ul_msg_cnt;i++){
	    srslte_enqueue_ue(ue_list, q->uplink_msg[i].rnti, tti,q->downlink_msg[i].downlink); 
	    fill_ue_statistics(ue_list, &q->uplink_msg[i]);
	}
    }
}
