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
#ifndef SRSRAN_PRB_DL_H_
#define SRSRAN_PRB_DL_H_

#include "srsran/config.h"

void prb_cp_ref(cf_t** input, cf_t** output, int offset, int nof_refs, int nof_intervals, bool advance_input);
void prb_cp(cf_t** input, cf_t** output, int nof_prb);
void prb_cp_half(cf_t** input, cf_t** output, int nof_prb);
void prb_put_ref_(cf_t** input, cf_t** output, int offset, int nof_refs, int nof_intervals);

#endif /* SRSRAN_PRB_DL_H_ */
