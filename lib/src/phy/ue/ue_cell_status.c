#include "srslte/phy/ue/ue_list.h"
#include "srslte/phy/ue/ue_cell_status.h"
#include "srslte/phy/ue/lte_scope.h"
#include "srslte/phy/ue/overhead.h"
#include <sys/types.h>
#include <sys/socket.h>

#define TTI_TO_IDX(i) (i%NOF_LOG_SF)
int srslte_UeCell_init(srslte_ue_cell_usage* q)
{
    q->targetRNTI   = 0;
    q->logFlag	    = false;
    q->printFlag    = false;
    q->FD	    = NULL;
    q->remote_sock  = 0;
    q->remote_flag  = 0;

    q->nof_cell     = 0;
    q->header	    = 0;
    
    for(int i=0;i<MAX_NOF_CA;i++){
	q->max_cell_prb[i]	= 0;
	q->nof_thread[i]	= 0;
	q->cell_triggered[i]	= false;

	memset(&q->cell_status[i].sf_status, 0, NOF_LOG_SF * sizeof(srslte_subframe_status));
	memset(&q->cell_status[i].dci_token, 0, NOF_LOG_SF * sizeof(bool));
    }
    return 0;
}

int srslte_UeCell_reset(srslte_ue_cell_usage* q)
{
    q->header	    = 0;
    for(int i=0;i<MAX_NOF_CA;i++){
	q->cell_triggered[i]	= false;
	memset(&q->cell_status[i].sf_status, 0, NOF_LOG_SF * sizeof(srslte_subframe_status));
	memset(&q->cell_status[i].dci_token, 0, NOF_LOG_SF * sizeof(bool));
    }
    return 0;
}

static int count_nof_taken_token(srslte_cell_status* q)
{
    int nof_token=0;
    for(int i=0;i<NOF_LOG_SF;i++){
	if(q->dci_token[i] == true){nof_token++;}
    }
    return nof_token;
} 

// check whether c is within the range of [a, b]
static bool within_range(uint16_t a, uint16_t b, uint16_t c)
{
    if(b < a){ b += NOF_LOG_SF;}
    if(c < a){ c += NOF_LOG_SF;}
    if( (c >= a) && (c <= b) ){
	return true;
    }else{
	return false;
    }
}

/*********************************************************
* MAIN: any dci decoder ask for dci token 
*********************************************************/
int srslte_UeCell_ask_for_dci_token(srslte_ue_cell_usage* q, int ca_idx, uint32_t tti)
{
    uint16_t index = TTI_TO_IDX(tti);
    int nof_taken_token;
    nof_taken_token = count_nof_taken_token(&q->cell_status[ca_idx]);
    if( nof_taken_token >= q->nof_thread[ca_idx]){
	printf("ERROR: all the token has been taken!\n");
	return -1;
    }
    q->cell_status[ca_idx].dci_token[index] = true; 
    //printf("ASKing DCI token: cell:%d tti:%d index:%d %d header:%d touched:%d\n", ca_idx, tti, index,index_new,  
//			    q->cell_status[ca_idx].header, q->cell_status[ca_idx].dci_touched);
    // Set the dci_touched to the largest tti of the dci messages that has been touched
    if( !within_range(q->cell_status[ca_idx].header, q->cell_status[ca_idx].dci_touched, index) ){
	 q->cell_status[ca_idx].dci_touched = index;
    }
    return 0;
}

static int enqueue_ue_dci(srslte_cell_status* q, srslte_dci_msg_paws* ue_dci, uint16_t index){
    q->sf_status[index].mcs_idx_tb1 = ue_dci->mcs_idx_tb1;
    q->sf_status[index].mcs_idx_tb2 = ue_dci->mcs_idx_tb2;

    q->sf_status[index].tbs_tb1 = ue_dci->tbs_tb1;
    q->sf_status[index].tbs_tb2 = ue_dci->tbs_tb2;

    q->sf_status[index].tbs_hm_tb1 = ue_dci->tbs_hm_tb1;
    q->sf_status[index].tbs_hm_tb2 = ue_dci->tbs_hm_tb2;

    q->sf_status[index].rv_tb1 = ue_dci->rv_tb1;
    q->sf_status[index].rv_tb2 = ue_dci->rv_tb2;

    q->sf_status[index].ndi_tb1 = ue_dci->ndi_tb1;
    q->sf_status[index].ndi_tb2 = ue_dci->ndi_tb2;

    return 0;
}

static int empty_ue_dci(srslte_cell_status* q,  uint16_t index){
    q->sf_status[index].mcs_idx_tb1 = 0;
    q->sf_status[index].mcs_idx_tb2 = 0;
    q->sf_status[index].tbs_tb1 = 0;
    q->sf_status[index].tbs_tb2 = 0;
    q->sf_status[index].tbs_hm_tb1 = 0;
    q->sf_status[index].tbs_hm_tb2 = 0;
    q->sf_status[index].rv_tb1 = 0;
    q->sf_status[index].rv_tb2 = 0;
    q->sf_status[index].ndi_tb1 = 0;
    q->sf_status[index].ndi_tb2 = 0;
    return 0;
}

static int enqueue_bw_usage(srslte_cell_status* q, srslte_subframe_bw_usage* bw_usage, uint16_t index, uint32_t tti){
    q->sf_status[index].tti = tti;
    q->sf_status[index].cell_dl_prb = bw_usage->cell_dl_prb;
    q->sf_status[index].cell_ul_prb = bw_usage->cell_ul_prb;
    q->sf_status[index].ue_dl_prb = bw_usage->ue_dl_prb;
    q->sf_status[index].ue_ul_prb = bw_usage->ue_ul_prb;
    return 0;
}
static bool no_token_taken_within_range(srslte_cell_status* q){
    uint16_t start  = q->header;
    uint16_t end    = q->dci_touched; 
    if( end < start){ end += NOF_LOG_SF;}
    
    for(int i=start;i<=end;i++){
	uint16_t index = TTI_TO_IDX(i);
	if(q->dci_token[index] == true){
	    return false;
	}
    }
    return true; 
}
static int update_single_cell_header(srslte_cell_status* q){
    // If all the subframe between header and dci_touched are all handled
    // change the header to the dci_touched 
    if( no_token_taken_within_range(q) ){
	q->header = q->dci_touched;
    }	
    return 0;
}
static int min_int_list(int* array, int num){
    int value = MAX_TTI;
    for(int i=0;i<num;i++){
	if(array[i] < value){
	    value = array[i];
	}
    }
    return value;
}
static int max_int_list(int* array, int num){
    int value = 0;
    for(int i=0;i<num;i++){
	if(array[i] > value){
	    value = array[i];
	}
    }
    return value;
}

static int unwrapping_tti(int* tti_array, int num){
    int max_tti = max_int_list(tti_array, num);
    for(int i=0;i<num;i++){
	if(tti_array[i] == max_tti){continue;}
	if(max_tti > tti_array[i] + MAX_TTI/2){
	    tti_array[i] += MAX_TTI;
	}
    }
    return 0; 
}

static void print_single_trace(srslte_ue_cell_usage* q, uint16_t index){

    for(int i=0;i<q->nof_cell;i++){
	uint16_t cell_header = q->cell_status[i].header;
	printf("%d\t%d\t%d\t%d\t%d\t",q->cell_status[i].sf_status[index].tti,
				q->cell_status[i].sf_status[cell_header].tti,
				q->cell_status[i].sf_status[index].cell_dl_prb, 
				q->cell_status[i].sf_status[index].ue_dl_prb,
				q->cell_triggered[i]);
    }
    printf("\n");
    return;
}
static void log_single_trace(srslte_ue_cell_usage* q, uint16_t index){
    FILE* FD = q->FD;
    if(FD == NULL){
	printf(" ERROR: FILE descriptor is not set !\n");
	return;
    }
    for(int i=0;i<q->nof_cell;i++){
	fprintf(FD, "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t",
		q->cell_status[i].sf_status[index].tti,
		q->cell_status[i].sf_status[index].cell_dl_prb, 
		q->cell_status[i].sf_status[index].ue_dl_prb,
		q->cell_status[i].sf_status[index].mcs_idx_tb1,
		q->cell_status[i].sf_status[index].mcs_idx_tb2,
		q->cell_status[i].sf_status[index].tbs_tb1 + q->cell_status[i].sf_status[index].tbs_tb2,
		q->cell_status[i].sf_status[index].tbs_hm_tb1 + q->cell_status[i].sf_status[index].tbs_hm_tb2,
		q->cell_status[i].sf_status[index].rv_tb1,
		q->cell_status[i].sf_status[index].rv_tb2,
		q->cell_status[i].sf_status[index].ndi_tb1,
		q->cell_status[i].sf_status[index].ndi_tb2,
		q->cell_triggered[i]);
    }
    fprintf(FD, "\n");
    return;
}
/*********************************  RATE related ******************************/
int lteCCA_sum_statics(srslte_ue_cell_usage* q, int cell_idx, int len, 
			int* cell_prb, int* ue_prb, int* ue_tbs, int* ue_tbs_hm){
    uint16_t cell_header = q->header;
    int	     cell_prb_sum   = 0;
    int	     ue_prb_sum	    = 0;
    int	     ue_tbs_sum	    = 0;
    int	     ue_tbs_hm_sum  = 0;

    for(int i=0;i<len;i++){
	int index = cell_header - len + 1 + i;
	if(index < 0){
	    index += NOF_LOG_SF;
	}
	int idx = TTI_TO_IDX(index);
	cell_prb_sum	+= (int) (q->cell_status[cell_idx].sf_status[idx].cell_dl_prb); 
	ue_prb_sum	+= (int) (q->cell_status[cell_idx].sf_status[idx].ue_dl_prb);
	ue_tbs_sum	+= (int) (q->cell_status[cell_idx].sf_status[idx].tbs_tb1);
	ue_tbs_sum	+= (int) (q->cell_status[cell_idx].sf_status[idx].tbs_tb2);
	ue_tbs_hm_sum	+= (int) (q->cell_status[cell_idx].sf_status[idx].tbs_hm_tb1);
	ue_tbs_hm_sum	+= (int) (q->cell_status[cell_idx].sf_status[idx].tbs_hm_tb2);
    }

    *cell_prb	= cell_prb_sum;
    *ue_prb	= ue_prb_sum;
    *ue_tbs	= ue_tbs_sum;
    *ue_tbs_hm	= ue_tbs_hm_sum;

    return 0;
}
int lteCCA_sum_cell_prb(srslte_ue_cell_usage* q, int cell_idx, int len){
    uint16_t cell_header = q->header;
    int	     cell_prb_sum   = 0;

    for(int i=0;i<len;i++){
	int index = cell_header - len + 1 + i;
	if(index < 0){
	    index += NOF_LOG_SF;
	}
	int idx = TTI_TO_IDX(index);
	cell_prb_sum	+= (int) (q->cell_status[cell_idx].sf_status[idx].cell_dl_prb); 
    }
    return cell_prb_sum;
}
int lteCCA_sum_ue_prb(srslte_ue_cell_usage* q, int cell_idx, int len){
    uint16_t cell_header = q->header;
    int	     ue_prb_sum	    = 0;
    for(int i=0;i<len;i++){
	int index = cell_header - len + 1 + i;
	if(index < 0){
	    index += NOF_LOG_SF;
	}
	int idx = TTI_TO_IDX(index);
	ue_prb_sum	+= (int) (q->cell_status[cell_idx].sf_status[idx].ue_dl_prb);
    }
    return ue_prb_sum;
}
int lteCCA_sum_tbs(srslte_ue_cell_usage* q, int cell_idx, int len, int* ue_tbs, int* ue_tbs_hm){
    uint16_t cell_header = q->header;
    int	     ue_tbs_sum	    = 0;
    int	     ue_tbs_hm_sum  = 0;
    for(int i=0;i<len;i++){
	int index = cell_header - len + 1 + i;
	if(index < 0){
	    index += NOF_LOG_SF;
	}
	int idx = TTI_TO_IDX(index);
	ue_tbs_sum	+= (int) (q->cell_status[cell_idx].sf_status[idx].tbs_tb1);

	ue_tbs_sum	+= (int) (q->cell_status[cell_idx].sf_status[idx].tbs_tb2);

	ue_tbs_hm_sum	+= (int) (q->cell_status[cell_idx].sf_status[idx].tbs_hm_tb1);

	ue_tbs_hm_sum	+= (int) (q->cell_status[cell_idx].sf_status[idx].tbs_hm_tb2);
    }

    *ue_tbs	= ue_tbs_sum;
    *ue_tbs_hm	= ue_tbs_hm_sum;
    return 0;
}
int lteCCA_tuning_tbs(int tbs){
    if(tbs < 1000){
	return tbs;
    }
    int rateM       = tbs / 1000;
    if(rateM <=0){ rateM = 1;}

    float overhead  = overhead_ratio_phy[11][rateM-1] + protocol_overhead;
    //printf("overhead: %f \n", overhead);
    int tuned_tbs = (int) ( (1 - overhead) * tbs);

    //printf("||| RateM:%d overhead %.4f tuned_tbs:%d\n", rateM, overhead, tuned_tbs);
    return tuned_tbs;
}

int lteCCA_predict_tbs_single_cell(srslte_ue_cell_usage* q, int cell_idx, int* tbs, int* tbs_hm, int* full_load, int *full_load_hm){
    int full_load_rate = 0, full_load_rate_hm = 0;
    int max_cell_prb	= q->max_cell_prb[cell_idx];

    // we calculate how many empty prbs are in the cell  -- EMPTY_CELL_LEN determines nof_sf we average
    int cell_sum_prb	= lteCCA_sum_cell_prb(q, cell_idx, EMPTY_CELL_LEN);
    int cell_empty_prb	= (EMPTY_CELL_LEN * max_cell_prb - cell_sum_prb) / EMPTY_CELL_LEN;

    // calculate how many prbs has been allocalted to the ue in the past AVE_UE_DL_PRB subframes 
    int ue_sum_prb	= lteCCA_sum_ue_prb(q, cell_idx, AVE_UE_DL_PRB);
    int ave_ue_prb	= ue_sum_prb / AVE_UE_DL_PRB; 
    
    // we average the physical layer rate in the nearest AVE_UE_PHY_RATE subframes 
    int ue_tbs		= 0;
    int ue_tbs_hm	= 0;
    lteCCA_sum_tbs(q, cell_idx, AVE_UE_PHY_RATE, &ue_tbs, &ue_tbs_hm);
    ue_sum_prb		= lteCCA_sum_ue_prb(q, cell_idx, AVE_UE_PHY_RATE);

    int ue_phy_rate	= 0;
    int ue_phy_rate_hm	= 0;
    if(ue_sum_prb <= 0){
	// the UE is not active in the past AVE_UE_PHY_RATE subframes,
	// we thus use the rate from last round
	ue_phy_rate	= q->cell_status[cell_idx].last_rate;
	ue_phy_rate_hm	= q->cell_status[cell_idx].last_rate_hm;
    }else{
	// otherwise, we use the calculated rate and update the last rate
	ue_phy_rate	= ue_tbs    / ue_sum_prb;
	ue_phy_rate_hm	= ue_tbs_hm / ue_sum_prb;
	q->cell_status[cell_idx].last_rate	= ue_phy_rate;
	q->cell_status[cell_idx].last_rate_hm	= ue_phy_rate_hm;
	full_load_rate	    = ue_phy_rate;
	full_load_rate_hm   = ue_phy_rate_hm;
    }
    // the available PRB for the user is estimated
    int exp_available_prb   = cell_empty_prb + ave_ue_prb;   // available prb for the ue
    if(exp_available_prb > max_cell_prb){
	exp_available_prb   = max_cell_prb;
    }

    //printf("empty prb:%d ue_prb:%d ue_tbs:%d tbs_hm:%d phy_rate:%d rate_hm:%d\n",
//	    cell_empty_prb, ave_ue_prb, ue_tbs, ue_tbs_hm, ue_phy_rate, ue_phy_rate_hm);

    int exp_tbs		    = exp_available_prb * ue_phy_rate;	// expected tbs for the ue
    int exp_tbs_hm	    = exp_available_prb * ue_phy_rate_hm;	// expected tbs hm
    int tuned_exp_tbs	    = lteCCA_tuning_tbs(exp_tbs);
    int tuned_exp_tbs_hm    = lteCCA_tuning_tbs(exp_tbs_hm);
    
    int full_load_tbs		= max_cell_prb  * full_load_rate;
    int full_load_tbs_hm	= max_cell_prb  * full_load_rate_hm;
    int tuned_full_load_tbs	=  lteCCA_tuning_tbs(full_load_tbs);
    int tuned_full_load_tbs_hm	=  lteCCA_tuning_tbs(full_load_tbs_hm);

    *tbs	    = tuned_exp_tbs;
    *tbs_hm	    = tuned_exp_tbs_hm;

    *full_load	    = tuned_full_load_tbs;
    *full_load_hm   = tuned_full_load_tbs_hm;

    return 0;
} 
int lteCCA_tbs_to_rate_us(int tbs){
    if(tbs <= 0){
        //printf("TBS estimation is 0!\n");
        return 9999;
    }
    int int_pkt_t_us = (int) ( (1000 * NOF_BITS_PER_PKT) / tbs);
    return int_pkt_t_us;
}
int lteCCA_cell_usage(srslte_ue_cell_usage* q){
    int cell_sum_prb    = lteCCA_sum_cell_prb(q, 0, EMPTY_CELL_LEN);
    float cell_usage_f	= (float) cell_sum_prb / (float) (EMPTY_CELL_LEN * q->max_cell_prb[0]);
    int cell_usage	= (int)( cell_usage_f * 100);
    return cell_usage;
}
     
int lteCCA_predict_rate(srslte_ue_cell_usage* q, int* rate, int* rate_hm, int* full_load, int* full_load_hm){
    int tbs, tbs_hm;
    int full_load_tbs, full_load_hm_tbs;
    int exp_tbs=0, exp_tbs_hm=0;
    int exp_full_load_tbs=0, exp_full_load_tbs_hm=0;

    // we always estimate the primary cell
    lteCCA_predict_tbs_single_cell(q, 0, &tbs, &tbs_hm, &full_load_tbs, &full_load_hm_tbs);
    exp_tbs	+= tbs;
    exp_tbs_hm	+= tbs_hm;

    exp_full_load_tbs	    += full_load_tbs;
    exp_full_load_tbs_hm    += full_load_hm_tbs;

    // estimate if there is a secondary cell
    int nof_cell = q->nof_cell;
    if(nof_cell > 1){
	for(int i=1;i<nof_cell;i++){
	    if(q->cell_triggered[i]){
		lteCCA_predict_tbs_single_cell(q, i, &tbs, &tbs_hm, &full_load_tbs, &full_load_hm_tbs);
		exp_tbs	    += tbs;
		exp_tbs_hm  += tbs_hm;
		exp_full_load_tbs	+= full_load_tbs;
		exp_full_load_tbs_hm    += full_load_hm_tbs;
	    }
	}
    }

    int probe_rate	= lteCCA_tbs_to_rate_us(exp_tbs); 
    int probe_rate_hm	= lteCCA_tbs_to_rate_us(exp_tbs_hm); 

    int full_load_rate	    = lteCCA_tbs_to_rate_us(exp_full_load_tbs); 
    int full_load_rate_hm   = lteCCA_tbs_to_rate_us(exp_full_load_tbs_hm); 

    *rate	= probe_rate;
    *rate_hm	= probe_rate_hm;

    *full_load	    =  full_load_rate;
    *full_load_hm   =  full_load_rate_hm;
    return 0;
}
int lteCCA_average_ue_rate(srslte_ue_cell_usage* q, int* rate, int* rate_hm){
     // sum the tbs of nearest subframes
    int ue_tbs		= 0;
    int ue_tbs_hm	= 0;
    int ave_tbs=0, ave_tbs_hm=0;

    lteCCA_sum_tbs(q, 0, AVE_UE_RATE, &ue_tbs, &ue_tbs_hm);
    ue_tbs	= ue_tbs / AVE_UE_RATE;
    ue_tbs_hm	= ue_tbs_hm / AVE_UE_RATE;
    ave_tbs	+= lteCCA_tuning_tbs(ue_tbs);
    ave_tbs_hm	+= lteCCA_tuning_tbs(ue_tbs_hm);

    //printf("sum tbs:%d tbs_hm:%d ave_tbs:%d tbs_hm:%d\n", ue_tbs, ue_tbs_hm, ave_tbs, ave_tbs_hm);

    int nof_cell = q->nof_cell;
    if(nof_cell > 1){
	for(int i=1;i<nof_cell;i++){
	    if(q->cell_triggered[i]){
		lteCCA_sum_tbs(q, i, AVE_UE_RATE, &ue_tbs, &ue_tbs_hm);
		ue_tbs	    = ue_tbs / AVE_UE_RATE;
		ue_tbs_hm   = ue_tbs_hm / AVE_UE_RATE;
		ave_tbs	    += lteCCA_tuning_tbs(ue_tbs);
		ave_tbs_hm  += lteCCA_tuning_tbs(ue_tbs_hm);
		//printf("sum tbs:%d tbs_hm:%d ave_tbs:%d tbs_hm:%d\n", ue_tbs, ue_tbs_hm, ave_tbs, ave_tbs_hm);
	    }
	}
    }
    int ue_rate	    = lteCCA_tbs_to_rate_us(ave_tbs);
    int ue_rate_hm  = lteCCA_tbs_to_rate_us(ave_tbs_hm);
    //printf("final ue_rate:%d ue_rate_hm:%d\n",ue_rate, ue_rate_hm);
    *rate	= ue_rate;
    *rate_hm	= ue_rate_hm;
    return 0;
}
int lteCCA_rate_predication(srslte_ue_cell_usage* q, srslte_lteCCA_rate* lteCCA_rate){
    int probe_rate=0, probe_rate_hm=0;
    int ue_rate=0, ue_rate_hm =0;
    int full_load=0, full_load_hm=0;

    lteCCA_predict_rate(q, &probe_rate, &probe_rate_hm, &full_load, &full_load_hm); 
    lteCCA_average_ue_rate(q, &ue_rate, &ue_rate_hm);
    int cell_usage = lteCCA_cell_usage(q);

    lteCCA_rate->probe_rate	= probe_rate;
    lteCCA_rate->probe_rate_hm	= probe_rate_hm;

    lteCCA_rate->full_load	= full_load;
    lteCCA_rate->full_load_hm	= full_load_hm;

    lteCCA_rate->ue_rate	= ue_rate;
    lteCCA_rate->ue_rate_hm	= ue_rate_hm;

    lteCCA_rate->cell_usage	= cell_usage;
    return 0;
}

int srslte_UeCell_get_status(srslte_ue_cell_usage* q, uint32_t last_tti, uint32_t* current_tti, bool* ca_active,
                                        int cell_dl_prb[][NOF_REPORT_SF], int ue_dl_prb[][NOF_REPORT_SF],
                                        int mcs_tb1[][NOF_REPORT_SF], int mcs_tb2[][NOF_REPORT_SF],
                                        int tbs[][NOF_REPORT_SF], int tbs_hm[][NOF_REPORT_SF])
{
    uint16_t cell_header = q->header;
    uint32_t cell_tti	 = q->cell_status[0].sf_status[cell_header].tti;
    uint16_t nof_cell	 = q->nof_cell;

    *current_tti = cell_tti;
    // We don't need to update the list since it is the same as last request
    if(cell_tti == last_tti){
	return 0;
    }
 
    for(int i=0;i<NOF_REPORT_SF;i++){
	int index = cell_header - NOF_REPORT_SF + 1 + i;
	if(index < 0){
	    index += NOF_LOG_SF;
	}
	int idx	  = TTI_TO_IDX(index);
	//printf("tti:%d -- ", q->cell_status[0].sf_status[idx].tti);

	for(int j=0;j<nof_cell;j++){	
	    cell_dl_prb[j][i]	= (int) (q->cell_status[j].sf_status[idx].cell_dl_prb); 
	    ue_dl_prb[j][i]	= (int) (q->cell_status[j].sf_status[idx].ue_dl_prb); 
	    mcs_tb1[j][i]	= (int) (q->cell_status[j].sf_status[idx].mcs_idx_tb1); 
	    mcs_tb2[j][i]	= (int) (q->cell_status[j].sf_status[idx].mcs_idx_tb2); 
	    tbs[j][i]		= (int) (q->cell_status[j].sf_status[idx].tbs_tb1 + q->cell_status[j].sf_status[idx].tbs_tb2); 
	    tbs_hm[j][i]	= (int) (q->cell_status[j].sf_status[idx].tbs_hm_tb1 + q->cell_status[j].sf_status[idx].tbs_hm_tb2); 
	    //if(j == 0){
	    //    printf(" |%d %d %d %d| ",cell_dl_prb[j][i],ue_dl_prb[j][i],tbs[j][i], tbs_hm[j][i]);	
	    //}
	}
    }
    for(int i=0;i<nof_cell;i++){
	ca_active[i] = q->cell_triggered[i];
    }
    return 0;
}

static void print_multi_trace(srslte_ue_cell_usage* q, uint16_t start, uint16_t end){
    for(int i=start+1;i<=end;i++){
	uint16_t index = TTI_TO_IDX(i);
	print_single_trace(q, index);
    }
    return;
}
static void log_multi_trace(srslte_ue_cell_usage* q, uint16_t start, uint16_t end){
    for(int i=start+1;i<=end;i++){
	uint16_t index = TTI_TO_IDX(i);
	log_single_trace(q, index);
    }
    return;
}

static int check_ue_active(srslte_subframe_status* q){
    if( (q->ue_dl_prb > 0) || (q->ue_ul_prb > 0) ){
	return 1;
    }else{
	return 0;
    }
}
static uint32_t tti_difference(uint32_t curr_time, uint32_t last_time){
    if(curr_time < last_time){
	curr_time += MAX_TTI;
    }
    return curr_time - last_time;
}
static int update_ca_trigger(srslte_ue_cell_usage* q){
    uint16_t nof_cell	= q->nof_cell;
    uint16_t header	= q->header;

    for(int i=0;i<nof_cell;i++){
	uint32_t curr_tti = q->cell_status[i].sf_status[header].tti;
	uint32_t last_tti = q->cell_status[i].last_active;

	if( check_ue_active(&q->cell_status[i].sf_status[header]) == 1){
	    q->cell_status[i].last_active = curr_tti;
	}else{
	    uint32_t inactive_time = tti_difference(curr_tti, last_tti);
	    if(inactive_time >= CA_DEACTIVE_TIME){
		q->cell_triggered[i] = false;
	    }
	}
    }
    return 0;
}

int print_lteCCA_rate(srslte_lteCCA_rate* lteCCA_rate){
    printf("probe rate:%d rate_hm:%d full load:%d load_hm:%d ue rate:%d ue_rate_hm:%d\n",
	lteCCA_rate->probe_rate, 
	lteCCA_rate->probe_rate_hm,
	lteCCA_rate->full_load, 
	lteCCA_rate->full_load_hm, 
	lteCCA_rate->ue_rate, 
	lteCCA_rate->ue_rate_hm);
    return 0;
}

static int update_structure_header(srslte_ue_cell_usage* q){
    uint16_t nof_cell = q->nof_cell;
    int	     tti_array[nof_cell];
    for(int i=0;i<nof_cell;i++){
	tti_array[i] = q->cell_status[i].sf_status[q->cell_status[i].header].tti; 
	//printf("%d\t",tti_array[i]);
    }
    //printf("\n");
    unwrapping_tti(tti_array, nof_cell); // unwrapping tti index 

    uint16_t targetTTI = min_int_list(tti_array, nof_cell); 
    uint16_t targetIndex    = TTI_TO_IDX(targetTTI);

    if( targetIndex == q->header){
	return 0;
    }
    if( targetIndex < q->header){
	targetIndex += NOF_LOG_SF;	
    }    
    if( targetIndex - q->header > 100){
	printf("Error: we move the header of the UE_CELL_USAGE too fast!\n");
    }

    // update the carrier aggregation status
    //update_ca_trigger(q);

    if( q->logFlag){
	log_multi_trace(q, q->header, targetIndex);
    }
    if( q->printFlag){
	print_multi_trace(q, q->header, targetIndex);
    }
    q->header = TTI_TO_IDX(targetIndex);
    update_ca_trigger(q);
    if(q->remote_flag && q->remote_sock!=0){
	//TODO remote send
	srslte_lteCCA_rate lteCCA_rate;
	lteCCA_rate_predication(q, &lteCCA_rate);	
	//print_lteCCA_rate(&lteCCA_rate);
	send(q->remote_sock, &lteCCA_rate, sizeof(srslte_lteCCA_rate), 0);
    }
    return 0;

}

/*********************************************************
* MAIN: any dci decoder return the dci token it has taken
*********************************************************/

int srslte_UeCell_return_dci_token(srslte_ue_cell_usage* q, int ca_idx, uint32_t tti, 
				    srslte_dci_msg_paws* ue_dci, srslte_subframe_bw_usage* bw_usage)
{
    uint16_t index = TTI_TO_IDX(tti);
    q->cell_status[ca_idx].dci_token[index] = false;
    
    //printf("Return DCI token: ca_idx:%d tti:%d st_header:%d\n", ca_idx, tti, q->header);
    // Enqueue the ue dci message if the target RNTI is set
    if( (q->targetRNTI != 0) && (q->targetRNTI == ue_dci->rnti)){
	enqueue_ue_dci(&q->cell_status[ca_idx], ue_dci, index);
	if(!q->cell_triggered[ca_idx]){
	    q->cell_triggered[ca_idx] = true;
	}
    }else{
	// empty the dci information if no ue dci is found
	empty_ue_dci(&q->cell_status[ca_idx], index);
    }	

    // We always enqueue cell status
    enqueue_bw_usage(&q->cell_status[ca_idx], bw_usage, index, tti);
    update_single_cell_header(&q->cell_status[ca_idx]);
    update_structure_header(q);
    return 0;
}


/* Functions for getting the parameters */
int srslte_UeCell_get_maxPRB(srslte_ue_cell_usage* q, int* cellMaxPrb){
    for(int i=0;i<q->nof_cell;i++){
	cellMaxPrb[i]	= q->max_cell_prb[i];
    }
    return 0;
}
int srslte_UeCell_get_nof_cell(srslte_ue_cell_usage* q){
    return q->nof_cell; 
}
uint16_t srslte_UeCell_get_targetRNTI(srslte_ue_cell_usage* q)
{
    return q->targetRNTI;
}
/* Functions for setting the parameters */

int srslte_UeCell_set_remote_sock(srslte_ue_cell_usage* q,  int sock){
    q->remote_sock = sock;
    return 0;
}
int srslte_UeCell_set_remote_flag(srslte_ue_cell_usage* q,  bool flag){
    q->remote_flag = flag;
    return 0;
}

int srslte_UeCell_set_file_descriptor(srslte_ue_cell_usage* q,  FILE* FD){
    q->FD = FD;
    return 0;
}

int srslte_UeCell_set_logFlag(srslte_ue_cell_usage* q, bool logFlag)
{
    q->logFlag = logFlag;
    if( logFlag && q->FD == NULL){
	printf("\n\n ERROR: FILE descriptor is not set !\n\n");
    }
    return 0;
}
int srslte_UeCell_set_printFlag(srslte_ue_cell_usage* q, bool printFlag)
{
    q->printFlag = printFlag;
    return 0;
}

int srslte_UeCell_set_nof_cells(srslte_ue_cell_usage* q, uint16_t nof_cells)
{
    q->nof_cell = nof_cells;
    return 0;
}
int srslte_UeCell_set_targetRNTI(srslte_ue_cell_usage* q, uint16_t targetRNTI)
{
    q->targetRNTI = targetRNTI;
    return 0;
}
int srslte_UeCell_set_prb(srslte_ue_cell_usage* q, uint16_t nof_prb, int ca_idx)
{
    q->max_cell_prb[ca_idx] = nof_prb;
    return 0;
}

int srslte_UeCell_set_nof_thread(srslte_ue_cell_usage* q, uint16_t nof_thread, int ca_idx)
{
    q->nof_thread[ca_idx] = nof_thread;
    return 0;
}
/* END of functions for setting the parameters */
