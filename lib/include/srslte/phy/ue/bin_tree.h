#ifndef SRSLTE_BIN_TREE_H
#define SRSLTE_BIN_TREE_H

#include "srslte/phy/ue/lte_scope.h"

typedef struct SRSLTE_API {
    srslte_dci_location_paws_t location;
    srslte_dci_msg_paws        dci_msg[NOF_UE_ALL_FORMATS];
    int nof_msg;

    int paraent;
    int left;
    int right;
    int layer;
    bool cleared;
}srslte_tree_ele;
SRSLTE_API int prune_tree_element(srslte_tree_ele* bin_tree, int index, srslte_dci_msg_paws* decode_ret);
SRSLTE_API int srslte_tree_blk2tree(srslte_dci_location_blk_paws_t* blk, srslte_tree_ele* tree);
SRSLTE_API void traversal_children_clear(srslte_tree_ele* bin_tree, int index);
//SRSLTE_API 
//SRSLTE_API 
//SRSLTE_API 

#endif
