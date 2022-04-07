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

#ifndef SRSRAN_TURBODECODER_IMPL_H
#define SRSRAN_TURBODECODER_IMPL_H

#include "srsran/config.h"

/* Interface for internal decoder implementation */
typedef enum SRSRAN_API {
  SRSRAN_TDEC_AUTO = 0,
  SRSRAN_TDEC_GENERIC,
  SRSRAN_TDEC_SSE,
  SRSRAN_TDEC_SSE_WINDOW,
  SRSRAN_TDEC_NEON_WINDOW,
  SRSRAN_TDEC_AVX_WINDOW,
  SRSRAN_TDEC_SSE8_WINDOW,
  SRSRAN_TDEC_AVX8_WINDOW,
  SRSRAN_TDEC_NOF_IMP
} srsran_tdec_impl_type_t;

#endif

#ifdef LLR_IS_8BIT
#define llr_t int8_t
#define type_name srsran_tdec_8bit_impl_t
#else
#ifdef LLR_IS_16BIT
#define llr_t int16_t
#define type_name srsran_tdec_16bit_impl_t
#else
#error "Unsupported LLR mode"
#endif
#endif

typedef struct SRSRAN_API {
  int (*tdec_init)(void** h, uint32_t max_long_cb);
  void (*tdec_free)(void* h);
  void (*tdec_dec)(void* h, llr_t* input, llr_t* app, llr_t* parity, llr_t* output, uint32_t long_cb);
  void (*tdec_extract_input)(llr_t* input, llr_t* syst, llr_t* parity0, llr_t* parity1, llr_t* app2, uint32_t long_cb);
  void (*tdec_decision_byte)(llr_t* app1, uint8_t* output, uint32_t long_cb);
} type_name;

#undef llr_t
#undef type_name