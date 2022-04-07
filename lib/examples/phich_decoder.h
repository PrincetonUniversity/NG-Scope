#ifndef DECODE_PHICH_H
#define DECODE_PHICH_H

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

#define HARQ_DELAY_MS   4
#define MSG3_DELAY_MS   2 // Delay added to HARQ_DELAY_MS
#define TTI_RX(tti)     (tti>HARQ_DELAY_MS?((tti-HARQ_DELAY_MS)%10240):(10240+tti-HARQ_DELAY_MS))
#define TTI_TX(tti)     ((tti+HARQ_DELAY_MS)%10240)
#define TTI_RX_ACK(tti) ((tti+(2*HARQ_DELAY_MS))%10240)

#define UL_PIDOF(tti)   (tti%(2*HARQ_DELAY_MS))

#define TTIMOD_SZ       (((2*HARQ_DELAY_MS) < 10)?10:20)
#define TTIMOD(tti)     (tti%TTIMOD_SZ)

typedef struct {
  bool enabled;
  uint32_t I_lowest;
  uint32_t n_dmrs;
} pending_ack_element;

typedef struct {
    pending_ack_element pending_ack[TTIMOD_SZ];
} pend_ack_list;

void init_pending_ack(pend_ack_list* ack_list);
void phich_reset_pending_ack(pend_ack_list* ack_list, uint32_t tti);
void phich_set_pending_ack(pend_ack_list* ack_list, uint32_t tti, uint32_t I_lowest, uint32_t n_dmrs);
bool phich_get_pending_ack(pend_ack_list* ack_list, uint32_t tti, uint32_t *I_lowest, uint32_t *n_dmrs);
bool phich_is_ack_enabled(pend_ack_list* ack_list, uint32_t tti);
bool phich_is_any_pending_ack(pend_ack_list* ack_list);


bool decode_phich(srsran_ue_dl_t* ue_dl,
                  srsran_dl_sf_cfg_t* sf_cfg_dl,
                  srsran_ue_dl_cfg_t* ue_dl_cfg,
                  pend_ack_list* ack_list,
                  srsran_phich_res_t* phich_res);

#endif

