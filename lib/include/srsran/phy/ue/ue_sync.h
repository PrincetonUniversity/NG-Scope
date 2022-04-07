/**
 * Copyright 2013-2021 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 *
 * srsRAN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsRAN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

/******************************************************************************
 *  File:         ue_sync.h
 *
 *  Description:  This object automatically manages the cell synchronization
 *                procedure.
 *
 *                The main function is srsran_ue_sync_get_buffer(), which returns
 *                a pointer to the aligned subframe of samples (before FFT). This
 *                function should be called regularly, returning every 1 ms.
 *                It reads from the USRP, aligns the samples to the subframe and
 *                performs time/freq synch.
 *
 *                It is also possible to read the signal from a file using the
 *                init function srsran_ue_sync_init_file(). The sampling frequency
 *                is derived from the number of PRB.
 *
 *                The function returns 1 when the signal is correctly acquired and
 *                the returned buffer is aligned with the subframe.
 *
 *  Reference:
 *****************************************************************************/

#ifndef SRSRAN_UE_SYNC_H
#define SRSRAN_UE_SYNC_H

#include <stdbool.h>

#include "srsran/config.h"
#include "srsran/phy/agc/agc.h"
#include "srsran/phy/ch_estimation/chest_dl.h"
#include "srsran/phy/common/timestamp.h"
#include "srsran/phy/dft/ofdm.h"
#include "srsran/phy/io/filesource.h"
#include "srsran/phy/phch/pbch.h"
#include "srsran/phy/sync/cfo.h"
#include "srsran/phy/sync/sync.h"

#define DEFAULT_SAMPLE_OFFSET_CORRECT_PERIOD  10
#define DEFAULT_SFO_EMA_COEFF                 0.1

#define DEFAULT_CFO_BW_PSS  0.05
#define DEFAULT_CFO_PSS_MIN 400  // typical bias of PSS estimation.
#define DEFAULT_CFO_BW_REF  0.08
#define DEFAULT_CFO_REF_MIN 0    // typical bias of REF estimation
#define DEFAULT_CFO_REF_MAX DEFAULT_CFO_PSS_MIN  // Maximum detection offset of REF based estimation

#define DEFAULT_PSS_STABLE_TIMEOUT     20  // Time after which the PSS is considered to be stable and we accept REF-CFO

#define DEFAULT_CFO_EMA_TRACK 0.05

typedef enum SRSRAN_API { SYNC_MODE_PSS, SYNC_MODE_GNSS } srsran_ue_sync_mode_t;
typedef enum SRSRAN_API { SF_FIND, SF_TRACK } srsran_ue_sync_state_t;

//#define MEASURE_EXEC_TIME

typedef int(ue_sync_recv_callback_t)(void*, cf_t* [SRSRAN_MAX_CHANNELS], uint32_t, srsran_timestamp_t*);

typedef struct SRSRAN_API {
  srsran_ue_sync_mode_t mode;
  srsran_sync_t         sfind;
  srsran_sync_t         strack;

  uint32_t max_prb;

  srsran_agc_t agc;
  bool do_agc; 
  uint32_t agc_period; 
  int decimate;
  void *stream; 
  void *stream_single;
  int (*recv_callback)(void*, cf_t* [SRSRAN_MAX_CHANNELS], uint32_t, srsran_timestamp_t*);
  int (*recv_callback_single)(void*, void*, uint32_t, srsran_timestamp_t*);
  srsran_timestamp_t last_timestamp;

  uint32_t nof_rx_antennas;

  srsran_filesource_t file_source;
  bool file_mode; 
  float file_cfo;
  bool file_wrap_enable;
  srsran_cfo_t        file_cfo_correct;

  srsran_ue_sync_state_t state;

  uint32_t frame_len; 
  uint32_t fft_size;
  uint32_t nof_recv_sf; // Number of subframes received each call to srsran_ue_sync_get_buffer
  uint32_t nof_avg_find_frames;
  uint32_t frame_find_cnt;
  uint32_t sf_len;

  /* These count half frames (5ms) */
  uint64_t frame_ok_cnt;
  uint32_t frame_no_cnt; 
  uint32_t frame_total_cnt; 
  
  /* this is the system frame number (SFN) */
  uint32_t frame_number;
  uint32_t sfn_offset; ///< An offset value provided by higher layers

  srsran_cell_t cell;
  uint32_t sf_idx;
      
  bool  cfo_is_copied;
  bool  cfo_correct_enable_track;
  bool  cfo_correct_enable_find;
  float cfo_current_value;
  float cfo_loop_bw_pss;
  float cfo_loop_bw_ref;
  float cfo_pss_min;
  float cfo_ref_min;
  float cfo_ref_max;

  uint32_t pss_stable_cnt;
  uint32_t pss_stable_timeout;
  bool     pss_is_stable;

  uint32_t peak_idx;
  int next_rf_sample_offset;
  int last_sample_offset; 
  float mean_sample_offset; 
  uint32_t sample_offset_correct_period;
  float sfo_ema; 
  

  #ifdef MEASURE_EXEC_TIME
  float mean_exec_time;
  #endif
} srsran_ue_sync_t;

SRSRAN_API int srsran_ue_sync_init(srsran_ue_sync_t* q,
                                   uint32_t          max_prb,
                                   bool              search_cell,
                                   int(recv_callback)(void*, void*, uint32_t, srsran_timestamp_t*),
                                   void* stream_handler);

SRSRAN_API int
srsran_ue_sync_init_multi(srsran_ue_sync_t* q,
                          uint32_t          max_prb,
                          bool              search_cell,
                          int(recv_callback)(void*, cf_t* [SRSRAN_MAX_CHANNELS], uint32_t, srsran_timestamp_t*),
                          uint32_t nof_rx_antennas,
                          void*    stream_handler);

SRSRAN_API int
srsran_ue_sync_init_multi_decim(srsran_ue_sync_t* q,
                                uint32_t          max_prb,
                                bool              search_cell,
                                int(recv_callback)(void*, cf_t* [SRSRAN_MAX_CHANNELS], uint32_t, srsran_timestamp_t*),
                                uint32_t nof_rx_antennas,
                                void*    stream_handler,
                                int      decimate);

SRSRAN_API int srsran_ue_sync_init_multi_decim_mode(
    srsran_ue_sync_t* q,
    uint32_t          max_prb,
    bool              search_cell,
    int(recv_callback)(void*, cf_t* [SRSRAN_MAX_CHANNELS], uint32_t, srsran_timestamp_t*),
    uint32_t              nof_rx_antennas,
    void*                 stream_handler,
    int                   decimate,
    srsran_ue_sync_mode_t mode);

SRSRAN_API int
srsran_ue_sync_init_file(srsran_ue_sync_t* q, uint32_t nof_prb, char* file_name, int offset_time, float offset_freq);

SRSRAN_API int srsran_ue_sync_init_file_multi(srsran_ue_sync_t* q,
                                              uint32_t          nof_prb,
                                              char*             file_name,
                                              int               offset_time,
                                              float             offset_freq,
                                              uint32_t          nof_rx_ant);

SRSRAN_API void srsran_ue_sync_free(srsran_ue_sync_t* q);

SRSRAN_API void srsran_ue_sync_file_wrap(srsran_ue_sync_t* q, bool enable);

SRSRAN_API int srsran_ue_sync_set_cell(srsran_ue_sync_t* q, srsran_cell_t cell);

SRSRAN_API void srsran_ue_sync_cfo_reset(srsran_ue_sync_t* q, float init_cfo_hz);

SRSRAN_API void srsran_ue_sync_reset(srsran_ue_sync_t* q);

SRSRAN_API void srsran_ue_sync_set_frame_type(srsran_ue_sync_t* q, srsran_frame_type_t frame_type);

SRSRAN_API void srsran_ue_sync_set_nof_find_frames(srsran_ue_sync_t* q, uint32_t nof_frames);

SRSRAN_API srsran_frame_type_t srsran_ue_sync_get_frame_type(srsran_ue_sync_t* q);

SRSRAN_API int srsran_ue_sync_start_agc(srsran_ue_sync_t* q,
                                        SRSRAN_AGC_CALLBACK(set_gain_callback),
                                        float min_gain,
                                        float max_gain,
                                        float init_gain_value);

SRSRAN_API uint32_t srsran_ue_sync_sf_len(srsran_ue_sync_t* q);

SRSRAN_API void srsran_ue_sync_set_agc_period(srsran_ue_sync_t* q, uint32_t period);

SRSRAN_API int
srsran_ue_sync_zerocopy(srsran_ue_sync_t* q, cf_t* input_buffer[SRSRAN_MAX_CHANNELS], const uint32_t max_num_samples);

SRSRAN_API void srsran_ue_sync_set_cfo_tol(srsran_ue_sync_t* q, float tol);

SRSRAN_API void srsran_ue_sync_copy_cfo(srsran_ue_sync_t* q, srsran_ue_sync_t* src_obj);

SRSRAN_API void srsran_ue_sync_set_cfo_loop_bw(srsran_ue_sync_t* q,
                                               float             bw_pss,
                                               float             bw_ref,
                                               float             pss_tol,
                                               float             ref_tol,
                                               float             ref_max,
                                               uint32_t          pss_stable_timeout);

SRSRAN_API void srsran_ue_sync_set_cfo_ema(srsran_ue_sync_t* q, float ema);

SRSRAN_API void srsran_ue_sync_set_cfo_ref(srsran_ue_sync_t* q, float res_cfo);

SRSRAN_API void srsran_ue_sync_set_cfo_i_enable(srsran_ue_sync_t* q, bool enable);

SRSRAN_API void srsran_ue_sync_set_N_id_2(srsran_ue_sync_t* q, uint32_t N_id_2);

SRSRAN_API uint32_t srsran_ue_sync_get_sfn(srsran_ue_sync_t* q);

SRSRAN_API uint32_t srsran_ue_sync_get_sfidx(srsran_ue_sync_t* q);

SRSRAN_API float srsran_ue_sync_get_cfo(srsran_ue_sync_t* q);

SRSRAN_API void srsran_ue_sync_cp_en(srsran_ue_sync_t* q, bool enabled);

SRSRAN_API float srsran_ue_sync_get_sfo(srsran_ue_sync_t* q);

SRSRAN_API int srsran_ue_sync_get_last_sample_offset(srsran_ue_sync_t* q);

SRSRAN_API void srsran_ue_sync_set_sfo_correct_period(srsran_ue_sync_t* q, uint32_t nof_subframes);

SRSRAN_API void srsran_ue_sync_set_sfo_ema(srsran_ue_sync_t* q, float ema_coefficient);

SRSRAN_API void srsran_ue_sync_get_last_timestamp(srsran_ue_sync_t* q, srsran_timestamp_t* timestamp);

SRSRAN_API int srsran_ue_sync_run_find_pss_mode(srsran_ue_sync_t* q, cf_t* input_buffer[SRSRAN_MAX_CHANNELS]);

SRSRAN_API int srsran_ue_sync_run_track_pss_mode(srsran_ue_sync_t* q, cf_t* input_buffer[SRSRAN_MAX_CHANNELS]);

SRSRAN_API int srsran_ue_sync_run_find_gnss_mode(srsran_ue_sync_t* q,
                                                 cf_t*             input_buffer[SRSRAN_MAX_CHANNELS],
                                                 const uint32_t    max_num_samples);

SRSRAN_API int srsran_ue_sync_run_track_gnss_mode(srsran_ue_sync_t* q, cf_t* input_buffer[SRSRAN_MAX_CHANNELS]);

SRSRAN_API int srsran_ue_sync_set_tti_from_timestamp(srsran_ue_sync_t* q, srsran_timestamp_t* rx_timestamp);

#endif // SRSRAN_UE_SYNC_H

