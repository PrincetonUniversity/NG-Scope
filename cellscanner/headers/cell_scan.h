#ifndef __CELL_SCAN_h__
#define __CELL_SCAN_H__

#include "srsran/phy/rf/rf_utils.h"

struct cells {
  srsran_cell_t cell;
  float         freq;
  int           dl_earfcn;
  float         power;
};

int cell_scan(srsran_rf_t * rf, cell_search_cfg_t * cell_detect_config,
            struct cells * results,
            int max_cells,
            int band);

#endif