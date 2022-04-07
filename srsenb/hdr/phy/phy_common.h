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

#ifndef SRSENB_PHCH_COMMON_H
#define SRSENB_PHCH_COMMON_H

#include "phy_interfaces.h"
#include "srsenb/hdr/phy/phy_ue_db.h"
#include "srsran/common/gen_mch_tables.h"
#include "srsran/common/interfaces_common.h"
#include "srsran/common/standard_streams.h"
#include "srsran/common/thread_pool.h"
#include "srsran/common/threads.h"
#include "srsran/interfaces/enb_metrics_interface.h"
#include "srsran/interfaces/phy_common_interface.h"
#include "srsran/interfaces/radio_interfaces.h"
#include "srsran/phy/channel/channel.h"
#include "srsran/radio/radio.h"

#include <map>
#include <srsran/common/tti_sempahore.h>
#include <string.h>

namespace srsenb {

class phy_common : public srsran::phy_common_interface
{
public:
  phy_common() = default;

  bool init(const phy_cell_cfg_list_t&    cell_list_,
            const phy_cell_cfg_list_nr_t& cell_list_nr_,
            srsran::radio_interface_phy*  radio_handler,
            stack_interface_phy_lte*      mac);
  void reset();
  void stop();

  /**
   * TTI transmission semaphore, used for ensuring that PHY workers transmit following start order
   */
  srsran::tti_semaphore<void*> semaphore;

  /**
   * Performs common end worker transmission tasks such as transmission and stack TTI execution
   *
   * @param tx_sem_id Semaphore identifier, the worker thread pointer is used
   * @param buffer baseband IQ sample buffer
   * @param tx_time timestamp to transmit samples
   * @param is_nr flag is true if it is called from NR
   */
  void worker_end(const worker_context_t& w_ctx, const bool& tx_enable, srsran::rf_buffer_t& buffer) override;

  // Common objects
  phy_args_t params = {};

  uint32_t get_nof_carriers_lte() { return static_cast<uint32_t>(cell_list_lte.size()); }
  uint32_t get_nof_carriers_nr() { return static_cast<uint32_t>(cell_list_nr.size()); }
  uint32_t get_nof_carriers() { return static_cast<uint32_t>(cell_list_lte.size() + cell_list_nr.size()); }
  uint32_t get_nof_prb(uint32_t cc_idx)
  {
    uint32_t ret = 0;

    if (cc_idx >= get_nof_carriers()) {
      // invalid CC index
      return ret;
    }

    if (cc_idx < cell_list_lte.size()) {
      ret = cell_list_lte[cc_idx].cell.nof_prb;
    } else if (cc_idx >= cell_list_lte.size()) {
      // offset CC index by all LTE carriers
      cc_idx -= cell_list_lte.size();
      if (cc_idx < cell_list_nr.size()) {
        ret = cell_list_nr[cc_idx].carrier.nof_prb;
      }
    }
    return ret;
  }
  uint32_t get_nof_ports(uint32_t cc_idx)
  {
    uint32_t ret = 0;

    if (cc_idx < cell_list_lte.size()) {
      ret = cell_list_lte[cc_idx].cell.nof_ports;
    } else if (cc_idx == 1 && !cell_list_nr.empty()) {
      // one RF port for basic NSA config
      ret = 1;
    }

    return ret;
  }
  uint32_t get_nof_rf_channels()
  {
    uint32_t count = 0;

    for (auto& cell : cell_list_lte) {
      count += cell.cell.nof_ports;
    }

    for (auto& cell : cell_list_nr) {
      count += cell.carrier.max_mimo_layers;
    }

    return count;
  }
  double get_ul_freq_hz(uint32_t cc_idx)
  {
    double ret = 0.0;

    if (cc_idx < cell_list_lte.size()) {
      ret = cell_list_lte[cc_idx].ul_freq_hz;
    }

    cc_idx -= cell_list_lte.size();
    if (cc_idx < cell_list_nr.size()) {
      ret = cell_list_nr[cc_idx].ul_freq_hz;
    }

    return ret;
  }
  double get_dl_freq_hz(uint32_t cc_idx)
  {
    double ret = 0.0;

    if (cc_idx < cell_list_lte.size()) {
      ret = cell_list_lte[cc_idx].dl_freq_hz;
    }

    cc_idx -= cell_list_lte.size();
    if (cc_idx < cell_list_nr.size()) {
      ret = cell_list_nr[cc_idx].dl_freq_hz;
    }

    return ret;
  }
  uint32_t get_rf_port(uint32_t cc_idx)
  {
    uint32_t ret = 0;

    if (cc_idx < cell_list_lte.size()) {
      ret = cell_list_lte[cc_idx].rf_port;
    }

    cc_idx -= cell_list_lte.size();
    if (cc_idx < cell_list_nr.size()) {
      ret = cell_list_nr[cc_idx].rf_port;
    }

    return ret;
  }
  srsran_cell_t get_cell(uint32_t cc_idx)
  {
    srsran_cell_t c = {};
    if (cc_idx < cell_list_lte.size()) {
      c = cell_list_lte[cc_idx].cell;
    }
    return c;
  }

  void set_cell_gain(uint32_t cell_id, float gain_db)
  {
    // Find LTE cell
    auto it_lte = std::find_if(
        cell_list_lte.begin(), cell_list_lte.end(), [cell_id](phy_cell_cfg_t& x) { return x.cell_id == cell_id; });

    // Check if the lte cell was found;
    if (it_lte != cell_list_lte.end()) {
      std::lock_guard<std::mutex> lock(cell_gain_mutex);
      it_lte->gain_db = gain_db;
      return;
    }

    // Find NR cell
    auto it_nr = std::find_if(
        cell_list_nr.begin(), cell_list_nr.end(), [cell_id](phy_cell_cfg_nr_t& x) { return x.cell_id == cell_id; });

    // Check if the nr cell was found;
    if (it_nr != cell_list_nr.end()) {
      std::lock_guard<std::mutex> lock(cell_gain_mutex);
      it_nr->gain_db = gain_db;
      return;
    }

    srsran::console("cell ID %d not found\n", cell_id);
  }

  float get_cell_gain(uint32_t cc_idx)
  {
    std::lock_guard<std::mutex> lock(cell_gain_mutex);
    if (cc_idx < cell_list_lte.size()) {
      return cell_list_lte.at(cc_idx).gain_db;
    }

    cc_idx -= cell_list_lte.size();
    if (cc_idx < cell_list_nr.size()) {
      return cell_list_nr.at(cc_idx).gain_db;
    }

    return 0.0f;
  }

  // Common Physical Uplink DMRS configuration
  srsran_refsignal_dmrs_pusch_cfg_t dmrs_pusch_cfg = {};

  srsran::radio_interface_phy* radio      = nullptr;
  stack_interface_phy_lte*     stack      = nullptr;
  srsran::channel_ptr          dl_channel = nullptr;

  /**
   * UE Database object, direct public access, all PHY threads should be able to access this attribute directly
   */
  phy_ue_db ue_db;

  void configure_mbsfn(srsran::phy_cfg_mbsfn_t* cfg);
  void build_mch_table();
  void build_mcch_table();
  bool is_mbsfn_sf(srsran_mbsfn_cfg_t* cfg, uint32_t phy_tti);
  void set_mch_period_stop(uint32_t stop);

  // Getters and setters for ul grants which need to be shared between workers
  const stack_interface_phy_lte::ul_sched_list_t get_ul_grants(uint32_t tti);
  void set_ul_grants(uint32_t tti, const stack_interface_phy_lte::ul_sched_list_t& ul_grants);
  void clear_grants(uint16_t rnti);

private:
  // Common objects for scheduling grants
  srsran::circular_array<stack_interface_phy_lte::ul_sched_list_t, TTIMOD_SZ> ul_grants   = {};
  std::mutex                                                                  grant_mutex = {};

  phy_cell_cfg_list_t    cell_list_lte;
  phy_cell_cfg_list_nr_t cell_list_nr;
  std::mutex             cell_gain_mutex;

  bool                    have_mtch_stop   = false;
  pthread_mutex_t         mtch_mutex       = {};
  pthread_cond_t          mtch_cvar        = {};
  srsran::phy_cfg_mbsfn_t mbsfn            = {};
  bool                    sib13_configured = false;
  bool                    mcch_configured  = false;
  uint8_t                 mch_table[40]    = {};
  uint8_t                 mcch_table[10]   = {};
  uint32_t                mch_period_stop  = 0;
  srsran::rf_buffer_t     tx_buffer        = {};
  bool                    is_mch_subframe(srsran_mbsfn_cfg_t* cfg, uint32_t phy_tti);
  bool                    is_mcch_subframe(srsran_mbsfn_cfg_t* cfg, uint32_t phy_tti);
};

} // namespace srsenb

#endif // SRSENB_PHCH_COMMON_H
