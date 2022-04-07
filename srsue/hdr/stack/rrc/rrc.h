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

#ifndef SRSUE_RRC_H
#define SRSUE_RRC_H

#include "rrc_cell.h"
#include "rrc_config.h"
#include "rrc_metrics.h"
#include "rrc_rlf_report.h"
#include "srsran/asn1/rrc_utils.h"
#include "srsran/common/bcd_helpers.h"
#include "srsran/common/block_queue.h"
#include "srsran/common/buffer_pool.h"
#include "srsran/common/common.h"
#include "srsran/common/common_lte.h"
#include "srsran/common/security.h"
#include "srsran/common/stack_procedure.h"
#include "srsran/interfaces/ue_interfaces.h"
#include "srsran/rrc/rrc_common.h"
#include "srsran/srslog/srslog.h"

#include <map>
#include <math.h>
#include <queue>

using srsran::byte_buffer_t;

namespace srsue {

class phy_controller;
class usim_interface_rrc;
class gw_interface_rrc;
class pdcp_interface_rrc;
class rlc_interface_rrc;
class nas_interface_rrc;
class phy_interface_rrc_lte;

class rrc : public rrc_interface_nas,
            public rrc_interface_phy_lte,
            public rrc_interface_mac,
            public rrc_interface_pdcp,
            public rrc_eutra_interface_rrc_nr,
            public rrc_interface_rlc,
            public srsran::timer_callback
{
public:
  rrc(stack_interface_rrc* stack_, srsran::task_sched_handle task_sched_);
  ~rrc();

  void init(phy_interface_rrc_lte* phy_,
            mac_interface_rrc*     mac_,
            rlc_interface_rrc*     rlc_,
            pdcp_interface_rrc*    pdcp_,
            nas_interface_rrc*     nas_,
            usim_interface_rrc*    usim_,
            gw_interface_rrc*      gw_,
            rrc_nr_interface_rrc*  rrc_nr_,
            const rrc_args_t&      args_);

  void stop();

  void get_metrics(rrc_metrics_t& m);

  // Timeout callback interface
  void timer_expired(uint32_t timeout_id) final;
  void srsran_rrc_log(const char* str);

  typedef enum { Rx = 0, Tx } direction_t;
  template <class T>
  void log_rrc_message(const std::string            source,
                       const direction_t            dir,
                       const srsran::byte_buffer_t* pdu,
                       const T&                     msg,
                       const std::string&           msg_type);

  std::string print_mbms();
  bool        mbms_service_start(uint32_t serv, uint32_t port);

  // NAS interface
  void     write_sdu(srsran::unique_byte_buffer_t sdu);
  void     enable_capabilities();
  uint16_t get_mcc();
  uint16_t get_mnc();
  bool     plmn_search() final;
  void     plmn_select(srsran::plmn_id_t plmn_id);
  bool     connection_request(srsran::establishment_cause_t cause, srsran::unique_byte_buffer_t dedicated_info_nas);
  void     set_ue_identity(srsran::s_tmsi_t s_tmsi);
  void     paging_completed(bool outcome) final;
  bool     has_nr_dc();

  // NR interface
  void new_cell_meas_nr(const std::vector<phy_meas_nr_t>& meas);
  void nr_rrc_con_reconfig_complete(bool status);

  // PHY interface
  void in_sync() final;
  void out_of_sync() final;
  void new_cell_meas(const std::vector<phy_meas_t>& meas);
  void cell_search_complete(cell_search_ret_t ret, phy_cell_t found_cell);
  void cell_select_complete(bool status);
  void set_config_complete(bool status);
  void set_scell_complete(bool status);

  // MAC interface
  void ra_completed() final;
  void release_pucch_srs();
  void run_tti();
  void ra_problem();

  // GW interface
  bool is_connected() final; // this is also NAS interface
  bool have_drb();

  // PDCP interface
  void write_pdu(uint32_t lcid, srsran::unique_byte_buffer_t pdu);
  void write_pdu_bcch_bch(srsran::unique_byte_buffer_t pdu);
  void write_pdu_bcch_dlsch(srsran::unique_byte_buffer_t pdu);
  void write_pdu_pcch(srsran::unique_byte_buffer_t pdu);
  void write_pdu_mch(uint32_t lcid, srsran::unique_byte_buffer_t pdu);
  void notify_pdcp_integrity_error(uint32_t lcid);

  bool srbs_flushed(); //< Check if data on SRBs still needs to be sent

protected:
  // Moved to protected to be accessible by unit tests
  void set_serving_cell(phy_cell_t phy_cell, bool discard_serving);
  bool has_neighbour_cell(uint32_t earfcn, uint32_t pci) const;
  bool is_serving_cell(uint32_t earfcn, uint32_t pci) const;
  int  start_cell_select();

  bool has_neighbour_cell_nr(uint32_t earfcn, uint32_t pci) const;

private:
  typedef struct {
    enum { PCCH, RLF, RA_COMPLETE, STOP } command;
    srsran::unique_byte_buffer_t pdu;
    uint16_t                     lcid;
  } cmd_msg_t;

  bool                           running = false;
  srsran::block_queue<cmd_msg_t> cmd_q;

  void process_pcch(srsran::unique_byte_buffer_t pdu);

  stack_interface_rrc*         stack = nullptr;
  srsran::task_sched_handle    task_sched;
  srslog::basic_logger&        logger;
  phy_interface_rrc_lte*       phy    = nullptr;
  mac_interface_rrc*           mac    = nullptr;
  rlc_interface_rrc*           rlc    = nullptr;
  pdcp_interface_rrc*          pdcp   = nullptr;
  nas_interface_rrc*           nas    = nullptr;
  usim_interface_rrc*          usim   = nullptr;
  gw_interface_rrc*            gw     = nullptr;
  rrc_nr_interface_rrc*        rrc_nr = nullptr;
  srsran::unique_byte_buffer_t dedicated_info_nas;

  void send_ul_ccch_msg(const asn1::rrc::ul_ccch_msg_s& msg);
  void send_ul_dcch_msg(uint32_t lcid, const asn1::rrc::ul_dcch_msg_s& msg);

  rrc_state_t      state = RRC_STATE_IDLE, last_state = RRC_STATE_IDLE;
  uint8_t          transaction_id = 0;
  srsran::s_tmsi_t ue_identity;
  bool             ue_identity_configured = false;

  // PHY controller state machine
  std::unique_ptr<phy_controller> phy_ctrl;

  rrc_args_t args = {};

  uint32_t cell_clean_cnt = 0;

  srsran::phy_cfg_t previous_phy_cfg = {};
  srsran::mac_cfg_t current_mac_cfg, previous_mac_cfg = {};

  void                         generate_as_keys();
  srsran::as_security_config_t sec_cfg = {};

  std::map<uint32_t, asn1::rrc::srb_to_add_mod_s> srbs;
  std::map<uint32_t, asn1::rrc::drb_to_add_mod_s> drbs;

  // RRC constants and timers
  uint32_t                            n310_cnt = 0, N310 = 0;
  uint32_t                            n311_cnt = 0, N311 = 0;
  srsran::timer_handler::unique_timer t300, t301, t302, t310, t311, t304;

  static const std::string rb_id_str[];

  const char* get_rb_name(uint32_t lcid) { return srsran::is_lte_rb(lcid) ? rb_id_str[lcid].c_str() : "invalid RB"; }

  // Var-RLF-Report class
  rrc_rlf_report var_rlf_report;

  // Measurements private subclass
  class rrc_meas;
  std::unique_ptr<rrc_meas> measurements;

  // List of strongest neighbour cell
  using unique_cell_t = std::unique_ptr<meas_cell_eutra>;
  meas_cell_list<meas_cell_eutra> meas_cells;

  meas_cell_list<meas_cell_nr> meas_cells_nr;

  bool                     initiated                  = false;
  asn1::rrc::reest_cause_e m_reest_cause              = asn1::rrc::reest_cause_e::nulltype;
  uint16_t                 m_reest_rnti               = 0;
  uint16_t                 m_reest_source_pci         = 0;
  bool                     reestablishment_started    = false;
  bool                     reestablishment_successful = false;

  // Interface from rrc_meas
  void               send_srb1_msg(const asn1::rrc::ul_dcch_msg_s& msg);
  std::set<uint32_t> get_cells(const uint32_t earfcn);
  float              get_cell_rsrp(const uint32_t earfcn, const uint32_t pci);
  float              get_cell_rsrq(const uint32_t earfcn, const uint32_t pci);
  meas_cell_eutra*   get_serving_cell();

  std::set<uint32_t> get_cells_nr(const uint32_t arfcn_nr);
  float              get_cell_rsrp_nr(const uint32_t arfcn_nr, const uint32_t pci_nr);
  float              get_cell_rsrq_nr(const uint32_t arfcn_nr, const uint32_t pci_nr);

  void                                          process_cell_meas();
  void                                          process_new_cell_meas(const std::vector<phy_meas_t>& meas);
  srsran::block_queue<std::vector<phy_meas_t> > cell_meas_q;

  void                                             process_cell_meas_nr();
  void                                             process_new_cell_meas_nr(const std::vector<phy_meas_nr_t>& meas);
  srsran::block_queue<std::vector<phy_meas_nr_t> > cell_meas_nr_q;

  // Cell selection/reselection functions/variables
  typedef struct {
    float Qrxlevmin;
    float Qrxlevminoffset;
    float Qqualmin;
    float Qqualminoffset;
    float s_intrasearchP;
    float q_hyst;
    float threshservinglow;
  } cell_resel_cfg_t;

  cell_resel_cfg_t cell_resel_cfg = {};

  float get_srxlev(float Qrxlevmeas);
  float get_squal(float Qqualmeas);

  /********************
   *  RRC Procedures
   *******************/

  enum class cs_result_t { changed_cell, same_cell, no_cell };

  // RRC procedures (fwd declared)
  class cell_search_proc;
  class si_acquire_proc;
  class serving_cell_config_proc;
  class cell_selection_proc;
  class connection_setup_proc;
  class connection_request_proc;
  class connection_reconf_no_ho_proc;
  class plmn_search_proc;
  class process_pcch_proc;
  class go_idle_proc;
  class cell_reselection_proc;
  class connection_reest_proc;
  class ho_proc;
  srsran::proc_t<cell_search_proc, rrc_interface_phy_lte::cell_search_ret_t> cell_searcher;
  srsran::proc_t<si_acquire_proc>                                            si_acquirer;
  srsran::proc_t<serving_cell_config_proc>                                   serv_cell_cfg;
  srsran::proc_t<cell_selection_proc, cs_result_t>                           cell_selector;
  srsran::proc_t<go_idle_proc>                                               idle_setter;
  srsran::proc_t<process_pcch_proc>                                          pcch_processor;
  srsran::proc_t<connection_request_proc>                                    conn_req_proc;
  srsran::proc_t<connection_setup_proc>                                      conn_setup_proc;
  srsran::proc_t<plmn_search_proc>                                           plmn_searcher;
  srsran::proc_t<cell_reselection_proc>                                      cell_reselector;
  srsran::proc_t<connection_reest_proc>                                      connection_reest;
  srsran::proc_t<ho_proc>                                                    ho_handler;
  srsran::proc_t<connection_reconf_no_ho_proc>                               conn_recfg_proc;

  srsran::proc_manager_list_t callback_list;

  bool cell_selection_criteria(float rsrp, float rsrq = 0);
  void cell_reselection(float rsrp, float rsrq);

  std::vector<uint32_t> ue_required_sibs;
  srsran::plmn_id_t     selected_plmn_id = {};
  bool                  plmn_is_selected = false;

  bool security_is_activated = false;

  // RLC interface
  void max_retx_attempted();
  void protocol_failure();

  // RRC NR interface
  void nr_scg_failure_information(const srsran::scg_failure_cause_t cause);
  void nr_notify_reconfiguration_failure();

  // Senders
  void send_con_request(srsran::establishment_cause_t cause);
  void send_con_restablish_request(asn1::rrc::reest_cause_e cause, uint16_t rnti, uint16_t pci, uint32_t cellid);
  void send_con_restablish_complete();
  void send_con_setup_complete(srsran::unique_byte_buffer_t nas_msg);
  void send_ul_info_transfer(srsran::unique_byte_buffer_t nas_msg);
  void send_security_mode_complete();
  void send_rrc_con_reconfig_complete(bool contains_nr_complete = false);

  // Parsers
  void process_pdu(uint32_t lcid, srsran::unique_byte_buffer_t pdu);
  void parse_dl_ccch(srsran::unique_byte_buffer_t pdu);
  void parse_dl_dcch(uint32_t lcid, srsran::unique_byte_buffer_t pdu);
  void parse_dl_info_transfer(uint32_t lcid, srsran::unique_byte_buffer_t pdu);
  void parse_pdu_bcch_dlsch(srsran::unique_byte_buffer_t pdu);
  void parse_pdu_mch(uint32_t lcid, srsran::unique_byte_buffer_t pdu);

  // Helpers
  void con_reconfig_failed();
  bool con_reconfig_ho(const asn1::rrc::rrc_conn_recfg_s& reconfig);
  void ho_failed();
  void start_go_idle();
  void rrc_connection_release(const std::string& cause);
  void radio_link_failure_push_cmd();
  void radio_link_failure_process();
  void leave_connected();
  void stop_timers();
  void start_con_restablishment(asn1::rrc::reest_cause_e cause);

  void log_rr_config_common();
  void log_phy_config_dedicated();
  void log_mac_config_dedicated();

  void apply_rr_config_common(asn1::rrc::rr_cfg_common_s* config, bool send_lower_layers);
  bool apply_rr_config_dedicated(const asn1::rrc::rr_cfg_ded_s* cnfg, bool is_handover = false);
  bool apply_rr_config_dedicated_on_ho_complete(const asn1::rrc::rr_cfg_ded_s& cnfg);
  void apply_scell_config(asn1::rrc::rrc_conn_recfg_r8_ies_s* reconfig_r8, bool enable_cqi);
  bool apply_scell_config_on_ho_complete(const asn1::rrc::rrc_conn_recfg_r8_ies_s& reconfig_r8);
  void apply_phy_config_dedicated(const asn1::rrc::phys_cfg_ded_s& phy_cnfg, bool is_handover);
  void apply_phy_scell_config(const asn1::rrc::scell_to_add_mod_r10_s& scell_config, bool enable_cqi);

  void apply_mac_config_dedicated_default();

  void handle_sib1();
  void handle_sib2();
  void handle_sib3();
  void handle_sib13();

  void     handle_con_setup(const asn1::rrc::rrc_conn_setup_s& setup);
  void     handle_con_reest(const asn1::rrc::rrc_conn_reest_s& setup);
  void     handle_rrc_con_reconfig(uint32_t lcid, const asn1::rrc::rrc_conn_recfg_s& reconfig);
  void     handle_ue_capability_enquiry(const asn1::rrc::ue_cap_enquiry_s& enquiry);
  void     handle_ue_info_request(const ue_info_request_r9_s& request);
  void     add_srb(const asn1::rrc::srb_to_add_mod_s& srb_cnfg);
  void     add_drb(const asn1::rrc::drb_to_add_mod_s& drb_cnfg);
  void     release_drb(uint32_t drb_id);
  uint32_t get_lcid_for_drb_id(const uint32_t& drb_id);
  uint32_t get_lcid_for_eps_bearer(const uint32_t& eps_bearer_id);
  uint32_t get_drb_id_for_eps_bearer(const uint32_t& eps_bearer_id);
  uint32_t get_eps_bearer_id_for_drb_id(const uint32_t& drb_id);
  void     add_mrb(uint32_t lcid, uint32_t port);

  // Helpers for setting default values
  void set_phy_default_pucch_srs();
  void set_phy_default();
  void set_mac_default();
  void set_rrc_default();

  bool nr_reconfiguration_proc(const asn1::rrc::rrc_conn_recfg_r8_ies_s& rx_recfg, bool* has_5g_nr_reconfig);

  // Helpers for nr communicaiton
  asn1::rrc::ue_cap_rat_container_s get_eutra_nr_capabilities();
  asn1::rrc::ue_cap_rat_container_s get_nr_capabilities();
};

} // namespace srsue

#endif // SRSUE_RRC_H
