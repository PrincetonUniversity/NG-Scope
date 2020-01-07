#ifndef SRSLTE_PRUNE_H
#define SRSLTE_PRUNE_H

#include "srslte/phy/ue/ue_dl.h"
#include "srslte/phy/ue/lte_scope.h"
#include "srslte/phy/phch/dci.h"
#include "srslte/phy/phch/pdcch.h"

SRSLTE_API void dci_msg_display(srslte_dci_msg_paws* dci_msg_yx);
SRSLTE_API void dci_msg_list_display(srslte_dci_msg_paws* dci_msg_yx, int nof_msg);
SRSLTE_API void record_dci_msg_log(FILE* FD_DCI, srslte_dci_msg_paws* dci_msg_yx, int nof_msg);

SRSLTE_API int nof_prb_one_subframe(srslte_dci_msg_paws* dci_ret, int msg_cnt);

SRSLTE_API void check_decoded_locations(srslte_dci_location_paws_t* locations,
                             uint32_t nof_locations,
                             int this_loc);

SRSLTE_API int dci_msg_location_pruning(srslte_active_ue_list_t* q, 
                                    srslte_dci_msg_paws* dci_msg_list,
                                    srslte_dci_msg_paws* dci_decode_ret,
                                    uint32_t tti);
SRSLTE_API int dci_subframe_pruning(srslte_active_ue_list_t* q,
                        srslte_dci_msg_paws*  dci_ret_input,
                        srslte_dci_msg_paws*  dci_ret_output,
			int max_prb,
                        int msg_cnt_input);

SRSLTE_API int srslte_subframe_prune_dl_ul_all(srslte_dci_msg_paws* dci_msg_vector,
                                    srslte_active_ue_list_t* active_ue_list,
                                    srslte_dci_subframe_t* dci_msg_subframe,
                                    uint32_t CELL_MAX_PRB,
                                    int msg_cnt);
#endif
