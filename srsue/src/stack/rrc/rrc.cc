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

#include "srsue/hdr/stack/rrc/rrc.h"
#include "srsran/asn1/rrc.h"
#include "srsran/common/bcd_helpers.h"
#include "srsran/common/security.h"
#include "srsran/common/standard_streams.h"
#include "srsran/interfaces/ue_gw_interfaces.h"
#include "srsran/interfaces/ue_nas_interfaces.h"
#include "srsran/interfaces/ue_pdcp_interfaces.h"
#include "srsran/interfaces/ue_rlc_interfaces.h"
#include "srsran/interfaces/ue_usim_interfaces.h"
#include "srsue/hdr/stack/rrc/phy_controller.h"
#include "srsue/hdr/stack/rrc/rrc_meas.h"
#include "srsue/hdr/stack/rrc/rrc_procedures.h"

#include <cstdlib>
#include <ctime>
#include <inttypes.h> // for printing uint64_t
#include <iostream>
#include <math.h>
#include <numeric>
#include <string.h>

std::atomic<bool> simulate_rlf{false};

using namespace srsran;
using namespace asn1::rrc;
using srsran::lte_srb;
using srsran::srb_to_lcid;

namespace srsue {

const static uint32_t NOF_REQUIRED_SIBS                = 4;
const static uint32_t required_sibs[NOF_REQUIRED_SIBS] = {0, 1, 2, 12}; // SIB1, SIB2, SIB3 and SIB13 (eMBMS)

/*******************************************************************************
  Base functions
*******************************************************************************/

rrc::rrc(stack_interface_rrc* stack_, srsran::task_sched_handle task_sched_) :
  stack(stack_),
  task_sched(task_sched_),
  state(RRC_STATE_IDLE),
  last_state(RRC_STATE_CONNECTED),
  logger(srslog::fetch_basic_logger("RRC")),
  measurements(new rrc_meas()),
  cell_searcher(this),
  si_acquirer(this),
  serv_cell_cfg(this),
  cell_selector(this),
  idle_setter(this),
  pcch_processor(this),
  conn_req_proc(this),
  plmn_searcher(this),
  cell_reselector(this),
  connection_reest(this),
  conn_setup_proc(this),
  ho_handler(this),
  conn_recfg_proc(this),
  meas_cells_nr(task_sched_),
  meas_cells(task_sched_)
{}

rrc::~rrc() = default;

template <class T>
void rrc::log_rrc_message(const std::string    source,
                          const direction_t    dir,
                          const byte_buffer_t* pdu,
                          const T&             msg,
                          const std::string&   msg_type)
{
  if (logger.debug.enabled()) {
    asn1::json_writer json_writer;
    msg.to_json(json_writer);
    logger.debug(pdu->msg,
                 pdu->N_bytes,
                 "%s - %s %s (%d B)",
                 source.c_str(),
                 (dir == Rx) ? "Rx" : "Tx",
                 msg_type.c_str(),
                 pdu->N_bytes);
    logger.debug("Content:\n%s", json_writer.to_string().c_str());
  } else if (logger.info.enabled()) {
    logger.info("%s - %s %s (%d B)", source.c_str(), (dir == Rx) ? "Rx" : "Tx", msg_type.c_str(), pdu->N_bytes);
  }
}

void rrc::init(phy_interface_rrc_lte* phy_,
               mac_interface_rrc*     mac_,
               rlc_interface_rrc*     rlc_,
               pdcp_interface_rrc*    pdcp_,
               nas_interface_rrc*     nas_,
               usim_interface_rrc*    usim_,
               gw_interface_rrc*      gw_,
               rrc_nr_interface_rrc*  rrc_nr_,
               const rrc_args_t&      args_)
{
  phy    = phy_;
  mac    = mac_;
  rlc    = rlc_;
  pdcp   = pdcp_;
  nas    = nas_;
  usim   = usim_;
  gw     = gw_;
  rrc_nr = rrc_nr_;
  args   = args_;

  auto on_every_cell_selection = [this](uint32_t earfcn, uint32_t pci, bool csel_result) {
    if (not csel_result) {
      meas_cell_eutra* c = meas_cells.find_cell(earfcn, pci);
      if (c != nullptr) {
        c->set_rsrp(-INFINITY);
      }
    }
  };
  phy_ctrl.reset(new phy_controller{phy, task_sched, on_every_cell_selection});

  state            = RRC_STATE_IDLE;
  plmn_is_selected = false;

  security_is_activated = false;

  t300 = task_sched.get_unique_timer();
  t301 = task_sched.get_unique_timer();
  t302 = task_sched.get_unique_timer();
  t310 = task_sched.get_unique_timer();
  t311 = task_sched.get_unique_timer();
  t304 = task_sched.get_unique_timer();

  var_rlf_report.init(task_sched);

  transaction_id = 0;

  cell_clean_cnt = 0;

  // Set default values for RRC. MAC and PHY are set to default themselves
  set_rrc_default();

  measurements->init(this);

  struct timeval tv;
  gettimeofday(&tv, NULL);
  logger.info("using srand seed of %ld", tv.tv_usec);

  // set seed (used in CHAP auth and attach)
  srand(tv.tv_usec);

  // initiate unique procedures
  ue_required_sibs.assign(&required_sibs[0], &required_sibs[NOF_REQUIRED_SIBS]);
  running   = true;
  initiated = true;
}

void rrc::stop()
{
  running = false;
  stop_timers();
  cmd_msg_t msg;
  msg.command = cmd_msg_t::STOP;
  cmd_q.push(std::move(msg));
}

void rrc::get_metrics(rrc_metrics_t& m)
{
  m.state = state;
  // Save strongest cells metrics
  for (auto& c : meas_cells) {
    phy_meas_t meas = {};
    meas.cfo_hz     = c->get_cfo_hz();
    meas.earfcn     = c->get_earfcn();
    meas.rsrq       = c->get_rsrq();
    meas.rsrp       = c->get_rsrp();
    meas.pci        = c->get_pci();
    m.neighbour_cells.push_back(meas);
  }
}

bool rrc::is_connected()
{
  return (RRC_STATE_CONNECTED == state);
}

/*
 *
 * RRC State Machine
 *
 */
void rrc::run_tti()
{
  if (!initiated) {
    return;
  }

  if (simulate_rlf.load(std::memory_order_relaxed)) {
    radio_link_failure_process();
    simulate_rlf.store(false, std::memory_order_relaxed);
  }

  // Process pending PHY measurements in IDLE/CONNECTED
  process_cell_meas();

  process_cell_meas_nr();

  // Process on-going callbacks, and clear finished callbacks
  callback_list.run();

  // Log state changes
  if (state != last_state) {
    logger.debug("State %s", rrc_state_text[state]);
    last_state = state;
  }

  // Run state machine
  switch (state) {
    case RRC_STATE_IDLE:
      break;
    case RRC_STATE_CONNECTED:
      measurements->run_tti();
      break;
    default:
      break;
  }

  // Handle Received Messages
  if (running) {
    cmd_msg_t msg;
    if (cmd_q.try_pop(&msg)) {
      switch (msg.command) {
        case cmd_msg_t::PCCH:
          process_pcch(std::move(msg.pdu));
          break;
        case cmd_msg_t::RLF:
          radio_link_failure_process();
          break;
        case cmd_msg_t::RA_COMPLETE:
          if (ho_handler.is_busy()) {
            ho_handler.trigger(ho_proc::ra_completed_ev{msg.lcid > 0});
          }
          break;
        case cmd_msg_t::STOP:
          return;
      }
    }
  }

  // Clean old neighbours
  cell_clean_cnt++;
  if (cell_clean_cnt == 1000) {
    meas_cells.clean_neighbours();
    cell_clean_cnt = 0;
  }
}

/*******************************************************************************
 *
 *
 *
 * NAS interface: PLMN search and RRC connection establishment
 *
 *
 *
 *******************************************************************************/

uint16_t rrc::get_mcc()
{
  return meas_cells.serving_cell().get_mcc();
}

uint16_t rrc::get_mnc()
{
  return meas_cells.serving_cell().get_mnc();
}

/* NAS interface to search for available PLMNs.
 * It goes through all known frequencies, synchronizes and receives SIB1 for each to extract PLMN.
 * The function is blocking and waits until all frequencies have been
 * searched and PLMNs are obtained.
 *
 * This function is thread-safe with connection_request()
 */
bool rrc::plmn_search()
{
  if (not plmn_searcher.launch()) {
    logger.error("Unable to initiate PLMN search");
    return false;
  }
  callback_list.add_proc(plmn_searcher);
  return true;
}

/* This is the NAS interface. When NAS requests to select a PLMN we have to
 * connect to either register or because there is pending higher layer traffic.
 */
void rrc::plmn_select(srsran::plmn_id_t plmn_id)
{
  plmn_is_selected = true;
  selected_plmn_id = plmn_id;

  logger.info("PLMN Selected %s", plmn_id.to_string().c_str());
}

/* 5.3.3.2 Initiation of RRC Connection Establishment procedure
 *
 * Higher layers request establishment of RRC connection while UE is in RRC_IDLE
 *
 * This procedure selects a suitable cell for transmission of RRCConnectionRequest and configures
 * it. Sends connectionRequest message and returns if message transmitted successfully.
 * It does not wait until completition of Connection Establishment procedure
 */
bool rrc::connection_request(srsran::establishment_cause_t cause, srsran::unique_byte_buffer_t dedicated_info_nas_)
{
  if (not conn_req_proc.launch(cause, std::move(dedicated_info_nas_))) {
    logger.error("Failed to initiate connection request procedure");
    return false;
  }
  callback_list.add_proc(conn_req_proc);
  return true;
}

void rrc::set_ue_identity(srsran::s_tmsi_t s_tmsi)
{
  ue_identity_configured = true;
  ue_identity            = s_tmsi;
  logger.info(
      "Set ue-Identity to 0x%" PRIu64 ":0x%" PRIu64 "", (uint64_t)ue_identity.mmec, (uint64_t)ue_identity.m_tmsi);
}

/*******************************************************************************
 *
 *
 *
 * PHY interface: neighbour and serving cell measurements and out-of-sync/in-sync
 *
 *
 *
 *******************************************************************************/

void rrc::cell_search_complete(rrc_interface_phy_lte::cell_search_ret_t cs_ret, phy_cell_t found_cell)
{
  phy_ctrl->cell_search_completed(cs_ret, found_cell);
}

void rrc::cell_select_complete(bool cs_ret)
{
  phy_ctrl->cell_selection_completed(cs_ret);
}

void rrc::set_config_complete(bool status)
{
  // Signal Reconfiguration Procedure that PHY configuration has completed
  phy_ctrl->set_config_complete();
  if (conn_setup_proc.is_busy()) {
    conn_setup_proc.trigger(status);
  }
  if (conn_recfg_proc.is_busy()) {
    conn_recfg_proc.trigger(status);
  }
}

void rrc::set_scell_complete(bool status) {}

/* This function is called from a NR PHY worker thus must return very quickly.
 * Queue the values of the measurements and process them from the RRC thread
 */
void rrc::new_cell_meas_nr(const std::vector<phy_meas_nr_t>& meas)
{
  cell_meas_nr_q.push(meas);
}

/* Processes all pending PHY measurements in queue.
 */
void rrc::process_cell_meas_nr()
{
  std::vector<phy_meas_nr_t> m;
  while (cell_meas_nr_q.try_pop(&m)) {
    if (cell_meas_nr_q.size() > 0) {
      logger.debug("MEAS:  Processing measurement. %zd measurements in queue", cell_meas_q.size());
    }
    process_new_cell_meas_nr(m);
  }
}

void rrc::process_new_cell_meas_nr(const std::vector<phy_meas_nr_t>& meas)
{
  // Convert vector
  std::vector<phy_meas_t> meas_lte;
  for (const auto& m : meas) {
    phy_meas_t tmp_meas = {};
    tmp_meas.cfo_hz     = m.cfo_hz;
    tmp_meas.earfcn     = m.arfcn_nr;
    tmp_meas.rsrp       = m.rsrp;
    tmp_meas.rsrq       = m.rsrp;
    tmp_meas.pci        = m.pci_nr;
    meas_lte.push_back(tmp_meas);
  }
  const std::function<void(meas_cell_nr&, const phy_meas_t&)> filter = [this](meas_cell_nr& c, const phy_meas_t& m) {
    c.set_rsrp(measurements->rsrp_filter(m.rsrp, c.get_rsrp()));
    c.set_rsrq(measurements->rsrq_filter(m.rsrq, c.get_rsrq()));
    c.set_cfo(m.cfo_hz);
  };

  logger.debug("MEAS:  Processing measurement of %zd cells", meas.size());

  bool neighbour_added = meas_cells_nr.process_new_cell_meas(meas_lte, filter);
}

void rrc::nr_rrc_con_reconfig_complete(bool status)
{
  if (conn_recfg_proc.is_busy()) {
    conn_recfg_proc.trigger(status);
  }
}

/* This function is called from a PHY worker thus must return very quickly.
 * Queue the values of the measurements and process them from the RRC thread
 */
void rrc::new_cell_meas(const std::vector<phy_meas_t>& meas)
{
  cell_meas_q.push(meas);
}
/* Processes all pending PHY measurements in queue.
 */
void rrc::process_cell_meas()
{
  std::vector<phy_meas_t> m;
  while (cell_meas_q.try_pop(&m)) {
    if (cell_meas_q.size() > 0) {
      logger.debug("MEAS:  Processing measurement. %zd measurements in queue", cell_meas_q.size());
    }
    process_new_cell_meas(m);
  }
}

void rrc::process_new_cell_meas(const std::vector<phy_meas_t>& meas)
{
  const std::function<void(meas_cell_eutra&, const phy_meas_t&)> filter = [this](meas_cell_eutra&  c,
                                                                                 const phy_meas_t& m) {
    c.set_rsrp(measurements->rsrp_filter(m.rsrp, c.get_rsrp()));
    c.set_rsrq(measurements->rsrq_filter(m.rsrq, c.get_rsrq()));
    c.set_cfo(m.cfo_hz);
  };

  logger.debug("MEAS:  Processing measurement of %zd cells", meas.size());

  bool neighbour_added = meas_cells.process_new_cell_meas(meas, filter);

  // Instruct measurements subclass to update phy with new cells to measure based on strongest neighbours
  if (state == RRC_STATE_CONNECTED && neighbour_added) {
    measurements->update_phy();
  }
}

// Detection of physical layer problems in RRC_CONNECTED (5.3.11.1)
void rrc::out_of_sync()
{
  // CAUTION: We do not lock in this function since they are called from real-time threads
  if (meas_cells.serving_cell().is_valid()) {
    phy_ctrl->out_sync();

    // upon receiving N310 consecutive "out-of-sync" indications for the PCell from lower layers while neither T300,
    //   T301, T304 nor T311 is running:
    if (state == RRC_STATE_CONNECTED) {
      // upon receiving N310 consecutive "out-of-sync" indications from lower layers while neither T300, T301, T304
      // nor T311 is running
      bool t311_running = t311.is_running() || connection_reest.is_busy();
      if (!t300.is_running() and !t301.is_running() and !t304.is_running() and !t310.is_running() and !t311_running) {
        logger.info("Received out-of-sync while in state %s. n310=%d, t311=%s, t310=%s",
                    rrc_state_text[state],
                    n310_cnt,
                    t311.is_running() ? "running" : "stop",
                    t310.is_running() ? "running" : "stop");
        n310_cnt++;
        if (n310_cnt == N310) {
          logger.info(
              "Detected %d out-of-sync from PHY. Trying to resync. Starting T310 timer %d ms", N310, t310.duration());
          t310.run();
          n310_cnt = 0;
        }
      }
    }
  }
}

// Recovery of physical layer problems (5.3.11.2)
void rrc::in_sync()
{
  // CAUTION: We do not lock in this function since they are called from real-time threads
  phy_ctrl->in_sync();
  if (t310.is_running()) {
    n311_cnt++;
    if (n311_cnt == N311) {
      t310.stop();
      n311_cnt = 0;
      logger.info("Detected %d in-sync from PHY. Stopping T310 timer", N311);
    }
  }
}

/*******************************************************************************
 *
 *
 *
 * Cell selection, reselection and neighbour cell database management
 *
 *
 *
 *******************************************************************************/

// Cell selection criteria Section 5.2.3.2 of 36.304
bool rrc::cell_selection_criteria(float rsrp, float rsrq)
{
  return std::isnormal(rsrp) and meas_cells.serving_cell().has_sib3() and get_srxlev(rsrp) > 0;
}

float rrc::get_srxlev(float Qrxlevmeas)
{
  // TODO: Do max power limitation
  float Pcompensation = 0;
  return Qrxlevmeas - (cell_resel_cfg.Qrxlevmin + cell_resel_cfg.Qrxlevminoffset) - Pcompensation;
}

float rrc::get_squal(float Qqualmeas)
{
  return Qqualmeas - (cell_resel_cfg.Qqualmin + cell_resel_cfg.Qqualminoffset);
}

// Cell reselection in IDLE Section 5.2.4 of 36.304
void rrc::cell_reselection(float rsrp, float rsrq)
{
  // Intra-frequency cell-reselection criteria

  if (get_srxlev(rsrp) > cell_resel_cfg.s_intrasearchP && rsrp > -95.0) {
    // UE may not perform intra-frequency measurements->
    phy->meas_stop();
  } else {
    // UE must start intra-frequency measurements
    auto pci = meas_cells.get_neighbour_pcis(meas_cells.serving_cell().get_earfcn());
    phy->set_cells_to_meas(meas_cells.serving_cell().get_earfcn(), pci);
  }

  // TODO: Inter-frequency cell reselection
}

// Set new serving cell
void rrc::set_serving_cell(phy_cell_t phy_cell, bool discard_serving)
{
  meas_cells.set_serving_cell(phy_cell, discard_serving);
}

int rrc::start_cell_select()
{
  if (not cell_selector.launch()) {
    logger.error("Failed to initiate a Cell Selection procedure...");
    return SRSRAN_ERROR;
  }
  callback_list.add_proc(cell_selector);
  return SRSRAN_SUCCESS;
}

bool rrc::has_neighbour_cell(uint32_t earfcn, uint32_t pci) const
{
  return meas_cells.has_neighbour_cell(earfcn, pci);
}

bool rrc::has_neighbour_cell_nr(uint32_t earfcn, uint32_t pci) const
{
  return meas_cells_nr.has_neighbour_cell(earfcn, pci);
}

bool rrc::is_serving_cell(uint32_t earfcn, uint32_t pci) const
{
  return meas_cells.serving_cell().phy_cell.earfcn == earfcn and meas_cells.serving_cell().phy_cell.pci == pci;
}

/*******************************************************************************
 *
 *
 *
 * eMBMS Related Functions
 *
 *
 *
 *******************************************************************************/
std::string rrc::print_mbms()
{
  mcch_msg_type_c   msg = meas_cells.serving_cell().mcch.msg;
  std::stringstream ss;
  for (uint32_t i = 0; i < msg.c1().mbsfn_area_cfg_r9().pmch_info_list_r9.size(); i++) {
    ss << "PMCH: " << i << std::endl;
    pmch_info_r9_s* pmch = &msg.c1().mbsfn_area_cfg_r9().pmch_info_list_r9[i];
    for (uint32_t j = 0; j < pmch->mbms_session_info_list_r9.size(); j++) {
      mbms_session_info_r9_s* sess = &pmch->mbms_session_info_list_r9[j];
      ss << "  Service ID: " << sess->tmgi_r9.service_id_r9.to_string();
      if (sess->session_id_r9_present) {
        ss << ", Session ID: " << sess->session_id_r9.to_string();
      }
      if (sess->tmgi_r9.plmn_id_r9.type() == tmgi_r9_s::plmn_id_r9_c_::types::explicit_value_r9) {
        ss << ", MCC: " << mcc_bytes_to_string(&sess->tmgi_r9.plmn_id_r9.explicit_value_r9().mcc[0]);
        ss << ", MNC: " << mnc_bytes_to_string(sess->tmgi_r9.plmn_id_r9.explicit_value_r9().mnc);
      } else {
        ss << ", PLMN index: " << sess->tmgi_r9.plmn_id_r9.plmn_idx_r9();
      }
      ss << ", LCID: " << sess->lc_ch_id_r9 << std::endl;
    }
  }
  return ss.str();
}

bool rrc::mbms_service_start(uint32_t serv, uint32_t port)
{
  bool ret = false;
  if (!meas_cells.serving_cell().has_mcch) {
    logger.error("MCCH not available at MBMS Service Start");
    return ret;
  }

  logger.info("%s", print_mbms().c_str());

  mcch_msg_type_c msg = meas_cells.serving_cell().mcch.msg;
  for (uint32_t i = 0; i < msg.c1().mbsfn_area_cfg_r9().pmch_info_list_r9.size(); i++) {
    pmch_info_r9_s* pmch = &msg.c1().mbsfn_area_cfg_r9().pmch_info_list_r9[i];
    for (uint32_t j = 0; j < pmch->mbms_session_info_list_r9.size(); j++) {
      mbms_session_info_r9_s* sess = &pmch->mbms_session_info_list_r9[j];
      if (serv == sess->tmgi_r9.service_id_r9.to_number()) {
        srsran::console("MBMS service started. Service id=%d, port=%d, lcid=%d\n", serv, port, sess->lc_ch_id_r9);
        ret = true;
        add_mrb(sess->lc_ch_id_r9, port);
      }
    }
  }
  return ret;
}

/*******************************************************************************
 *
 *
 *
 * Other functions
 *
 *
 *
 *******************************************************************************/

/*
 * 5.3.11.3 Detection of RLF
 * The RLF procedure starts:
 *   - upon T310 expiry;
 *   - upon random access problem indication from MAC while neither T300, T301, T304 nor T311 is running; or
 *   - upon indication from RLC that the maximum number of retransmissions has been reached:
 */
void rrc::radio_link_failure_push_cmd()
{
  cmd_msg_t msg;
  msg.command = cmd_msg_t::RLF;
  cmd_q.push(std::move(msg));
}

/*
 * Perform the actions upon detection of radio link failure (5.3.11.3)
 * This function must be executed from the main RRC task to avoid stack loops
 */
void rrc::radio_link_failure_process()
{
  // TODO: Generate and store failure report
  srsran::console("Warning: Detected Radio-Link Failure\n");

  // Store the information in VarRLF-Report
  var_rlf_report.set_failure(meas_cells, rrc_rlf_report::rlf);

  if (state == RRC_STATE_CONNECTED) {
    if (security_is_activated) {
      logger.info("Detected Radio-Link Failure with active AS security. Starting ConnectionReestablishment...");
      start_con_restablishment(reest_cause_e::other_fail);
    } else {
      logger.info("Detected Radio-Link Failure with AS security deactivated. Going to IDLE...");
      start_go_idle();
    }
  } else {
    logger.info("Detected Radio-Link Failure while RRC_IDLE. Ignoring it.");
  }
}

/* Reception of PUCCH/SRS release procedure (Section 5.3.13) */
void rrc::release_pucch_srs()
{
  // Apply default configuration for PUCCH (CQI and SR) and SRS (release)
  if (initiated) {
    set_phy_default_pucch_srs();
  }
}

void rrc::ra_problem()
{
  if (not t300.is_running() and not t301.is_running() and not t304.is_running() and not t311.is_running()) {
    logger.warning("MAC indicated RA problem. Starting RLF");
    radio_link_failure_push_cmd();
  } else {
    logger.warning("MAC indicated RA problem but either T300, T301, T304 or T311 is running. Ignoring it.");
  }
}

void rrc::max_retx_attempted()
{
  // TODO: Handle the radio link failure
  logger.warning("Max RLC reTx attempted. Starting RLF");
  radio_link_failure_push_cmd();
}

void rrc::protocol_failure()
{
  logger.warning("RLC protocol failure detected");
}

void rrc::timer_expired(uint32_t timeout_id)
{
  if (timeout_id == t310.id()) {
    logger.info("Timer T310 expired: Radio Link Failure");
    radio_link_failure_push_cmd();
  } else if (timeout_id == t311.id()) {
    srsran::console("Timer T311 expired: Going to RRC IDLE\n");
    if (connection_reest.is_idle()) {
      logger.info("Timer T311 expired: Going to RRC IDLE");
      start_go_idle();
    } else {
      // Do nothing, this is handled by the procedure
      connection_reest.trigger(connection_reest_proc::t311_expiry{});
    }
  } else if (timeout_id == t301.id()) {
    if (state == RRC_STATE_IDLE) {
      logger.info("Timer T301 expired: Already in IDLE.");
    } else {
      logger.info("Timer T301 expired: Going to RRC IDLE");
      connection_reest.trigger(connection_reest_proc::t301_expiry{});
    }
  } else if (timeout_id == t302.id()) {
    logger.info("Timer T302 expired. Informing NAS about barrier alleviation");
    nas->set_barring(srsran::barring_t::none);
  } else if (timeout_id == t300.id()) {
    // Do nothing, handled in connection_request()
  } else if (timeout_id == t304.id()) {
    srsran::console("Timer t304 expired: Handover failed\n");
    logger.info("Timer t304 expired: Handover failed");
    ho_failed();
  } else {
    logger.error("Timeout from unknown timer id %d", timeout_id);
  }
}

bool rrc::nr_reconfiguration_proc(const rrc_conn_recfg_r8_ies_s& rx_recfg, bool* has_5g_nr_reconfig)
{
  if (!(rx_recfg.non_crit_ext_present && rx_recfg.non_crit_ext.non_crit_ext_present &&
        rx_recfg.non_crit_ext.non_crit_ext.non_crit_ext_present &&
        rx_recfg.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext_present &&
        rx_recfg.non_crit_ext.non_crit_ext.non_crit_ext_present &&
        rx_recfg.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext_present &&
        rx_recfg.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext_present &&
        rx_recfg.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext_present &&
        rx_recfg.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext
            .non_crit_ext_present)) {
    return true;
  }

  const asn1::rrc::rrc_conn_recfg_v1510_ies_s* rrc_conn_recfg_v1510_ies =
      &rx_recfg.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext;

  if (!rrc_conn_recfg_v1510_ies->nr_cfg_r15_present) {
    return true;
  }

  bool                endc_release_and_add_r15                = false;
  bool                nr_secondary_cell_group_cfg_r15_present = false;
  asn1::dyn_octstring nr_secondary_cell_group_cfg_r15;
  bool                sk_counter_r15_present           = false;
  uint32_t            sk_counter_r15                   = 0;
  bool                nr_radio_bearer_cfg1_r15_present = false;
  asn1::dyn_octstring nr_radio_bearer_cfg1_r15;

  switch (rrc_conn_recfg_v1510_ies->nr_cfg_r15.type()) {
    case setup_opts::options::release:
      logger.info("NR config R15 of type release");
      break;
    case setup_opts::options::setup:
      endc_release_and_add_r15 = rrc_conn_recfg_v1510_ies->nr_cfg_r15.setup().endc_release_and_add_r15;
      if (rrc_conn_recfg_v1510_ies->nr_cfg_r15.setup().nr_secondary_cell_group_cfg_r15_present) {
        nr_secondary_cell_group_cfg_r15_present = true;
        nr_secondary_cell_group_cfg_r15 = rrc_conn_recfg_v1510_ies->nr_cfg_r15.setup().nr_secondary_cell_group_cfg_r15;
      }
      break;
    default:
      logger.error("NR config R15 type not defined");
      break;
  }
  if (rrc_conn_recfg_v1510_ies->sk_counter_r15_present) {
    sk_counter_r15_present = true;
    sk_counter_r15         = rrc_conn_recfg_v1510_ies->sk_counter_r15;
  }

  if (rrc_conn_recfg_v1510_ies->nr_radio_bearer_cfg1_r15_present) {
    nr_radio_bearer_cfg1_r15_present = true;
    nr_radio_bearer_cfg1_r15         = rrc_conn_recfg_v1510_ies->nr_radio_bearer_cfg1_r15;
  }
  *has_5g_nr_reconfig = true;
  return rrc_nr->rrc_reconfiguration(endc_release_and_add_r15,
                                     nr_secondary_cell_group_cfg_r15_present,
                                     nr_secondary_cell_group_cfg_r15,
                                     sk_counter_r15_present,
                                     sk_counter_r15,
                                     nr_radio_bearer_cfg1_r15_present,
                                     nr_radio_bearer_cfg1_r15);
}
/*******************************************************************************
 *
 *
 *
 * Connection Control: Establishment, Reconfiguration, Reestablishment and Release
 *
 *
 *
 *******************************************************************************/

void rrc::send_con_request(srsran::establishment_cause_t cause)
{
  logger.debug("Preparing RRC Connection Request");

  // Prepare ConnectionRequest packet
  ul_ccch_msg_s              ul_ccch_msg;
  rrc_conn_request_r8_ies_s* rrc_conn_req =
      &ul_ccch_msg.msg.set_c1().set_rrc_conn_request().crit_exts.set_rrc_conn_request_r8();

  if (ue_identity_configured) {
    rrc_conn_req->ue_id.set_s_tmsi();
    srsran::to_asn1(&rrc_conn_req->ue_id.s_tmsi(), ue_identity);
  } else {
    rrc_conn_req->ue_id.set_random_value();
    // TODO use proper RNG
    uint64_t random_id = 0;
    for (uint i = 0; i < 5; i++) { // fill random ID bytewise, 40 bits = 5 bytes
      random_id |= ((uint64_t)rand() & 0xFF) << i * 8;
    }
    rrc_conn_req->ue_id.random_value().from_number(random_id);
  }
  rrc_conn_req->establishment_cause = (establishment_cause_opts::options)cause;

  send_ul_ccch_msg(ul_ccch_msg);
}

/* RRC connection re-establishment procedure (5.3.7.4) */
void rrc::send_con_restablish_request(reest_cause_e cause, uint16_t crnti, uint16_t pci, uint32_t cellid)
{
  // Clean reestablishment type
  reestablishment_successful = false;

  // set the reestablishmentCellId in the VarRLF-Report to the global cell identity of the selected cell;
  var_rlf_report.set_reest_gci(meas_cells.serving_cell().get_cell_id_bit(), meas_cells.serving_cell().get_plmn_asn1(0));

  if (cause.value != reest_cause_opts::ho_fail) {
    if (cause.value != reest_cause_opts::other_fail) {
      pci = meas_cells.serving_cell().get_pci();
    }
    cellid = meas_cells.serving_cell().get_cell_id();
  }

  // Compute shortMAC-I
  uint8_t       varShortMAC_packed[16] = {};
  asn1::bit_ref bref(varShortMAC_packed, sizeof(varShortMAC_packed));

  // ASN.1 encode VarShortMAC-Input
  var_short_mac_input_s varmac;
  varmac.cell_id.from_number(cellid);
  varmac.pci = pci;
  varmac.c_rnti.from_number(crnti);
  varmac.pack(bref);
  uint32_t N_bits  = (uint32_t)bref.distance(varShortMAC_packed);
  uint32_t N_bytes = ((N_bits - 1) / 8 + 1);

  logger.info(
      "Encoded varShortMAC: cellId=0x%x, PCI=%d, rnti=0x%x (%d bytes, %d bits)", cellid, pci, crnti, N_bytes, N_bits);

  // Compute MAC-I
  uint8_t mac_key[4] = {};
  switch (sec_cfg.integ_algo) {
    case INTEGRITY_ALGORITHM_ID_128_EIA1:
      security_128_eia1(&sec_cfg.k_rrc_int[16],
                        0xffffffff, // 32-bit all to ones
                        0x1f,       // 5-bit all to ones
                        1,          // 1-bit to one
                        varShortMAC_packed,
                        N_bytes,
                        mac_key);
      break;
    case INTEGRITY_ALGORITHM_ID_128_EIA2:
      security_128_eia2(&sec_cfg.k_rrc_int[16],
                        0xffffffff, // 32-bit all to ones
                        0x1f,       // 5-bit all to ones
                        1,          // 1-bit to one
                        varShortMAC_packed,
                        N_bytes,
                        mac_key);
      break;
    case INTEGRITY_ALGORITHM_ID_128_EIA3:
      security_128_eia3(&sec_cfg.k_rrc_int[16],
                        0xffffffff, // 32-bit all to ones
                        0x1f,       // 5-bit all to ones
                        1,          // 1-bit to one
                        varShortMAC_packed,
                        N_bytes,
                        mac_key);
      break;
    default:
      logger.info("Unsupported integrity algorithm during reestablishment");
  }

  // Prepare ConnectionRestalishmentRequest packet
  asn1::rrc::ul_ccch_msg_s         ul_ccch_msg;
  rrc_conn_reest_request_r8_ies_s* rrc_conn_reest_req =
      &ul_ccch_msg.msg.set_c1().set_rrc_conn_reest_request().crit_exts.set_rrc_conn_reest_request_r8();

  rrc_conn_reest_req->ue_id.c_rnti.from_number(crnti);
  rrc_conn_reest_req->ue_id.pci = pci;
  rrc_conn_reest_req->ue_id.short_mac_i.from_number(mac_key[2] << 8 | mac_key[3]);
  rrc_conn_reest_req->reest_cause = cause;

  srsran::console("RRC Connection Reestablishment to PCI=%d, EARFCN=%d (Cause: \"%s\")\n",
                  meas_cells.serving_cell().phy_cell.pci,
                  meas_cells.serving_cell().phy_cell.earfcn,
                  cause.to_string());
  logger.info("RRC Connection Reestablishment to PCI=%d, EARFCN=%d (Cause: \"%s\")",
              meas_cells.serving_cell().phy_cell.pci,
              meas_cells.serving_cell().phy_cell.earfcn,
              cause.to_string());
  send_ul_ccch_msg(ul_ccch_msg);
}

void rrc::send_con_restablish_complete()
{
  logger.debug("Preparing RRC Connection Reestablishment Complete");
  srsran::console("RRC Connected\n");

  // Prepare ConnectionSetupComplete packet
  ul_dcch_msg_s ul_dcch_msg;
  ul_dcch_msg.msg.set_c1().set_rrc_conn_reest_complete().crit_exts.set_rrc_conn_reest_complete_r8();
  ul_dcch_msg.msg.c1().rrc_conn_reest_complete().rrc_transaction_id = transaction_id;

  // Include rlf-InfoAvailable
  if (var_rlf_report.has_info()) {
    ul_dcch_msg.msg.c1().rrc_conn_reest_complete().crit_exts.rrc_conn_reest_complete_r8().non_crit_ext_present = true;
    ul_dcch_msg.msg.c1()
        .rrc_conn_reest_complete()
        .crit_exts.rrc_conn_reest_complete_r8()
        .non_crit_ext.rlf_info_available_r9_present = true;
  }

  send_ul_dcch_msg(srb_to_lcid(lte_srb::srb1), ul_dcch_msg);

  reestablishment_successful = true;
}

void rrc::send_con_setup_complete(srsran::unique_byte_buffer_t nas_msg)
{
  logger.debug("Preparing RRC Connection Setup Complete");

  // Prepare ConnectionSetupComplete packet
  asn1::rrc::ul_dcch_msg_s          ul_dcch_msg;
  rrc_conn_setup_complete_r8_ies_s* rrc_conn_setup_complete =
      &ul_dcch_msg.msg.set_c1().set_rrc_conn_setup_complete().crit_exts.set_c1().set_rrc_conn_setup_complete_r8();

  ul_dcch_msg.msg.c1().rrc_conn_setup_complete().rrc_transaction_id = transaction_id;

  // Include rlf-InfoAvailable
  if (var_rlf_report.has_info()) {
    rrc_conn_setup_complete->non_crit_ext_present                                     = true;
    rrc_conn_setup_complete->non_crit_ext.non_crit_ext_present                        = true;
    rrc_conn_setup_complete->non_crit_ext.non_crit_ext.rlf_info_available_r10_present = true;
  }

  rrc_conn_setup_complete->sel_plmn_id = 1;
  rrc_conn_setup_complete->ded_info_nas.resize(nas_msg->N_bytes);
  memcpy(rrc_conn_setup_complete->ded_info_nas.data(), nas_msg->msg, nas_msg->N_bytes); // TODO Check!

  send_ul_dcch_msg(srb_to_lcid(lte_srb::srb1), ul_dcch_msg);
}

void rrc::send_ul_info_transfer(unique_byte_buffer_t nas_msg)
{
  uint32_t lcid = (uint32_t)(rlc->has_bearer(srb_to_lcid(lte_srb::srb2)) ? lte_srb::srb2 : lte_srb::srb1);

  // Prepare UL INFO packet
  asn1::rrc::ul_dcch_msg_s   ul_dcch_msg;
  ul_info_transfer_r8_ies_s* rrc_ul_info_transfer =
      &ul_dcch_msg.msg.set_c1().set_ul_info_transfer().crit_exts.set_c1().set_ul_info_transfer_r8();

  rrc_ul_info_transfer->ded_info_type.set_ded_info_nas();
  rrc_ul_info_transfer->ded_info_type.ded_info_nas().resize(nas_msg->N_bytes);
  memcpy(rrc_ul_info_transfer->ded_info_type.ded_info_nas().data(), nas_msg->msg, nas_msg->N_bytes); // TODO Check!

  send_ul_dcch_msg(lcid, ul_dcch_msg);
}

void rrc::send_security_mode_complete()
{
  logger.debug("Preparing Security Mode Complete");

  // Prepare Security Mode Command Complete
  ul_dcch_msg_s ul_dcch_msg;
  ul_dcch_msg.msg.set_c1().set_security_mode_complete().crit_exts.set_security_mode_complete_r8();
  ul_dcch_msg.msg.c1().security_mode_complete().rrc_transaction_id = transaction_id;

  send_ul_dcch_msg(srb_to_lcid(lte_srb::srb1), ul_dcch_msg);
}

void rrc::send_rrc_con_reconfig_complete(bool contains_nr_complete)
{
  logger.debug("Preparing RRC Connection Reconfig Complete");

  ul_dcch_msg_s                     ul_dcch_msg;
  rrc_conn_recfg_complete_r8_ies_s* rrc_conn_recfg_complete_r8 =
      &ul_dcch_msg.msg.set_c1().set_rrc_conn_recfg_complete().crit_exts.set_rrc_conn_recfg_complete_r8();
  ul_dcch_msg.msg.c1().rrc_conn_recfg_complete().rrc_transaction_id = transaction_id;

  // Include rlf-InfoAvailable
  if (var_rlf_report.has_info()) {
    rrc_conn_recfg_complete_r8->non_crit_ext_present                                     = true;
    rrc_conn_recfg_complete_r8->non_crit_ext.non_crit_ext_present                        = true;
    rrc_conn_recfg_complete_r8->non_crit_ext.non_crit_ext.rlf_info_available_r10_present = true;
  }

  if (contains_nr_complete == true) {
    logger.debug("Preparing RRC Connection Reconfig Complete with NR Complete");

    rrc_conn_recfg_complete_r8->non_crit_ext_present                                                     = true;
    rrc_conn_recfg_complete_r8->non_crit_ext.non_crit_ext_present                                        = true;
    rrc_conn_recfg_complete_r8->non_crit_ext.non_crit_ext.non_crit_ext_present                           = true;
    rrc_conn_recfg_complete_r8->non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext_present              = true;
    rrc_conn_recfg_complete_r8->non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext_present = true;

    rrc_conn_recfg_complete_v1430_ies_s* rrc_conn_recfg_complete_v1430_ies =
        &rrc_conn_recfg_complete_r8->non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext;

    rrc_conn_recfg_complete_v1430_ies->non_crit_ext_present                     = true;
    rrc_conn_recfg_complete_v1430_ies->non_crit_ext.scg_cfg_resp_nr_r15_present = true;
    rrc_conn_recfg_complete_v1430_ies->non_crit_ext.scg_cfg_resp_nr_r15.from_string("00");
  }
  send_ul_dcch_msg(srb_to_lcid(lte_srb::srb1), ul_dcch_msg);
}

void rrc::ra_completed()
{
  cmd_msg_t msg;
  msg.command = cmd_msg_t::RA_COMPLETE;
  msg.lcid    = 1;
  cmd_q.push(std::move(msg));
}

bool rrc::con_reconfig_ho(const rrc_conn_recfg_s& reconfig)
{
  if (not ho_handler.launch(reconfig)) {
    logger.error("Unable to launch Handover Preparation procedure");
    return false;
  }
  callback_list.add_proc(ho_handler);
  return true;
}

void rrc::start_go_idle()
{
  if (not idle_setter.launch()) {
    logger.info("Failed to set RRC to IDLE");
    return;
  }
  callback_list.add_proc(idle_setter);
}

// HO failure from T304 expiry 5.3.5.6
void rrc::ho_failed()
{
  ho_handler.trigger(ho_proc::t304_expiry{});

  // Store the information in VarRLF-Report
  var_rlf_report.set_failure(meas_cells, rrc_rlf_report::hof);

  start_con_restablishment(reest_cause_e::ho_fail);
}

// Reconfiguration failure or Section 5.3.5.5
void rrc::con_reconfig_failed()
{
  // Set previous PHY/MAC configuration
  phy_ctrl->set_cell_config(previous_phy_cfg, 0);
  mac->set_config(previous_mac_cfg);

  // And restore current configs
  current_mac_cfg = previous_mac_cfg;

  if (security_is_activated) {
    // Start the Reestablishment Procedure
    start_con_restablishment(reest_cause_e::recfg_fail);
  } else {
    start_go_idle();
  }
}

void rrc::handle_rrc_con_reconfig(uint32_t lcid, const rrc_conn_recfg_s& reconfig)
{
  previous_phy_cfg = phy_ctrl->current_cell_config()[0];
  previous_mac_cfg = current_mac_cfg;

  const rrc_conn_recfg_r8_ies_s& reconfig_r8 = reconfig.crit_exts.c1().rrc_conn_recfg_r8();
  if (reconfig_r8.mob_ctrl_info_present) {
    con_reconfig_ho(reconfig);
  } else {
    if (not conn_recfg_proc.launch(reconfig)) {
      logger.error("Unable to launch Handover Preparation procedure");
      return;
    }
    callback_list.add_proc(conn_recfg_proc);
  }
}

/* Actions upon reception of RRCConnectionRelease 5.3.8.3 */
void rrc::rrc_connection_release(const std::string& cause)
{
  // Save idleModeMobilityControlInfo, etc.
  srsran::console("Received RRC Connection Release (releaseCause: %s)\n", cause.c_str());

  if (has_nr_dc()) {
    rrc_nr->rrc_release();
  }

  // delay actions by 60ms as per 5.3.8.3
  task_sched.defer_callback(60, [this]() { start_go_idle(); });
}

/// TS 36.331, 5.3.12 - UE actions upon leaving RRC_CONNECTED
void rrc::leave_connected()
{
  srsran::console("RRC IDLE\n");
  logger.info("Leaving RRC_CONNECTED state");
  state                 = RRC_STATE_IDLE;
  security_is_activated = false;

  // 1> reset MAC;
  mac->reset();

  // 1> stop all timers that are running except T320;
  stop_timers();

  // 1> release all radio resources, including release of the RLC entity, the MAC configuration and the associated
  //    PDCP entity for all established RBs
  rlc->reset();
  pdcp->reset();
  set_mac_default();
  stack->reset_eps_bearers();

  // 1> indicate the release of the RRC connection to upper layers together with the release cause;
  nas->left_rrc_connected();

  // 1> if leaving RRC_CONNECTED was not triggered by reception of the MobilityFromEUTRACommand message:
  //    2> enter RRC_IDLE by performing cell selection in accordance with the cell selection process, defined for the
  //       case of leaving RRC_CONNECTED, as specified in TS 36.304 [4];
  logger.info("Going RRC_IDLE");
  if (phy->cell_is_camping()) {
    // Receive paging
    mac->pcch_start_rx();
  }
}

void rrc::stop_timers()
{
  t300.stop();
  t301.stop();
  t310.stop();
  t311.stop();
  t304.stop();
}

/* Implementation of procedure in 3GPP 36.331 Section 5.3.7.2: Initiation
 *
 * This procedure shall be only initiated when:
 *   - upon detecting radio link failure, in accordance with 5.3.11; or
 *   - upon handover failure, in accordance with 5.3.5.6; or
 *   - upon mobility from E-UTRA failure, in accordance with 5.4.3.5; or
 *   - upon integrity check failure indication from lower layers; or
 *   - upon an RRC connection reconfiguration failure, in accordance with 5.3.5.5;
 *
 *   The parameter cause shall indicate the cause of the reestablishment according to the sections mentioned adobe.
 */
void rrc::start_con_restablishment(reest_cause_e cause)
{
  if (not connection_reest.launch(cause)) {
    logger.info("Failed to launch connection re-establishment procedure");
  }

  callback_list.add_proc(connection_reest);
}

/**
 * Check whether data on SRB1 or SRB2 still needs to be sent.
 * If the bearer is suspended it will not be considered.
 *
 * @return True if no further data needs to be sent on SRBs, False otherwise
 */
bool rrc::srbs_flushed()
{
  // Check SRB1
  if (rlc->has_data(srb_to_lcid(lte_srb::srb1)) && not rlc->is_suspended(srb_to_lcid(lte_srb::srb1))) {
    return false;
  }

  // Check SRB2
  if (rlc->has_data(srb_to_lcid(lte_srb::srb2)) && not rlc->is_suspended(srb_to_lcid(lte_srb::srb2))) {
    return false;
  }

  return true;
}

/*******************************************************************************
 *
 * Interface from RRC measurements class
 *
 *******************************************************************************/
void rrc::send_srb1_msg(const ul_dcch_msg_s& msg)
{
  send_ul_dcch_msg(srb_to_lcid(lte_srb::srb1), msg);
}

std::set<uint32_t> rrc::get_cells(const uint32_t earfcn)
{
  return meas_cells.get_neighbour_pcis(earfcn);
}

float rrc::get_cell_rsrp(const uint32_t earfcn, const uint32_t pci)
{
  meas_cell_eutra* c = meas_cells.get_neighbour_cell_handle(earfcn, pci);
  return (c != nullptr) ? c->get_rsrp() : NAN;
}

float rrc::get_cell_rsrq(const uint32_t earfcn, const uint32_t pci)
{
  meas_cell_eutra* c = meas_cells.get_neighbour_cell_handle(earfcn, pci);
  return (c != nullptr) ? c->get_rsrq() : NAN;
}

meas_cell_eutra* rrc::get_serving_cell()
{
  return &meas_cells.serving_cell();
}

std::set<uint32_t> rrc::get_cells_nr(const uint32_t arfcn_nr)
{
  return meas_cells_nr.get_neighbour_pcis(arfcn_nr);
}

float rrc::get_cell_rsrp_nr(const uint32_t arfcn_nr, const uint32_t pci_nr)
{
  meas_cell_nr* c = meas_cells_nr.get_neighbour_cell_handle(arfcn_nr, pci_nr);
  return (c != nullptr) ? c->get_rsrp() : NAN;
}

float rrc::get_cell_rsrq_nr(const uint32_t arfcn_nr, const uint32_t pci_nr)
{
  meas_cell_nr* c = meas_cells_nr.get_neighbour_cell_handle(arfcn_nr, pci_nr);
  return (c != nullptr) ? c->get_rsrq() : NAN;
}

/*******************************************************************************
 *
 *
 *
 * Reception of Broadcast messages (MIB and SIBs)
 *
 *
 *
 *******************************************************************************/
void rrc::write_pdu_bcch_bch(unique_byte_buffer_t pdu)
{
  bcch_bch_msg_s    bch_msg;
  asn1::cbit_ref    bch_bref(pdu->msg, pdu->N_bytes);
  asn1::SRSASN_CODE err = bch_msg.unpack(bch_bref);

  if (err != asn1::SRSASN_SUCCESS) {
    logger.error("Could not unpack BCCH-BCH message.");
    return;
  }

  log_rrc_message("BCCH-BCH", Rx, pdu.get(), bch_msg, "MIB");

  // Nothing else to do ..
}

void rrc::write_pdu_bcch_dlsch(unique_byte_buffer_t pdu)
{
  parse_pdu_bcch_dlsch(std::move(pdu));
}

void rrc::parse_pdu_bcch_dlsch(unique_byte_buffer_t pdu)
{
  // Stop BCCH search after successful reception of 1 BCCH block
  mac->bcch_stop_rx();

  bcch_dl_sch_msg_s dlsch_msg;
  asn1::cbit_ref    dlsch_bref(pdu->msg, pdu->N_bytes);
  asn1::SRSASN_CODE err = dlsch_msg.unpack(dlsch_bref);

  if (err != asn1::SRSASN_SUCCESS or dlsch_msg.msg.type().value != bcch_dl_sch_msg_type_c::types_opts::c1) {
    logger.error(pdu->msg, pdu->N_bytes, "Could not unpack BCCH DL-SCH message (%d B).", pdu->N_bytes);
    return;
  }

  log_rrc_message("BCCH-DLSCH", Rx, pdu.get(), dlsch_msg, dlsch_msg.msg.c1().type().to_string());

  if (dlsch_msg.msg.c1().type() == bcch_dl_sch_msg_type_c::c1_c_::types::sib_type1) {
    logger.info("Processing SIB1 (1/1)");
    meas_cells.serving_cell().set_sib1(dlsch_msg.msg.c1().sib_type1());
    si_acquirer.trigger(si_acquire_proc::sib_received_ev{});
    handle_sib1();
  } else {
    sys_info_r8_ies_s::sib_type_and_info_l_& sib_list =
        dlsch_msg.msg.c1().sys_info().crit_exts.sys_info_r8().sib_type_and_info;
    for (uint32_t i = 0; i < sib_list.size(); ++i) {
      logger.info("Processing SIB%d (%d/%d)", sib_list[i].type().to_number(), i, sib_list.size());
      switch (sib_list[i].type().value) {
        case sib_info_item_c::types::sib2:
          if (not meas_cells.serving_cell().has_sib2()) {
            meas_cells.serving_cell().set_sib2(sib_list[i].sib2());
          }
          handle_sib2();
          si_acquirer.trigger(si_acquire_proc::sib_received_ev{});
          break;
        case sib_info_item_c::types::sib3:
          if (not meas_cells.serving_cell().has_sib3()) {
            meas_cells.serving_cell().set_sib3(sib_list[i].sib3());
          }
          handle_sib3();
          si_acquirer.trigger(si_acquire_proc::sib_received_ev{});
          break;
        case sib_info_item_c::types::sib13_v920:
          if (not meas_cells.serving_cell().has_sib13()) {
            meas_cells.serving_cell().set_sib13(sib_list[i].sib13_v920());
          }
          handle_sib13();
          si_acquirer.trigger(si_acquire_proc::sib_received_ev{});
          break;
        default:
          logger.warning("SIB%d is not supported", sib_list[i].type().to_number());
      }
    }
  }
}

void rrc::handle_sib1()
{
  const sib_type1_s* sib1 = meas_cells.serving_cell().sib1ptr();
  logger.info("SIB1 received, CellID=%d, si_window=%d, sib2_period=%d",
              meas_cells.serving_cell().get_cell_id() & 0xfff,
              sib1->si_win_len.to_number(),
              sib1->sched_info_list[0].si_periodicity.to_number());

  // Print SIB scheduling info
  for (uint32_t i = 0; i < sib1->sched_info_list.size(); ++i) {
    si_periodicity_r12_e p = sib1->sched_info_list[i].si_periodicity;
    for (uint32_t j = 0; j < sib1->sched_info_list[i].sib_map_info.size(); ++j) {
      sib_type_e t = sib1->sched_info_list[i].sib_map_info[j];
      logger.debug("SIB scheduling info, sib_type=%d, si_periodicity=%d", t.to_number(), p.to_number());
    }
  }

  // Set TDD Config
  if (sib1->tdd_cfg_present) {
    srsran_tdd_config_t tdd_config = {};
    tdd_config.sf_config           = sib1->tdd_cfg.sf_assign.to_number();
    tdd_config.ss_config           = sib1->tdd_cfg.special_sf_patterns.to_number();
    phy->set_config_tdd(tdd_config);
  }
}

void rrc::handle_sib2()
{
  logger.info("SIB2 received");

  const sib_type2_s* sib2 = meas_cells.serving_cell().sib2ptr();

  // Apply RACH and timeAlginmentTimer configuration
  set_mac_cfg_t_rach_cfg_common(&current_mac_cfg, sib2->rr_cfg_common.rach_cfg_common);
  set_mac_cfg_t_time_alignment(&current_mac_cfg, sib2->time_align_timer_common);
  mac->set_config(current_mac_cfg);

  // Set MBSFN configs
  if (sib2->mbsfn_sf_cfg_list_present) {
    srsran::mbsfn_sf_cfg_t list[ASN1_RRC_MAX_MBSFN_ALLOCS];
    for (uint32_t i = 0; i < sib2->mbsfn_sf_cfg_list.size(); ++i) {
      list[i] = srsran::make_mbsfn_sf_cfg(sib2->mbsfn_sf_cfg_list[i]);
    }
    phy->set_config_mbsfn_sib2(&list[0], sib2->mbsfn_sf_cfg_list.size());
  }

  // Apply PHY RR Config Common
  srsran::phy_cfg_t& current_pcell = phy_ctrl->current_cell_config()[0];
  set_phy_cfg_t_common_pdsch(&current_pcell, sib2->rr_cfg_common.pdsch_cfg_common);
  set_phy_cfg_t_common_pusch(&current_pcell, sib2->rr_cfg_common.pusch_cfg_common);
  set_phy_cfg_t_common_pucch(&current_pcell, sib2->rr_cfg_common.pucch_cfg_common);
  set_phy_cfg_t_common_pwr_ctrl(&current_pcell, sib2->rr_cfg_common.ul_pwr_ctrl_common);
  set_phy_cfg_t_common_prach(
      &current_pcell, &sib2->rr_cfg_common.prach_cfg.prach_cfg_info, sib2->rr_cfg_common.prach_cfg.root_seq_idx);
  set_phy_cfg_t_common_srs(&current_pcell, sib2->rr_cfg_common.srs_ul_cfg_common);

  // According to 3GPP 36.331 v12 UE-EUTRA-Capability field descriptions
  // Allow 64QAM for:
  //   ue-Category 5 and 8 when enable64QAM (without suffix)
  //   ue-CategoryUL 5 and 13 when enable64QAM (with suffix)
  // enable64QAM-v1270 shall be ignored if enable64QAM (without suffix) is false
  if (args.ue_category == 5 || (args.release >= 10 && args.ue_category == 8)) {
    set_phy_cfg_t_enable_64qam(&current_pcell, sib2->rr_cfg_common.pusch_cfg_common.pusch_cfg_basic.enable64_qam);
  } else if (args.release >= 12 && sib2->rr_cfg_common.pusch_cfg_common.pusch_cfg_basic.enable64_qam) {
    if (args.ue_category_ul == 5 || args.ue_category_ul == 13) {
      // ASN1 Generator simplifies enable64QAM-v1270 because it is an enumeration that is always true
      set_phy_cfg_t_enable_64qam(&current_pcell, sib2->rr_cfg_common.pusch_cfg_common_v1270.is_present());
    } else {
      set_phy_cfg_t_enable_64qam(&current_pcell, false);
    }
  } else {
    set_phy_cfg_t_enable_64qam(&current_pcell, false);
  }

  phy_ctrl->set_cell_config(current_pcell);

  log_rr_config_common();

  auto timer_expire_func = [this](uint32_t tid) { timer_expired(tid); };
  t300.set(sib2->ue_timers_and_consts.t300.to_number(), timer_expire_func);
  t301.set(sib2->ue_timers_and_consts.t301.to_number(), timer_expire_func);
  t310.set(sib2->ue_timers_and_consts.t310.to_number(), timer_expire_func);
  t311.set(sib2->ue_timers_and_consts.t311.to_number(), timer_expire_func);
  N310 = sib2->ue_timers_and_consts.n310.to_number();
  N311 = sib2->ue_timers_and_consts.n311.to_number();

  logger.info("Set Constants and Timers: N310=%d, N311=%d, t300=%d, t301=%d, t310=%d, t311=%d",
              N310,
              N311,
              t300.duration(),
              t301.duration(),
              t310.duration(),
              t311.duration());
}

void rrc::handle_sib3()
{
  logger.info("SIB3 received");

  const sib_type3_s* sib3 = meas_cells.serving_cell().sib3ptr();

  // cellReselectionInfoCommon
  cell_resel_cfg.q_hyst = sib3->cell_resel_info_common.q_hyst.to_number();

  // cellReselectionServingFreqInfo
  cell_resel_cfg.threshservinglow = sib3->thresh_serving_low_q_r9; // TODO: Check first if present

  // intraFreqCellReselectionInfo
  cell_resel_cfg.Qrxlevmin = sib3->intra_freq_cell_resel_info.q_rx_lev_min * 2; // multiply by two
  if (sib3->intra_freq_cell_resel_info.s_intra_search_present) {
    cell_resel_cfg.s_intrasearchP = sib3->intra_freq_cell_resel_info.s_intra_search;
  } else {
    cell_resel_cfg.s_intrasearchP = INFINITY;
  }
}

void rrc::handle_sib13()
{
  logger.info("SIB13 received");

  const sib_type13_r9_s* sib13 = meas_cells.serving_cell().sib13ptr();

  phy->set_config_mbsfn_sib13(srsran::make_sib13(*sib13));
  add_mrb(0, 0); // Add MRB0
}

/*******************************************************************************
 *
 *
 *
 * Reception of Paging messages
 *
 *
 *
 *******************************************************************************/
void rrc::write_pdu_pcch(unique_byte_buffer_t pdu)
{
  cmd_msg_t msg;
  msg.pdu     = std::move(pdu);
  msg.command = cmd_msg_t::PCCH;
  cmd_q.push(std::move(msg));
}

void rrc::paging_completed(bool outcome)
{
  pcch_processor.trigger(process_pcch_proc::paging_complete{outcome});
}

void rrc::process_pcch(unique_byte_buffer_t pdu)
{
  if (pdu->N_bytes <= 0 or pdu->N_bytes >= SRSRAN_MAX_BUFFER_SIZE_BITS) {
    logger.error(pdu->msg, pdu->N_bytes, "Dropping PCCH message with %d B", pdu->N_bytes);
    return;
  }

  pcch_msg_s     pcch_msg;
  asn1::cbit_ref bref(pdu->msg, pdu->N_bytes);
  if (pcch_msg.unpack(bref) != asn1::SRSASN_SUCCESS or pcch_msg.msg.type().value != pcch_msg_type_c::types_opts::c1) {
    logger.error(pdu->msg, pdu->N_bytes, "Failed to unpack PCCH message (%d B)", pdu->N_bytes);
    return;
  }

  log_rrc_message("PCCH", Rx, pdu.get(), pcch_msg, pcch_msg.msg.c1().type().to_string());

  if (not ue_identity_configured) {
    logger.warning("Received paging message but no ue-Identity is configured");
    return;
  }

  paging_s* paging = &pcch_msg.msg.c1().paging();
  if (paging->paging_record_list.size() > ASN1_RRC_MAX_PAGE_REC) {
    paging->paging_record_list.resize(ASN1_RRC_MAX_PAGE_REC);
  }

  if (not pcch_processor.launch(*paging)) {
    logger.error("Failed to launch process PCCH procedure");
    return;
  }

  // we do not care about the outcome
  callback_list.add_proc(pcch_processor);
}

void rrc::write_pdu_mch(uint32_t lcid, srsran::unique_byte_buffer_t pdu)
{
  if (pdu->N_bytes <= 0 or pdu->N_bytes >= SRSRAN_MAX_BUFFER_SIZE_BITS) {
    return;
  }
  // TODO: handle MCCH notifications and update MCCH
  if (0 != lcid or meas_cells.serving_cell().has_mcch) {
    return;
  }
  parse_pdu_mch(lcid, std::move(pdu));
}

void rrc::parse_pdu_mch(uint32_t lcid, srsran::unique_byte_buffer_t pdu)
{
  asn1::cbit_ref bref(pdu->msg, pdu->N_bytes);
  if (meas_cells.serving_cell().mcch.unpack(bref) != asn1::SRSASN_SUCCESS or
      meas_cells.serving_cell().mcch.msg.type().value != mcch_msg_type_c::types_opts::c1) {
    logger.error("Failed to unpack MCCH message");
    return;
  }
  meas_cells.serving_cell().has_mcch = true;
  phy->set_config_mbsfn_mcch(srsran::make_mcch_msg(meas_cells.serving_cell().mcch));
  log_rrc_message(
      "MCH", Rx, pdu.get(), meas_cells.serving_cell().mcch, meas_cells.serving_cell().mcch.msg.c1().type().to_string());
  if (args.mbms_service_id >= 0) {
    logger.info("Attempting to auto-start MBMS service %d", args.mbms_service_id);
    mbms_service_start(args.mbms_service_id, args.mbms_service_port);
  }
}

/*******************************************************************************
 *
 *
 * Packet processing
 *
 *
 *******************************************************************************/

void rrc::send_ul_ccch_msg(const ul_ccch_msg_s& msg)
{
  // Reset and reuse sdu buffer if provided
  unique_byte_buffer_t pdcp_buf = srsran::make_byte_buffer();
  if (pdcp_buf == nullptr) {
    logger.error("Couldn't allocate PDU in %s().", __FUNCTION__);
    return;
  }

  asn1::bit_ref bref(pdcp_buf->msg, pdcp_buf->get_tailroom());
  msg.pack(bref);
  bref.align_bytes_zero();
  pdcp_buf->N_bytes = (uint32_t)bref.distance_bytes(pdcp_buf->msg);
  pdcp_buf->set_timestamp();

  // Set UE contention resolution ID in MAC
  uint64_t uecri      = 0;
  uint8_t* ue_cri_ptr = (uint8_t*)&uecri;
  uint32_t nbytes     = 6;
  for (uint32_t i = 0; i < nbytes; i++) {
    ue_cri_ptr[nbytes - i - 1] = pdcp_buf->msg[i];
  }

  logger.debug("Setting UE contention resolution ID: %" PRIu64 "", uecri);
  mac->set_contention_id(uecri);

  uint32_t lcid = srb_to_lcid(lte_srb::srb0);
  log_rrc_message(get_rb_name(lcid), Tx, pdcp_buf.get(), msg, msg.msg.c1().type().to_string());

  rlc->write_sdu(lcid, std::move(pdcp_buf));
}

void rrc::send_ul_dcch_msg(uint32_t lcid, const ul_dcch_msg_s& msg)
{
  // Reset and reuse sdu buffer if provided
  unique_byte_buffer_t pdcp_buf = srsran::make_byte_buffer();
  if (pdcp_buf == nullptr) {
    logger.error("Couldn't allocate PDU in %s().", __FUNCTION__);
    return;
  }

  asn1::bit_ref bref(pdcp_buf->msg, pdcp_buf->get_tailroom());
  msg.pack(bref);
  bref.align_bytes_zero();
  pdcp_buf->N_bytes = (uint32_t)bref.distance_bytes(pdcp_buf->msg);
  pdcp_buf->set_timestamp();

  if (msg.msg.type() == ul_dcch_msg_type_c::types_opts::options::c1) {
    log_rrc_message(get_rb_name(lcid), Tx, pdcp_buf.get(), msg, msg.msg.c1().type().to_string());
  } else if (msg.msg.type() == ul_dcch_msg_type_c::types_opts::options::msg_class_ext) {
    if (msg.msg.msg_class_ext().type() == ul_dcch_msg_type_c::msg_class_ext_c_::types_opts::options::c2) {
      log_rrc_message(get_rb_name(lcid), Tx, pdcp_buf.get(), msg, msg.msg.msg_class_ext().c2().type().to_string());
    } else {
      log_rrc_message(get_rb_name(lcid), Tx, pdcp_buf.get(), msg, msg.msg.msg_class_ext().type().to_string());
    }
  }

  pdcp->write_sdu(lcid, std::move(pdcp_buf));
}

void rrc::write_sdu(srsran::unique_byte_buffer_t sdu)
{
  if (state == RRC_STATE_IDLE) {
    logger.warning("Received ULInformationTransfer SDU when in IDLE");
    return;
  }
  send_ul_info_transfer(std::move(sdu));
}

void rrc::write_pdu(uint32_t lcid, unique_byte_buffer_t pdu)
{
  process_pdu(lcid, std::move(pdu));
}

void rrc::notify_pdcp_integrity_error(uint32_t lcid)
{
  logger.warning("Received integrity protection failure indication, lcid=%u", lcid);
}

void rrc::process_pdu(uint32_t lcid, srsran::unique_byte_buffer_t pdu)
{
  logger.debug("RX PDU, LCID: %d", lcid);
  switch (static_cast<lte_srb>(lcid)) {
    case lte_srb::srb0:
      parse_dl_ccch(std::move(pdu));
      break;
    case lte_srb::srb1:
    case lte_srb::srb2:
      parse_dl_dcch(lcid, std::move(pdu));
      break;
    default:
      logger.error("RX PDU with invalid bearer id: %d", lcid);
      break;
  }
}

void rrc::parse_dl_ccch(unique_byte_buffer_t pdu)
{
  asn1::cbit_ref           bref(pdu->msg, pdu->N_bytes);
  asn1::rrc::dl_ccch_msg_s dl_ccch_msg;
  if (dl_ccch_msg.unpack(bref) != asn1::SRSASN_SUCCESS or
      dl_ccch_msg.msg.type().value != dl_ccch_msg_type_c::types_opts::c1) {
    logger.error(pdu->msg, pdu->N_bytes, "Failed to unpack DL-CCCH message (%d B)", pdu->N_bytes);
    return;
  }
  log_rrc_message(
      get_rb_name(srb_to_lcid(lte_srb::srb0)), Rx, pdu.get(), dl_ccch_msg, dl_ccch_msg.msg.c1().type().to_string());

  dl_ccch_msg_type_c::c1_c_* c1 = &dl_ccch_msg.msg.c1();
  switch (dl_ccch_msg.msg.c1().type().value) {
    case dl_ccch_msg_type_c::c1_c_::types::rrc_conn_reject: {
      // 5.3.3.8
      rrc_conn_reject_r8_ies_s* reject_r8 = &c1->rrc_conn_reject().crit_exts.c1().rrc_conn_reject_r8();
      logger.info("Received ConnectionReject. Wait time: %d", reject_r8->wait_time);
      srsran::console("Received ConnectionReject. Wait time: %d\n", reject_r8->wait_time);

      t300.stop();

      if (reject_r8->wait_time) {
        nas->set_barring(srsran::barring_t::all);
        t302.set(reject_r8->wait_time * 1000, [this](uint32_t tid) { timer_expired(tid); });
        t302.run();
      } else {
        // Perform the actions upon expiry of T302 if wait time is zero
        nas->set_barring(srsran::barring_t::none);
        start_go_idle();
      }
    } break;
    case dl_ccch_msg_type_c::c1_c_::types::rrc_conn_setup: {
      transaction_id                   = c1->rrc_conn_setup().rrc_transaction_id;
      rrc_conn_setup_s conn_setup_copy = c1->rrc_conn_setup();
      task_sched.defer_task([this, conn_setup_copy]() { handle_con_setup(conn_setup_copy); });
      break;
    }
    case dl_ccch_msg_type_c::c1_c_::types::rrc_conn_reest: {
      srsran::console("Reestablishment OK\n");
      transaction_id                   = c1->rrc_conn_reest().rrc_transaction_id;
      rrc_conn_reest_s conn_reest_copy = c1->rrc_conn_reest();
      task_sched.defer_task([this, conn_reest_copy]() { handle_con_reest(conn_reest_copy); });
      break;
    }
    /* Reception of RRCConnectionReestablishmentReject 5.3.7.8 */
    case dl_ccch_msg_type_c::c1_c_::types::rrc_conn_reest_reject:
      connection_reest.trigger(c1->rrc_conn_reest_reject());
      break;
    default:
      logger.error("The provided DL-CCCH message type is not recognized");
      break;
  }
}

void rrc::parse_dl_dcch(uint32_t lcid, unique_byte_buffer_t pdu)
{
  asn1::cbit_ref           bref(pdu->msg, pdu->N_bytes);
  asn1::rrc::dl_dcch_msg_s dl_dcch_msg;
  if (dl_dcch_msg.unpack(bref) != asn1::SRSASN_SUCCESS or
      dl_dcch_msg.msg.type().value != dl_dcch_msg_type_c::types_opts::c1) {
    logger.error(pdu->msg, pdu->N_bytes, "Failed to unpack DL-DCCH message (%d B)", pdu->N_bytes);
    return;
  }
  log_rrc_message(get_rb_name(lcid), Rx, pdu.get(), dl_dcch_msg, dl_dcch_msg.msg.c1().type().to_string());

  dl_dcch_msg_type_c::c1_c_* c1 = &dl_dcch_msg.msg.c1();
  switch (dl_dcch_msg.msg.c1().type().value) {
    case dl_dcch_msg_type_c::c1_c_::types::dl_info_transfer:
      pdu = srsran::make_byte_buffer();
      if (pdu == nullptr) {
        logger.error("Couldn't allocate PDU in %s().", __FUNCTION__);
        return;
      }
      pdu->N_bytes = c1->dl_info_transfer().crit_exts.c1().dl_info_transfer_r8().ded_info_type.ded_info_nas().size();
      memcpy(pdu->msg,
             c1->dl_info_transfer().crit_exts.c1().dl_info_transfer_r8().ded_info_type.ded_info_nas().data(),
             pdu->N_bytes);
      nas->write_pdu(lcid, std::move(pdu));
      break;
    case dl_dcch_msg_type_c::c1_c_::types::security_mode_cmd:
      transaction_id = c1->security_mode_cmd().rrc_transaction_id;

      sec_cfg.cipher_algo = (CIPHERING_ALGORITHM_ID_ENUM)c1->security_mode_cmd()
                                .crit_exts.c1()
                                .security_mode_cmd_r8()
                                .security_cfg_smc.security_algorithm_cfg.ciphering_algorithm.value;
      sec_cfg.integ_algo = (INTEGRITY_ALGORITHM_ID_ENUM)c1->security_mode_cmd()
                               .crit_exts.c1()
                               .security_mode_cmd_r8()
                               .security_cfg_smc.security_algorithm_cfg.integrity_prot_algorithm.value;

      logger.info("Received Security Mode Command eea: %s, eia: %s",
                  ciphering_algorithm_id_text[sec_cfg.cipher_algo],
                  integrity_algorithm_id_text[sec_cfg.integ_algo]);

      // Generate AS security keys
      generate_as_keys();
      security_is_activated = true;

      // Configure PDCP for security
      pdcp->config_security(lcid, sec_cfg);
      pdcp->enable_integrity(lcid, DIRECTION_TXRX);
      send_security_mode_complete();
      pdcp->enable_encryption(lcid, DIRECTION_TXRX);
      break;
    case dl_dcch_msg_type_c::c1_c_::types::rrc_conn_recfg: {
      transaction_id         = c1->rrc_conn_recfg().rrc_transaction_id;
      rrc_conn_recfg_s recfg = c1->rrc_conn_recfg();
      task_sched.defer_task([this, lcid, recfg]() { handle_rrc_con_reconfig(lcid, recfg); });
      break;
    }
    case dl_dcch_msg_type_c::c1_c_::types::ue_cap_enquiry:
      transaction_id = c1->ue_cap_enquiry().rrc_transaction_id;
      handle_ue_capability_enquiry(c1->ue_cap_enquiry());
      break;
    case dl_dcch_msg_type_c::c1_c_::types::rrc_conn_release:
      rrc_connection_release(c1->rrc_conn_release().crit_exts.c1().rrc_conn_release_r8().release_cause.to_string());
      break;
    case dl_dcch_msg_type_c::c1_c_::types::ue_info_request_r9:
      transaction_id = c1->ue_info_request_r9().rrc_transaction_id;
      handle_ue_info_request(c1->ue_info_request_r9());
      break;
    default:
      logger.error("The provided DL-CCCH message type is not recognized or supported");
      break;
  }
}

// Security helper used by Security Mode Command and Mobility handling routines
void rrc::generate_as_keys(void)
{
  uint8_t k_asme[32] = {};
  nas->get_k_asme(k_asme, 32);
  logger.debug(k_asme, 32, "UE K_asme");
  logger.debug("Generating K_enb with UL NAS COUNT: %d", nas->get_k_enb_count());
  usim->generate_as_keys(k_asme, nas->get_k_enb_count(), &sec_cfg);
  logger.info(sec_cfg.k_rrc_enc.data(), 32, "RRC encryption key - k_rrc_enc");
  logger.info(sec_cfg.k_rrc_int.data(), 32, "RRC integrity key  - k_rrc_int");
  logger.info(sec_cfg.k_up_enc.data(), 32, "UP encryption key  - k_up_enc");
}

/*******************************************************************************
 *
 *
 *
 * Capabilities Message
 *
 *
 *
 *******************************************************************************/
void rrc::enable_capabilities()
{
  bool enable_ul_64 = args.ue_category >= 5 &&
                      meas_cells.serving_cell().sib2ptr()->rr_cfg_common.pusch_cfg_common.pusch_cfg_basic.enable64_qam;
  logger.info("%s 64QAM PUSCH", enable_ul_64 ? "Enabling" : "Disabling");
}

void rrc::handle_ue_capability_enquiry(const ue_cap_enquiry_s& enquiry)
{
  logger.debug("Preparing UE Capability Info");

  ul_dcch_msg_s         ul_dcch_msg;
  ue_cap_info_r8_ies_s* info = &ul_dcch_msg.msg.set_c1().set_ue_cap_info().crit_exts.set_c1().set_ue_cap_info_r8();
  ul_dcch_msg.msg.c1().ue_cap_info().rrc_transaction_id = transaction_id;

  // resize container to fit all requested RATs
  info->ue_cap_rat_container_list.resize(enquiry.crit_exts.c1().ue_cap_enquiry_r8().ue_cap_request.size());
  uint32_t rat_idx = 0;

  for (uint32_t i = 0; i < enquiry.crit_exts.c1().ue_cap_enquiry_r8().ue_cap_request.size(); i++) {
    if (enquiry.crit_exts.c1().ue_cap_enquiry_r8().ue_cap_request[i] == rat_type_e::eutra) {
      // adding EUTRA caps
      info->ue_cap_rat_container_list[0].rat_type = rat_type_e::eutra;

      // Check UE config arguments bounds
      if (args.release < SRSRAN_RELEASE_MIN || args.release > SRSRAN_RELEASE_MAX) {
        uint32_t new_release = SRSRAN_MIN(SRSRAN_RELEASE_MAX, SRSRAN_MAX(SRSRAN_RELEASE_MIN, args.release));
        logger.error("Release is %d. It is out of bounds (%d ... %d), setting it to %d",
                     args.release,
                     SRSRAN_RELEASE_MIN,
                     SRSRAN_RELEASE_MAX,
                     new_release);
        args.release = new_release;
      }

      args.ue_category = (uint32_t)strtol(args.ue_category_str.c_str(), nullptr, 10);
      if (args.ue_category < SRSRAN_UE_CATEGORY_MIN || args.ue_category > SRSRAN_UE_CATEGORY_MAX) {
        uint32_t new_category =
            SRSRAN_MIN(SRSRAN_UE_CATEGORY_MAX, SRSRAN_MAX(SRSRAN_UE_CATEGORY_MIN, args.ue_category));
        logger.error("UE Category is %d. It is out of bounds (%d ... %d), setting it to %d",
                     args.ue_category,
                     SRSRAN_UE_CATEGORY_MIN,
                     SRSRAN_UE_CATEGORY_MAX,
                     new_category);
        args.ue_category = new_category;
      }

      ue_eutra_cap_s cap;
      cap.access_stratum_release = (access_stratum_release_e::options)(args.release - SRSRAN_RELEASE_MIN);
      cap.ue_category            = (uint8_t)((args.ue_category < 1 || args.ue_category > 5) ? 4 : args.ue_category);
      cap.pdcp_params.max_num_rohc_context_sessions_present     = false;
      cap.pdcp_params.supported_rohc_profiles.profile0x0001_r15 = false;
      cap.pdcp_params.supported_rohc_profiles.profile0x0002_r15 = false;
      cap.pdcp_params.supported_rohc_profiles.profile0x0003_r15 = false;
      cap.pdcp_params.supported_rohc_profiles.profile0x0004_r15 = false;
      cap.pdcp_params.supported_rohc_profiles.profile0x0006_r15 = false;
      cap.pdcp_params.supported_rohc_profiles.profile0x0101_r15 = false;
      cap.pdcp_params.supported_rohc_profiles.profile0x0102_r15 = false;
      cap.pdcp_params.supported_rohc_profiles.profile0x0103_r15 = false;
      cap.pdcp_params.supported_rohc_profiles.profile0x0104_r15 = false;

      cap.phy_layer_params.ue_specific_ref_sigs_supported = false;
      cap.phy_layer_params.ue_tx_ant_sel_supported        = false;

      cap.rf_params.supported_band_list_eutra.resize(args.nof_supported_bands);
      cap.meas_params.band_list_eutra.resize(args.nof_supported_bands);
      for (uint32_t k = 0; k < args.nof_supported_bands; k++) {
        cap.rf_params.supported_band_list_eutra[k].band_eutra  = args.supported_bands[k];
        cap.rf_params.supported_band_list_eutra[k].half_duplex = false;
        cap.meas_params.band_list_eutra[k].inter_freq_band_list.resize(1);
        cap.meas_params.band_list_eutra[k].inter_freq_band_list[0].inter_freq_need_for_gaps = true;
      }

      cap.feature_group_inds_present = true;
      cap.feature_group_inds.from_number(args.feature_group);

      ue_eutra_cap_v1280_ies_s* ue_eutra_cap_v1280_ies;
      ue_eutra_cap_v1360_ies_s* ue_eutra_cap_v1360_ies;
      ue_eutra_cap_v1450_ies_s* ue_eutra_cap_v1450_ies;
      if (args.release > 8) {
        ue_eutra_cap_v920_ies_s cap_v920;

        cap_v920.phy_layer_params_v920.enhanced_dual_layer_fdd_r9_present                        = false;
        cap_v920.phy_layer_params_v920.enhanced_dual_layer_tdd_r9_present                        = false;
        cap_v920.inter_rat_params_geran_v920.dtm_r9_present                                      = false;
        cap_v920.inter_rat_params_geran_v920.e_redirection_geran_r9_present                      = false;
        cap_v920.csg_proximity_ind_params_r9.inter_freq_proximity_ind_r9_present                 = false;
        cap_v920.csg_proximity_ind_params_r9.intra_freq_proximity_ind_r9_present                 = false;
        cap_v920.csg_proximity_ind_params_r9.utran_proximity_ind_r9_present                      = false;
        cap_v920.neigh_cell_si_acquisition_params_r9.inter_freq_si_acquisition_for_ho_r9_present = false;
        cap_v920.neigh_cell_si_acquisition_params_r9.intra_freq_si_acquisition_for_ho_r9_present = false;
        cap_v920.neigh_cell_si_acquisition_params_r9.utran_si_acquisition_for_ho_r9_present      = false;
        cap_v920.son_params_r9.rach_report_r9_present                                            = false;

        cap.non_crit_ext_present = true;
        cap.non_crit_ext         = cap_v920;
      }

      if (args.release > 9) {
        phy_layer_params_v1020_s phy_layer_params_v1020;
        phy_layer_params_v1020.two_ant_ports_for_pucch_r10_present             = false;
        phy_layer_params_v1020.tm9_with_minus8_tx_fdd_r10_present              = false;
        phy_layer_params_v1020.pmi_disabling_r10_present                       = false;
        phy_layer_params_v1020.cross_carrier_sched_r10_present                 = args.support_ca;
        phy_layer_params_v1020.simul_pucch_pusch_r10_present                   = false;
        phy_layer_params_v1020.multi_cluster_pusch_within_cc_r10_present       = false;
        phy_layer_params_v1020.non_contiguous_ul_ra_within_cc_list_r10_present = false;

        band_combination_params_r10_l combination_params;
        if (args.support_ca) {
          for (uint32_t k = 0; k < args.nof_supported_bands; k++) {
            ca_mimo_params_dl_r10_s ca_mimo_params_dl;
            ca_mimo_params_dl.ca_bw_class_dl_r10                = ca_bw_class_r10_e::f;
            ca_mimo_params_dl.supported_mimo_cap_dl_r10_present = false;

            ca_mimo_params_ul_r10_s ca_mimo_params_ul;
            ca_mimo_params_ul.ca_bw_class_ul_r10                = ca_bw_class_r10_e::f;
            ca_mimo_params_ul.supported_mimo_cap_ul_r10_present = false;

            band_params_r10_s band_params;
            band_params.band_eutra_r10             = args.supported_bands[k];
            band_params.band_params_dl_r10_present = true;
            band_params.band_params_dl_r10.push_back(ca_mimo_params_dl);
            band_params.band_params_ul_r10_present = true;
            band_params.band_params_ul_r10.push_back(ca_mimo_params_ul);

            combination_params.push_back(band_params);
          }
        }

        rf_params_v1020_s rf_params;
        rf_params.supported_band_combination_r10.push_back(combination_params);

        ue_eutra_cap_v1020_ies_s cap_v1020;
        if (args.ue_category >= 6 && args.ue_category <= 8) {
          cap_v1020.ue_category_v1020_present = true;
          cap_v1020.ue_category_v1020         = (uint8_t)args.ue_category;
        } else {
          // Do not populate UE category for this release if the category is out of range
        }
        cap_v1020.phy_layer_params_v1020_present = true;
        cap_v1020.phy_layer_params_v1020         = phy_layer_params_v1020;
        cap_v1020.rf_params_v1020_present        = args.support_ca;
        cap_v1020.rf_params_v1020                = rf_params;

        ue_eutra_cap_v940_ies_s cap_v940;
        cap_v940.non_crit_ext_present = true;
        cap_v940.non_crit_ext         = cap_v1020;

        cap.non_crit_ext.non_crit_ext_present = true;
        cap.non_crit_ext.non_crit_ext         = cap_v940;
      }

      if (args.release > 10) {
        ue_eutra_cap_v11a0_ies_s cap_v11a0;
        if (args.ue_category >= 11 && args.ue_category <= 12) {
          cap_v11a0.ue_category_v11a0         = (uint8_t)args.ue_category;
          cap_v11a0.ue_category_v11a0_present = true;
        } else {
          // Do not populate UE category for this release if the category is out of range
        }

        ue_eutra_cap_v1180_ies_s cap_v1180;
        cap_v1180.non_crit_ext_present = true;
        cap_v1180.non_crit_ext         = cap_v11a0;

        ue_eutra_cap_v1170_ies_s cap_v1170;
        cap_v1170.non_crit_ext_present = true;
        cap_v1170.non_crit_ext         = cap_v1180;
        if (args.ue_category >= 9 && args.ue_category <= 10) {
          cap_v1170.ue_category_v1170         = (uint8_t)args.ue_category;
          cap_v1170.ue_category_v1170_present = true;
        } else {
          // Do not populate UE category for this release if the category is out of range
        }

        ue_eutra_cap_v1130_ies_s cap_v1130;
        cap_v1130.non_crit_ext_present = true;
        cap_v1130.non_crit_ext         = cap_v1170;

        ue_eutra_cap_v1090_ies_s cap_v1090;
        cap_v1090.non_crit_ext_present = true;
        cap_v1090.non_crit_ext         = cap_v1130;

        ue_eutra_cap_v1060_ies_s cap_v1060;
        cap_v1060.non_crit_ext_present = true;
        cap_v1060.non_crit_ext         = cap_v1090;

        cap.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext_present = true;
        cap.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext         = cap_v1060;
      }

      if (args.release > 11) {
        supported_band_list_eutra_v1250_l supported_band_list_eutra_v1250;
        for (uint32_t k = 0; k < args.nof_supported_bands; k++) {
          supported_band_eutra_v1250_s supported_band_eutra_v1250;
          // According to 3GPP 36.306 v12 Table 4.1A-1, 256QAM is supported for ue_category_dl 11-16
          supported_band_eutra_v1250.dl_minus256_qam_r12_present = (args.ue_category_dl >= 11);

          // According to 3GPP 36.331 v12 UE-EUTRA-Capability field descriptions
          // This field is only present when the field ue-CategoryUL is considered to 5 or 13.
          supported_band_eutra_v1250.ul_minus64_qam_r12_present = true;

          supported_band_list_eutra_v1250.push_back(supported_band_eutra_v1250);
        }

        rf_params_v1250_s rf_params_v1250;
        rf_params_v1250.supported_band_list_eutra_v1250_present = true;
        rf_params_v1250.supported_band_list_eutra_v1250         = supported_band_list_eutra_v1250;

        ue_eutra_cap_v1250_ies_s cap_v1250;

        // Optional UE Category UL/DL
        // Warning: Make sure the UE Category UL/DL matches with 3GPP 36.306 Table 4.1A-6
        if (args.ue_category_dl >= 0) {
          cap_v1250.ue_category_dl_r12_present = true;
          cap_v1250.ue_category_dl_r12         = (uint8_t)args.ue_category_dl;
        } else {
          // Do not populate UE category for this release if the category is not available
        }
        if (args.ue_category_ul >= 0) {
          cap_v1250.ue_category_ul_r12_present = true;
          cap_v1250.ue_category_ul_r12         = (uint8_t)args.ue_category_ul;
        } else {
          // Do not populate UE category for this release if the category is not available
        }
        cap_v1250.rf_params_v1250_present = true;
        cap_v1250.rf_params_v1250         = rf_params_v1250;

        cap.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext
            .non_crit_ext.non_crit_ext_present = true;
        cap.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext
            .non_crit_ext.non_crit_ext = cap_v1250;
        // 12.50
        cap.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext
            .non_crit_ext.non_crit_ext.non_crit_ext_present = true;
        // 12.60
        cap.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext
            .non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext_present = true;
        // 12.70
        cap.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext
            .non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext_present = true;
      }
      // Release 13
      if (args.release > 12) {
        // 12.80
        ue_eutra_cap_v1280_ies =
            &cap.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext
                 .non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext;
        ue_eutra_cap_v1280_ies->non_crit_ext_present = true;
        // 13.10
        ue_eutra_cap_v1280_ies->non_crit_ext.non_crit_ext_present = true;
        // 13.20
        ue_eutra_cap_v1280_ies->non_crit_ext.non_crit_ext.non_crit_ext_present = true;
        // 13.30
        ue_eutra_cap_v1280_ies->non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext_present = true;
        // 13.40
        ue_eutra_cap_v1280_ies->non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext_present = true;
        // 13.50
        ue_eutra_cap_v1280_ies->non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext_present =
            true;
      }
      // Release 14
      if (args.release > 13) {
        // 13.60
        ue_eutra_cap_v1360_ies =
            &ue_eutra_cap_v1280_ies->non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext;
        ue_eutra_cap_v1360_ies->non_crit_ext_present = true;
        // 14.30
        ue_eutra_cap_v1360_ies->non_crit_ext.non_crit_ext_present = true;
        // 14.40
        ue_eutra_cap_v1360_ies->non_crit_ext.non_crit_ext.non_crit_ext_present = true;
        // 14.50
        ue_eutra_cap_v1360_ies->non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext_present = true;
      }
      // Release 15
      if (args.release > 14) {
        ue_eutra_cap_v1450_ies = &ue_eutra_cap_v1360_ies->non_crit_ext.non_crit_ext.non_crit_ext;
        // 14.60
        ue_eutra_cap_v1450_ies->non_crit_ext_present              = true;
        ue_eutra_cap_v1450_ies->non_crit_ext.non_crit_ext_present = true;

        irat_params_nr_r15_s irat_params_nr_r15;
        irat_params_nr_r15.en_dc_r15_present                     = true;
        irat_params_nr_r15.supported_band_list_en_dc_r15_present = true;

        uint32_t nof_supported_nr_bands = args.supported_bands_nr.size();
        irat_params_nr_r15.supported_band_list_en_dc_r15.resize(nof_supported_nr_bands);
        for (uint32_t k = 0; k < nof_supported_nr_bands; k++) {
          irat_params_nr_r15.supported_band_list_en_dc_r15[k].band_nr_r15 = args.supported_bands_nr[k];
        }

        ue_eutra_cap_v1450_ies->non_crit_ext.non_crit_ext.irat_params_nr_r15_present = true;
        ue_eutra_cap_v1450_ies->non_crit_ext.non_crit_ext.irat_params_nr_r15         = irat_params_nr_r15;
        ue_eutra_cap_v1450_ies->non_crit_ext.non_crit_ext.non_crit_ext_present       = true;

        // 15.10
        ue_eutra_cap_v1510_ies_s* ue_cap_enquiry_v1510_ies   = &ue_eutra_cap_v1450_ies->non_crit_ext.non_crit_ext;
        ue_cap_enquiry_v1510_ies->pdcp_params_nr_r15_present = true;
        ue_cap_enquiry_v1510_ies->pdcp_params_nr_r15.sn_size_lo_r15_present = true;
      }

      // Pack caps and copy to cap info
      uint8_t       buf[64] = {};
      asn1::bit_ref bref(buf, sizeof(buf));
      if (cap.pack(bref) != asn1::SRSASN_SUCCESS) {
        logger.error("Error packing EUTRA capabilities");
        return;
      }
      bref.align_bytes_zero();
      auto cap_len = (uint32_t)bref.distance_bytes(buf);
      info->ue_cap_rat_container_list[rat_idx].ue_cap_rat_container.resize(cap_len);
      memcpy(info->ue_cap_rat_container_list[rat_idx].ue_cap_rat_container.data(), buf, cap_len);
      rat_idx++;
    } else if (enquiry.crit_exts.c1().ue_cap_enquiry_r8().ue_cap_request[i] == rat_type_e::eutra_nr && has_nr_dc()) {
      info->ue_cap_rat_container_list[rat_idx] = get_eutra_nr_capabilities();
      logger.info("Including EUTRA-NR capabilities in UE Capability Info (%d B)",
                  info->ue_cap_rat_container_list[rat_idx].ue_cap_rat_container.size());
      rat_idx++;
    } else if (enquiry.crit_exts.c1().ue_cap_enquiry_r8().ue_cap_request[i] == rat_type_e::nr && has_nr_dc()) {
      info->ue_cap_rat_container_list[rat_idx] = get_nr_capabilities();
      logger.info("Including NR capabilities in UE Capability Info (%d B)",
                  info->ue_cap_rat_container_list[rat_idx].ue_cap_rat_container.size());
      rat_idx++;
    } else {
      logger.error("RAT Type of UE Cap request not supported or not configured");
    }
  }
  // resize container back to the actually filled items
  info->ue_cap_rat_container_list.resize(rat_idx);

  if (enquiry.crit_exts.c1().ue_cap_enquiry_r8().non_crit_ext_present) {
    if (enquiry.crit_exts.c1().ue_cap_enquiry_r8().non_crit_ext.non_crit_ext_present) {
      if (enquiry.crit_exts.c1().ue_cap_enquiry_r8().non_crit_ext.non_crit_ext.non_crit_ext_present) {
        if (enquiry.crit_exts.c1().ue_cap_enquiry_r8().non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext_present) {
          if (enquiry.crit_exts.c1()
                  .ue_cap_enquiry_r8()
                  .non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext_present) {
            if (enquiry.crit_exts.c1()
                    .ue_cap_enquiry_r8()
                    .non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext
                    .requested_freq_bands_nr_mrdc_r15_present) {
              logger.debug("Requested Freq Bands NR MRDC R15 present");
            }
          }
        }
      }
    }
  }

  send_ul_dcch_msg(srb_to_lcid(lte_srb::srb1), ul_dcch_msg);
}

/*******************************************************************************
 *
 *
 *
 * UEInformationRequest message
 *
 *
 *
 *******************************************************************************/
void rrc::handle_ue_info_request(const ue_info_request_r9_s& request)
{
  logger.debug("Preparing UEInformationResponse message");

  ul_dcch_msg_s          ul_dcch_msg;
  ue_info_resp_r9_ies_s* resp =
      &ul_dcch_msg.msg.set_c1().set_ue_info_resp_r9().crit_exts.set_c1().set_ue_info_resp_r9();
  ul_dcch_msg.msg.c1().ue_info_resp_r9().rrc_transaction_id = transaction_id;

  // if rach-ReportReq is set to true, set the contents of the rach-Report in the UEInformationResponse message as
  // follows
  if (request.crit_exts.c1().ue_info_request_r9().rach_report_req_r9) {
    // todo...
  }

  // Include rlf-Report if rlf-ReportReq is set to true
  if (request.crit_exts.c1().ue_info_request_r9().rlf_report_req_r9 && var_rlf_report.has_info()) {
    resp->rlf_report_r9_present = true;
    resp->rlf_report_r9         = var_rlf_report.get_report();

    // fixme: should be cleared upon successful delivery
    var_rlf_report.clear();
  }

  send_ul_dcch_msg(srb_to_lcid(lte_srb::srb1), ul_dcch_msg);
}

/*******************************************************************************
 *
 *
 *
 * PHY and MAC Radio Resource configuration
 *
 *
 *
 *******************************************************************************/

void rrc::log_rr_config_common()
{
  logger.info("Set RACH ConfigCommon: NofPreambles=%d, ResponseWindow=%d, ContentionResolutionTimer=%d ms",
              current_mac_cfg.rach_cfg.nof_preambles,
              current_mac_cfg.rach_cfg.responseWindowSize,
              current_mac_cfg.rach_cfg.contentionResolutionTimer);

  const srsran::phy_cfg_t& current_pcell = phy_ctrl->current_cell_config()[0];
  logger.info("Set PUSCH ConfigCommon: P0_pusch=%f, DMRS cs=%d, delta_ss=%d, N_sb=%d",
              current_pcell.ul_cfg.power_ctrl.p0_ue_pusch,
              current_pcell.ul_cfg.dmrs.cyclic_shift,
              current_pcell.ul_cfg.dmrs.delta_ss,
              current_pcell.ul_cfg.hopping.n_sb);

  logger.info("Set PUCCH ConfigCommon: DeltaShift=%d, CyclicShift=%d, N1=%d, NRB=%d",
              current_pcell.ul_cfg.pucch.delta_pucch_shift,
              current_pcell.ul_cfg.pucch.N_cs,
              current_pcell.ul_cfg.pucch.n1_pucch_an_cs[0][0],
              current_pcell.ul_cfg.pucch.n_rb_2);

  logger.info("Set PRACH ConfigCommon: SeqIdx=%d, HS=%s, FreqOffset=%d, ZC=%d, ConfigIndex=%d",
              current_pcell.prach_cfg.root_seq_idx,
              current_pcell.prach_cfg.hs_flag ? "yes" : "no",
              current_pcell.prach_cfg.freq_offset,
              current_pcell.prach_cfg.zero_corr_zone,
              current_pcell.prach_cfg.config_idx);

  if (current_pcell.ul_cfg.srs.configured) {
    logger.info("Set SRS ConfigCommon: BW-Configuration=%d, SF-Configuration=%d, Simult-ACKNACK=%s",
                current_pcell.ul_cfg.srs.bw_cfg,
                current_pcell.ul_cfg.srs.subframe_config,
                current_pcell.ul_cfg.srs.simul_ack ? "yes" : "no");
  }
}

void rrc::apply_rr_config_common(rr_cfg_common_s* config, bool send_lower_layers)
{
  logger.info("Applying MAC/PHY config common");

  if (config->rach_cfg_common_present) {
    set_mac_cfg_t_rach_cfg_common(&current_mac_cfg, config->rach_cfg_common);
  }

  srsran::phy_cfg_t& current_pcell = phy_ctrl->current_cell_config()[0];
  if (config->prach_cfg.prach_cfg_info_present) {
    set_phy_cfg_t_common_prach(&current_pcell, &config->prach_cfg.prach_cfg_info, config->prach_cfg.root_seq_idx);
  } else {
    set_phy_cfg_t_common_prach(&current_pcell, NULL, config->prach_cfg.root_seq_idx);
  }

  if (config->pdsch_cfg_common_present) {
    set_phy_cfg_t_common_pdsch(&current_pcell, config->pdsch_cfg_common);
  }

  set_phy_cfg_t_common_pusch(&current_pcell, config->pusch_cfg_common);

  if (config->phich_cfg_present) {
    // TODO
  }

  if (config->pucch_cfg_common_present) {
    set_phy_cfg_t_common_pucch(&current_pcell, config->pucch_cfg_common);
  }

  if (config->srs_ul_cfg_common_present) {
    set_phy_cfg_t_common_srs(&current_pcell, config->srs_ul_cfg_common);
  }

  if (config->ul_pwr_ctrl_common_present) {
    set_phy_cfg_t_common_pwr_ctrl(&current_pcell, config->ul_pwr_ctrl_common);
  }

  log_rr_config_common();

  if (send_lower_layers) {
    mac->set_config(current_mac_cfg);
    phy_ctrl->set_cell_config(current_pcell);
  }
}

void rrc::log_phy_config_dedicated()
{
  srsran::phy_cfg_t& current_pcell = phy_ctrl->current_cell_config()[0];
  if (current_pcell.dl_cfg.cqi_report.periodic_configured) {
    logger.info("Set cqi-PUCCH-ResourceIndex=%d, cqi-pmi-ConfigIndex=%d, cqi-FormatIndicatorPeriodic=%d",
                current_pcell.ul_cfg.pucch.n_pucch_2,
                current_pcell.dl_cfg.cqi_report.pmi_idx,
                current_pcell.dl_cfg.cqi_report.periodic_mode);
  }
  if (current_pcell.dl_cfg.cqi_report.aperiodic_configured) {
    logger.info("Set cqi-ReportModeAperiodic=%d", current_pcell.dl_cfg.cqi_report.aperiodic_mode);
  }

  if (current_pcell.ul_cfg.pucch.sr_configured) {
    logger.info("Set PHY config ded: SR-n_pucch=%d, SR-ConfigIndex=%d",
                current_pcell.ul_cfg.pucch.n_pucch_sr,
                current_pcell.ul_cfg.pucch.I_sr);
  }

  if (current_pcell.ul_cfg.srs.configured) {
    logger.info("Set PHY config ded: SRS-ConfigIndex=%d, SRS-bw=%d, SRS-Nrcc=%d, SRS-hop=%d, SRS-Ncs=%d",
                current_pcell.ul_cfg.srs.I_srs,
                current_pcell.ul_cfg.srs.B,
                current_pcell.ul_cfg.srs.n_rrc,
                current_pcell.ul_cfg.srs.b_hop,
                current_pcell.ul_cfg.srs.n_srs);
  }
}

// Apply default physical common and dedicated configuration
void rrc::set_phy_default()
{
  if (phy_ctrl != nullptr) {
    phy_ctrl->set_phy_to_default();
  } else {
    logger.info("RRC not initialized. Skipping default PHY config.");
  }
}

// Apply provided PHY config
void rrc::apply_phy_config_dedicated(const phys_cfg_ded_s& phy_cnfg, bool is_handover)
{
  logger.info("Applying PHY config dedicated");

  srsran::phy_cfg_t& current_pcell = phy_ctrl->current_cell_config()[0];
  set_phy_cfg_t_dedicated_cfg(&current_pcell, phy_cnfg);
  if (is_handover) {
    current_pcell.ul_cfg.pucch.sr_configured             = false;
    current_pcell.dl_cfg.cqi_report.periodic_configured  = false;
    current_pcell.dl_cfg.cqi_report.aperiodic_configured = false;
  }

  log_phy_config_dedicated();

  phy_ctrl->set_cell_config(current_pcell);
}

void rrc::apply_phy_scell_config(const scell_to_add_mod_r10_s& scell_config, bool enable_cqi)
{
  srsran_cell_t scell  = {};
  uint32_t      earfcn = 0;

  if (phy == nullptr) {
    logger.info("RRC not initialized. Skipping PHY config.");
    return;
  }

  logger.info("Applying PHY config to scell");

  // Initialise default parameters from primary cell
  earfcn = meas_cells.serving_cell().get_earfcn();

  // Parse identification
  if (scell_config.cell_identif_r10_present) {
    scell.id = scell_config.cell_identif_r10.pci_r10;
    earfcn   = scell_config.cell_identif_r10.dl_carrier_freq_r10;
  }

  // Parse radio resource
  if (scell_config.rr_cfg_common_scell_r10_present) {
    const rr_cfg_common_scell_r10_s* rr_cfg     = &scell_config.rr_cfg_common_scell_r10;
    auto                             non_ul_cfg = &rr_cfg->non_ul_cfg_r10;
    scell.frame_type                            = (rr_cfg->tdd_cfg_v1130.is_present()) ? SRSRAN_TDD : SRSRAN_FDD;
    scell.nof_prb                               = non_ul_cfg->dl_bw_r10.to_number();
    scell.nof_ports                             = non_ul_cfg->ant_info_common_r10.ant_ports_count.to_number();
    scell.phich_length = (non_ul_cfg->phich_cfg_r10.phich_dur.value == phich_cfg_s::phich_dur_opts::normal)
                             ? SRSRAN_PHICH_NORM
                             : SRSRAN_PHICH_EXT;

    // Avoid direct conversion between different phich resource enum
    switch (non_ul_cfg->phich_cfg_r10.phich_res.value) {
      case phich_cfg_s::phich_res_opts::one_sixth:
        scell.phich_resources = SRSRAN_PHICH_R_1_6;
        break;
      case phich_cfg_s::phich_res_opts::half:
        scell.phich_resources = SRSRAN_PHICH_R_1_2;
        break;
      case phich_cfg_s::phich_res_opts::one:
        scell.phich_resources = SRSRAN_PHICH_R_1;
        break;
      case phich_cfg_s::phich_res_opts::two:
      case phich_cfg_s::phich_res_opts::nulltype:
        scell.phich_resources = SRSRAN_PHICH_R_2;
        break;
    }
  }

  // Initialize scell config with pcell cfg
  srsran::phy_cfg_t scell_cfg = phy_ctrl->current_cell_config()[0];
  set_phy_cfg_t_scell_config(&scell_cfg, scell_config);

  if (not enable_cqi) {
    scell_cfg.dl_cfg.cqi_report.periodic_configured  = false;
    scell_cfg.dl_cfg.cqi_report.aperiodic_configured = false;
  }

  if (!phy->set_scell(scell, scell_config.scell_idx_r10, earfcn)) {
    logger.error("Adding SCell cc_idx=%d", scell_config.scell_idx_r10);
  } else if (!phy_ctrl->set_cell_config(scell_cfg, scell_config.scell_idx_r10)) {
    logger.error("Setting SCell configuration for cc_idx=%d", scell_config.scell_idx_r10);
  }
}

void rrc::log_mac_config_dedicated()
{
  logger.info("Set MAC main config: harq-MaxReTX=%d, bsr-TimerReTX=%d, bsr-TimerPeriodic=%d, SR %s (dsr-TransMax=%d)",
              current_mac_cfg.harq_cfg.max_harq_msg3_tx,
              current_mac_cfg.bsr_cfg.retx_timer,
              current_mac_cfg.bsr_cfg.periodic_timer,
              current_mac_cfg.sr_cfg.enabled ? "enabled" : "disabled",
              current_mac_cfg.sr_cfg.dsr_transmax);
  if (current_mac_cfg.phr_cfg.enabled) {
    logger.info("Set MAC PHR config: periodicPHR-Timer=%d, prohibitPHR-Timer=%d, dl-PathlossChange=%d",
                current_mac_cfg.phr_cfg.periodic_timer,
                current_mac_cfg.phr_cfg.prohibit_timer,
                current_mac_cfg.phr_cfg.db_pathloss_change);
  }
}

// 3GPP 36.331 v10 9.2.2 Default MAC main configuration
void rrc::apply_mac_config_dedicated_default()
{
  logger.info("Setting MAC default configuration");
  current_mac_cfg.set_mac_main_cfg_default();
  mac->set_config(current_mac_cfg);
  log_mac_config_dedicated();
}

/**
 * Applies RadioResource Config changes to lower layers
 * @param cnfg
 * @param is_handover - whether the SR, CQI, SRS, measurement configs take effect immediately (see TS
 * 36.331 5.3.5.4)
 * @return
 */
bool rrc::apply_rr_config_dedicated(const rr_cfg_ded_s* cnfg, bool is_handover)
{
  if (cnfg->phys_cfg_ded_present) {
    apply_phy_config_dedicated(cnfg->phys_cfg_ded, is_handover);
    // Apply SR configuration to MAC (leave SR unconfigured during handover)
    if (not is_handover and cnfg->phys_cfg_ded.sched_request_cfg_present) {
      set_mac_cfg_t_sched_request_cfg(&current_mac_cfg, cnfg->phys_cfg_ded.sched_request_cfg);
    }
  }

  logger.info("Applying MAC config dedicated");

  if (cnfg->mac_main_cfg_present) {
    if (cnfg->mac_main_cfg.type() == rr_cfg_ded_s::mac_main_cfg_c_::types::default_value) {
      current_mac_cfg.set_mac_main_cfg_default();
    } else {
      set_mac_cfg_t_main_cfg(&current_mac_cfg, cnfg->mac_main_cfg.explicit_value());
    }
    mac->set_config(current_mac_cfg);
    log_mac_config_dedicated();
  } else if (not is_handover and cnfg->phys_cfg_ded.sched_request_cfg_present) {
    // If MAC-main not set but SR config is set, use directly mac->set_config to update config
    mac->set_config(current_mac_cfg);
    log_mac_config_dedicated();
  }

  if (cnfg->sps_cfg_present) {
    // TODO
  }
  if (cnfg->rlf_timers_and_consts_r9.is_present() and cnfg->rlf_timers_and_consts_r9->type() == setup_e::setup) {
    auto timer_expire_func = [this](uint32_t tid) { timer_expired(tid); };
    t301.set(cnfg->rlf_timers_and_consts_r9->setup().t301_r9.to_number(), timer_expire_func);
    t310.set(cnfg->rlf_timers_and_consts_r9->setup().t310_r9.to_number(), timer_expire_func);
    t311.set(cnfg->rlf_timers_and_consts_r9->setup().t311_r9.to_number(), timer_expire_func);
    N310 = cnfg->rlf_timers_and_consts_r9->setup().n310_r9.to_number();
    N311 = cnfg->rlf_timers_and_consts_r9->setup().n311_r9.to_number();

    logger.info("Updated Constants and Timers: N310=%d, N311=%d, t300=%u, t301=%u, t310=%u, t311=%u",
                N310,
                N311,
                t300.duration(),
                t301.duration(),
                t310.duration(),
                t311.duration());
  }
  for (uint32_t i = 0; i < cnfg->srb_to_add_mod_list.size(); i++) {
    // TODO: handle SRB modification
    add_srb(cnfg->srb_to_add_mod_list[i]);
  }
  for (uint32_t i = 0; i < cnfg->drb_to_release_list.size(); i++) {
    release_drb(cnfg->drb_to_release_list[i]);
  }
  for (uint32_t i = 0; i < cnfg->drb_to_add_mod_list.size(); i++) {
    // TODO: handle DRB modification
    add_drb(cnfg->drb_to_add_mod_list[i]);
  }
  return true;
}

bool rrc::apply_rr_config_dedicated_on_ho_complete(const rr_cfg_ded_s& cnfg)
{
  logger.info("Applying MAC/PHY config dedicated on HO complete");

  // Apply SR+CQI configuration to PHY
  if (cnfg.phys_cfg_ded_present) {
    apply_phy_config_dedicated(cnfg.phys_cfg_ded, false);
  }

  // Apply SR configuration to MAC
  if (cnfg.phys_cfg_ded.sched_request_cfg_present) {
    set_mac_cfg_t_sched_request_cfg(&current_mac_cfg, cnfg.phys_cfg_ded.sched_request_cfg);
    mac->set_config(current_mac_cfg);
    log_mac_config_dedicated();
  }
  return true;
}

/*
 * Extracts and applies SCell configuration from an ASN.1 reconfiguration struct
 */
void rrc::apply_scell_config(rrc_conn_recfg_r8_ies_s* reconfig_r8, bool enable_cqi)
{
  if (reconfig_r8->non_crit_ext_present) {
    auto reconfig_r890 = &reconfig_r8->non_crit_ext;
    if (reconfig_r890->non_crit_ext_present) {
      auto* reconfig_r920 = &reconfig_r890->non_crit_ext;
      if (reconfig_r920->non_crit_ext_present) {
        auto* reconfig_r1020 = &reconfig_r920->non_crit_ext;

        // Handle Add/Modify SCell list
        if (reconfig_r1020->scell_to_add_mod_list_r10_present) {
          for (uint32_t i = 0; i < reconfig_r1020->scell_to_add_mod_list_r10.size(); i++) {
            auto scell_config = &reconfig_r1020->scell_to_add_mod_list_r10[i];

            // Limit enable64_qam, if the ue does not
            // since the phy does not have information about the RRC category and release, the RRC shall limit the
            if (scell_config->rr_cfg_common_scell_r10_present) {
              // enable64_qam
              auto rr_cfg_common_scell = &scell_config->rr_cfg_common_scell_r10;
              if (rr_cfg_common_scell->ul_cfg_r10_present) {
                auto ul_cfg           = &rr_cfg_common_scell->ul_cfg_r10;
                auto pusch_cfg_common = &ul_cfg->pusch_cfg_common_r10;

                // According to 3GPP 36.331 v12 UE-EUTRA-Capability field descriptions
                // Allow 64QAM for:
                //   ue-Category 5 and 8 when enable64QAM (without suffix)
                if (pusch_cfg_common->pusch_cfg_basic.enable64_qam) {
                  if (args.ue_category != 5 && args.ue_category != 8 && args.ue_category != 13) {
                    pusch_cfg_common->pusch_cfg_basic.enable64_qam = false;
                  }
                }
              }
            }

            // Call mac reconfiguration
            mac->reconfiguration(scell_config->scell_idx_r10, true);

            // Call phy reconfiguration
            apply_phy_scell_config(*scell_config, enable_cqi);
          }
        }

        // Handle Remove SCell list
        if (reconfig_r1020->scell_to_release_list_r10_present) {
          for (uint32_t i = 0; i < reconfig_r1020->scell_to_release_list_r10.size(); i++) {
            // Call mac reconfiguration
            mac->reconfiguration(reconfig_r1020->scell_to_release_list_r10[i], false);

            // Call phy reconfiguration
            // TODO: Implement phy layer cell removal
          }
        }
      }
    }
  }
}

bool rrc::apply_scell_config_on_ho_complete(const asn1::rrc::rrc_conn_recfg_r8_ies_s& reconfig_r8)
{
  if (reconfig_r8.non_crit_ext_present) {
    auto& reconfig_r890 = reconfig_r8.non_crit_ext;
    if (reconfig_r890.non_crit_ext_present) {
      auto& reconfig_r920 = reconfig_r890.non_crit_ext;
      if (reconfig_r920.non_crit_ext_present) {
        auto& reconfig_r1020 = reconfig_r920.non_crit_ext;

        // Handle Add/Modify SCell list
        if (reconfig_r1020.scell_to_add_mod_list_r10_present) {
          for (const auto& scell_config : reconfig_r1020.scell_to_add_mod_list_r10) {
            // Call phy reconfiguration
            apply_phy_scell_config(scell_config, true);
          }
        }
      }
    }
  }
  return true;
}

void rrc::handle_con_setup(const rrc_conn_setup_s& setup)
{
  // Must enter CONNECT before stopping T300
  state = RRC_STATE_CONNECTED;
  t300.stop();
  t302.stop();
  srsran::console("RRC Connected\n");

  // defer transmission of Setup Complete until PHY reconfiguration has been completed
  if (not conn_setup_proc.launch(&setup.crit_exts.c1().rrc_conn_setup_r8().rr_cfg_ded, std::move(dedicated_info_nas))) {
    logger.error("Failed to initiate connection setup procedure");
    return;
  }
  callback_list.add_proc(conn_setup_proc);
}

/* Reception of RRCConnectionReestablishment by the UE 5.3.7.5 */
void rrc::handle_con_reest(const rrc_conn_reest_s& reest)
{
  connection_reest.trigger(reest);
}

void rrc::add_srb(const srb_to_add_mod_s& srb_cnfg)
{
  // Setup PDCP
  pdcp->add_bearer(srb_cnfg.srb_id, make_srb_pdcp_config_t(srb_cnfg.srb_id, true));
  if (lte_srb::srb2 == static_cast<lte_srb>(srb_cnfg.srb_id)) {
    pdcp->config_security(srb_cnfg.srb_id, sec_cfg);
    pdcp->enable_integrity(srb_cnfg.srb_id, DIRECTION_TXRX);
    pdcp->enable_encryption(srb_cnfg.srb_id, DIRECTION_TXRX);
  }

  // Setup RLC
  if (srb_cnfg.rlc_cfg_present) {
    rlc->add_bearer(srb_cnfg.srb_id, make_rlc_config_t(srb_cnfg));
  }

  // Setup MAC
  uint8_t log_chan_group       = 0;
  uint8_t priority             = 0;
  int     prioritized_bit_rate = 0;
  int     bucket_size_duration = 0;

  // TODO: Move this configuration to mac_interface_rrc
  if (srb_cnfg.lc_ch_cfg_present) {
    if (srb_cnfg.lc_ch_cfg.type() == srb_to_add_mod_s::lc_ch_cfg_c_::types::default_value) {
      // Set default SRB values as defined in Table 9.2.1
      switch (static_cast<lte_srb>(srb_cnfg.srb_id)) {
        case lte_srb::srb0:
          logger.error("Setting SRB0: Should not be set by RRC");
          break;
        case lte_srb::srb1:
          priority             = 1;
          prioritized_bit_rate = -1;
          bucket_size_duration = 0;
          break;
        case lte_srb::srb2:
          priority             = 3;
          prioritized_bit_rate = -1;
          bucket_size_duration = 0;
          break;
        default:
          logger.error("Invalid SRB configuration");
          return;
      }
    } else {
      if (srb_cnfg.lc_ch_cfg.explicit_value().lc_ch_sr_mask_r9_present) {
        // TODO
      }
      if (srb_cnfg.lc_ch_cfg.explicit_value().ul_specific_params_present) {
        if (srb_cnfg.lc_ch_cfg.explicit_value().ul_specific_params.lc_ch_group_present)
          log_chan_group = srb_cnfg.lc_ch_cfg.explicit_value().ul_specific_params.lc_ch_group;

        priority             = srb_cnfg.lc_ch_cfg.explicit_value().ul_specific_params.prio;
        prioritized_bit_rate = srb_cnfg.lc_ch_cfg.explicit_value().ul_specific_params.prioritised_bit_rate.to_number();
        bucket_size_duration = srb_cnfg.lc_ch_cfg.explicit_value().ul_specific_params.bucket_size_dur.to_number();
      }
    }
    mac->setup_lcid(srb_cnfg.srb_id, log_chan_group, priority, prioritized_bit_rate, bucket_size_duration);
  }

  srbs[srb_cnfg.srb_id] = srb_cnfg;
  logger.info("Added radio bearer %s", get_rb_name(srb_cnfg.srb_id));
}

void rrc::add_drb(const drb_to_add_mod_s& drb_cnfg)
{
  if (!drb_cnfg.pdcp_cfg_present || !drb_cnfg.rlc_cfg_present || !drb_cnfg.lc_ch_cfg_present) {
    logger.error("Cannot add DRB - incomplete configuration");
    return;
  }
  uint32_t lcid = 0;
  if (drb_cnfg.lc_ch_id_present) {
    lcid = drb_cnfg.lc_ch_id;
  } else {
    lcid = srsran::MAX_LTE_SRB_ID + drb_cnfg.drb_id;
    logger.warning("LCID not present, using %d", lcid);
  }

  // Setup RLC
  rlc->add_bearer(lcid, make_rlc_config_t(drb_cnfg.rlc_cfg));

  // Setup PDCP
  pdcp_config_t pdcp_cfg = make_drb_pdcp_config_t(drb_cnfg.drb_id, true, drb_cnfg.pdcp_cfg);
  pdcp->add_bearer(lcid, pdcp_cfg);
  pdcp->config_security(lcid, sec_cfg);
  pdcp->enable_encryption(lcid);

  // Setup MAC
  uint8_t log_chan_group       = 0;
  uint8_t priority             = 1;
  int     prioritized_bit_rate = -1;
  int     bucket_size_duration = -1;
  if (drb_cnfg.lc_ch_cfg.ul_specific_params_present) {
    if (drb_cnfg.lc_ch_cfg.ul_specific_params.lc_ch_group_present) {
      log_chan_group = drb_cnfg.lc_ch_cfg.ul_specific_params.lc_ch_group;
    } else {
      logger.warning("LCG not present, setting to 0");
    }
    priority             = drb_cnfg.lc_ch_cfg.ul_specific_params.prio;
    prioritized_bit_rate = drb_cnfg.lc_ch_cfg.ul_specific_params.prioritised_bit_rate.to_number();
    bucket_size_duration = drb_cnfg.lc_ch_cfg.ul_specific_params.bucket_size_dur.to_number();
  }
  mac->setup_lcid(lcid, log_chan_group, priority, prioritized_bit_rate, bucket_size_duration);

  uint8_t eps_bearer_id = 5; // default?
  if (drb_cnfg.eps_bearer_id_present) {
    eps_bearer_id = drb_cnfg.eps_bearer_id;
  }

  // register EPS bearer over LTE PDCP
  stack->add_eps_bearer(eps_bearer_id, srsran::srsran_rat_t::lte, lcid);

  drbs[drb_cnfg.drb_id] = drb_cnfg;
  logger.info("Added DRB Id %d (LCID=%d)", drb_cnfg.drb_id, lcid);
}

void rrc::release_drb(uint32_t drb_id)
{
  if (drbs.find(drb_id) != drbs.end()) {
    logger.info("Releasing DRB Id %d", drb_id);

    // remvove RLC and PDCP for this LCID
    uint32_t lcid = get_lcid_for_drb_id(drb_id);
    rlc->del_bearer(lcid);
    pdcp->del_bearer(lcid);
    // TODO: implement bearer removal at MAC

    // remove EPS bearer associated with this DRB from Stack (GW will trigger service request if needed)
    stack->remove_eps_bearer(get_eps_bearer_id_for_drb_id(drb_id));
    drbs.erase(drb_id);
  } else {
    logger.error("Couldn't release DRB Id %d. Doesn't exist.", drb_id);
  }
}

/**
 * @brief check if this DRB id exists and return it's LCID
 *
 * if the DRB couldn't be found, 0 is returned. This is an invalid
 * LCID for DRB and the caller should handle it.
 */
uint32_t rrc::get_lcid_for_drb_id(const uint32_t& drb_id)
{
  uint32_t lcid = 0;
  if (drbs.find(drb_id) != drbs.end()) {
    asn1::rrc::drb_to_add_mod_s drb_cnfg = drbs[drb_id];
    if (drb_cnfg.lc_ch_id_present) {
      lcid = drb_cnfg.lc_ch_id;
    } else {
      lcid = srsran::MAX_LTE_SRB_ID + drb_cnfg.drb_id;
    }
  }
  return lcid;
}

uint32_t rrc::get_lcid_for_eps_bearer(const uint32_t& eps_bearer_id)
{
  // check if this bearer id exists and return it's LCID
  uint32_t lcid                        = 0;
  uint32_t drb_id                      = 0;
  drb_id                               = get_drb_id_for_eps_bearer(eps_bearer_id);
  asn1::rrc::drb_to_add_mod_s drb_cnfg = drbs[drb_id];
  if (drb_cnfg.lc_ch_id_present) {
    lcid = drb_cnfg.lc_ch_id;
  } else {
    lcid = srsran::MAX_LTE_SRB_ID + drb_cnfg.drb_id;
    logger.warning("LCID not present, using %d", lcid);
  }
  return lcid;
}

uint32_t rrc::get_eps_bearer_id_for_drb_id(const uint32_t& drb_id)
{
  // check if this bearer id exists and return it's LCID
  for (auto& drb : drbs) {
    if (drb.first == drb_id) {
      return drb.second.eps_bearer_id;
    }
  }
  return 0;
}

uint32_t rrc::get_drb_id_for_eps_bearer(const uint32_t& eps_bearer_id)
{
  // check if this bearer id exists and return it's LCID
  for (auto& drb : drbs) {
    if (drb.second.eps_bearer_id == eps_bearer_id) {
      return drb.first;
    }
  }
  return 0;
}

bool rrc::has_nr_dc()
{
  return (args.release >= 15);
}

void rrc::add_mrb(uint32_t lcid, uint32_t port)
{
  gw->add_mch_port(lcid, port);
  rlc->add_bearer_mrb(lcid);
  mac->mch_start_rx(lcid);
  logger.info("Added MRB bearer for lcid:%d", lcid);
}

// PHY CONFIG DEDICATED Defaults (3GPP 36.331 v10 9.2.4)
void rrc::set_phy_default_pucch_srs()
{
  if (phy_ctrl != nullptr) {
    phy_ctrl->set_phy_to_default_pucch_srs();
  } else {
    logger.info("RRC not initialized. Skipping default PUCCH/SRS config.");
  }

  // SR configuration affects to MAC SR too
  current_mac_cfg.sr_cfg.reset();
  mac->set_config(current_mac_cfg.sr_cfg);
}

void rrc::set_mac_default()
{
  apply_mac_config_dedicated_default();
}

void rrc::set_rrc_default()
{
  N310                   = 1;
  N311                   = 1;
  auto timer_expire_func = [this](uint32_t tid) { timer_expired(tid); };
  t304.set(1000, timer_expire_func);
  t310.set(1000, timer_expire_func);
  t311.set(1000, timer_expire_func);
}

const std::string rrc::rb_id_str[] =
    {"SRB0", "SRB1", "SRB2", "DRB1", "DRB2", "DRB3", "DRB4", "DRB5", "DRB6", "DRB7", "DRB8"};
// Helpers for nr communicaiton

asn1::rrc::ue_cap_rat_container_s rrc::get_eutra_nr_capabilities()
{
  srsran::byte_buffer_t             caps_buf;
  asn1::rrc::ue_cap_rat_container_s cap;
  rrc_nr->get_eutra_nr_capabilities(&caps_buf);
  cap.rat_type = asn1::rrc::rat_type_e::eutra_nr;
  cap.ue_cap_rat_container.resize(caps_buf.N_bytes);
  memcpy(cap.ue_cap_rat_container.data(), caps_buf.msg, caps_buf.N_bytes);
  return cap;
}

asn1::rrc::ue_cap_rat_container_s rrc::get_nr_capabilities()
{
  srsran::byte_buffer_t             caps_buf;
  asn1::rrc::ue_cap_rat_container_s cap;
  rrc_nr->get_nr_capabilities(&caps_buf);
  cap.rat_type = asn1::rrc::rat_type_e::nr;
  cap.ue_cap_rat_container.resize(caps_buf.N_bytes);
  memcpy(cap.ue_cap_rat_container.data(), caps_buf.msg, caps_buf.N_bytes);
  return cap;
}

void rrc::nr_notify_reconfiguration_failure()
{
  logger.warning("Notify reconfiguration about NR reconfiguration failure");
  if (conn_recfg_proc.is_busy()) {
    conn_recfg_proc.trigger(false);
  }
}
//  5.6.13a   NR SCG failure information
void rrc::nr_scg_failure_information(const scg_failure_cause_t cause)
{
  logger.warning("Sending NR SCG failure information with cause %s", to_string(cause));
  ul_dcch_msg_s           ul_dcch_msg;
  scg_fail_info_nr_r15_s& scg_fail_info_nr = ul_dcch_msg.msg.set_msg_class_ext().set_c2().set_scg_fail_info_nr_r15();
  scg_fail_info_nr.crit_exts.set_c1().set_scg_fail_info_nr_r15();
  scg_fail_info_nr.crit_exts.c1().scg_fail_info_nr_r15().fail_report_scg_nr_r15_present = true;
  scg_fail_info_nr.crit_exts.c1().scg_fail_info_nr_r15().fail_report_scg_nr_r15.fail_type_r15 =
      (fail_report_scg_nr_r15_s::fail_type_r15_opts::options)cause;
  send_ul_dcch_msg(srb_to_lcid(lte_srb::srb1), ul_dcch_msg);
}

} // namespace srsue
