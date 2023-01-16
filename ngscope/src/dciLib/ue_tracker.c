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
int find_top_n_ue(ngscope_ue_tracker_t* q, int top_n){
	int max_i = 0;
	uint32_t max_v = 0;
	uint32_t max_freq 	= q->top_N_ue_freq[top_n-1];
	for(int i=0; i<=65535; i++){
		if( (q->ue_cnt[i] < max_freq) && (q->ue_cnt[i] > max_v) && (q->active_ue_list[i]) ){
			max_i = i;
			max_v = q->ue_cnt[i];
		}
	}
	return max_i;
}

void remove_ue_from_list(ngscope_ue_tracker_t* q, uint16_t rnti){
	//printf("remove %d from the list!\n", rnti);
	q->ue_cnt[rnti] 		= 0;
	q->ue_last_active[rnti] = 0;
	q->ue_enter_time[rnti] 	= 0;
    q->ue_dl_cnt[rnti] 		= 0;
    q->ue_ul_cnt[rnti] 		= 0;

	q->active_ue_list[rnti] = false;
	// remove the ue from topN
	for(int i=0; i<TOPN; i++){
		if(q->top_N_ue_rnti[i] == rnti){
			// shift the array to the left, but we left one position open, 
			// we need to fill that position
			shift_array_uint16_left(q->top_N_ue_rnti, TOPN, i);
			shift_array_uint32_left(q->top_N_ue_freq, TOPN, i);
			q->top_N_ue_rnti[TOPN-1] = 0;
			q->top_N_ue_freq[TOPN-1] = 0;

			// find the most freqent rnti except the TOPN
			int index = find_top_n_ue(q, TOPN-1);

			if(index > 0){
				q->top_N_ue_rnti[TOPN-1] = (uint16_t)index;
				q->top_N_ue_freq[TOPN-1] = q->ue_cnt[index];
			}

			break;
		}
	}
	return;
}

// NOTE: TOP-N array ue freq is always sorted 
// when we insert the rnti, we want to check whether we find top 20 most frequently observed rnti 
bool update_ue_tracker_topN(ngscope_ue_tracker_t* q, uint16_t rnti){
	uint32_t ue_cnt = q->ue_cnt[rnti];
	bool ret = false;
	int idx = -1;
	// check if the rnti is in top N or not
	for(int i=0; i<TOPN; i++){
		if(q->top_N_ue_rnti[i] == rnti){
			q->top_N_ue_freq[i] = ue_cnt;
			idx = i; 
			break;
		}
	}
	// if rnti is in TOPN
	if(idx >= 0){
		// switch the position of 
		for(int i=0; i<TOPN; i++){
			if(q->top_N_ue_freq[i] < ue_cnt && i < idx){
				// don't switch if we are switching with ourself
				if(idx == i) break; 
				swap_array_uint16(q->top_N_ue_rnti, TOPN, i, idx);
				swap_array_uint32(q->top_N_ue_freq, TOPN, i, idx);
				ret = true;
				break;
			}
		}
	}
	// if rnti is not inside TOPN and now we need to delete one in the array
	// (only happen if we found that the RNTI is active  
	else{
		if(q->active_ue_list[rnti]){
			// shift the array and insert new rnti
			for(int i=0; i<TOPN; i++){
				if(q->top_N_ue_freq[i] < ue_cnt){
					// shift the vec and insert the value into the array
					shift_array_uint32_right(q->top_N_ue_freq, TOPN, i);
					q->top_N_ue_freq[i] = ue_cnt;

					// shift the vec and insert the value into the array
					shift_array_uint16_right(q->top_N_ue_rnti, TOPN, i);
					q->top_N_ue_rnti[i] = rnti;
					ret = true;
					break;
				}
			}
		}
	}
	
	return ret;
}

int kick_inactive_ue(ngscope_ue_tracker_t* q, uint32_t tti){
	int active_ue = 0;
	for(int i=0; i<=65535; i++){
		if(q->active_ue_list[i] && q->ue_cnt[i]){
			int t_diff = tti_difference(q->ue_last_active[i], tti);
			// if the active ue is inactive for INACTIVE_UE_THD_T ms 
			if(t_diff >= ACTIVE_UE_THD_T){
				remove_ue_from_list(q, i);
			}
		}else{
			if(q->ue_cnt[i]){
				int t_diff = tti_difference(q->ue_last_active[i], tti);
				// if the non-active ue hasn't been observed for 500 ms 
				if(t_diff >= INACTIVE_UE_THD_T){
					remove_ue_from_list(q, i);
				}
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
	if(tti_difference(q->ue_last_active[rnti], tti) < ACTIVE_TTI_T || 
			q->ue_cnt[rnti] > ACTIVE_UE_CNT_THD){
		q->active_ue_list[rnti] = true;
		//printf("tti:%d found active UE: cnt:%d \n", tti, q->ue_cnt[rnti]);
	}
		
	// then we update its last active tti
    q->ue_last_active[rnti] = tti; 

	// check whether we need to update the top N
	bool updated; 
	updated = update_ue_tracker_topN(q, rnti);

	//printf("TTI:%d enqueue rnti:%d is active:%d ue_cnt:%d updated inside the TopN:%d\n", tti, rnti, q->active_ue_list[rnti], q->ue_cnt[rnti], updated);
    return;
}

void ngscope_ue_tracker_update_per_tti(ngscope_ue_tracker_t* q, uint32_t tti){
	// remove those ue that has been inactive for 
	int nof_active_ue 	= kick_inactive_ue(q, tti);
	q->nof_active_ue 	= nof_active_ue;
	return;
}

void ngscope_ue_tracker_info(ngscope_ue_tracker_t* q, uint32_t tti){
	printf("TTI:%d Nof active ue:%d ", tti, q->nof_active_ue);
	for(int i=0; i<TOPN; i++){
		printf("%d|%d ", q->top_N_ue_rnti[i], q->top_N_ue_freq[i]);
	}
	printf("\n");
	return;
}

