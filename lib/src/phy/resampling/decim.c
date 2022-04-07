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

#include "srsran/phy/resampling/decim.h"
#include "srsran/phy/utils/debug.h"
#include <complex.h>
#include <math.h>

/* Performs integer linear decimation by a factor of M */
void srsran_decim_c(cf_t* input, cf_t* output, int M, int len)
{
  int i;
  for (i = 0; i < len / M; i++) {
    output[i] = input[i * M];
  }
}

/* Performs integer linear decimation by a factor of M */
void srsran_decim_f(float* input, float* output, int M, int len)
{
  int i;
  for (i = 0; i < len / M; i++) {
    output[i] = input[i * M];
  }
}
