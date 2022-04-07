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

#ifndef SRSUE_PROC_SR_H
#define SRSUE_PROC_SR_H

#include "srsran/interfaces/ue_mac_interfaces.h"
#include "srsran/srslog/srslog.h"
#include <mutex>
#include <stdint.h>

/* Scheduling Request procedure as defined in 5.4.4 of 36.321 */

namespace srsue {

class ra_proc;
class phy_interface_mac_lte;
class rrc_interface_mac;

class sr_proc
{
public:
  explicit sr_proc(srslog::basic_logger& logger);
  void init(ra_proc* ra, phy_interface_mac_lte* phy_h, rrc_interface_mac* rrc);
  void step(uint32_t tti);
  void set_config(srsran::sr_cfg_t& cfg);
  void reset();
  void start();

private:
  bool need_tx(uint32_t tti);

  int  sr_counter;
  bool is_pending_sr;

  srsran::sr_cfg_t sr_cfg;

  ra_proc*               ra;
  rrc_interface_mac*     rrc;
  phy_interface_mac_lte* phy_h;
  srslog::basic_logger&  logger;

  // Protects access to is_pending_sr, which can be accessed by PHY worker
  std::mutex mutex = {};

  bool initiated;
};

} // namespace srsue

#endif // SRSUE_PROC_SR_H
