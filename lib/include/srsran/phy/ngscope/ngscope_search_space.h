#ifndef NGSCOPE_SEARCH_SPACE_H
#define NGSCOPE_SEARCH_SPACE_H

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


uint32_t srsran_ngscope_search_space_all_yx(srsran_pdcch_t*             q, 
                                                uint32_t                cfi, 
                                                srsran_dci_location_t*  c);

uint32_t srsran_ngscope_search_space_block_yx(srsran_pdcch_t*           q,    
                                                uint32_t                cfi, 
                                                srsran_dci_location_t*  c);


#endif
