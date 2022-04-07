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
 *  File:         timestamp.h
 *
 *  Description:  A simple timestamp struct with separate variables for full and frac seconds.
 *                Separate variables are used to avoid loss of precision in our frac seconds.
 *                Only positive timestamps are supported.
 *
 *  Reference:
 *********************************************************************************************/

#ifndef SRSRAN_TIMESTAMP_H
#define SRSRAN_TIMESTAMP_H

#include "srsran/config.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

typedef struct SRSRAN_API {
  time_t full_secs;
  double frac_secs;
} srsran_timestamp_t;

#ifdef __cplusplus
extern "C" {
#endif

SRSRAN_API int srsran_timestamp_init(srsran_timestamp_t* t, time_t full_secs, double frac_secs);

SRSRAN_API void srsran_timestamp_init_uint64(srsran_timestamp_t* ts_time, uint64_t ts_count, double base_srate);

SRSRAN_API int srsran_timestamp_copy(srsran_timestamp_t* dest, srsran_timestamp_t* src);

SRSRAN_API int srsran_timestamp_compare(const srsran_timestamp_t* a, const srsran_timestamp_t* b);

SRSRAN_API int srsran_timestamp_add(srsran_timestamp_t* t, time_t full_secs, double frac_secs);

SRSRAN_API int srsran_timestamp_sub(srsran_timestamp_t* t, time_t full_secs, double frac_secs);

SRSRAN_API double srsran_timestamp_real(const srsran_timestamp_t* t);

SRSRAN_API bool srsran_timestamp_iszero(const srsran_timestamp_t* t);

SRSRAN_API uint32_t srsran_timestamp_uint32(srsran_timestamp_t* t);

SRSRAN_API uint64_t srsran_timestamp_uint64(const srsran_timestamp_t* t, double srate);

#ifdef __cplusplus
}
#endif

#endif // SRSRAN_TIMESTAMP_H
