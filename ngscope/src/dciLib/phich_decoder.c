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


extern pthread_mutex_t     ack_mutex;

// Computes SF->TTI at which PHICH will be received according to TS 36.213 Table 9.1.2-1.
// TS 36.213 Table 9.1.2-1.
const static uint32_t k_phich[7][10] = {{0, 0, 4, 7, 6, 0, 0, 4, 7, 6},
                                  {0, 0, 4, 6, 0, 0, 0, 4, 6, 0},
                                  {0, 0, 6, 0, 0, 0, 0, 6, 0, 0},
                                  {0, 0, 6, 6, 6, 0, 0, 0, 0, 0},
                                  {0, 0, 6, 6, 0, 0, 0, 0, 0, 0},
                                  {0, 0, 6, 0, 0, 0, 0, 0, 0, 0},
                                  {0, 0, 4, 6, 6, 0, 0, 4, 7, 0}}; 

// DCI 0/PUSCH Timing according to TS 36.213 Table 8-2.
// Table 8-2
const static uint32_t k_pusch[7][10] = {
    {4, 6, 0, 0, 0, 4, 6, 0, 0, 0},
    {0, 6, 0, 0, 4, 0, 6, 0, 0, 4},
    {0, 0, 0, 4, 0, 0, 0, 0, 4, 0},
    {4, 0, 0, 0, 0, 0, 0, 0, 4, 4},
    {0, 0, 0, 0, 0, 0, 0, 0, 4, 4},
    {0, 0, 0, 0, 0, 0, 0, 0, 4, 0},
    {7, 7, 0, 0, 0, 7, 7, 0, 0, 5},
};


bool subframe_is_ulgrant_tdd(uint32_t tti, uint32_t sf_config){
  if(k_pusch[sf_config][tti%10] == 0){
    return false;
  } else {
    return true;
  }
}

uint32_t get_phich_tti_tdd(uint32_t tti, uint32_t sf_config) {
  uint32_t tti_pusch = (tti + k_pusch[sf_config][tti % 10])%10240;
  uint32_t tti_res = (tti_pusch + k_phich[sf_config][tti_pusch % 10])%10240;
  return tti_res;
}

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
                  srsran_cell_t* cell,
                  pend_ack_list* ack_list,
                  srsran_phich_res_t* phich_res)
{
    uint32_t I_lowest, n_dmrs;
    srsran_phich_grant_t phich_grant = {};

    if (phich_get_pending_ack(ack_list, sf_cfg_dl->tti, &I_lowest, &n_dmrs)) {
        phich_grant.n_prb_lowest    = I_lowest;
        phich_grant.n_dmrs          = n_dmrs;
        phich_grant.I_phich         = 0;
        if (srsran_ue_dl_decode_phich(ue_dl, sf_cfg_dl, ue_dl_cfg, &phich_grant, phich_res)!=0) {
            perror("Error decoding PHICH");
            phich_reset_pending_ack(ack_list, sf_cfg_dl->tti);
            return false;
        }

        // If a NACK is received, set a new PHICH for later tti.
        if (phich_res->ack_value==0) {
            // printf("PHICH: NACK, Set New PHICH\n");
            uint32_t tti = sf_cfg_dl->tti;
            uint32_t tti_phich = 0;

            if(cell->frame_type == SRSRAN_FDD){
              tti_phich = TTI_RX_ACK(tti);
            } else if(cell->frame_type == SRSRAN_TDD){
              tti_phich = get_phich_tti_tdd(tti, sf_cfg_dl->tdd_config.sf_config);
            }

            pthread_mutex_lock(&ack_mutex);
            phich_set_pending_ack(ack_list, tti_phich, I_lowest, n_dmrs);
            pthread_mutex_unlock(&ack_mutex);
        }
        
        // Reset PHICH for current tti.
        phich_reset_pending_ack(ack_list, sf_cfg_dl->tti);
    }else{
        return false;
    }
    return true;
}
