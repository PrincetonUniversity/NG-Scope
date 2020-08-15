#include <sys/types.h>
#include <sys/socket.h>

#include "srslte/phy/ue/ue_list.h"
#include "srslte/phy/ue/lte_scope.h"
#include "srslte/phy/ue/overhead.h"
#include "srslte/phy/ue/LTESCOPE_GLOBAL.h"

#include "ue_cell_status.h"
#define TTI_TO_IDX(i) (i%NOF_LOG_SF)

// count how many token has been taken
static int count_nof_taken_token(srslte_cell_status* q)
{
    int nof_token=0;
    for(int i=0;i<NOF_LOG_SF;i++){
        if(q->dci_token[i] == true){
	    nof_token++;
	}
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


/********************************************************
* Enqueue the decoded dci message 
********************************************************/
// --> enqueue the downlink 
static int enqueue_dci_downlink(srslte_subframe_status* q, srslte_dci_msg_paws* dci_array, int msg_cnt)
{
    int cell_dl_prb = 0;

    for(int i=0;i<MAX_NOF_MSG_PER_SF;i++){
	if(i < msg_cnt){
	    q->dl_rnti_list[i]	= dci_array[i].rnti;
	    q->dl_nof_prb[i]	= dci_array[i].nof_prb;
	    cell_dl_prb		+=  dci_array[i].nof_prb;
	    
	    q->dl_mcs_tb1[i]	= (dci_array[i].mcs_idx_tb1 & 0xFF);
	    q->dl_mcs_tb2[i]	= (dci_array[i].mcs_idx_tb2 & 0xFF);
	    q->dl_tbs_tb1[i]	= dci_array[i].tbs_tb1;
	    q->dl_tbs_tb2[i]	= dci_array[i].tbs_tb2;
	    q->dl_tbs_hm_tb1[i] = dci_array[i].tbs_hm_tb1;
	    q->dl_tbs_hm_tb2[i] = dci_array[i].tbs_hm_tb2;
	    q->dl_rv_tb1[i]	= (dci_array[i].rv_tb1 & 0xFF);
	    q->dl_rv_tb2[i]	= (dci_array[i].rv_tb2 & 0xFF);
	    q->dl_ndi_tb1[i]	= dci_array[i].ndi_tb1;
	    q->dl_ndi_tb2[i]	= dci_array[i].ndi_tb2;
	}else{
	    q->dl_rnti_list[i]	= 0; 
	    q->dl_nof_prb[i]	= 0;

	    q->dl_mcs_tb1[i]	= 0;
	    q->dl_mcs_tb2[i]	= 0;
	    q->dl_tbs_tb1[i]	= 0; 
	    q->dl_tbs_tb2[i]	= 0; 
	    q->dl_tbs_hm_tb1[i] = 0; 
	    q->dl_tbs_hm_tb2[i] = 0; 
	    q->dl_rv_tb1[i]	= 0;
	    q->dl_rv_tb2[i]	= 0;
	    q->dl_ndi_tb1[i]	= 0;
	    q->dl_ndi_tb2[i]	= 0;
	}	
    }
    return cell_dl_prb;
}

// --> enqueue the downlink 
static int enqueue_dci_uplink(srslte_subframe_status* q, srslte_dci_msg_paws* dci_array, int msg_cnt)
{
    int cell_ul_prb = 0;
    for(int i=0;i<MAX_NOF_MSG_PER_SF;i++){
	if(i < msg_cnt){
	    q->ul_rnti_list[i]	= dci_array[i].rnti;
	    q->ul_nof_prb[i]	= dci_array[i].nof_prb; 
	    cell_ul_prb		+=  dci_array[i].nof_prb;

	    q->ul_mcs[i]	= (dci_array[i].mcs_idx_tb1 & 0xFF);
	    q->ul_tbs[i]	= dci_array[i].tbs_tb1;
	    q->ul_tbs_hm[i]	= dci_array[i].tbs_hm_tb1;
	    q->ul_rv[i]		= (dci_array[i].rv_tb1 & 0xFF);
	    q->ul_ndi[i]	= dci_array[i].ndi_tb1;
	}else{
	    q->ul_nof_prb[i]	= 0; 
	    q->ul_mcs[i]	= 0;
	    q->ul_tbs[i]	= 0;
	    q->ul_tbs_hm[i]	= 0;
	    q->ul_rv[i]		= 0;
	    q->ul_ndi[i]	= 0;
	}	
    }
    return cell_ul_prb;
}

// --> MAIN: enqueue the downlink/uplink and update the cell utilization 
static int enqueue_dci_per_subframe(srslte_cell_status* q, srslte_dci_subframe_t* dci_list, 
				    uint16_t index, uint32_t tti)
{
    /* two tasks need to be done in this function 
    *  1, enqueue the decoded control message (clear the list if the number of message is small)
    *  2, calculate the cell utilization information
    */
    int cell_ul_prb, cell_dl_prb;
    cell_dl_prb = enqueue_dci_downlink(&(q->sf_status[index]), dci_list->downlink_msg, dci_list->dl_msg_cnt); 
    cell_ul_prb = enqueue_dci_uplink(&(q->sf_status[index]), dci_list->uplink_msg, dci_list->ul_msg_cnt); 

    // update the cell utilization information
    q->sf_status[index].tti = tti;
    q->sf_status[index].cell_dl_prb = cell_dl_prb;
    q->sf_status[index].cell_ul_prb = cell_ul_prb;
    q->sf_status[index].nof_msg_dl  = dci_list->dl_msg_cnt;
    q->sf_status[index].nof_msg_ul  = dci_list->ul_msg_cnt;
    
    return 0;
}

static int enqueue_rf_status_per_subframe(srslte_cell_status* q, srslte_subframe_rf_status* rf_status,
				    uint16_t index, uint32_t tti)
{

    q->sf_status[index].rsrq  = rf_status->rsrq;
    q->sf_status[index].rsrp0 = rf_status->rsrp0;
    q->sf_status[index].rsrp1 = rf_status->rsrp1;
    q->sf_status[index].noise = rf_status->noise;

    return 0;
}
/********************************************************
* Update the cell header of a single cell 
********************************************************/
// --> no token is taken between the current header, 
//     and the most recent touched subframe
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

// --> MAIN: update the header of a single cell 
static int update_single_cell_header(srslte_cell_status* q){
    // If all the subframe between header and dci_touched are all handled
    // change the header to the dci_touched
    if( no_token_taken_within_range(q) ){
        q->header = q->dci_touched;
    }
    return 0;
}

/********************************************************
* Update the header of the whole status structure 
********************************************************/
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

// --> display single message
static void display_single_subframe_dci(srslte_ue_cell_usage* q, uint16_t index){

    for(int i=0;i<q->nof_cell;i++){
        printf("|%d\t%d\t%d\t",q->cell_status[i].sf_status[index].tti,
			      q->cell_status[i].sf_status[index].cell_dl_prb,
			      q->cell_status[i].sf_status[index].cell_ul_prb);
    }
    printf("\n");
    return;
}

// --> display the decoded dci messages
static void display_dci_message(srslte_ue_cell_usage* q, uint16_t start, uint16_t end){

    for(int i=start+1;i<=end;i++){
        uint16_t index = TTI_TO_IDX(i);
        display_single_subframe_dci(q, index);
    }

    return;
}
/* All the cells are synchronized if the header TTI are the same and are not zero*/
static bool check_synchronization(srslte_ue_cell_usage* q){
    uint16_t nof_cell = q->nof_cell;

    /* the first array is always not empty, we use it as the anchor
     * if the tti of other cells (the same sf) are different from the anchor
     * we know the cells are not synchronized */
    uint32_t anchor_TTI = q->cell_status[0].sf_status[q->header].tti;
    uint32_t target_TTI;

    if(anchor_TTI == 0){
	return false;	// TTI cannot be zero
    }

    if(nof_cell == 1){
	if(anchor_TTI <= 0){
	    return false;   // If there is only one cell and the anchor_TTI is zero
	}
    }else if(nof_cell > 1){
	for(int i=1;i<nof_cell;i++){
	    target_TTI = q->cell_status[i].sf_status[q->header].tti;
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
// --> MAIN: update the structure header 
static int update_structure_header(srslte_ue_cell_usage* q){
    uint16_t nof_cell = q->nof_cell;
    int      tti_array[nof_cell];

    for(int i=0;i<nof_cell;i++){
        tti_array[i] = q->cell_status[i].sf_status[q->cell_status[i].header].tti;
        //printf("%d\t",tti_array[i]);
    }
    //printf("\n");
    unwrapping_tti(tti_array, nof_cell); // unwrapping tti index

    uint16_t current_header = q->header;	// current header
    uint16_t targetTTI	    = min_tti_list(tti_array, nof_cell); // find the smallest tti 
    uint16_t targetIndex    = TTI_TO_IDX(targetTTI);		 // translate the TTI

    // If the index equals the header, we should not move the header of the structure
    if( targetIndex == q->header){
        return 0;
    }

    if( targetIndex < q->header){
        targetIndex += NOF_LOG_SF;
    }

    if( targetIndex - q->header > 100){
        printf("WARNNING: the header is moved for interval of %d subframes!\n", (targetIndex - q->header));
    }
   
    // print the dci
    //display_dci_message(q, current_header, targetIndex);

    // update the header
    q->header = TTI_TO_IDX(targetIndex);

    // check the syncrhonization state
    if(q->sync_flag == false){
	q->sync_flag = check_synchronization(q);
    }
    return 0;
}


// structure init and reset 

/*********************************************************
// Init-- initialize the structure 
*********************************************************/
int cell_status_init(srslte_ue_cell_usage* q)
{
    //q->targetRNTI   = 0;
    q->printFlag    = false;

    q->logFlag      = false;
    q->dci_fd	    = NULL;

    q->remote_flag  = false;
    q->remote_sock  = 0;
    
    q->sync_flag    = false;

    q->nof_cell     = 0;

    q->header       = 0;
    q->stat_header  = 0;

    for(int i=0;i<MAX_NOF_CA;i++){
        q->max_cell_prb[i]      = 0;
        q->nof_thread[i]        = 0;
        q->cell_triggered[i]    = false;

        memset(&q->cell_status[i].sf_status, 0, NOF_LOG_SF * sizeof(srslte_subframe_status));
        memset(&q->cell_status[i].dci_token, 0, NOF_LOG_SF * sizeof(bool));
    }
    return 0;
}

/*********************************************************
* Reset--only reset the subframe related information
*********************************************************/
int cell_status_reset(srslte_ue_cell_usage* q)
{
    q->header       = 0;
    for(int i=0;i<MAX_NOF_CA;i++){
        q->cell_triggered[i]    = false;
        memset(&q->cell_status[i].sf_status, 0, NOF_LOG_SF * sizeof(srslte_subframe_status));
        memset(&q->cell_status[i].dci_token, 0, NOF_LOG_SF * sizeof(bool));
    }
    return 0;
}

/*********************************************************
* MAIN: dci decoder asks for dci token
*********************************************************/
int cell_status_ask_for_dci_token(srslte_ue_cell_usage* q, int ca_idx, uint32_t tti)
{
    uint16_t	index = TTI_TO_IDX(tti);
    int		nof_taken_token;
    // check how many token has been taken (the number must be smaller than nof thread)
    nof_taken_token = count_nof_taken_token(&q->cell_status[ca_idx]);
    if( nof_taken_token >= q->nof_thread[ca_idx]){
        printf("ERROR: all the token has been taken!\n");
        return -1;
    }
    // If the token number is valid, give the token to the thread 
    q->cell_status[ca_idx].dci_token[index] = true;

    //printf("ASKing DCI token: cell:%d tti:%d index:%d %d header:%d touched:%d\n", ca_idx, tti, index,index_new,
//                          q->cell_status[ca_idx].header, q->cell_status[ca_idx].dci_touched);

    // Set the dci_touched to the largest tti of the dci messages that has been touched
    if( !within_range(q->cell_status[ca_idx].header, q->cell_status[ca_idx].dci_touched, index) ){
         q->cell_status[ca_idx].dci_touched = index;
    }
    return 0;
}

/*********************************************************
* MAIN: dci decoder returns the dci token it has taken
*********************************************************/
int cell_status_return_dci_token(srslte_ue_cell_usage* q, int ca_idx, uint32_t tti,
                                    srslte_dci_subframe_t* dci_list,
				    srslte_subframe_rf_status* rf_status )
{
    //printf("Return token -- TTI:%d\n", tti);
    uint16_t index = TTI_TO_IDX(tti);

    // return the token
    q->cell_status[ca_idx].dci_token[index] = false;
    //printf("Return DCI token: ca_idx:%d tti:%d st_header:%d\n", ca_idx, tti, q->header);

    // Enqueue the dci messages
    enqueue_dci_per_subframe(&q->cell_status[ca_idx], dci_list, index, tti);

    // Enqueue the RF status
    enqueue_rf_status_per_subframe(&q->cell_status[ca_idx], rf_status, index, tti);

    // update the header of a single cell        
    update_single_cell_header(&q->cell_status[ca_idx]);

    // update the header of the whole status structure
    update_structure_header(q);
    //printf("END of return!\n");
    return 0;
}

/*********************************************************
* Functions for setting and getting the configuration 
*********************************************************/
// --> MAIN: get the targetRNTI
//uint16_t cell_status_get_targetRNTI(srslte_ue_cell_usage* q)
//{
//    return q->targetRNTI;
//}

// --> MAIN: set the nof cell we monitor
int cell_status_set_nof_cells(srslte_ue_cell_usage* q, uint16_t nof_cells)
{
    q->nof_cell = nof_cells;
    return 0;
}

// --> MAIN: set the nof PRBs of each cell 
int cell_status_set_prb(srslte_ue_cell_usage* q, uint16_t nof_prb, int ca_idx)
{
    q->max_cell_prb[ca_idx] = nof_prb;
    return 0;
}

// --> MAIN: set the nof decoding threads for monitoring each cell 
int cell_status_set_nof_thread(srslte_ue_cell_usage* q, uint16_t nof_thread, int ca_idx)
{
    q->nof_thread[ca_idx] = nof_thread;
    return 0;
}

