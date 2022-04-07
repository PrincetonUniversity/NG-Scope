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

/**********************************************************************************************
 *  File:         ch_awgn.h
 *
 *  Description:  Additive white gaussian noise channel object
 *
 *  Reference:
 *********************************************************************************************/

#include "srsran/config.h"
#include <stdint.h>

#ifndef SRSRAN_CH_AWGN_H
#define SRSRAN_CH_AWGN_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The srsRAN channel AWGN implements an efficient Box-Muller Method accelerated with SIMD.
 */
typedef struct {
  float*   table_cos;
  float*   table_log;
  uint32_t rand_state;
  float    std_dev;
} srsran_channel_awgn_t;

/**
 * Initialization function of the channel AWGN object
 *
 * @param q AWGN channel object
 * @param seed random generator seed
 */
SRSRAN_API int srsran_channel_awgn_init(srsran_channel_awgn_t* q, uint32_t seed);

/**
 * Sets the noise level N0 in decibels full scale (dBfs)
 *
 * @param q AWGN channel object
 * @param n0_dBfs noise level
 */
SRSRAN_API int srsran_channel_awgn_set_n0(srsran_channel_awgn_t* q, float n0_dBfs);

/**
 * Runs the complex AWGN channel
 *
 * @param q AWGN channel object
 * @param in complex input array
 * @param out complex output array
 * @param length number of samples
 */
SRSRAN_API void srsran_channel_awgn_run_c(srsran_channel_awgn_t* q, const cf_t* in, cf_t* out, uint32_t length);

/**
 * Runs the real AWGN channel
 *
 * @param q AWGN channel object
 * @param in real input array
 * @param out real output array
 * @param length number of samples
 */
SRSRAN_API void srsran_channel_awgn_run_f(srsran_channel_awgn_t* q, const float* in, float* out, uint32_t length);

/**
 * Free AWGN channel generator data
 *
 * @param q AWGN channel object
 */
SRSRAN_API void srsran_channel_awgn_free(srsran_channel_awgn_t* q);

SRSRAN_API void srsran_ch_awgn_c(const cf_t* input, cf_t* output, float variance, uint32_t len);

SRSRAN_API void srsran_ch_awgn_f(const float* x, float* y, float variance, uint32_t len);

SRSRAN_API float srsran_ch_awgn_get_variance(float ebno_db, float rate);

#ifdef __cplusplus
}
#endif

#endif // SRSRAN_CH_AWGN_H
