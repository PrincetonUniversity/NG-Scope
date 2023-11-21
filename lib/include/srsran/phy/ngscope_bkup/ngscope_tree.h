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
#include "srsran/phy/ue/ngscope_search_space.h"

typedef struct{
	int loc_idx;
	int format_idx;
}tree_loc_t;

typedef struct{
	ngscope_dci_msg_t 		dci_array[MAX_NOF_FORMAT+1][MAX_CANDIDATES_ALL];
	srsran_dci_location_t 	dci_location[MAX_CANDIDATES_ALL];
	int 					nof_location;
	int 					nof_cce;
}ngscope_tree_t;

void srsran_ngscope_tree_copy_dci_fromArray2PerSub(ngscope_tree_t* q,
                                        ngscope_dci_per_sub_t* dci_per_sub,
                                        int format,
                                        int idx);

void srsran_ngscope_tree_CP_match(ngscope_tree_t* q, 
                                    int             blk_idx, 
                                    int             loc_idx, 
									uint16_t 		targetRNTI,
                                    int*            nof_matched,
                                    int*            root_idx,
                                    int*            format_idx);

int  srsran_ngscope_tree_prune_node(ngscope_tree_t* q,
                                        int nof_matched,
                                        int root,
										uint16_t rnti,
                                        int* format_vec,
                                        int* format_idx);

int srsran_ngscope_tree_check_nodes(ngscope_tree_t* q,
                                        int                 index);

ngscope_dci_msg_t srsran_ngscope_tree_find_rnti(ngscope_tree_t* q, uint16_t rnti);
int srsran_ngscope_tree_find_rnti_range(ngscope_tree_t* q, int loc_idx, uint16_t rnti_min, uint16_t rnti_max);
int srsran_ngscope_tree_clear_dciArray_nodes(ngscope_tree_t* q, int index);


int srsran_ngscope_tree_non_empty_nodes(ngscope_tree_t* q);

int srsran_ngscope_tree_solo_nodes(ngscope_tree_t* q,
                                    ngscope_dci_per_sub_t*  dci_per_sub);

int srsran_ngscope_tree_prune_tree(ngscope_tree_t* q);

int ngscope_tree_init(ngscope_tree_t* q);
int ngscope_tree_set_locations(ngscope_tree_t* q, srsran_pdcch_t* pdcch, uint32_t cfi);
int ngscope_tree_set_cce(ngscope_tree_t* q, int nof_cce);

int srsran_ngscope_tree_copy_rnti(ngscope_tree_t* q,
                                    ngscope_dci_per_sub_t* 	dci_per_sub,
									uint16_t 				rnti);

int srsran_ngscope_tree_put_dl_dci(ngscope_tree_t* q, int format_idx, int loc_idx, float decode_prob, float corr,
									srsran_dci_dl_t* 		dci_dl,
									srsran_pdsch_grant_t* 	dci_dl_grant);

int srsran_ngscope_tree_put_ul_dci(ngscope_tree_t* q, int format_idx, int loc_idx, float decode_prob, float corr,
									srsran_dci_ul_t* 		dci_ul,
									srsran_pusch_grant_t* 	dci_ul_grant);

/****************** PLOT RELATED *************/
void srsran_ngscope_tree_plot_multi(ngscope_tree_t* q);
void srsran_ngscope_tree_plot_loc(ngscope_tree_t* q);


#endif
