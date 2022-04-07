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
 *  File:         convcoder.h
 *
 *  Description:  Convolutional encoder.
 *                LTE uses a tail biting convolutional code with constraint length 7
 *                and coding rate 1/3.
 *
 *  Reference:    3GPP TS 36.212 version 10.0.0 Release 10 Sec. 5.1.3.1
 *********************************************************************************************/

#ifndef SRSRAN_CONVCODER_H
#define SRSRAN_CONVCODER_H

#include "srsran/config.h"
#include <stdbool.h>

typedef struct SRSRAN_API {
  uint32_t R;
  uint32_t K;
  int      poly[3];
  bool     tail_biting;
} srsran_convcoder_t;

SRSRAN_API int srsran_convcoder_encode(srsran_convcoder_t* q, uint8_t* input, uint8_t* output, uint32_t frame_length);

#endif // SRSRAN_CONVCODER_H
