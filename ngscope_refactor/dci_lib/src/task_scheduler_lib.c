#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <unistd.h>

#include "task_scheduler_lib.h"

/********************** callback wrapper **********************/
int srsran_rf_recv_wrapper(void* h, cf_t* data_[SRSRAN_MAX_PORTS], uint32_t nsamples, srsran_timestamp_t* t)
{
  DEBUG(" ----  Receive %d samples  ----", nsamples);
  void* ptr[SRSRAN_MAX_PORTS];
  for (int i = 0; i < SRSRAN_MAX_PORTS; i++) {
    ptr[i] = data_[i];
  }
  // return srsran_rf_recv_with_time_multi(h, ptr, nsamples, true, NULL, NULL);
  return srsran_rf_recv_with_time_multi(h, ptr, nsamples, true, &t->full_secs, &t->frac_secs);
}

static SRSRAN_AGC_CALLBACK(srsran_rf_set_rx_gain_th_wrapper_)
{
  srsran_rf_set_rx_gain_th((srsran_rf_t*)h, gain_db);
}
/******************** End of callback wrapper ********************/

// Init MIB
int task_mib_init_imp(srsran_ue_mib_t* ue_mib, cf_t* sf_buffer[SRSRAN_MAX_PORTS], srsran_cell_t* cell)
{
  if (srsran_ue_mib_init(ue_mib, sf_buffer[0], cell->nof_prb)) {
    ERROR("Error initaiting UE MIB decoder");
    exit(-1);
  }
  if (srsran_ue_mib_set_cell(ue_mib, *cell)) {
    ERROR("Error initaiting UE MIB decoder");
    exit(-1);
  }
  return SRSRAN_SUCCESS;
}

// Initialize UE sync
int ue_sync_init_imp(srsran_ue_sync_t*  ue_sync,
                     srsran_rf_t*       rf,
                     srsran_cell_t*     cell,
                     cell_search_cfg_t* cell_detect_config,
                     prog_args_t        prog_args,
                     float              search_cell_cfo)
{
  int decimate = 0;
  if (prog_args.decimate) {
    if (prog_args.decimate > 4 || prog_args.decimate < 0) {
      printf("Invalid decimation factor, setting to 1 \n");
    } else {
      decimate = prog_args.decimate;
    }
  }
  // Init the structure
  if (srsran_ue_sync_init_multi_decim(ue_sync,
                                      cell->nof_prb,
                                      cell->id == 1000,
                                      srsran_rf_recv_wrapper,
                                      prog_args.rf_nof_rx_ant,
                                      (void*)rf,
                                      decimate)) {
    ERROR("Error initiating ue_sync");
    exit(-1);
  }
  // UE sync set cell info
  if (srsran_ue_sync_set_cell(ue_sync, *cell)) {
    ERROR("Error initiating ue_sync");
    exit(-1);
  }

  // Disable CP based CFO estimation during find
  ue_sync->cfo_current_value       = search_cell_cfo / 15000;
  ue_sync->cfo_is_copied           = true;
  ue_sync->cfo_correct_enable_find = true;
  srsran_sync_set_cfo_cp_enable(&ue_sync->sfind, false, 0);

  // set AGC
  if (prog_args.rf_gain < 0) {
    srsran_rf_info_t* rf_info = srsran_rf_get_info(rf);
    srsran_ue_sync_start_agc(ue_sync,
                             srsran_rf_set_rx_gain_th_wrapper_,
                             rf_info->min_rx_gain,
                             rf_info->max_rx_gain,
                             cell_detect_config->init_agc);
  }
  ue_sync->cfo_correct_enable_track = !prog_args.disable_cfo;

  return SRSRAN_SUCCESS;
}

// Init the task scheduler
int task_scheduler_init(prog_args_t       prog_args,
                        srsran_rf_t*      rf,
                        srsran_cell_t*    cell,
                        srsran_ue_sync_t* ue_sync,
                        srsran_ue_mib_t*  ue_mib,
                        srsran_ue_dl_t*   ue_dl,
                        uint32_t          rf_nof_rx_ant,
                        cf_t*             sync_buffer[SRSRAN_MAX_PORTS])
{
  float search_cell_cfo = 0;

  cell_search_cfg_t cell_detect_config = {.max_frames_pbch      = SRSRAN_DEFAULT_MAX_FRAMES_PBCH,
                                          .max_frames_pss       = SRSRAN_DEFAULT_MAX_FRAMES_PSS,
                                          .nof_valid_pss_frames = SRSRAN_DEFAULT_NOF_VALID_PSS_FRAMES,
                                          .init_agc             = 0,
                                          .force_tdd            = false};

  // First of all, start the radio and get the cell information
  radio_init_and_start(rf, cell, prog_args, &cell_detect_config, &search_cell_cfo);
  printf("Radio inited!\n");

  // Next, let's get the ue_sync ready
  ue_sync_init_imp(ue_sync, rf, cell, &cell_detect_config, prog_args, search_cell_cfo);
  printf("ue sync inited!\n");

  uint32_t max_num_samples = 3 * SRSRAN_SF_LEN_PRB(cell->nof_prb); /// Length in complex samples
  for (int j = 0; j < prog_args.rf_nof_rx_ant; j++) {
    sync_buffer[j] = srsran_vec_cf_malloc(max_num_samples);
  }

  // Now, let's init the mib decoder
  task_mib_init_imp(ue_mib, sync_buffer[0], cell);
  printf("task mib inited!\n");

  // init the UE_DL entity
  if (srsran_ue_dl_init(ue_dl, sync_buffer, cell->nof_prb, rf_nof_rx_ant)) {
    ERROR("Error initiating UE downlink processing module");
    exit(-1);
  }
  printf("ue_dl inited!\n");

  if (srsran_ue_dl_set_cell(ue_dl, *cell)) {
    ERROR("Error initiating UE downlink processing module");
    exit(-1);
  }

  printf("Task Scheduler inited\n");
  return SRSRAN_SUCCESS;
}

int task_init_decode_config(srsran_dl_sf_cfg_t*     dl_sf,
                            srsran_ue_dl_cfg_t*     ue_dl_cfg,
                            srsran_pdsch_cfg_t*     pdsch_cfg,
                            srsran_cell_t*          cell,
                            prog_args_t*            prog_args,
                            srsran_softbuffer_rx_t* rx_softbuffers)
{
  /************************* Init dl_sf **************************/
  if (cell->frame_type == SRSRAN_TDD && prog_args->tdd_special_sf >= 0 && prog_args->sf_config >= 0) {
    dl_sf->tdd_config.ss_config = prog_args->tdd_special_sf;
    // dl_sf->tdd_config.sf_config  = prog_args.sf_config;
    // dl_sf->tdd_config.sf_config  = 2;
    dl_sf->tdd_config.configured = true;
  }

  dl_sf->sf_type                         = SRSRAN_SF_NORM; // Ingore the MBSFN
  ue_dl_cfg->cfg.tm                      = (srsran_tm_t)3;
  ue_dl_cfg->cfg.pdsch.use_tbs_index_alt = true;

  // dci_decoder->dl_sf.tdd_config.ss_config  = prog_args.tdd_special_sf;
  ////dci_decoder->dl_sf.tdd_config.sf_config  = prog_args.sf_config;
  // dci_decoder->dl_sf.tdd_config.sf_config  = 2;
  // dci_decoder->dl_sf.tdd_config.configured = true;

  /************************* Init ue_dl_cfg **************************/
  srsran_chest_dl_cfg_t chest_pdsch_cfg = {};
  chest_pdsch_cfg.cfo_estimate_enable   = prog_args->enable_cfo_ref;
  chest_pdsch_cfg.cfo_estimate_sf_mask  = 1023;
  chest_pdsch_cfg.estimator_alg         = srsran_chest_dl_str2estimator_alg(prog_args->estimator_alg);
  chest_pdsch_cfg.sync_error_enable     = true;

  // Set PDSCH channel estimation (we don't consider MBSFN)
  ue_dl_cfg->chest_cfg = chest_pdsch_cfg;

  // test: enable cif
  // dci_decoder->ue_dl_cfg.cfg.dci.cif_enabled = true;
  // dci_decoder->ue_dl_cfg.cfg.dci.cif_present = true;
  // dci_decoder->ue_dl_cfg.cfg.dci.multiple_csi_request_enabled = true;

  /************************* Init pdsch_cfg **************************/
  pdsch_cfg->meas_evm_en = true;
  // Allocate softbuffer buffers
  // srsran_softbuffer_rx_t rx_softbuffers[SRSRAN_MAX_CODEWORDS];
  for (uint32_t i = 0; i < SRSRAN_MAX_CODEWORDS; i++) {
    pdsch_cfg->softbuffers.rx[i] = &rx_softbuffers[i];
    srsran_softbuffer_rx_init(pdsch_cfg->softbuffers.rx[i], cell->nof_prb);
  }

  pdsch_cfg->rnti = prog_args->rnti;

  return SRSRAN_SUCCESS;
}

// Decode MIB Messages
int task_ue_mib_decode_sfn(srsran_ue_mib_t* ue_mib, srsran_cell_t* cell, uint32_t* sfn, bool decode_pdcch)
{
  uint8_t bch_payload[SRSRAN_BCH_PAYLOAD_LEN];
  int     sfn_offset;
  int     n = srsran_ue_mib_decode(ue_mib, bch_payload, NULL, &sfn_offset);
  if (n < 0) {
    ERROR("Error decoding UE MIB");
    exit(-1);
  } else if (n == SRSRAN_UE_MIB_FOUND) {
    srsran_pbch_mib_unpack(bch_payload, cell, sfn);
    if (!decode_pdcch) {
      srsran_cell_fprint(stdout, cell, *sfn);
      printf("Decoded MIB. SFN: %d, offset: %d\n", *sfn, sfn_offset);
    }
    *sfn = (*sfn + sfn_offset) % 1024;
  }
  return SRSRAN_SUCCESS;
}
