#ifndef NGSCOPE_SRCH_SPACE_H
#define NGSCOPE_SRCH_SPACE_H

#include "srsran/phy/phch/dci.h"
#include "srsran/phy/phch/pdcch.h"

uint32_t srsran_ngscope_search_space_all_yx(srsran_pdcch_t* q, uint32_t cfi, srsran_dci_location_t* c);
uint32_t srsran_ngscope_search_space_block_yx(srsran_pdcch_t* q, uint32_t cfi, srsran_dci_location_t* c);

#endif
