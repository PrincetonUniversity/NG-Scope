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


#include "ngscope/hdr/dciLib/ue_list.h"

void enqueue_ue_list_per_rnti(ngscope_ue_list_t* q, uint32_t tti, uint16_t rnti, bool dl){
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

void ngscope_ue_list_enqueue_rnti_per_sf_per_cell(ngscope_ue_list_t q[MAX_NOF_RF_DEV], ngscope_status_buffer_t* dci_buf){

	int cell_idx = dci_buf->cell_idx;	
	for(int j=0; j<dci_buf->dci_per_sub.nof_dl_dci; j++){
		enqueue_ue_list_per_rnti(&(q[cell_idx]), dci_buf->tti, \
				dci_buf->dci_per_sub.dl_msg[j].rnti, true);
	}

	for(int j=0; j<dci_buf->dci_per_sub.nof_ul_dci; j++){
		enqueue_ue_list_per_rnti(&(q[cell_idx]), dci_buf->tti, \
				dci_buf->dci_per_sub.ul_msg[j].rnti, false);
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

int ngscope_ue_list_print_ue_freq(ngscope_ue_list_t* q){
	// find the max frequency ue in the ue list
	find_max_freq_ue(q);

	// print the max frequency ue in the ue list
    printf("High Freq UE: DL rnti:%d freq:%d | UL rnti:%d freq:%d | Total rnti:%d freq: %d\n",
        q->max_dl_freq_ue, q->ue_dl_cnt[q->max_dl_freq_ue], 
        q->max_ul_freq_ue, q->ue_ul_cnt[q->max_ul_freq_ue], 
        q->max_freq_ue, q->ue_cnt[q->max_freq_ue]);
    return 0;
}


