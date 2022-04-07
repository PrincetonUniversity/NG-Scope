#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>

#include "srsran/srsran.h"
#include "ngscope/hdr/dciLib/phich_decoder.h"

void init_pending_ack(pend_ack_list* ack_list){
    for(int i=0; i< TTIMOD_SZ;i++){
        ack_list->pending_ack[i].enabled  = false;
        ack_list->pending_ack[i].I_lowest = 0;
        ack_list->pending_ack[i].n_dmrs   = 0;
    }
}
void phich_reset_pending_ack(pend_ack_list* ack_list, uint32_t tti) {
  ack_list->pending_ack[TTIMOD(tti)].enabled = false;
}

void phich_set_pending_ack(pend_ack_list* ack_list, uint32_t tti, uint32_t I_lowest, uint32_t n_dmrs) {
  ack_list->pending_ack[TTIMOD(tti)].enabled  = true;
  ack_list->pending_ack[TTIMOD(tti)].I_lowest = I_lowest;
  ack_list->pending_ack[TTIMOD(tti)].n_dmrs = n_dmrs;
}

bool phich_get_pending_ack(pend_ack_list* ack_list, uint32_t tti, uint32_t *I_lowest, uint32_t *n_dmrs) {
  if (I_lowest) {
    *I_lowest = ack_list->pending_ack[TTIMOD(tti)].I_lowest;
  }
  if (n_dmrs) {
    *n_dmrs = ack_list->pending_ack[TTIMOD(tti)].n_dmrs;
  }
  return ack_list->pending_ack[TTIMOD(tti)].enabled;
}


bool phich_is_ack_enabled(pend_ack_list* ack_list, uint32_t tti) {
  return phich_get_pending_ack(ack_list, tti, NULL, NULL);
}

bool phich_is_any_pending_ack(pend_ack_list* ack_list) {
  for (int i=0;i<TTIMOD_SZ;i++) {
    if (ack_list->pending_ack[i].enabled) {
      return true;
    }
  }
  return false;
}

bool decode_phich(srsran_ue_dl_t* ue_dl,
                  srsran_dl_sf_cfg_t* sf_cfg_dl,
                  srsran_ue_dl_cfg_t* ue_dl_cfg,
                  pend_ack_list* ack_list,
                  srsran_phich_res_t* phich_res)
{
    uint32_t I_lowest, n_dmrs;
    srsran_phich_grant_t phich_grant = {};

    if (phich_get_pending_ack(ack_list, sf_cfg_dl->tti, &I_lowest, &n_dmrs)) {
        phich_grant.n_prb_lowest    = I_lowest;
        phich_grant.n_dmrs          = n_dmrs;
        phich_grant.I_phich         = 0;
        if (srsran_ue_dl_decode_phich(ue_dl, sf_cfg_dl, ue_dl_cfg, &phich_grant, phich_res)) {
            perror("Decoding PHICH");
            return false;
        }
        phich_reset_pending_ack(ack_list, sf_cfg_dl->tti);
    }else{
        return false;
    }
    return true;
}
