#ifndef SRSRAN_NGSCOPE_H
#define SRSRAN_NGSCOPE_H

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


#include "srsran/phy/ue/ngscope_tree.h"
#include "srsran/phy/ue/ngscope_st.h"

/* Yaxiong's dci search function */
//SRSRAN_API int srsran_ngscope_search_all_space_yx(srsran_ue_dl_t*     q,
//                                 srsran_dl_sf_cfg_t* sf,
//                                 srsran_ue_dl_cfg_t* cfg,
//                                 srsran_pdsch_cfg_t* pdsch_cfg);
//
//SRSRAN_API int srsran_ngscope_search_all_space_array_yx(srsran_ue_dl_t*     q,
//                                 srsran_dl_sf_cfg_t* sf,
//                                 srsran_ue_dl_cfg_t* cfg,
//                                 srsran_pdsch_cfg_t* pdsch_cfg,
//                                 ngscope_dci_msg_t dci_array[][MAX_CANDIDATES_ALL],
//                                 srsran_dci_location_t dci_location[MAX_CANDIDATES_ALL],
//                                 ngscope_dci_per_sub_t* dci_per_sub);
//

SRSRAN_API int srsran_ngscope_search_all_space_array_yx(srsran_ue_dl_t*     	q,
														 srsran_dl_sf_cfg_t* 			sf,
														 srsran_ue_dl_cfg_t* 			cfg,
														 srsran_pdsch_cfg_t* 			pdsch_cfg,
														 ngscope_dci_per_sub_t* 		dci_per_sub,
														 ngscope_tree_t* 				tree,
														 uint16_t 						targetRNTI);

SRSRAN_API int srsran_ngscope_search_all_space_array_singleUE_yx(srsran_ue_dl_t*     q,
																 srsran_dl_sf_cfg_t* sf,
																 srsran_ue_dl_cfg_t* cfg,
																 srsran_pdsch_cfg_t* pdsch_cfg,
																 ngscope_dci_per_sub_t* dci_per_sub,
																 uint16_t targetRNTI);

SRSRAN_API int srsran_ngscope_decode_dci_singleUE_yx(srsran_ue_dl_t*     q,
													 srsran_dl_sf_cfg_t* sf,
													 srsran_ue_dl_cfg_t* cfg,
													 srsran_pdsch_cfg_t* pdsch_cfg,
													 ngscope_dci_per_sub_t* dci_per_sub,
													 uint16_t targetRNTI);

#endif
