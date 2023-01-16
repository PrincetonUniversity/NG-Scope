#ifndef NGSCOPE_UE_TRACKER_H
#define NGSCOPE_UE_TRACKER_H

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
#include "ngscope_util.h"

// we record the top N most frequently observed UE
#define TOPN 10

// If we met the same ue within ACTIVE_TTI_T subframes, 
// such a ue is active ue
#define ACTIVE_TTI_T 20
// An ue becomes active if it has been observed for ACTIVE_UE_CNT_THD times
#define ACTIVE_UE_CNT_THD 50


// ue becomes inactive and been removed if it is inactive for INACTIVE_UE_THD_T
#define ACTIVE_UE_THD_T 5000
#define INACTIVE_UE_THD_T 500
typedef struct{
  // Yaxiong's Modification: RNTI list
  bool      active_ue_list[65536];
  uint32_t  ue_cnt[65536];
  uint32_t  ue_dl_cnt[65536];
  uint32_t  ue_ul_cnt[65536];

  uint32_t  ue_last_active[65536];
  uint32_t  ue_enter_time[65536];

// the top 20 ue rnti and its frequency
  uint16_t  top_N_ue_rnti[TOPN];
  uint32_t  top_N_ue_freq[TOPN];

  uint16_t  max_freq_ue;
  uint16_t  max_dl_freq_ue;
  uint16_t  max_ul_freq_ue;
  uint16_t  nof_active_ue;

}ngscope_ue_tracker_t;

void ngscope_ue_tracker_update_per_tti(ngscope_ue_tracker_t* q, uint32_t tti);
void ngscope_ue_tracker_enqueue_ue_rnti(ngscope_ue_tracker_t* q, uint32_t tti, uint16_t rnti, bool dl);
void ngscope_ue_tracker_update_per_tti(ngscope_ue_tracker_t* q, uint32_t tti);
void ngscope_ue_tracker_info(ngscope_ue_tracker_t* q, uint32_t tti);
#endif
