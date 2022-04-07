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

#define Error(fmt, ...) logger.error(fmt, ##__VA_ARGS__)
#define Warning(fmt, ...) logger.warning(fmt, ##__VA_ARGS__)
#define Info(fmt, ...) logger.info(fmt, ##__VA_ARGS__)
#define Debug(fmt, ...) logger.debug(fmt, ##__VA_ARGS__)

#include "srsue/hdr/stack/mac/proc_phr.h"
#include "srsran/interfaces/ue_phy_interfaces.h"

namespace srsue {

phr_proc::phr_proc(srslog::basic_logger& logger) : logger(logger)
{
  initiated        = false;
  last_pathloss_db = 0;
  phr_cfg          = {};
}

void phr_proc::init(phy_interface_mac_lte* phy_h_, srsran::ext_task_sched_handle* task_sched_)
{
  phy_h      = phy_h_;
  task_sched = task_sched_;
  initiated  = true;

  timer_periodic = task_sched->get_unique_timer();
  timer_prohibit = task_sched->get_unique_timer();

  reset();
}

void phr_proc::reset()
{
  std::lock_guard<std::mutex> lock(mutex);
  timer_periodic.stop();
  timer_prohibit.stop();
  phr_is_triggered = false;
}

void phr_proc::set_config(srsran::phr_cfg_t& cfg)
{
  std::lock_guard<std::mutex> lock(mutex);

  phr_cfg = cfg;

  // First stop timers. If enabled==false or value is Inf, won't be re-started
  timer_periodic.stop();
  timer_prohibit.stop();

  if (cfg.enabled) {
    // Setup timers and trigger PHR when configuration changed by higher layers
    if (phr_cfg.periodic_timer > 0) {
      timer_periodic.set(phr_cfg.periodic_timer, [this](uint32_t tid) { timer_expired(tid); });
      timer_periodic.run();
      phr_is_triggered = true;
      Info("PHR:   Configured timer periodic %d ms", phr_cfg.periodic_timer);
    }

    if (phr_cfg.prohibit_timer > 0) {
      timer_prohibit.set(phr_cfg.prohibit_timer, [this](uint32_t tid) { timer_expired(tid); });
      timer_prohibit.run();
      Info("PHR:   Configured timer prohibit %d ms", phr_cfg.prohibit_timer);
      phr_is_triggered = true;
    }
  }
}

bool phr_proc::pathloss_changed()
{
  if (!phr_cfg.enabled) {
    return false;
  }

  int cur_pathloss_db = (int)phy_h->get_pathloss_db();

  if (abs(cur_pathloss_db - last_pathloss_db) > phr_cfg.db_pathloss_change && phr_cfg.db_pathloss_change > 0) {
    last_pathloss_db = cur_pathloss_db;
    return true;
  } else {
    return false;
  }
}

void phr_proc::start_periodic_timer()
{
  std::lock_guard<std::mutex> lock(mutex);
  if (phr_cfg.enabled && phr_cfg.periodic_timer > 0) {
    timer_periodic.run();
  }
}

/* Trigger PHR when timers exires */
void phr_proc::timer_expired(uint32_t timer_id)
{
  std::lock_guard<std::mutex> lock(mutex);
  if (!phr_cfg.enabled) {
    Warning("PHR:   %s timer triggered but PHR has been disabled",
            timer_id == timer_periodic.id() ? "Periodic" : "Prohibit");
    return;
  }
  if (timer_id == timer_periodic.id()) {
    timer_periodic.run();
    Debug("PHR:   Triggered by timer periodic (timer expired).");
    phr_is_triggered = true;
  } else if (timer_id == timer_prohibit.id()) {
    if (pathloss_changed()) {
      Info("PHR:   Triggered by pathloss difference. cur_pathloss_db=%d (timer expired)", last_pathloss_db);
      phr_is_triggered = true;
    }
  } else {
    logger.warning("Received timer callback from unknown timer_id=%d", timer_id);
  }
}

void phr_proc::step()
{
  std::lock_guard<std::mutex> lock(mutex);

  if (phr_cfg.enabled && initiated) {
    if (pathloss_changed() && timer_prohibit.is_expired()) {
      Info("PHR:   Triggered by pathloss difference. cur_pathloss_db=%d", last_pathloss_db);
      phr_is_triggered = true;
    }
  }
}

bool phr_proc::generate_phr_on_ul_grant(float* phr)
{
  std::lock_guard<std::mutex> lock(mutex);
  if (phr_is_triggered) {
    if (phr) {
      *phr = phy_h->get_phr();
    }

    Debug("PHR:   Generating PHR=%f", phr ? *phr : 0.0);

    timer_periodic.run();
    timer_prohibit.run();

    phr_is_triggered = false;

    return true;
  } else {
    return false;
  }
}

bool phr_proc::is_extended()
{
  std::lock_guard<std::mutex> lock(mutex);
  return phr_cfg.extended;
}
} // namespace srsue
