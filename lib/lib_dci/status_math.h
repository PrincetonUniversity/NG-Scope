#ifndef STATUS_MATH_H
#define STATUS_MATH_H

#include "srslte/phy/ue/lte_scope.h"
#include "srslte/phy/ue/LTESCOPE_GLOBAL.h"

#include "load_config.h"
#include "ue_cell_status.h"


int lteCCA_sum_cell_prb(srslte_ue_cell_usage* q, int sum_len, int* cell_prb_dl, int* cell_prb_ul);


#endif
