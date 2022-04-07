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

#ifndef SRSUE_DL_SPS_H
#define SRSUE_DL_SPS_H

#include "srsran/common/timers.h"

/* Downlink Semi-Persistent schedulign (Section 5.10.1) */

namespace srsue {

class dl_sps
{
public:
  void clear() {}
  void reset() {}
  bool get_pending_grant(uint32_t tti, mac_interface_phy_lte::mac_grant_dl_t* grant) { return false; }

private:
};

} // namespace srsue

#endif // SRSUE_DL_SPS_H
