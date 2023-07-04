#ifndef NGSCOPE_UE_LIST_H
#define NGSCOPE_UE_LIST_H

#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "srsran/srsran.h"
#include "ngscope_def.h"

typedef struct{
  // Yaxiong's Modification: RNTI list
  bool      active_ue_list[65536];
  uint32_t  ue_cnt[65536];
  uint32_t  ue_dl_cnt[65536];
  uint32_t  ue_ul_cnt[65536];

  uint32_t  ue_last_active[65536];
  uint32_t  ue_enter_time[65536];

  uint16_t  max_freq_ue;
  uint16_t  max_dl_freq_ue;
  uint16_t  max_ul_freq_ue;
  uint16_t  nof_active_ue;

}ngscope_ue_list_t;

void ngscope_ue_list_enqueue_rnti_per_sf_per_cell(ngscope_ue_list_t q[MAX_NOF_RF_DEV], ngscope_status_buffer_t* dci_buf);
int ngscope_ue_list_print_ue_freq(ngscope_ue_list_t* q);
#endif
