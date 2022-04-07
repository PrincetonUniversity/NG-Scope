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

#ifndef SRSUE_UL_HARQ_H
#define SRSUE_UL_HARQ_H

#include "mux.h"
#include "proc_ra.h"
#include "srsran/common/interfaces_common.h"
#include "srsran/common/mac_pcap.h"
#include "srsran/common/timers.h"
#include "ul_sps.h"

using namespace srsran;

namespace srsue {

class ul_harq_entity
{
public:
  ul_harq_entity(const uint8_t cc_idx_);

  bool init(ue_rnti* rntis_, ra_proc* ra_proc_h_, mux* mux_unit_);

  void reset();
  void reset_ndi();
  void set_config(srsran::ul_harq_cfg_t& harq_cfg);

  void start_pcap(srsran::mac_pcap* pcap_);

  /***************** PHY->MAC interface for UL processes **************************/
  void new_grant_ul(mac_interface_phy_lte::mac_grant_ul_t grant, mac_interface_phy_lte::tb_action_ul_t* action);

  int   get_current_tbs(uint32_t pid);
  float get_average_retx();

private:
  class ul_harq_process
  {
  public:
    ul_harq_process();
    ~ul_harq_process();

    bool init(uint32_t pid_, ul_harq_entity* parent);
    void reset();
    void reset_ndi();

    uint32_t get_rv();
    bool     has_grant();
    bool     get_ndi();
    bool     is_sps();

    uint32_t get_nof_retx();
    int      get_current_tbs();

    // Implements Section 5.4.2.1
    void new_grant_ul(mac_interface_phy_lte::mac_grant_ul_t grant, mac_interface_phy_lte::tb_action_ul_t* action);

  private:
    /// Thread safe wrapper for a mac_grant_ul_t object.
    class lockable_grant
    {
      mac_interface_phy_lte::mac_grant_ul_t grant = {};
      mutable std::mutex                    mutex;

    public:
      void set(const mac_interface_phy_lte::mac_grant_ul_t& other)
      {
        std::lock_guard<std::mutex> lock(mutex);
        grant = other;
      }

      void reset()
      {
        std::lock_guard<std::mutex> lock(mutex);
        grant = {};
      }

      void set_ndi(bool ndi)
      {
        std::lock_guard<std::mutex> lock(mutex);
        grant.tb.ndi = ndi;
      }

      bool get_ndi() const
      {
        std::lock_guard<std::mutex> lock(mutex);
        return grant.tb.ndi;
      }

      uint32_t get_tbs() const
      {
        std::lock_guard<std::mutex> lock(mutex);
        return grant.tb.tbs;
      }

      int get_rv() const
      {
        std::lock_guard<std::mutex> lock(mutex);
        return grant.tb.rv;
      }

      void set_rv(int rv)
      {
        std::lock_guard<std::mutex> lock(mutex);
        grant.tb.rv = rv;
      }
    };
    lockable_grant cur_grant;

    uint32_t              pid;
    std::atomic<uint32_t> current_tx_nb       = {0};
    std::atomic<uint32_t> current_irv         = {0};
    std::atomic<bool>     harq_feedback       = {false};
    std::atomic<bool>     is_grant_configured = {false};
    bool                  is_initiated;

    srslog::basic_logger&  logger;
    ul_harq_entity*        harq_entity;
    srsran_softbuffer_tx_t softbuffer;

    const static int               payload_buffer_len = 128 * 1024;
    std::unique_ptr<byte_buffer_t> payload_buffer     = nullptr;
    uint8_t*                       pdu_ptr;

    void generate_tx(mac_interface_phy_lte::tb_action_ul_t* action);
    void generate_retx(mac_interface_phy_lte::mac_grant_ul_t grant, mac_interface_phy_lte::tb_action_ul_t* action);
    void generate_new_tx(mac_interface_phy_lte::mac_grant_ul_t grant, mac_interface_phy_lte::tb_action_ul_t* action);
  };

  ul_sps ul_sps_assig;

  std::vector<ul_harq_process> proc;

  mux*                  mux_unit = nullptr;
  srsran::mac_pcap*     pcap     = nullptr;
  srslog::basic_logger& logger;

  ue_rnti* rntis = nullptr;

  srsran::ul_harq_cfg_t harq_cfg = {};
  std::mutex            config_mutex;

  std::atomic<float>    average_retx{0};
  std::atomic<uint64_t> nof_pkts{0};
  ra_proc*              ra_procedure = nullptr;

  uint8_t cc_idx = 0;
};

typedef std::unique_ptr<ul_harq_entity>                     ul_harq_entity_ptr;
typedef std::array<ul_harq_entity_ptr, SRSRAN_MAX_CARRIERS> ul_harq_entity_vector;

} // namespace srsue

#endif // SRSUE_UL_HARQ_H
