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

#ifndef SRSENB_CC_WORKER_H
#define SRSENB_CC_WORKER_H

#include <string.h>

#include "../phy_common.h"
#include "srsran/srslog/srslog.h"

#define LOG_EXECTIME

namespace srsenb {
namespace lte {

class cc_worker
{
public:
  cc_worker(srslog::basic_logger& logger);
  ~cc_worker();
  void init(phy_common* phy, uint32_t cc_idx);
  void reset();

  cf_t* get_buffer_rx(uint32_t antenna_idx);
  cf_t* get_buffer_tx(uint32_t antenna_idx);
  void  set_tti(uint32_t tti);

  int      add_rnti(uint16_t rnti);
  void     rem_rnti(uint16_t rnti);
  uint32_t get_nof_rnti();

  /* These are used by the GUI plotting tools */
  int  read_ce_abs(float* ce_abs);
  int  read_ce_arg(float* ce_abs);
  int  read_pusch_d(cf_t* pusch_d);
  int  read_pucch_d(cf_t* pusch_d);
  void start_plot();

  void work_ul(const srsran_ul_sf_cfg_t& ul_sf, stack_interface_phy_lte::ul_sched_t& ul_grants);
  void work_dl(const srsran_dl_sf_cfg_t&            dl_sf_cfg,
               stack_interface_phy_lte::dl_sched_t& dl_grants,
               stack_interface_phy_lte::ul_sched_t& ul_grants,
               srsran_mbsfn_cfg_t*                  mbsfn_cfg);

  uint32_t get_metrics(std::vector<phy_metrics_t>& metrics);

private:
  constexpr static float PUSCH_RL_SNR_DB_TH = 1.0f;
  constexpr static float PUCCH_RL_CORR_TH   = 0.15f;

  int  encode_pdsch(stack_interface_phy_lte::dl_sched_grant_t* grants, uint32_t nof_grants);
  int  encode_pmch(stack_interface_phy_lte::dl_sched_grant_t* grant, srsran_mbsfn_cfg_t* mbsfn_cfg);
  bool decode_pusch_rnti(stack_interface_phy_lte::ul_sched_grant_t& ul_grant,
                         srsran_ul_cfg_t&                           ul_cfg,
                         srsran_pusch_res_t&                        pusch_res);
  void decode_pusch(stack_interface_phy_lte::ul_sched_grant_t* grants, uint32_t nof_pusch);
  int  encode_phich(stack_interface_phy_lte::ul_sched_ack_t* acks, uint32_t nof_acks);
  int  encode_pdcch_dl(stack_interface_phy_lte::dl_sched_grant_t* grants, uint32_t nof_grants);
  int  encode_pdcch_ul(stack_interface_phy_lte::ul_sched_grant_t* grants, uint32_t nof_grants);
  int  decode_pucch();

  /* Common objects */
  srslog::basic_logger& logger;
  phy_common*           phy       = nullptr;
  bool                  initiated = false;

  cf_t*    signal_buffer_rx[SRSRAN_MAX_PORTS] = {};
  cf_t*    signal_buffer_tx[SRSRAN_MAX_PORTS] = {};
  uint32_t tti_rx = 0, tti_tx_dl = 0, tti_tx_ul = 0;

  srsran_enb_dl_t enb_dl = {};
  srsran_enb_ul_t enb_ul = {};

  srsran_dl_sf_cfg_t dl_sf = {};
  srsran_ul_sf_cfg_t ul_sf = {};

  srsran_softbuffer_tx_t temp_mbsfn_softbuffer = {};

  // Class to store user information
  class ue
  {
  public:
    explicit ue(uint16_t rnti_) : rnti(rnti_)
    {
      // Do nothing
    }

    srsran_phich_grant_t phich_grant = {};

    void     metrics_read(phy_metrics_t* metrics);
    void     metrics_dl(uint32_t mcs);
    void     metrics_ul(uint32_t mcs, float rssi, float sinr, float turbo_iters);
    void     metrics_ul_pucch(float sinr);
    uint32_t get_rnti() const { return rnti; }

  private:
    uint32_t      rnti    = 0;
    phy_metrics_t metrics = {};
  };

  // Component carrier index
  uint32_t cc_idx = 0;

  // Each worker keeps a local copy of the user database. Uses more memory but more efficient to manage concurrency
  std::map<uint16_t, ue*> ue_db;
  std::mutex              mutex;
};

} // namespace lte
} // namespace srsenb

#endif // SRSENB_CC_WORKER_H
