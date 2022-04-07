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

#include "srsenb/hdr/stack/gnb_stack_nr.h"
#include "srsran/common/standard_streams.h"
#include "srsran/srsran.h"
#include <srsran/interfaces/enb_metrics_interface.h>

namespace srsenb {

gnb_stack_nr::gnb_stack_nr(srslog::sink& log_sink) :
  task_sched{512, 128},
  thread("gNB"),
  mac_logger(srslog::fetch_basic_logger("MAC-NR", log_sink)),
  rlc_logger(srslog::fetch_basic_logger("RLC-NR", log_sink, false)),
  pdcp_logger(srslog::fetch_basic_logger("PDCP-NR", log_sink, false)),
  rrc_logger(srslog::fetch_basic_logger("RRC-NR", log_sink, false)),
  stack_logger(srslog::fetch_basic_logger("STCK-NR", log_sink, false)),
  mac(&task_sched),
  rrc(&task_sched),
  pdcp(&task_sched, pdcp_logger),
  rlc(rlc_logger)
{
  sync_task_queue    = task_sched.make_task_queue();
  gtpu_task_queue    = task_sched.make_task_queue();
  metrics_task_queue = task_sched.make_task_queue();
  gnb_task_queue     = task_sched.make_task_queue();
  x2_task_queue      = task_sched.make_task_queue();
}

gnb_stack_nr::~gnb_stack_nr()
{
  stop();
}

std::string gnb_stack_nr::get_type()
{
  return "nr";
}

int gnb_stack_nr::init(const gnb_stack_args_t& args_,
                       const rrc_nr_cfg_t&     rrc_cfg_,
                       phy_interface_stack_nr* phy_,
                       x2_interface*           x2_)
{
  args = args_;
  phy  = phy_;

  // setup logging
  mac_logger.set_level(srslog::str_to_basic_level(args.log.mac_level));
  rlc_logger.set_level(srslog::str_to_basic_level(args.log.rlc_level));
  pdcp_logger.set_level(srslog::str_to_basic_level(args.log.pdcp_level));
  rrc_logger.set_level(srslog::str_to_basic_level(args.log.rrc_level));
  stack_logger.set_level(srslog::str_to_basic_level(args.log.stack_level));

  mac_logger.set_hex_dump_max_size(args.log.mac_hex_limit);
  rlc_logger.set_hex_dump_max_size(args.log.rlc_hex_limit);
  pdcp_logger.set_hex_dump_max_size(args.log.pdcp_hex_limit);
  rrc_logger.set_hex_dump_max_size(args.log.rrc_hex_limit);
  stack_logger.set_hex_dump_max_size(args.log.stack_hex_limit);

  // Init all layers
  if (mac.init(args.mac, phy, nullptr, &rlc, &rrc) != SRSRAN_SUCCESS) {
    stack_logger.error("Couldn't initialize MAC-NR");
    return SRSRAN_ERROR;
  }

  rlc.init(&pdcp, &rrc, &mac, task_sched.get_timer_handler());
  pdcp.init(&rlc, &rrc, x2_);

  if (rrc.init(rrc_cfg_, phy, &mac, &rlc, &pdcp, nullptr, nullptr, x2_) != SRSRAN_SUCCESS) {
    stack_logger.error("Couldn't initialize RRC");
    return SRSRAN_ERROR;
  }

  // TODO: add SDAP, NGAP
  //  m_gtpu->init(args.s1ap.gtp_bind_addr, args.s1ap.mme_addr,
  //      args.expert.m1u_multiaddr, args.expert.m1u_if_addr, nullptr, &gtpu_log,
  //      args.expert.enable_mbsfn);

  running = true;

  start(STACK_MAIN_THREAD_PRIO);

  return SRSRAN_SUCCESS;
}

void gnb_stack_nr::stop()
{
  if (running) {
    gnb_task_queue.push([this]() { stop_impl(); });
    wait_thread_finish();
  }
}

void gnb_stack_nr::stop_impl()
{
  rrc.stop();
  pdcp.stop();
  mac.stop();

  task_sched.stop();
  srsran::get_background_workers().stop();

  running = false;
}

bool gnb_stack_nr::switch_on()
{
  // Nothing to be done here
  return true;
}

void gnb_stack_nr::run_thread()
{
  while (running) {
    task_sched.run_next_task();
  }
}

void gnb_stack_nr::tti_clock()
{
  sync_task_queue.push([this]() { tti_clock_impl(); });
}

void gnb_stack_nr::tti_clock_impl()
{
  //  m_ngap->run_tti();
  task_sched.tic();
}

void gnb_stack_nr::process_pdus() {}

/********************************************************
 *
 * Interface for upper layer timers
 *
 *******************************************************/

bool gnb_stack_nr::get_metrics(srsenb::stack_metrics_t* metrics)
{
  bool metrics_ready = false;

  // use stack thread to query RRC metrics
  auto ret = metrics_task_queue.try_push([this, metrics, &metrics_ready]() {
    rrc.get_metrics(metrics->rrc);
    {
      std::lock_guard<std::mutex> lock(metrics_mutex);
      metrics_ready = true;
    }
    metrics_cvar.notify_one();
  });
  if (not ret.has_value()) {
    return false;
  }

  // obtain MAC metrics (do not use stack thread)
  mac.get_metrics(metrics->mac);

  // wait for RRC result
  std::unique_lock<std::mutex> lock(metrics_mutex);
  metrics_cvar.wait(lock, [&metrics_ready]() { return metrics_ready; });
  return true;
}

// Temporary GW interface
void gnb_stack_nr::write_sdu(uint32_t lcid, srsran::unique_byte_buffer_t sdu)
{
  // not implemented
}

bool gnb_stack_nr::has_active_radio_bearer(uint32_t eps_bearer_id)
{
  return false;
}
int gnb_stack_nr::slot_indication(const srsran_slot_cfg_t& slot_cfg)
{
  return mac.slot_indication(slot_cfg);
}
int gnb_stack_nr::get_dl_sched(const srsran_slot_cfg_t& slot_cfg, dl_sched_t& dl_sched)
{
  return mac.get_dl_sched(slot_cfg, dl_sched);
}
int gnb_stack_nr::get_ul_sched(const srsran_slot_cfg_t& slot_cfg, ul_sched_t& ul_sched)
{
  return mac.get_ul_sched(slot_cfg, ul_sched);
}
int gnb_stack_nr::pucch_info(const srsran_slot_cfg_t& slot_cfg, const mac_interface_phy_nr::pucch_info_t& pucch_info)
{
  return mac.pucch_info(slot_cfg, pucch_info);
}
int gnb_stack_nr::pusch_info(const srsran_slot_cfg_t& slot_cfg, mac_interface_phy_nr::pusch_info_t& pusch_info)
{
  return mac.pusch_info(slot_cfg, pusch_info);
}

void gnb_stack_nr::rach_detected(const rach_info_t& rach_info)
{
  mac.rach_detected(rach_info);
}

} // namespace srsenb