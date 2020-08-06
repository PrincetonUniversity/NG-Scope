#include <sys/types.h>
#include <sys/socket.h>

#include "srslte/phy/ue/ue_list.h"
#include "srslte/phy/ue/lte_scope.h"
#include "srslte/phy/ue/overhead.h"
#include "srslte/phy/ue/LTESCOPE_GLOBAL.h"

#include "ue_cell_status.h"
#define TTI_TO_IDX(i) (i%NOF_LOG_SF)

extern bool all_RF_ready;
extern pthread_mutex_t mutex_RF_ready;


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

// --> MAIN: enqueue the downlink/uplink and update the cell utilization 
static int enqueue_csi_per_subframe(srslte_cell_status* q, srslte_ue_dl_t* ue_dl, 
				    uint16_t index, uint32_t tti,
				    int nof_tx, int nof_rx, int nof_prb)
{
   // we need all the tx antennas are all set
    pthread_mutex_lock(&mutex_RF_ready);
    if(all_RF_ready == false){
        pthread_mutex_unlock(&mutex_RF_ready);
        return 1;
    }
    pthread_mutex_unlock(&mutex_RF_ready);

 
    q->sf_status[index].tti = tti;

    float *csi_amp;
    float *csi_phase;

    //csi_amp	= q->sf_status[index].csi_amp; 
    //csi_phase	= q->sf_status[index].csi_phase; 

    //printf("Enter loop:%f \n",q->sf_status[index].csi_amp[0][0][0]);

    for(int i=0;i<nof_tx;i++){
	for(int j=0;j<nof_rx;j++){
	    csi_amp	= q->sf_status[index].csi_amp[i][j]; 
	    csi_phase	= q->sf_status[index].csi_phase[i][j]; 

	    cf_t* csi_ptr   = ue_dl->ce_m[i][j];	
	    //printf("finally\n");
	    for(int k=0;k<nof_prb*12;k++){
		//printf("index:%d i:%d j:%d k:%d ", index, i, j, k);
		//printf("amp:%f phase:%f \n",  csi_amp[k], csi_phase[k]);
		csi_amp[k]    = cabsf(csi_ptr[k]);
		csi_phase[k]  = cargf(csi_ptr[k]);
	    }
	    //printf("\n");
	}
    }
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

    //uint16_t current_header = q->header;	// current header
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


// Allocate memory for 3d array
static float*** alloc_3d_array(int n1, int n2, int n3)
{
    float ***array;
    printf("n1:%d n2:%d n3:%d\n",n1, n2, n3);
    array = (float***)malloc(n1*sizeof(float**));
    for(int i=0;i<n1;i++){
	array[i] = (float **)malloc(n2*sizeof(float *));
	for(int j=0; j<n2; j++){
	    array[i][j]	= (float*)malloc(n3*sizeof(float));
	    for(int k=0; k<n3; k++){
		array[i][j][k]	= 0; 
	    }
	}
    }
    return array;
}

// Free memory for 3d array
static void free_3d_array(float ***array, int n1, int n2, int n3)
{
    for(int i=0;i<n1;i++){
	for(int j=0;j<n2;j++){
	    free(array[i][j]);
	} 
    }
    for(int i=0;i<n1;i++){
	free(array[i]);
    }
    free(array);
}

// structure init and reset 
/*********************************************************
// Init-- initialize the structure 
*********************************************************/

//--> MAIN init the structure
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
	q->nof_tx_ant[i]	= 0;
	q->nof_rx_ant[i]	= 0;

        q->nof_thread[i]        = 0;

        memset(&q->cell_status[i].dci_token, 0, NOF_LOG_SF * sizeof(bool));
	for(int j=0;j<NOF_LOG_SF;j++){
	    q->cell_status[i].sf_status[i].tti = 0;
		
	    //q->cell_status[i].sf_status[i].csi_amp = NULL;
	    //q->cell_status[i].sf_status[i].csi_phase = NULL;
	    for(int m=0;m<4;m++){
		for(int n=0;n<4;n++){
		    q->cell_status[i].sf_status[i].csi_amp[m][n] = NULL;
		    q->cell_status[i].sf_status[i].csi_phase[m][n] = NULL;
		}
	    }
	}
    }
    return 0;
}

/*********************************************************
* MAIN: allocate memory for csi matrix
*********************************************************/
int cell_status_allocate_csi_matrix(srslte_ue_cell_usage* q)
{
    printf("ALLOCATE memory for 3d array\n");
    int nof_cell    = q->nof_cell;
    int nof_rx_ant, nof_tx_ant, nof_prb;

    for(int i=0;i<nof_cell;i++){
	nof_rx_ant  = q->nof_rx_ant[i];
	nof_tx_ant  = q->nof_tx_ant[i];
	nof_prb	    = q->max_cell_prb[i];
	//printf("cellIdx:%d rx_ant:%d tx_ant:%d nof_prb:%d\n", i, nof_rx_ant, nof_tx_ant, nof_prb);	
	for(int j=0;j<NOF_LOG_SF;j++){
	    //q->cell_status[i].sf_status[j].csi_amp = alloc_3d_array(nof_tx_ant, nof_rx_ant, nof_prb * 12);
	    for(int tx_idx=0;tx_idx<nof_tx_ant;tx_idx++){ 
		for(int rx_idx=0;rx_idx<nof_rx_ant;rx_idx++){ 
		    //printf("tx_idx:%d rx_idx:%d\n",tx_idx, rx_idx);
		    q->cell_status[i].sf_status[j].csi_amp[tx_idx][rx_idx]	= calloc(1,nof_prb*12*sizeof(float));
		    q->cell_status[i].sf_status[j].csi_phase[tx_idx][rx_idx]	= calloc(1,nof_prb*12*sizeof(float));
		}
	    }
	}
    }
    return 0;
}

/*********************************************************
* MAIN: exit and free memory for csi matrix
*********************************************************/
int cell_status_exit_free_csi_matrix(srslte_ue_cell_usage* q)
{
    int nof_cell    = q->nof_cell;
    int nof_rx_ant, nof_tx_ant, nof_prb;

    for(int i=0;i<nof_cell;i++){
	nof_rx_ant  = q->nof_rx_ant[i];
	nof_tx_ant  = q->nof_tx_ant[i];
	nof_prb	    = q->max_cell_prb[i];

	for(int j=0;j<NOF_LOG_SF;j++){
	    //free_3d_array(q->cell_status[i].sf_status[j].csi_amp, nof_tx_ant, nof_rx_ant, nof_prb * 12);
	    for(int rx_idx=0;rx_idx<nof_tx_ant;rx_idx++){ 
		for(int tx_idx=0;tx_idx<nof_rx_ant;tx_idx++){ 
		    free(q->cell_status[i].sf_status[j].csi_amp[tx_idx][rx_idx]);
		    free(q->cell_status[i].sf_status[j].csi_phase[tx_idx][rx_idx]);
		}
	    }
	}
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
        //memset(&q->cell_status[i].sf_status, 0, NOF_LOG_SF * sizeof(srslte_subframe_status));
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
int cell_status_return_dci_token(srslte_ue_cell_usage* q, srslte_ue_dl_t* ue_dl, uint16_t ca_idx, uint32_t tti)
{
    //printf("Return token -- TTI:%d\n", tti);
    uint16_t index = TTI_TO_IDX(tti);

    // return the token
    q->cell_status[ca_idx].dci_token[index] = false;
    //printf("Return DCI token: ca_idx:%d tti:%d st_header:%d\n", ca_idx, tti, q->header);

    // Enqueue the csi messages
    enqueue_csi_per_subframe(&q->cell_status[ca_idx], ue_dl, index, tti, 
				q->nof_tx_ant[ca_idx], q->nof_rx_ant[ca_idx], q->max_cell_prb[ca_idx]);
    //printf("enqueue csi per_subframe\n");

    // update the header of a single cell        
    update_single_cell_header(&q->cell_status[ca_idx]);
    //printf("update the header of single cell\n");

    // update the header of the whole status structure
    update_structure_header(q);
    //printf("update the header of the whole structure\n");

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

// --> MAIN: set the nof rx antennas of each usrp
int cell_status_set_rx_ant(srslte_ue_cell_usage* q, uint16_t nof_rx_ant, int ca_idx)
{
    q->nof_rx_ant[ca_idx] = nof_rx_ant;
    return 0;
}

// --> MAIN: set the nof tx antennas of each usrp
int cell_status_set_tx_ant(srslte_ue_cell_usage* q, uint16_t nof_tx_ant, int ca_idx)
{
    q->nof_tx_ant[ca_idx] = nof_tx_ant;
    return 0;
}


// --> MAIN: set the nof decoding threads for monitoring each cell 
int cell_status_set_nof_thread(srslte_ue_cell_usage* q, uint16_t nof_thread, int ca_idx)
{
    q->nof_thread[ca_idx] = nof_thread;
    return 0;
}

