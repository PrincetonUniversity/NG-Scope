#include "srsran/srsran.h"
#include <string.h>

#include "srsran/phy/ue/ngscope_st.h"

int srsran_pdcch_get_nof_location_yx(srsran_pdcch_t* q, uint32_t cfi){
    return ((cfi>0&&cfi<4)?q->nof_cce[cfi-1]:0);
}

/* Generate all the possible search space that the control channel has
 * The q is necessary since we need the NOF_CCE
 */
uint32_t srsran_ngscope_search_space_all_yx(srsran_pdcch_t* q, uint32_t cfi, srsran_dci_location_t* c)
{
    int nof_location = srsran_pdcch_get_nof_location_yx(q, cfi);
    //printf("nof cce:%d ->| ", NOF_CCE(cfi));
    uint32_t i, L, k;

    int l;
    k = 0;
    for (l = 3; l >= 0; l--) {
        L = (1 << l);
        for (i = 0; i < nof_location / (L); i++) {
            int ncce = (L) * (i % (nof_location / (L)));
            c[k].L = l;
            c[k].ncce = ncce;
            k++;
        }
    }
    return k;
}

/*calculate the mean llr */
static float mean_llr(srsran_pdcch_t* q, int ncce, int l){
    float mean = 0;
    for (int j=0;j<PDCCH_FORMAT_NOF_BITS(l);j++) {
        mean += fabsf(q->llr[ncce * 72 + j]);
    }
    mean /= PDCCH_FORMAT_NOF_BITS(l);
    return mean;
}


void check_node_based_on_llr(srsran_dci_location_t dci_location[MAX_CANDIDATES_ALL],
                                        int nof_location)
{
    int nof_blk     = nof_location / 15;
    for(int i=0; i<nof_blk; i++){
        int start_idx = i * 15;
        for(int j=7; j<15; j++){
            // let's first check all the leaf nodes inside a tree
            if(dci_location[start_idx+j].mean_llr < LLR_RATIO){ 
                // we skip the location if the llr is too small
                dci_location[start_idx+j].checked = true; 
            }
        }
        // set the third level of the tree
        for(int j=3; j<7; j++){
            int idx = start_idx+j;
            int left_child  = 2 * idx + 1; 
            int right_child = 2 * idx + 2;
            if( dci_location[left_child].checked || dci_location[right_child].checked){
                dci_location[idx].checked = true;
            }
        }
        // set the second level of the tree
        for(int j=1; j<3; j++){
            int idx = start_idx+j;
            int left_child  = 2 * idx + 1; 
            int right_child = 2 * idx + 2;
            if( dci_location[left_child].checked || dci_location[right_child].checked){
                dci_location[idx].checked = true;
            }
        }
        // set the root 
        int j = 0; 
        int idx = start_idx+j;
        int left_child  = 2 * idx + 1; 
        int right_child = 2 * idx + 2;
        if( dci_location[left_child].checked || dci_location[right_child].checked){
            dci_location[idx].checked = true;
        }
    }
    return;
}

/*****************************************************************************************
 * Each block is a combination of 8 CCEs.
 * Depending on the aggregation level L, we have 8 L=0, 4 L=1, 2 L=2, 1 L=8
 * Therefore, each block contains 15 possible locations
*****************************************************************************************/
uint32_t srsran_ngscope_search_space_block_yx(srsran_pdcch_t* q, uint32_t cfi, srsran_dci_location_t* c)
{
    uint32_t nof_location = srsran_pdcch_get_nof_location_yx(q, cfi);

    // nof full blocks
    int nof_L3 = nof_location / 8;          

    // nof total blocks include blocks that are not full
    int nof_blk = (int)ceil((double)nof_location / 8); 

    // handle the full blocks
    for(int i=0; i < nof_blk; i++){
        c[i*15].L = 3;
        c[i*15].ncce = 8 * i;
        c[i*15].mean_llr = mean_llr(q, c[i*15].ncce, c[i*15].L);

        for(int j=0; j<2; j++){
            c[i*15+j+1].L       = 2;
            c[i*15+j+1].ncce    = 8 * i + 4 * j;
            c[i*15+j+1].mean_llr = mean_llr(q, c[i*15+j+1].ncce, c[i*15+j+1].L);
        }
        for(int j=0; j<4; j++){
            c[i*15+j+3].L       = 1;
            c[i*15+j+3].ncce    = 8 * i + 2 * j;
            c[i*15+j+3].mean_llr = mean_llr(q, c[i*15+j+3].ncce, c[i*15+j+3].L);
        }
        for(int j=0; j<8; j++){
            c[i*15+j+7].L       = 0;
            c[i*15+j+7].ncce    = 8 * i + j;
            c[i*15+j+7].mean_llr = mean_llr(q, c[i*15+j+7].ncce, c[i*15+j+7].L);
        }
    }

    for(int i=0;i<nof_blk*15;i++){
        c[i].checked = false;
    }
    
    /* now mark those invalide location */
      
    int rem = nof_location % 8;
    int loc_idx = nof_L3 * 15;
    //int cce_idx = 8 * nof_L3;
    //printf("nof_cce:%d nof_L3:%d nof_blk:%d total:%d rem:%d\n", nof_location, nof_L3, nof_blk, nof_blk * 15, rem); 

    if(rem != 0){
       // set locations with L =3 this will always be true
       c[loc_idx].checked = true;
           
       // set L = 0
       for(int i=0; i<8; i++){
           if(c[loc_idx + 7 + i].ncce >= nof_location){
               c[loc_idx + 7 + i].checked = true;
           }
       }
       // set locations with L = 1
       for(int i=0; i<4; i++){
           int idx = 3 + i;

           if( (c[loc_idx + 2*idx +1]. checked == false) && (c[loc_idx + 2*idx +2]. checked == false)){
               c[loc_idx + idx].checked = false; 
           }else{
               c[loc_idx + idx].checked = true; 
           }
       }
       // set locations with L = 2
       for(int i=0; i<2; i++){
           int idx = 1 + i;
           if( (c[loc_idx + 2*idx +1]. checked == false) && (c[loc_idx + 2*idx +2]. checked == false)){
               c[loc_idx + idx].checked = false; 
           }else{
               c[loc_idx + idx].checked = true; 
           }
       }
    }
    //check_node_based_on_llr(c, nof_blk * 15);
    return nof_blk * 15; 
}


