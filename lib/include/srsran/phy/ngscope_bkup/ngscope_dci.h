#ifndef SRSRAN_NGSCOPE_DCI_H
#define SRSRAN_NGSCOPE_DCI_H

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

#include "srsran/phy/ue/ue_dl.h"
#include "srsran/phy/ue/ngscope_tree.h"

int srsran_ngscope_unpack_dl_dci_2grant(srsran_ue_dl_t*     q,
                                        srsran_dl_sf_cfg_t* sf,
                                        srsran_ue_dl_cfg_t* cfg,
                                        srsran_pdsch_cfg_t* pdsch_cfg,
                                        srsran_dci_msg_t* dci_msg,
                                        srsran_dci_dl_t* dci_dl,
                                        srsran_pdsch_grant_t* dci_dl_grant);

//void copy_single_dl_dci(ngscope_dci_msg_t* 			dci_array;
//							srsran_dci_location_t 	loc,
//							float decode_prob, float corr,
//							srsran_dci_dl_t* 		dci_dl,
//							srsran_pdsch_grant_t* 	dci_dl_grant);
//

void srsran_ngscope_dci_into_array_dl(ngscope_dci_msg_t dci_array[][MAX_CANDIDATES_ALL],
                                        int i, int j,
										srsran_dci_location_t loc,
                                        float decode_prob, float corr,
                                        srsran_dci_dl_t* dci_dl,
                                        srsran_pdsch_grant_t* dci_dl_grant);

int srsran_ngscope_unpack_ul_dci_2grant(srsran_ue_dl_t*     q,
                                        srsran_dl_sf_cfg_t* sf,
                                        srsran_ue_dl_cfg_t* cfg,
                                        srsran_pdsch_cfg_t* pdsch_cfg,
                                        srsran_dci_msg_t* dci_msg,
                                        srsran_dci_ul_t* dci_ul,
                                        srsran_pusch_grant_t* dci_ul_grant);

//void copy_single_ul_dci(ngscope_dci_msg_t* 			dci_array,
//							srsran_dci_location_t 	loc,
//							float decode_prob, float corr,
//							srsran_dci_ul_t* 		dci_ul,
//							srsran_pusch_grant_t* 	dci_ul_grant);
//

void srsran_ngscope_dci_into_array_ul(ngscope_dci_msg_t dci_array[][MAX_CANDIDATES_ALL],
                                        int i, int j,
										srsran_dci_location_t loc,
                                        float decode_prob, float corr,
                                        srsran_dci_ul_t* dci_ul,
                                        srsran_pusch_grant_t* dci_ul_grant);


SRSRAN_API int srsran_ngscope_dci_prune(ngscope_tree_t* q,
                                        uint32_t sf_idx);

int srsran_ngscope_dci_prune_ret(ngscope_dci_per_sub_t* q);
#endif
