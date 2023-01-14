#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>


#include "ngscope/hdr/dciLib/ue_tracker.h"
#include "ngscope/hdr/dciLib/ngscope_util.h"

// inti the structure
void ngscope_ue_tracker_init(ngscope_ue_tracker_t* q){
	for(int i=0; i<65535; i++){
		q->active_ue_list[i] 	= false;
		q->ue_cnt[i] 			= 0;
		q->ue_dl_cnt[i] 		= 0;
		q->ue_ul_cnt[i] 		= 0;
		q->ue_last_active[i] 	= 0;
		q->ue_enter_time[i] 	= 0;
	}
	for(int i=0; i<TOPN; i++){
		q->top_N_ue_rnti[i] 	= 0;
		q->top_N_ue_freq[i] 	= 0;
	}
	q->max_freq_ue 		= 0;
	q->max_dl_freq_ue 	= 0;
	q->max_ul_freq_ue 	= 0;
	q->nof_active_ue 	= 0;

	return;
}
void remove_ue_from_list(ngscope_ue_tracker_t* q, uint16_t rnti){
	q->ue_cnt[rnti] 		= 0;
	q->ue_last_active[rnti] = 0;
	q->ue_enter_time[rnti] 	= 0;
    q->ue_dl_cnt[rnti] 		= 0;
    q->ue_ul_cnt[rnti] 		= 0;

	q->active_ue_list[rnti] = false;

	return;
}
// when we insert the rnti, we want to check whether we find top 20 most frequently observed rnti 
void update_ue_tracker_w_ue_rnti(ngscope_ue_tracker_t* q, uint16_t rnti){
	uint32_t ue_cnt = q->ue_cnt[rnti];
	for(int i=0; i<TOPN; i++){
		if(q->top_N_ue_freq[i] < ue_cnt){
			// shift the vec and insert the value into the array
			shift_array_uint32(q->top_N_ue_freq, TOPN, i);
			q->top_N_ue_freq[i] = ue_cnt;

			// shift the vec and insert the value into the array
			shift_array_uint16(q->top_N_ue_rnti, TOPN, i);
			q->top_N_ue_rnti[i] = rnti;
		}
	}
	return;
}

int kick_inactive_ue(ngscope_ue_tracker_t* q, uint32_t tti){
	int active_ue = 0;
	for(int i=0; i<65535; i++){
		int t_diff = tti_distance(q->ue_last_active[i], tti);
		if(q->active_ue_list[i]){
			// if the active ue is inactive for INACTIVE_UE_THD_T ms 
			if(t_diff >= ACTIVE_UE_THD_T){
				remove_ue_from_list(q, i);
			}
		}else{
			// if the non-active ue hasn't been observed for 500 ms 
			if(t_diff >= INACTIVE_UE_THD_T){
				remove_ue_from_list(q, i);
			}
		}

		// just for statistics
		if(q->active_ue_list[i]){
			active_ue++;
		}
	}
	return active_ue;
}

void ngscope_ue_tracker_enqueue_ue_rnti(ngscope_ue_tracker_t* q, uint32_t tti, uint16_t rnti, bool dl){
	// if the ue is not active before
    if(q->ue_cnt[rnti] == 0){
        q->ue_enter_time[rnti] = tti;
    }    
    q->ue_cnt[rnti]++;
    if(dl){
        q->ue_dl_cnt[rnti]++;
    }else{
        q->ue_ul_cnt[rnti]++;
    }

	// judge whether this ue is active or not
	if(tti_distance(q->ue_last_active[rnti], tti) < ACTIVE_TTI_T || 
			q->ue_cnt[rnti] > ACTIVE_UE_CNT_THD){
		q->active_ue_list[rnti] = true;
	}
		
	// then we update its last active tti
    q->ue_last_active[rnti] = tti; 
	// check whether we need to update the top 20
	update_ue_tracker_w_ue_rnti(q, rnti);

    return;
}

void ngscope_ue_tracker_update_per_tti(ngscope_ue_tracker_t* q, uint32_t tti){
	// remove those ue that has been inactive for 
	int nof_active_ue 	= kick_inactive_ue(q, tti);
	q->nof_active_ue 	= nof_active_ue;
	return;
}
//
//void find_max_freq_ue(ngscope_ue_tracker_t* q){
//    // we only count the C-RNTI
//    int max_cnt = 0, idx = 0; 
//    int max_dl_cnt =0, dl_idx = 0;
//    int max_ul_cnt =0, ul_idx = 0;
//    for(int i=10; i<65524; i++){
//        if(q->ue_cnt[i] > max_cnt){
//            max_cnt = q->ue_cnt[i];
//            idx     = i;
//        } 
//        if(q->ue_dl_cnt[i] > max_dl_cnt){
//            max_dl_cnt = q->ue_dl_cnt[i];
//            dl_idx     = i;
//        }
//        if(q->ue_ul_cnt[i] > max_ul_cnt){
//            max_ul_cnt = q->ue_ul_cnt[i];
//            ul_idx     = i;
//        }
//    }
//    q->max_freq_ue = idx;
//    q->max_dl_freq_ue = dl_idx;
//    q->max_ul_freq_ue = ul_idx;
//    return;
//}

//int ngscope_ue_list_print_ue_freq(ngscope_ue_tracker_t* q){
//	// find the max frequency ue in the ue list
//	find_max_freq_ue(q);
//
//	// print the max frequency ue in the ue list
//    printf("High Freq UE: DL rnti:%d freq:%d | UL rnti:%d freq:%d | Total rnti:%d freq: %d\n",
//        q->max_dl_freq_ue, q->ue_dl_cnt[q->max_dl_freq_ue], 
//        q->max_ul_freq_ue, q->ue_ul_cnt[q->max_ul_freq_ue], 
//        q->max_freq_ue, q->ue_cnt[q->max_freq_ue]);
//    return 0;
//}


