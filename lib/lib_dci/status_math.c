#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#include "srslte/phy/ue/lte_scope.h"
#include "srslte/phy/ue/LTESCOPE_GLOBAL.h"

#include "status_math.h"

#define TTI_TO_IDX(i) (i%NOF_LOG_SF)

// calculate the total utilized prbs of every cell towers
int lteCCA_sum_cell_prb(srslte_ue_cell_usage* q, int sum_len, int* cell_prb_dl, int* cell_prb_ul){
    uint16_t cell_header = q->header;
    int	nof_cell = (int) q->nof_cell;

    // init the output
    for(int i=0;i<nof_cell;i++){
	cell_prb_dl[i] = 0;
	cell_prb_ul[i] = 0;
    }

    for(int i=0;i<sum_len;i++){
        int index = cell_header - sum_len + 1 + i;
        if(index < 0){
            index += NOF_LOG_SF;
        }
        int idx = TTI_TO_IDX(index);

	for(int j=0;j<nof_cell;j++){
	    cell_prb_dl[j]   += (int) (q->cell_status[j].sf_status[idx].cell_dl_prb);
	    cell_prb_ul[j]   += (int) (q->cell_status[j].sf_status[idx].cell_ul_prb);
	}
    }
    return 0;
}

