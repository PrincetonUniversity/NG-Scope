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

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "srsran/phy/sync/psss.h"

#include "srsran/phy/dft/ofdm.h"
#include "srsran/phy/utils/debug.h"
#include "srsran/phy/utils/vector_simd.h"
#include <srsran/phy/utils/vector.h>

// Generates the sidelink sequences that are used to detect PSSS
int srsran_psss_init(srsran_psss_t* q, uint32_t nof_prb, srsran_cp_t cp)
{
  int ret = SRSRAN_ERROR_INVALID_INPUTS;

  if (q != NULL) {
    // Generate the 2 PSSS sequences
    for (uint32_t i = 0; i < 2; i++) {
      if (srsran_psss_generate(q->psss_signal[i], i) != SRSRAN_SUCCESS) {
        ERROR("Error srsran_psss_generate");
        return SRSRAN_ERROR;
      }
    }

    /*
     * Initialization of buffers for find PSSS operations
     */
    uint32_t sf_n_samples = srsran_symbol_sz(nof_prb) * 15;
    uint32_t fft_size     = sf_n_samples * 2;

    q->psss_sf_freq = srsran_vec_malloc(sizeof(cf_t*) * 2);
    if (!q->psss_sf_freq) {
      return SRSRAN_ERROR;
    }
    for (uint32_t i = 0; i < 2; ++i) {
      q->psss_sf_freq[i] = srsran_vec_cf_malloc(fft_size);
      if (!q->psss_sf_freq[i]) {
        return SRSRAN_ERROR;
      }
    }

    q->input_pad_freq = srsran_vec_cf_malloc(fft_size);
    if (!q->input_pad_freq) {
      return SRSRAN_ERROR;
    }
    q->input_pad_time = srsran_vec_cf_malloc(fft_size);
    if (!q->input_pad_time) {
      return SRSRAN_ERROR;
    }

    srsran_ofdm_t psss_tx;
    if (srsran_ofdm_tx_init(&psss_tx, cp, q->input_pad_freq, q->input_pad_time, nof_prb)) {
      printf("Error creating iFFT object\n");
      return SRSRAN_ERROR;
    }
    srsran_ofdm_set_normalize(&psss_tx, true);
    srsran_ofdm_set_freq_shift(&psss_tx, 0.5);

    srsran_dft_plan_t plan;
    if (srsran_dft_plan(&plan, fft_size, SRSRAN_DFT_FORWARD, SRSRAN_DFT_COMPLEX)) {
      return SRSRAN_ERROR;
    }
    srsran_dft_plan_set_norm(&plan, true);
    srsran_dft_plan_set_mirror(&plan, true);

    // Create empty subframes with only PSSS
    for (uint32_t N_id_2 = 0; N_id_2 < 2; ++N_id_2) {
      srsran_vec_cf_zero(q->input_pad_freq, fft_size);
      srsran_vec_cf_zero(q->input_pad_time, fft_size);

      srsran_psss_put_sf_buffer(q->psss_signal[N_id_2], q->input_pad_freq, nof_prb, cp);
      srsran_ofdm_tx_sf(&psss_tx);

      srsran_dft_run_c(&plan, q->input_pad_time, q->psss_sf_freq[N_id_2]);
      srsran_vec_conj_cc(q->psss_sf_freq[N_id_2], q->psss_sf_freq[N_id_2], fft_size);
    }

    srsran_dft_plan_free(&plan);
    srsran_ofdm_tx_free(&psss_tx);

    q->dot_prod_output = srsran_vec_cf_malloc(fft_size);
    if (!q->dot_prod_output) {
      return SRSRAN_ERROR;
    }
    q->dot_prod_output_time = srsran_vec_cf_malloc(fft_size);
    if (!q->dot_prod_output_time) {
      return SRSRAN_ERROR;
    }
    q->shifted_output = srsran_vec_cf_malloc(fft_size);
    if (!q->shifted_output) {
      return SRSRAN_ERROR;
    }
    q->shifted_output_abs = srsran_vec_f_malloc(fft_size);
    if (!q->shifted_output_abs) {
      return SRSRAN_ERROR;
    }

    if (srsran_dft_plan(&q->plan_input, fft_size, SRSRAN_DFT_FORWARD, SRSRAN_DFT_COMPLEX)) {
      return SRSRAN_ERROR;
    }
    srsran_dft_plan_set_mirror(&q->plan_input, true);
    // srsran_dft_plan_set_dc(&psss->plan_input, true);
    srsran_dft_plan_set_norm(&q->plan_input, true);

    if (srsran_dft_plan(&q->plan_out, fft_size, SRSRAN_DFT_BACKWARD, SRSRAN_DFT_COMPLEX)) {
      return SRSRAN_ERROR;
    }
    // srsran_dft_plan_set_mirror(&psss->plan_out, true);
    srsran_dft_plan_set_dc(&q->plan_out, true);
    srsran_dft_plan_set_norm(&q->plan_out, true);

    ret = SRSRAN_SUCCESS;
  }
  return ret;
}

/**
 * This function calculates the Zadoff-Chu sequence.
 * @param signal Output array.
 * Reference: 3GPP TS 36.211 version 15.6.0 Release 15 Sec. 9.7.1.1
 */
int srsran_psss_generate(cf_t* psss_signal, uint32_t N_id_2)
{
  int         i;
  float       arg;
  int         sign         = -1;
  const float root_value[] = {26.0, 37.0};

  if (N_id_2 > 1) {
    ERROR("Invalid N_id_2 %d", N_id_2);
    return SRSRAN_ERROR;
  }

  for (i = 0; i < SRSRAN_PSSS_LEN / 2; i++) {
    arg                     = (float)sign * M_PI * root_value[N_id_2] * ((float)i * ((float)i + 1.0)) / 63.0;
    __real__ psss_signal[i] = cosf(arg);
    __imag__ psss_signal[i] = sinf(arg);
  }
  for (i = SRSRAN_PSSS_LEN / 2; i < SRSRAN_PSSS_LEN; i++) {
    arg                     = (float)sign * M_PI * root_value[N_id_2] * (((float)i + 2.0) * ((float)i + 1.0)) / 63.0;
    __real__ psss_signal[i] = cosf(arg);
    __imag__ psss_signal[i] = sinf(arg);
  }
  return SRSRAN_SUCCESS;
}

/**
 * Mapping PSSS to resource elements
 * Reference: 3GPP TS 36.211 version 15.6.0 Release 15 Sec. 9.7.1.2
 */
void srsran_psss_put_sf_buffer(cf_t* psss_signal, cf_t* sf_buffer, uint32_t nof_prb, srsran_cp_t cp)
{
  // Normal cyclic prefix l = 1,2
  // Extended cyclic prefix l = 0,1
  for (int i = 0; i < 2; i++) {
    uint32_t k = (SRSRAN_CP_NSYMB(cp) - (5 + i)) * nof_prb * SRSRAN_NRE + nof_prb * SRSRAN_NRE / 2 - 31;
    srsran_vec_cf_zero(&sf_buffer[k - 5], 5);
    memcpy(&sf_buffer[k], psss_signal, SRSRAN_PSSS_LEN * sizeof(cf_t));
    srsran_vec_cf_zero(&sf_buffer[k + SRSRAN_PSSS_LEN], 5);
  }
}

/** Performs PSSS correlation.
 * Returns the index of the PSSS correlation peak in a subframe (corr_peak_pos).
 * The value of the correlation is stored in corr_peak_value.
 * The subframe starts at corr_peak_pos - sf_n_samples.
 *
 * * Input buffer must be subframe_size long.
 */
int srsran_psss_find(srsran_psss_t* q, cf_t* input, uint32_t nof_prb, srsran_cp_t cp)
{
  if (q == NULL || input == NULL) {
    return SRSRAN_ERROR_INVALID_INPUTS;
  }

  // One array for each N_id_2
  float    corr_peak_value[2] = {};
  uint32_t corr_peak_pos[2]   = {};

  uint32_t sf_n_samples = (uint32_t)SRSRAN_SF_LEN_PRB(nof_prb);
  uint32_t fft_size     = sf_n_samples * 2;

  srsran_vec_cf_zero(q->input_pad_freq, fft_size);
  srsran_vec_cf_zero(q->input_pad_time, fft_size);

  memcpy(q->input_pad_time, input, sizeof(cf_t) * sf_n_samples);

  srsran_dft_run_c(&q->plan_input, q->input_pad_time, q->input_pad_freq);

  for (int i = 0; i < 2; i++) {
    // .*
    srsran_vec_prod_ccc(q->psss_sf_freq[i], q->input_pad_freq, q->dot_prod_output, fft_size);

    // IFFT
    srsran_dft_run_c(&q->plan_out, q->dot_prod_output, q->dot_prod_output_time);

    srsran_vec_cf_zero(q->shifted_output, fft_size);
    srsran_vec_cf_copy(q->shifted_output, &q->dot_prod_output_time[fft_size / 2], fft_size / 2);
    srsran_vec_cf_copy(&q->shifted_output[fft_size / 2], q->dot_prod_output_time, fft_size / 2);

    // Peak detection
    q->corr_peak_pos = -1;
    srsran_vec_f_zero(q->shifted_output_abs, fft_size);
    srsran_vec_abs_cf_simd(q->shifted_output, q->shifted_output_abs, fft_size);

    // Experimental Validation
    uint32_t symbol_sz = (uint32_t)srsran_symbol_sz(nof_prb);
    int      cp_len    = SRSRAN_CP_SZ(symbol_sz, cp);

    // Correlation output peaks:
    //
    //       |
    //       |                              |
    //       |                              |
    //       |                 |            |            |
    //       |...______________|____________|____________|______________... t (samples)
    //                     side peak    main peak    side peak

    // Find the main peak
    q->corr_peak_pos   = srsran_vec_max_fi(q->shifted_output_abs, fft_size);
    q->corr_peak_value = q->shifted_output_abs[q->corr_peak_pos];
    if ((q->corr_peak_pos < sf_n_samples) || (q->corr_peak_pos > fft_size - symbol_sz)) {
      q->corr_peak_pos = -1;
      continue;
    }
    srsran_vec_f_zero(&q->shifted_output_abs[q->corr_peak_pos - (symbol_sz / 2)], symbol_sz);

    // Find the first side peak
    uint32_t peak_1_pos   = srsran_vec_max_fi(q->shifted_output_abs, fft_size);
    float    peak_1_value = q->shifted_output_abs[peak_1_pos];
    if ((peak_1_pos >= (q->corr_peak_pos - (symbol_sz + cp_len) - 2)) &&
        (peak_1_pos <= (q->corr_peak_pos - (symbol_sz + cp_len) + 2))) {
      ; // Do nothing
    } else if ((peak_1_pos >= (q->corr_peak_pos + (symbol_sz + cp_len) - 2)) &&
               (peak_1_pos <= (q->corr_peak_pos + (symbol_sz + cp_len) + 2))) {
      ; // Do nothing
    } else {
      q->corr_peak_pos = -1;
      continue;
    }
    srsran_vec_f_zero(&q->shifted_output_abs[peak_1_pos - (symbol_sz / 2)], symbol_sz);

    // Find the second side peak
    uint32_t peak_2_pos   = srsran_vec_max_fi(q->shifted_output_abs, fft_size);
    float    peak_2_value = q->shifted_output_abs[peak_2_pos];
    if ((peak_2_pos >= (q->corr_peak_pos - (symbol_sz + cp_len) - 2)) &&
        (peak_2_pos <= (q->corr_peak_pos - (symbol_sz + cp_len) + 2))) {
      ; // Do nothing
    } else if ((peak_2_pos >= (q->corr_peak_pos + (symbol_sz + cp_len) - 2)) &&
               (peak_2_pos <= (q->corr_peak_pos + (symbol_sz + cp_len) + 2))) {
      ; // Do nothing
    } else {
      q->corr_peak_pos = -1;
      continue;
    }
    srsran_vec_f_zero(&q->shifted_output_abs[peak_2_pos - (symbol_sz / 2)], symbol_sz);

    float threshold_above = q->corr_peak_value / 2.0f * 1.4f;
    if ((peak_1_value > threshold_above) || (peak_2_value > threshold_above)) {
      q->corr_peak_pos = -1;
      continue;
    }

    float threshold_below = q->corr_peak_value / 2.0f * 0.6f;
    if ((peak_1_value < threshold_below) || (peak_2_value < threshold_below)) {
      q->corr_peak_pos = -1;
      continue;
    }

    corr_peak_value[i] = q->corr_peak_value;
    corr_peak_pos[i]   = (uint32_t)q->corr_peak_pos;
  }

  q->N_id_2 = srsran_vec_max_fi(corr_peak_value, 2);
  if (corr_peak_value[q->N_id_2] == 0.0f) {
    return SRSRAN_ERROR;
  }

  q->corr_peak_pos   = corr_peak_pos[q->N_id_2];
  q->corr_peak_value = corr_peak_value[q->N_id_2];

  return SRSRAN_SUCCESS;
}

void srsran_psss_free(srsran_psss_t* q)
{
  if (q) {
    srsran_dft_plan_free(&q->plan_out);
    srsran_dft_plan_free(&q->plan_input);

    if (q->shifted_output) {
      free(q->shifted_output);
    }
    if (q->shifted_output_abs) {
      free(q->shifted_output_abs);
    }
    if (q->dot_prod_output) {
      free(q->dot_prod_output);
    }
    if (q->dot_prod_output_time) {
      free(q->dot_prod_output_time);
    }
    if (q->input_pad_freq) {
      free(q->input_pad_freq);
    }
    if (q->input_pad_time) {
      free(q->input_pad_time);
    }
    if (q->psss_sf_freq) {
      for (int N_id_2 = 0; N_id_2 < 2; ++N_id_2) {
        if (q->psss_sf_freq[N_id_2]) {
          free(q->psss_sf_freq[N_id_2]);
        }
      }
      free(q->psss_sf_freq);
    }

    bzero(q, sizeof(srsran_psss_t));
  }
}
