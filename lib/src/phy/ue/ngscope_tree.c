#include "srsran/srsran.h"
#include <string.h>

#include "srsran/phy/ue/ngscope_st.h"
#include "srsran/phy/ue/ngscope_dci.h"

//#define PDCCH_FORMAT_NOF_BITS(i) ((1 << i) * 72)


/* Copy the DCI message from the dci array to the dci per subframe struct
 * After copying, the DCI message inside the dci array will be deleted!
 */
void srsran_ngscope_tree_copy_dci_fromArray2PerSub(ngscope_tree_t* q,
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
            dci_per_sub->ul_msg[dci_per_sub->nof_ul_dci] 		= q->dci_array[format][idx];
            dci_per_sub->ul_msg[dci_per_sub->nof_ul_dci].format = SRSRAN_DCI_FORMAT0;
            dci_per_sub->nof_ul_dci++;
			//if(dci_array[format][idx].rnti == 2867){
			//	printf("COPY RNTI: L:%d ncce:%d\n", dci_array[format][idx].loc.L, dci_array[format][idx].loc.ncce);
			//}
        }
    }else{
        if(dci_per_sub->nof_dl_dci < MAX_DCI_PER_SUB){
            // Downlink dci messages (we only record maximum of 10 message per subframe )
            dci_per_sub->dl_msg[dci_per_sub->nof_dl_dci] 		= q->dci_array[format][idx];
            dci_per_sub->dl_msg[dci_per_sub->nof_dl_dci].format = ngscope_index_to_format(format);
            dci_per_sub->nof_dl_dci++;
			//if(dci_array[format][idx].rnti == 2867){
			//	printf("COPY RNTI: L:%d ncce:%d\n", dci_array[format][idx].loc.L, dci_array[format][idx].loc.ncce);
			//}
        }
    }
    ZERO_OBJECT(q->dci_array[format][idx]);

    return;
}


/******************************************************** 
* Child-Parent Matching
*********************************************************/
int ngscope_tree_block_left_child_idx(int par_idx){
    return 2*par_idx + 1;
}

int ngscope_tree_block_right_child_idx(int par_idx){
    return 2*par_idx + 2;
}


int ngscope_tree_subBlock_right_child_idx(int par_idx){
    return 2*par_idx + 2;
}

static int match_two_dci_vec(ngscope_dci_msg_t dci_array[][MAX_CANDIDATES_ALL], 
								int 		root, 
								int 		child,
								uint16_t 	targetRNTI,
								int*  		matched_format,
								int* 		matched_root){
    int nof_matched = 0;
    for(int i=0;i<MAX_NOF_FORMAT+1;i++){
        //printf("ROOT-RNTI:%d Child-RNTI:%d ",dci_array[i][root].rnti, dci_array[i][child].rnti);
		if(dci_array[i][root].rnti == targetRNTI){
				matched_format[nof_matched] = i;
                nof_matched 	= 1;
				*matched_root 	= root;
				break;
		}else if(dci_array[i][child].rnti == targetRNTI){
				matched_format[nof_matched] = i;
                nof_matched 	= 1;
				*matched_root 	= child;
				break;
		}else if( (dci_array[i][root].rnti > 0) && (dci_array[i][child].rnti > 0)){
            if(dci_array[i][root].rnti == dci_array[i][child].rnti){
                matched_format[nof_matched] = i;
                nof_matched++;
				*matched_root 	= root;
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
void srsran_ngscope_tree_CP_match(ngscope_tree_t* q,
//ngscope_dci_msg_t dci_array[][MAX_CANDIDATES_ALL], 
                                        //int nof_location, 
                                        int blk_idx, 
                                        int loc_idx,
										uint16_t targetRNTI,
                                        int* nof_matched_dci,
                                        int* root_idx, 
                                        int* format_idx)
{
    int start_idx   = blk_idx * 15; 
    int nof_matched = 0; 
    int root;
    int left_child;
	int matched_root = 0;
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
        nof_matched = match_two_dci_vec(q->dci_array, root, left_child, targetRNTI, format_idx, &matched_root);
        //nof_matched = match_two_dci_vec(dci_array, root, left_child, format_idx, &matched_root);
        if(nof_matched > 0){
            //printf("FIND MATACH!\n");
            break;
        }
    }
    if(nof_matched > 0){
        //printf("FIND MATACH!\n");
        //*format_idx = matched_ret; 
        *nof_matched_dci    = nof_matched;
        //*root_idx           = root;
        *root_idx           = matched_root;
   	}
	return;
}

/******************************************************** 
* We should only decode one dci from each node a tree
*********************************************************/
int  srsran_ngscope_tree_prune_node(ngscope_tree_t* q,
                                        int nof_matched,
                                        int root, 
										uint16_t targetRNTI,
                                        int* format_vec,
                                        int* format_idx)
{
    int nof_format = nof_matched;
    // count number of format 0 and 4 
    int cnt = 0;
    int idx[2] = {0};

    for(int i=0; i<nof_format; i++){
		if(q->dci_array[format_vec[i]][root].rnti == targetRNTI){
			*format_idx = format_vec[i];
			return 1;
		}
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
        *format_idx = q->dci_array[idx[0]][root].corr > q->dci_array[idx[1]][root].corr ? idx[0]:idx[1];
        return 1; 
    }else if(cnt == 0){
        /* if there are multiple non format 0/4 dcis, pick the one with the highest corr */
        int tmp_idx = 0;
        float tmp_corr = 0;
        for(int i=0; i<nof_format; i++){
            if( q->dci_array[format_vec[i]][root].corr >= tmp_corr){
                tmp_corr = q->dci_array[format_vec[i]][root].corr;
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

/******************************************************** 
* Check the node, including this node and its parent 
*********************************************************/
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
int srsran_ngscope_tree_check_nodes(ngscope_tree_t* q,
                                    int index)
{
    int start_idx   = (index / 15) * 15;
    int idx_in_tree = index % 15;
    check_node(q->dci_location, start_idx, idx_in_tree); 
    return SRSRAN_SUCCESS;
}

/******************************************************** 
* Clear array nodes, including its child and its parent
*********************************************************/
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
        // if current node is the root node return true
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
int srsran_ngscope_tree_clear_dciArray_nodes(ngscope_tree_t* q, int index)
{
    int start_idx   = (index / 15) * 15;
    int idx_in_tree = index % 15;
	for(int i=0; i<MAX_NOF_FORMAT+1;i++){
        ZERO_OBJECT(q->dci_array[i][index]); 
    }
    // Clear the child nodes
    clear_dciArray_child_node(q->dci_array, start_idx, idx_in_tree); 
    // Clear the parent nodes
    clear_dciArray_parent_node(q->dci_array, start_idx, idx_in_tree); 

    return SRSRAN_SUCCESS;
}
/******************************************************** 
* Clear array nodes, including its child and its parent
*********************************************************/

bool is_empty_node(ngscope_dci_msg_t dci_array[][MAX_CANDIDATES_ALL], int index){
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
int srsran_ngscope_tree_non_empty_nodes(ngscope_tree_t* q){
    int nof_node = 0;
    for(int i=0; i<q->nof_location; i++){
        if(is_empty_node(q->dci_array, i) == false){
            nof_node ++;
        }
    }
    return nof_node;
}


int srsran_ngscope_tree_prune_tree(ngscope_tree_t* q){
	for(int i=0; i<MAX_NOF_FORMAT+1; i++){
		for(int j=0; j<q->nof_location; j++){
			if(q->dci_array[i][j].corr < 0.5){
				ZERO_OBJECT(q->dci_array[i][j]);
				continue;
			}
			if(q->dci_array[i][j].decode_prob < 75){
				ZERO_OBJECT(q->dci_array[i][j]);
				continue;
			}
		}
	}
	return 0;
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

int srsran_ngscope_tree_solo_nodes(ngscope_tree_t* q, ngscope_dci_per_sub_t*  dci_per_sub){
    for(int i=0; i<q->nof_location; i++){
        int idx_in_tree = i % 15;
        // we only check for leaf node 
        if( (idx_in_tree >= 7) && (idx_in_tree <= 14)){
            if(is_solo_leaf_node(q->dci_array, q->dci_location, i)){
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

void srsran_ngscope_tree_plot_loc(ngscope_tree_t* q){
	printf("NOF_LOC:%d --> ", q->nof_location);
	for(int i=0; i<q->nof_location; i++){
		printf("%d %d %f |", q->dci_location[i].L, q->dci_location[i].ncce, q->dci_location[i].mean_llr);
	}
	printf("\n\n");
}
void srsran_ngscope_tree_plot_multi(ngscope_tree_t* q)
{
  int loc_idx = 0;
  while(loc_idx < q->nof_location){
    for(int i=0;i<15;i++){
        printf("|%d %d %.2f| ", q->dci_location[loc_idx].L, q->dci_location[loc_idx].ncce, q->dci_location[loc_idx].mean_llr);
        for(int j=0;j<MAX_NOF_FORMAT+1;j++){
            printf("{%d %d %1.2f %1.2f}-", q->dci_array[j][loc_idx].rnti, q->dci_array[j][loc_idx].prb,
                        q->dci_array[j][loc_idx].decode_prob, q->dci_array[j][loc_idx].corr);
        }
        //if( (i==0) || (i==2) || (i==6)) printf("\n");
        if( (i%2 == 0) ) printf("\n");

        loc_idx++;
        if(loc_idx>= q->nof_location){
            break;
        }
    }
    printf("\n\n");
  }
  //printf("\n");
}


/*    Tree related operations */
// Init the tree
int ngscope_tree_init(ngscope_tree_t* q){
	for(int i=0; i<MAX_NOF_FORMAT+1; i++){
		for(int j=0; j<MAX_CANDIDATES_ALL; j++){
			ZERO_OBJECT(q->dci_array[i][j]);
		}
	}

	for(int i=0; i<MAX_CANDIDATES_ALL; i++){
		ZERO_OBJECT(q->dci_location[i]);
	}
	q->nof_location = 0;
	q->nof_cce 		= 0;
	return 0;
}

// set the searching space
int ngscope_tree_set_locations(ngscope_tree_t* q, srsran_pdcch_t* pdcch, uint32_t cfi){
	q->nof_location = srsran_ngscope_search_space_block_yx(pdcch, cfi, q->dci_location);
	return q->nof_location;
}

int ngscope_tree_set_cce(ngscope_tree_t* q, int nof_cce){
	q->nof_cce = nof_cce;
	return 1;
}
int srsran_ngscope_tree_find_rnti_range(ngscope_tree_t* q,
										int 	 loc_idx,
										uint16_t rnti_min,
										uint16_t rnti_max)
{
	for(int j=0; j<MAX_NOF_FORMAT+1; j++){
		if( (q->dci_array[j][loc_idx].rnti >= rnti_min) && 
				(q->dci_array[j][loc_idx].rnti <= rnti_max)){
			return j;
		}
	}
    return -1;
}


ngscope_dci_msg_t srsran_ngscope_tree_find_rnti(ngscope_tree_t* q,
											uint16_t rnti)
{
	ngscope_dci_msg_t ret;
	ZERO_OBJECT(ret);

    for(int i=0; i<q->nof_location; i++){
    	for(int j=0; j<MAX_NOF_FORMAT+1; j++){
			if(q->dci_array[j][i].rnti == rnti){
				q->dci_array[j][i].format = ngscope_index_to_format(j);
				return q->dci_array[j][i];
			}
        }
    }
    return ret;
}

int srsran_ngscope_tree_copy_rnti(ngscope_tree_t*   		q,
                                 ngscope_dci_per_sub_t* 	dci_per_sub,
								 uint16_t 					rnti)
{
	int ret = 0;
    for(int i=0; i<q->nof_location; i++){
    	for(int j=0; j<MAX_NOF_FORMAT+1; j++){
			if(q->dci_array[j][i].rnti == rnti){
				srsran_ngscope_tree_copy_dci_fromArray2PerSub(q, dci_per_sub, j, i);
    			ZERO_OBJECT(q->dci_array[j][i]);
				ret++;;
			}
        }
    }
    return ret;
}

//  Put the decoded downlink dci message into the tree
int srsran_ngscope_tree_put_dl_dci(ngscope_tree_t* q, int format_idx, int loc_idx, float decode_prob, float corr,  
									srsran_dci_dl_t* dci_dl,
									srsran_pdsch_grant_t* dci_dl_grant){
	srsran_ngscope_dci_into_array_dl(q->dci_array, format_idx, loc_idx, q->dci_location[loc_idx], decode_prob, corr,  dci_dl, dci_dl_grant);
	return 0;
}

//  Put the decoded downlink dci message into the tree
int srsran_ngscope_tree_put_ul_dci(ngscope_tree_t* q, int format_idx, int loc_idx, float decode_prob, float corr,  
									srsran_dci_ul_t* 		dci_ul,
									srsran_pusch_grant_t* 	dci_ul_grant){
	srsran_ngscope_dci_into_array_ul(q->dci_array, format_idx, loc_idx, q->dci_location[loc_idx], decode_prob, corr, dci_ul, dci_ul_grant);
	return 0;
}
