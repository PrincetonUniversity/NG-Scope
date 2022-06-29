#include "srsran/srsran.h"
#include <string.h>

#include "srsran/phy/ue/ngscope_st.h"

#define PDCCH_FORMAT_NOF_BITS(i) ((1 << i) * 72)

/* Copy the DCI message from the dci array to the dci per subframe struct
 * After copying, the DCI message inside the dci array will be deleted!
 */
void srsran_ngscope_tree_copy_dci_fromArray2PerSub(ngscope_dci_msg_t dci_array[][MAX_CANDIDATES_ALL],
                                        ngscope_dci_per_sub_t* dci_per_sub,
                                        int format,
                                        int idx)
{
    if( (format  < 0) || (idx<0) || (idx >= MAX_CANDIDATES_ALL)){
        ERROR("Format or IDX is invalid!\n");
        return;
    }
    if((format == 0)  ){
        if(dci_per_sub->nof_ul_dci < MAX_DCI_PER_SUB){
            // Format 0 uplink (we only record maximum of 10 message per subframe )
            dci_per_sub->ul_msg[dci_per_sub->nof_ul_dci] = dci_array[format][idx];
            dci_per_sub->ul_msg[dci_per_sub->nof_ul_dci].format = SRSRAN_DCI_FORMAT0;
            dci_per_sub->nof_ul_dci++;
        }
    }else{
        if(dci_per_sub->nof_dl_dci < MAX_DCI_PER_SUB){
            // Downlink dci messages (we only record maximum of 10 message per subframe )
            dci_per_sub->dl_msg[dci_per_sub->nof_dl_dci] = dci_array[format][idx];
            dci_per_sub->dl_msg[dci_per_sub->nof_dl_dci].format = ngscope_index_to_format(format);
            dci_per_sub->nof_dl_dci++;
        }
    }
    ZERO_OBJECT(dci_array[format][idx]);
    return;
}


int ngscope_tree_block_left_child_idx(int par_idx){
    return 2*par_idx + 1;
}

int ngscope_tree_block_right_child_idx(int par_idx){
    return 2*par_idx + 2;
}


int ngscope_tree_subBlock_right_child_idx(int par_idx){
    return 2*par_idx + 2;
}
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

static int match_two_dci_vec(ngscope_dci_msg_t dci_array[][MAX_CANDIDATES_ALL], int root, int child, int*  matched_format){
    int nof_matched = 0;
    for(int i=0;i<MAX_NOF_FORMAT+1;i++){
        //printf("ROOT-RNTI:%d Child-RNTI:%d ",dci_array[i][root].rnti, dci_array[i][child].rnti);
        if( (dci_array[i][root].rnti > 0) && (dci_array[i][child].rnti > 0)){
            if(dci_array[i][root].rnti == dci_array[i][child].rnti){
                matched_format[nof_matched] = i;
                nof_matched++;
                //return i; 
            }
        }
    }
    //printf("\n");
    return nof_matched; 
}
/* Child-parent matching process -->
 * If the RNTI (acutally dci will be more accurate) of the child and parent matches
 * the decoding is correct !
 * return the matched index of the index and aggregation level of the parent node
 */
void srsran_ngscope_tree_CP_match(ngscope_dci_msg_t dci_array[][MAX_CANDIDATES_ALL], 
                                        int nof_location, 
                                        int blk_idx, 
                                        int loc_idx,
                                        int* nof_matched_dci,
                                        int* root_idx, 
                                        int* format_idx)
{
    int start_idx   = blk_idx * 15; 
    int nof_matched = 0; 
    int root;
    int left_child;
    // In a tree, only the node with idx 0-6 can be parent node
    for(int i=0;i<7;i++){
        root       = start_idx + i;
        left_child = start_idx + 2*i + 1;
        //printf("root:%d child:%d\n", root, left_child);

        // we haven't decoded the root or child yet
        if( (root >loc_idx) || (left_child > loc_idx)){
            break;
        }
        //int matched_format[MAX_NOF_FORMAT+1] = {0};
        nof_matched = match_two_dci_vec(dci_array, root, left_child, format_idx);
        if(nof_matched > 0){
            //printf("FIND MATACH!\n");
            break;
        }
    }
    if(nof_matched > 0){
        //printf("FIND MATACH!\n");
        //*format_idx = matched_ret; 
        *nof_matched_dci    = nof_matched;
        *root_idx           = root;
   }

    return;
}

int  srsran_ngscope_tree_prune_node(ngscope_dci_msg_t dci_array[][MAX_CANDIDATES_ALL], 
                                        int nof_matched,
                                        int root, 
                                        int* format_vec,
                                        int* format_idx)
{
    int nof_format = nof_matched;
    // count number of format 0 and 4 
    int cnt = 0;
    int idx[2] = {0};
    for(int i=0; i<nof_format; i++){
        if( (format_vec[i] == 0) || (format_vec[i] == 4)){
            //idx[cnt] = i; 
            idx[cnt] = format_vec[i]; 
            cnt++;
        }
    } 
    if(cnt == 1){
        /* If there is only one dci with format 0 or 4, return it */
        *format_idx = idx[0];
        return 1;
    }else if(cnt == 2){
        /* If there are two dcis with format 0 or 4, pick the high corr*/
        *format_idx = dci_array[idx[0]][root].corr > dci_array[idx[1]][root].corr ? idx[0]:idx[1];
        return 1; 
    }else if(cnt == 0){
        /* if there are multiple non format 0/4 dcis, pick the one with the highest corr */
        int tmp_idx = 0;
        float tmp_corr = 0;
        for(int i=0; i<nof_format; i++){
            if( dci_array[format_vec[i]][root].corr >= tmp_corr){
                tmp_corr = dci_array[format_vec[i]][root].corr;
                tmp_idx  = format_vec[i];
            }
        } 
        *format_idx = tmp_idx;
        return 1;
    }else{
        printf("ERROR: The number format 0/4 must be in the range of [0, 2]!\n");
        return 0;
    }
    return 0;        
}

bool is_leaf_node(int index_in_tree){
    if( (index_in_tree >= 7) && (index_in_tree <= 14)){
        return true;
    }else{
        return false;
    }
}

bool check_node(srsran_dci_location_t dci_location[MAX_CANDIDATES_ALL],
                    int start_idx,
                    int idx_in_tree);

bool check_node(srsran_dci_location_t dci_location[MAX_CANDIDATES_ALL],
                    int start_idx,
                    int idx_in_tree)
{
    dci_location[start_idx + idx_in_tree].checked = true; 
    if(is_leaf_node(idx_in_tree)){
        // if current node is the leaf node return true
        return true;
    }else{
        // check left child
        check_node(dci_location, start_idx, 2 * idx_in_tree +1);
        // check right child
        check_node(dci_location, start_idx, 2 * idx_in_tree +2);
    }
    return true; 
}

/*  Check the all the child nodes, if the root is identified as matched node 
 *  By checking the nodes, we save the effort to decode them
 */
int srsran_ngscope_tree_check_nodes(srsran_dci_location_t dci_location[MAX_CANDIDATES_ALL],
                                    int index)
{
    int start_idx   = (index / 15) * 15;
    int idx_in_tree = index % 15;
    check_node(dci_location, start_idx, idx_in_tree); 
    return SRSRAN_SUCCESS;
}
                                   

bool clear_dciArray_child_node(ngscope_dci_msg_t dci_array[][MAX_CANDIDATES_ALL],
                    int start_idx,
                    int idx_in_tree);

bool clear_dciArray_child_node(ngscope_dci_msg_t dci_array[][MAX_CANDIDATES_ALL],
                    int start_idx,
                    int idx_in_tree)
{
    for(int i=0; i<MAX_NOF_FORMAT+1;i++){
        ZERO_OBJECT(dci_array[i][start_idx + idx_in_tree]); 
    }
    if(is_leaf_node(idx_in_tree)){
        // if current node is the leaf node return true
        return true;
    }else{
        // clear left child
        clear_dciArray_child_node(dci_array, start_idx, 2 * idx_in_tree +1);
        // clear right child
        clear_dciArray_child_node(dci_array, start_idx, 2 * idx_in_tree +2);
    }
    return true; 
}

bool clear_dciArray_parent_node(ngscope_dci_msg_t dci_array[][MAX_CANDIDATES_ALL],
                    int start_idx,
                    int idx_in_tree);

bool clear_dciArray_parent_node(ngscope_dci_msg_t dci_array[][MAX_CANDIDATES_ALL],
                    int start_idx,
                    int idx_in_tree)
{
    for(int i=0; i<MAX_NOF_FORMAT+1;i++){
        ZERO_OBJECT(dci_array[i][start_idx + idx_in_tree]); 
    }
    if(idx_in_tree == 0){
        // if current node is the rrot node return true
        return true;
    }else{
        // clear parent node
        int par_idx = (idx_in_tree-1)/2;
        clear_dciArray_parent_node(dci_array, start_idx, par_idx);
    }
    return true; 
}

/* When one node is identified as matched node, we should copy the decoded dci messaged to output
 * After the copying, we should remove the message from the dci_array
 * Since the dci_array should only contain those uncertain messages to be ideintified
 */
int srsran_ngscope_tree_clear_dciArray_nodes(ngscope_dci_msg_t dci_array[][MAX_CANDIDATES_ALL],
                                                int index)
{
    int start_idx   = (index / 15) * 15;
    int idx_in_tree = index % 15;
    // Clear the child nodes
    clear_dciArray_child_node(dci_array, start_idx, idx_in_tree); 
    // Clear the parent nodes
    clear_dciArray_parent_node(dci_array, start_idx, idx_in_tree); 

    return SRSRAN_SUCCESS;
}

bool is_empty_node(ngscope_dci_msg_t dci_array[][MAX_CANDIDATES_ALL], 
                        int index){
    bool ret = true;
    for(int i=0; i<MAX_NOF_FORMAT+1; i++){
        if(dci_array[i][index].rnti != 0){
            ret = false; 
            break;
        } 
    }
    return ret;
}

/* Count the number of non empty nodes in the dci array */ 
int srsran_ngscope_tree_non_empty_nodes(ngscope_dci_msg_t dci_array[][MAX_CANDIDATES_ALL], 
                                            int nof_locations){
    int nof_node = 0;
    for(int i=0; i<nof_locations; i++){
        if(is_empty_node(dci_array, i) == false){
            nof_node ++;
        }
    }
    return nof_node;
}

bool is_empty_node_regarding_llr(srsran_dci_location_t dci_location[MAX_CANDIDATES_ALL],
                                    int index){
    if(dci_location[index].mean_llr < LLR_RATIO){
        return true;
    }else{
        return false;
    }
    return true;
}

bool is_solo_leaf_node(ngscope_dci_msg_t        dci_array[][MAX_CANDIDATES_ALL], 
                        srsran_dci_location_t   dci_location[MAX_CANDIDATES_ALL],
                        int                     index){
    int idx_in_tree = index % 15;
    
    if(is_empty_node_regarding_llr(dci_location, index) ||
                    is_empty_node(dci_array, index) ){
        // empty node cannot be solo node
        return false;
    }
        
    if( (idx_in_tree % 2) == 0){
        // node with even index
        if(is_empty_node_regarding_llr(dci_location, index-1)){
            // its left silbing is empty
            return true;
        }
    }else{
        // node with odd index
        if(is_empty_node_regarding_llr(dci_location, index+1)){
            // its right silbing is empty
            return true;
        }
    }
    return false;
}
int srsran_ngscope_pick_single_location(ngscope_dci_msg_t        dci_array[][MAX_CANDIDATES_ALL],
                                        int loc_idx){
    for(int i=0;i<MAX_NOF_FORMAT+1;i++){
        if(dci_array[i][loc_idx].decode_prob == 100){
            return i;
        }
    }
    return -1;
}

int srsran_ngscope_tree_solo_nodes(ngscope_dci_msg_t        dci_array[][MAX_CANDIDATES_ALL],
                                    srsran_dci_location_t   dci_location[MAX_CANDIDATES_ALL],
                                    ngscope_dci_per_sub_t*  dci_per_sub,
                                    int                     nof_location){
    for(int i=0; i<nof_location; i++){
        int idx_in_tree = i % 15;
        // we only check for leaf node 
        if( (idx_in_tree >= 7) && (idx_in_tree <= 14)){
            if(is_solo_leaf_node(dci_array, dci_location, i)){
                printf("Find one solo leaf node inside the tree!\n");
                
                // copy the matched dci message to the results
                //srsran_ngscope_tree_copy_dci_fromArray2PerSub(dci_array, dci_per_sub, matched_format, matched_root);

                // delete the messages in the dci array 
                //srsran_ngscope_tree_clear_dciArray_nodes(dci_array, matched_root);

            }
        }
    }
    return SRSRAN_SUCCESS;
}

void srsran_ngscope_tree_plot_multi(ngscope_dci_msg_t      dci_array[][MAX_CANDIDATES_ALL],
                                    srsran_dci_location_t  dci_location[MAX_CANDIDATES_ALL],
                                    uint32_t nof_location)
{
  int loc_idx = 0;
  while(loc_idx < nof_location){
    for(int i=0;i<15;i++){
        printf("|%d %d %.2f| ", dci_location[loc_idx].L, dci_location[loc_idx].ncce, dci_location[loc_idx].mean_llr);
        for(int j=0;j<MAX_NOF_FORMAT+1;j++){
            printf("{%d %d %1.2f %1.2f}-", dci_array[j][loc_idx].rnti, dci_array[j][loc_idx].prb,
                        dci_array[j][loc_idx].decode_prob, dci_array[j][loc_idx].corr);
        }
        //if( (i==0) || (i==2) || (i==6)) printf("\n");
        if( (i%2 == 0) ) printf("\n");

        loc_idx++;
        if(loc_idx>= nof_location){
            break;
        }
    }
    printf("\n\n");
  }
  //printf("\n");
}
