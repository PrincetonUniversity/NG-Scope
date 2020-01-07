#include "srslte/phy/ue/lte_scope.h"
#include "srslte/phy/ue/bin_tree.h"

int srslte_tree_blk2tree(srslte_dci_location_blk_paws_t* blk, srslte_tree_ele* tree)
{
    tree[0].location	= blk->loc_L3[0];
    tree[0].paraent	= 0;
    tree[0].left	= 1;
    tree[0].right	= 2;
    tree[0].layer	= 0;
    tree[0].nof_msg	= 0;
    tree[0].cleared     = false;
    bzero(tree[0].dci_msg, NOF_UE_ALL_FORMATS * sizeof(srslte_dci_msg_paws));

    for(int i=1;i<=2;i++){
	bzero(tree[i].dci_msg, NOF_UE_ALL_FORMATS * sizeof(srslte_dci_msg_paws));
	tree[i].location    = blk->loc_L2[i-1];
	tree[i].paraent	    = 0;
	tree[i].left	    = 2*i + 1;
	tree[i].right	    = 2*i + 2;
	tree[i].layer	    = 1;
	tree[i].nof_msg	    = 0;
	tree[i].cleared     = false;
    } 
    for(int i=3;i<=6;i++){
	bzero(tree[i].dci_msg, NOF_UE_ALL_FORMATS * sizeof(srslte_dci_msg_paws));
	tree[i].location    = blk->loc_L1[i-3];
	tree[i].paraent	    = (i-1)/2;
	tree[i].left	    = 2*i + 1;
	tree[i].right	    = 2*i + 2;
	tree[i].layer	    = 2;
	tree[i].nof_msg	    = 0;
	tree[i].cleared     = false;
    }
    for(int i=7;i<=14;i++){
	bzero(tree[i].dci_msg, NOF_UE_ALL_FORMATS * sizeof(srslte_dci_msg_paws));
	tree[i].location    = blk->loc_L0[i-7];
	tree[i].paraent	    = (i-1)/2;
	tree[i].left	    = 2*i + 1;
	tree[i].right	    = 2*i + 2;
	tree[i].layer	    = 3;
	tree[i].nof_msg	    = 0;
	tree[i].cleared     = false;
    }
    return 0;
}

int array_element_match(srslte_dci_msg_paws* array1, srslte_dci_msg_paws* array2){
    int match_count = 0;
    int match_idx = -1;

    for(int i=0;i<NOF_UE_ALL_FORMATS;i++){
	uint16_t rnti1 = array1[i].rnti; 
	uint16_t rnti2 = array2[i].rnti; 
	if( (rnti2 == 0) || (rnti1 == 0)){
	     continue;
	}
	if( rnti1 == rnti2){
	    match_count += 1;
	    match_idx = i;
	}	
    }
    if( match_count > 1){
	printf("CAREFULL: we have 2 rnti matches found!\n");
    }
    return match_idx;
}
void reverse_traversal(srslte_tree_ele* bin_tree, int index, int* match_idx, int* match_format);
void reverse_traversal(srslte_tree_ele* bin_tree, int index, int* match_idx, int* match_format)
{
    if( bin_tree[index].layer == 0){
	return;
    }
    int my_paraent = bin_tree[index].paraent; 
    //printf("|%d %d| ", index, my_paraent);
    int format  = array_element_match(bin_tree[index].dci_msg, bin_tree[my_paraent].dci_msg);
    if(format >= 0){ 
	printf("%d  Matched format:%d my_idx:%d my_paraent:%d rnti:%d prb: %d my_prob:%5.1f my_paraent_prob:%5.1f\n",
		bin_tree[my_paraent].dci_msg[format].tti, format, index, my_paraent,
		bin_tree[my_paraent].dci_msg[format].rnti, bin_tree[my_paraent].dci_msg[format].nof_prb,
	    bin_tree[index].dci_msg[format].decode_prob, bin_tree[my_paraent].dci_msg[format].decode_prob);
	match_format[bin_tree[my_paraent].layer]    = format;
	match_idx[bin_tree[my_paraent].layer]	    = my_paraent;
    }
    //printf("\n");
    reverse_traversal(bin_tree, my_paraent, match_idx, match_format);
}
void traversal_children_clear(srslte_tree_ele* bin_tree, int index);
void traversal_children_clear(srslte_tree_ele* bin_tree, int index)
{
    bin_tree[index].cleared = true;
    if( bin_tree[index].layer == 3){
	// we reach the leafs 
	return;
    }
    int left	= bin_tree[index].left;
    int right	= bin_tree[index].right;
    traversal_children_clear(bin_tree, left);
    traversal_children_clear(bin_tree, right);
}

void reverse_traversal_paraent_clear(srslte_tree_ele* bin_tree, int index);
void reverse_traversal_paraent_clear(srslte_tree_ele* bin_tree, int index)
{
    bin_tree[index].cleared = true;
    if( bin_tree[index].layer == 0){
	// we reach the leafs 
	return;
    }
    int paraent = bin_tree[index].paraent;
    reverse_traversal_paraent_clear(bin_tree, paraent);
}

int prune_tree_element(srslte_tree_ele* bin_tree, int index, srslte_dci_msg_paws* decode_ret)
{
    int match_idx[4]; // maximum 4 layers 
    int match_format[4]; // maximum 4 layers 

    for(int i=0;i<4;i++){
	match_idx[i] = -1;
	match_format[i] = -1;
    }

    reverse_traversal(bin_tree, index, match_idx, match_format); 

    for(int i=0;i<4;i++){
	if(match_idx[i] >= 0){
	    //printf("match index layer0:%d 1:%d 2:%d 3:%d\n", match_idx[0],match_idx[1],match_idx[2],match_idx[3]);
	    memcpy(decode_ret, &bin_tree[match_idx[i]].dci_msg[match_format[i]], sizeof(srslte_dci_msg_paws));
	    traversal_children_clear(bin_tree, match_idx[i]);
	    reverse_traversal_paraent_clear(bin_tree, match_idx[i]);
	    return 1;
	}
    }
    return 0;
}


