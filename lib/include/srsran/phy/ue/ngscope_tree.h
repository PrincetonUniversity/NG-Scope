#ifndef SRSRAN_NGSCOPE_TREE_H
#define SRSRAN_NGSCOPE_TREE_H

#include <stdbool.h>
#include "srsran/phy/ch_estimation/chest_dl.h"
#include "srsran/phy/common/phy_common.h"
#include "srsran/phy/dft/ofdm.h"

#include "srsran/phy/phch/dci.h"
#include "srsran/phy/phch/pcfich.h"
#include "srsran/phy/phch/pdcch.h"
#include "srsran/phy/phch/pdsch.h"
#include "srsran/phy/phch/pdsch_cfg.h"
#include "srsran/phy/phch/phich.h"
#include "srsran/phy/phch/pmch.h"
#include "srsran/phy/phch/ra.h"
#include "srsran/phy/phch/regs.h"

#include "srsran/phy/ue/ngscope_st.h"

void srsran_ngscope_tree_copy_dci_fromArray2PerSub(ngscope_dci_msg_t dci_array[][MAX_CANDIDATES_ALL],
                                        ngscope_dci_per_sub_t* dci_per_sub,
                                        int format,
                                        int idx);

uint32_t srsran_ngscope_search_space_all_yx(srsran_pdcch_t*             q, 
                                                uint32_t                cfi, 
                                                srsran_dci_location_t*  c);

uint32_t srsran_ngscope_search_space_block_yx(srsran_pdcch_t*           q,    
                                                uint32_t                cfi, 
                                                srsran_dci_location_t*  c);

void srsran_ngscope_tree_CP_match(ngscope_dci_msg_t  dci_array[][MAX_CANDIDATES_ALL], 
                                    int             nof_location, 
                                    int             blk_idx, 
                                    int             loc_idx, 
                                    int*            nof_matched,
                                    int*            root_idx,
                                    int*            format_idx);

int  srsran_ngscope_tree_prune_node(ngscope_dci_msg_t dci_array[][MAX_CANDIDATES_ALL],
                                        int nof_matched,
                                        int root,
                                        int* format_vec,
                                        int* format_idx);
int srsran_ngscope_tree_check_nodes(srsran_dci_location_t   dci_location[MAX_CANDIDATES_ALL],
                                        int                 index);

int srsran_ngscope_tree_clear_dciArray_nodes(ngscope_dci_msg_t  dci_array[][MAX_CANDIDATES_ALL],
                                              int               index);

int srsran_ngscope_tree_non_empty_nodes(ngscope_dci_msg_t   dci_array[][MAX_CANDIDATES_ALL], 
                                        int                 nof_locations);

int srsran_ngscope_tree_solo_nodes(ngscope_dci_msg_t        dci_array[][MAX_CANDIDATES_ALL],
                                    srsran_dci_location_t   dci_location[MAX_CANDIDATES_ALL],
                                    ngscope_dci_per_sub_t*  dci_per_sub,
                                    int                     nof_location);

void srsran_ngscope_tree_plot_multi(ngscope_dci_msg_t      dci_array[][MAX_CANDIDATES_ALL],
                                    srsran_dci_location_t  dci_location[MAX_CANDIDATES_ALL],
                                    uint32_t nof_location);
#endif
