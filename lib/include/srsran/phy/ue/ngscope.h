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

//#define MAX_CANDIDATES_ALL 180
//#define MAX_NOF_FORMAT 4
//
//typedef struct{
//    uint32_t mcs;
//    uint32_t tbs;
//    uint32_t rv;
//    //bool     ndi;
//}ngscope_dci_tb_t;
//
//
///* only used for decoding phich*/
//typedef struct{
//    uint32_t n_dmrs; 
//    uint32_t n_prb_tilde; 
//}ngscope_dci_phich_t;
//
//
//typedef struct{
//    uint16_t rnti;
//    uint32_t prb;
//    uint32_t harq;
//    int      nof_tb;
//    bool     dl; 
//    // information of the transport block
//    ngscope_dci_tb_t    tb[2];
//
//    // parameters stored for decoding phich     
//    ngscope_dci_phich_t phich;  
//}ngscope_dci_msg_t;
//
/* Yaxiong's dci search function */
SRSRAN_API int srsran_ngscope_search_all_space_yx(srsran_ue_dl_t*     q,
                                 srsran_dl_sf_cfg_t* sf,
                                 srsran_ue_dl_cfg_t* cfg,
                                 srsran_pdsch_cfg_t* pdsch_cfg);
SRSRAN_API int srsran_ngscope_search_all_space_array_yx(srsran_ue_dl_t*     q,
                                 srsran_dl_sf_cfg_t* sf,
                                 srsran_ue_dl_cfg_t* cfg,
                                 srsran_pdsch_cfg_t* pdsch_cfg,
                                 ngscope_dci_msg_t dci_array[][MAX_CANDIDATES_ALL],
                                 srsran_dci_location_t dci_location[MAX_CANDIDATES_ALL],
                                 ngscope_dci_per_sub_t* dci_per_sub);
#endif
