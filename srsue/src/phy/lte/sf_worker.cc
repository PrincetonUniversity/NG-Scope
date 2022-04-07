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

#include "srsran/srsran.h"

#include "srsran/common/standard_streams.h"
#include "srsue/hdr/phy/lte/sf_worker.h"
#include <string.h>

#define Error(fmt, ...)                                                                                                \
  if (SRSRAN_DEBUG_ENABLED)                                                                                            \
  logger.error(fmt, ##__VA_ARGS__)
#define Warning(fmt, ...)                                                                                              \
  if (SRSRAN_DEBUG_ENABLED)                                                                                            \
  logger.warning(fmt, ##__VA_ARGS__)
#define Info(fmt, ...)                                                                                                 \
  if (SRSRAN_DEBUG_ENABLED)                                                                                            \
  logger.info(fmt, ##__VA_ARGS__)
#define Debug(fmt, ...)                                                                                                \
  if (SRSRAN_DEBUG_ENABLED)                                                                                            \
  logger.debug(fmt, ##__VA_ARGS__)

/* This is to visualize the channel response */
#ifdef ENABLE_GUI
#include "srsgui/srsgui.h"
#include <semaphore.h>

static void      init_plots(srsue::lte::sf_worker* worker);
static pthread_t plot_thread;
static sem_t     plot_sem;
static int       plot_worker_id = -1;
static bool      plot_nr_enable = false;
#else
#pragma message "Compiling without srsGUI support"
#endif
/*********************************************/

namespace srsue {
namespace lte {

sf_worker::sf_worker(uint32_t max_prb, phy_common* phy_, srslog::basic_logger& logger) : logger(logger)
{
  phy = phy_;

  // ue_sync in phy.cc requires a buffer for 3 subframes
  for (uint32_t r = 0; r < phy->args->nof_lte_carriers; r++) {
    cc_workers.push_back(new cc_worker(r, max_prb, phy, logger));
  }
}

sf_worker::~sf_worker()
{
  for (uint32_t r = 0; r < phy->args->nof_lte_carriers; r++) {
    delete cc_workers[r];
  }
}

void sf_worker::reset_cell_nolock(uint32_t cc_idx)
{
  cc_workers[cc_idx]->reset_cell_nolock();
}

bool sf_worker::set_cell_nolock(uint32_t cc_idx, srsran_cell_t cell_)
{
  if (cc_idx < cc_workers.size()) {
    if (!cc_workers[cc_idx]->set_cell_nolock(cell_)) {
      Error("Setting cell for cc=%d", cc_idx);
      return false;
    }
  } else {
    Error("Setting cell for cc=%d; Not enough CC workers (%zd);", cc_idx, cc_workers.size());
  }

  if (cc_idx == 0) {
    std::lock_guard<std::mutex> lock(cell_mutex);
    cell           = cell_;
    cell_initiated = true;
    cell_init_cond.notify_one();
  }

  return true;
}

cf_t* sf_worker::get_buffer(uint32_t carrier_idx, uint32_t antenna_idx)
{
  return cc_workers[carrier_idx]->get_rx_buffer(antenna_idx);
}

uint32_t sf_worker::get_buffer_len()
{
  if (cc_workers.empty()) {
    return 0;
  }
  return cc_workers.at(0)->get_buffer_len();
}

void sf_worker::set_context(const srsran::phy_common_interface::worker_context_t& w_ctx)
{
  context.copy(w_ctx);

  for (auto& cc_worker : cc_workers) {
    cc_worker->set_tti(w_ctx.sf_idx);
  }

  logger.set_context(w_ctx.sf_idx);
}

void sf_worker::set_prach(cf_t* prach_ptr_, float prach_power_)
{
  prach_ptr   = prach_ptr_;
  prach_power = prach_power_;
}

void sf_worker::set_cfo_nolock(const uint32_t& cc_idx, float cfo)
{
  cc_workers[cc_idx]->set_cfo_nolock(cfo);
}

void sf_worker::set_tdd_config_nolock(srsran_tdd_config_t config)
{
  for (auto& cc_worker : cc_workers) {
    cc_worker->set_tdd_config_nolock(config);
  }
  tdd_config = config;
}

void sf_worker::set_config_nolock(uint32_t cc_idx, const srsran::phy_cfg_t& phy_cfg)
{
  if (cc_idx < cc_workers.size()) {
    cc_workers[cc_idx]->set_config_nolock(phy_cfg);
    if (cc_idx > 0) {
      // Update DCI config for PCell
      cc_workers[0]->upd_config_dci_nolock(phy_cfg.dl_cfg.dci);
    }
  } else {
    Error("Setting config for cc=%d; Invalid cc_idx", cc_idx);
  }
}

void sf_worker::work_imp()
{
  uint32_t            tti           = context.sf_idx;
  srsran::rf_buffer_t tx_signal_ptr = {};
  if (!cell_initiated) {
    phy->worker_end(context, false, tx_signal_ptr);
    return;
  }

  bool     rx_signal_ok    = false;
  bool     tx_signal_ready = false;
  uint32_t nof_samples     = SRSRAN_SF_LEN_PRB(cell.nof_prb);

  /***** Downlink Processing *******/

  // Loop through all carriers. carrier_idx=0 is PCell
  for (uint32_t carrier_idx = 0; carrier_idx < cc_workers.size(); carrier_idx++) {
    // Process all DL and special subframes
    if (srsran_sfidx_tdd_type(tdd_config, tti % 10) != SRSRAN_TDD_SF_U || cell.frame_type == SRSRAN_FDD) {
      srsran_mbsfn_cfg_t mbsfn_cfg;
      ZERO_OBJECT(mbsfn_cfg);

      if (carrier_idx == 0 && phy->is_mbsfn_sf(&mbsfn_cfg, tti)) {
        rx_signal_ok =
            cc_workers[0]->work_dl_mbsfn(mbsfn_cfg); // Don't do chest_ok in mbsfn since it trigger measurements
      } else {
        if (phy->cell_state.is_configured(carrier_idx)) {
          rx_signal_ok = cc_workers[carrier_idx]->work_dl_regular();
        }
      }
    }
  }
  tx_signal_ptr.set_nof_samples(nof_samples);

  /***** Uplink Generation + Transmission *******/

  /* If TTI+4 is an uplink subframe (TODO: Support short PRACH and SRS in UpPts special subframes) */
  if ((srsran_sfidx_tdd_type(tdd_config, TTI_TX(tti) % 10) == SRSRAN_TDD_SF_U) || cell.frame_type == SRSRAN_FDD) {
    // Generate Uplink signal if no PRACH pending
    if (!prach_ptr) {
      // Common UCI data object for all carriers
      srsran_uci_data_t uci_data;
      reset_uci(&uci_data);

      uint32_t uci_cc_idx = phy->get_ul_uci_cc(TTI_TX(tti));

      // Fill periodic CQI data; In case of periodic CSI report collision, lower carrier index have preference, so
      // stop as soon as either CQI data is enabled or RI is carried
      for (uint32_t carrier_idx = 0; carrier_idx < phy->args->nof_lte_carriers and not uci_data.cfg.cqi.data_enable and
                                     uci_data.cfg.cqi.ri_len == 0;
           carrier_idx++) {
        if (phy->cell_state.is_active(carrier_idx, TTI_TX(tti))) {
          cc_workers[carrier_idx]->set_uci_periodic_cqi(&uci_data);
        }
      }

      // Loop through all carriers
      for (uint32_t carrier_idx = 0; carrier_idx < phy->args->nof_lte_carriers; carrier_idx++) {
        if (phy->cell_state.is_active(carrier_idx, tti)) {
          tx_signal_ready |= cc_workers[carrier_idx]->work_ul(uci_cc_idx == carrier_idx ? &uci_data : nullptr);

          // Set signal pointer based on offset
          tx_signal_ptr.set(carrier_idx, 0, phy->args->nof_rx_ant, cc_workers[carrier_idx]->get_tx_buffer(0));
        }
      }
    }
  }

  // Set PRACH buffer signal pointer
  if (prach_ptr) {
    tx_signal_ready = true;
    tx_signal_ptr.set(0, prach_ptr);
    prach_ptr = nullptr;
  }

  // Call worker_end to transmit the signal
  phy->worker_end(context, tx_signal_ready, tx_signal_ptr);

  if (rx_signal_ok) {
    update_measurements();
  }

  /* Tell the plotting thread to draw the plots */
#ifdef ENABLE_GUI
  if ((int)get_id() == plot_worker_id) {
    sem_post(&plot_sem);
  }
#endif
}

/********************* Uplink common control functions ****************************/

void sf_worker::reset_uci(srsran_uci_data_t* uci_data)
{
  srsran_uci_data_reset(uci_data);
}

/**************************** Measurements **************************/

void sf_worker::update_measurements()
{
  std::vector<phy_meas_t> serving_cells = {};
  for (uint32_t cc_idx = 0; cc_idx < cc_workers.size(); cc_idx++) {
    cf_t* rssi_power_buffer = nullptr;
    // Setting rssi_power_buffer to nullptr disables RSSI update. Do it only by worker 0
    if (cc_idx == 0 && get_id() == 0) {
      rssi_power_buffer = cc_workers[0]->get_rx_buffer(0);
    }
    cc_workers[cc_idx]->update_measurements(serving_cells, rssi_power_buffer);
  }
  // Send report to stack
  if (not serving_cells.empty()) {
    phy->stack->new_cell_meas(serving_cells);
  }
}

/***********************************************************
 *
 * Interface for Plot visualization
 *
 ***********************************************************/

void sf_worker::start_plot()
{
#ifdef ENABLE_GUI
  if (plot_worker_id == -1) {
    plot_worker_id = get_id();
    plot_nr_enable = phy->args->nof_nr_carriers > 0;
    srsran::console("Starting plot for worker_id=%d\n", plot_worker_id);
    init_plots(this);
  } else {
    srsran::console("Trying to start a plot but already started by worker_id=%d\n", plot_worker_id);
  }
#else
  srsran::console("Trying to start a plot but plots are disabled (ENABLE_GUI constant in sf_worker.cc)\n");
#endif
}

int sf_worker::read_ce_abs(float* ce_abs, uint32_t tx_antenna, uint32_t rx_antenna)
{
  return cc_workers[0]->read_ce_abs(ce_abs, tx_antenna, rx_antenna);
}

int sf_worker::read_pdsch_d(cf_t* pdsch_d)
{
  return cc_workers[0]->read_pdsch_d(pdsch_d);
}

float sf_worker::get_cfo()
{
  sync_metrics_t::array_t sync_metrics = {};
  phy->get_sync_metrics(sync_metrics);
  return sync_metrics[0].cfo;
}
} // namespace lte
} // namespace srsue

/***********************************************************
 *
 * PLOT TO VISUALIZE THE CHANNEL RESPONSEE
 *
 ***********************************************************/

#ifdef ENABLE_GUI
plot_real_t    pce[SRSRAN_MAX_PORTS][SRSRAN_MAX_PORTS];
plot_scatter_t pconst;
plot_scatter_t pconst_nr;
bool           pconst_nr_ready = false;
#define SCATTER_PDSCH_BUFFER_LEN (20 * 6 * SRSRAN_SF_LEN_RE(SRSRAN_MAX_PRB, SRSRAN_CP_NORM))
#define SCATTER_PDSCH_PLOT_LEN 4000
float tmp_plot[SCATTER_PDSCH_BUFFER_LEN];
cf_t  tmp_plot2[SRSRAN_SF_LEN_RE(SRSRAN_MAX_PRB, SRSRAN_CP_NORM)];
bool  plot_quit = false;

#define CFO_PLOT_LEN 0 /* Set to non zero for enabling CFO plot */
#if CFO_PLOT_LEN > 0
static plot_real_t pcfo;
static uint32_t    icfo = 0;
static float       cfo_buffer[CFO_PLOT_LEN];
#endif /* CFO_PLOT_LEN > 0 */

#define SYNC_PLOT_LEN 0 /* Set to non zero for enabling Sync error plot */
#if SYNC_PLOT_LEN > 0
static plot_real_t psync;
static uint32_t    isync = 0;
static float       sync_buffer[SYNC_PLOT_LEN];
#endif /* SYNC_PLOT_LEN > 0 */

static void* plot_thread_run(void* arg)
{
  auto     worker    = (srsue::lte::sf_worker*)arg;
  uint32_t row_count = 0;

  sdrgui_init();
  for (uint32_t tx = 0; tx < worker->get_cell_nof_ports(); tx++) {
    for (uint32_t rx = 0; rx < worker->get_rx_nof_antennas(); rx++) {
      char str_buf[64];
      snprintf(str_buf, 64, "|H%d%d|", rx, tx);
      plot_real_init(&pce[tx][rx]);
      plot_real_setTitle(&pce[tx][rx], str_buf);
      plot_real_setLabels(&pce[tx][rx], (char*)"Index", (char*)"dB");
      plot_real_setYAxisScale(&pce[tx][rx], -40, 40);

      plot_real_addToWindowGrid(&pce[tx][rx], (char*)"srsue", tx, rx);
    }
  }
  row_count = worker->get_rx_nof_antennas();

  plot_scatter_init(&pconst);
  plot_scatter_setTitle(&pconst, (char*)"LTE - PDSCH - Equalized Symbols");
  plot_scatter_setXAxisScale(&pconst, -4, 4);
  plot_scatter_setYAxisScale(&pconst, -4, 4);
  plot_scatter_addToWindowGrid(&pconst, (char*)"srsue", 0, row_count++);

  if (plot_nr_enable) {
    plot_scatter_init(&pconst_nr);
    plot_scatter_setTitle(&pconst_nr, (char*)"NR - PDSCH - Equalized Symbols");
    plot_scatter_setXAxisScale(&pconst_nr, -4, 4);
    plot_scatter_setYAxisScale(&pconst_nr, -4, 4);
    plot_scatter_addToWindowGrid(&pconst_nr, (char*)"srsue", 0, row_count++);
    pconst_nr_ready = true;
  }

#if CFO_PLOT_LEN > 0
  plot_real_init(&pcfo);
  plot_real_setTitle(&pcfo, (char*)"CFO (Hz)");
  plot_real_setLabels(&pcfo, (char*)"Time", (char*)"Hz");
  plot_real_setYAxisScale(&pcfo, -4000, 4000);

  plot_scatter_addToWindowGrid(&pcfo, (char*)"srsue", 1, row_count++);
#endif /* CFO_PLOT_LEN > 0 */

#if SYNC_PLOT_LEN > 0
  plot_real_init(&psync);
  plot_real_setTitle(&psync, (char*)"Sync error (in samples)");
  plot_real_setLabels(&psync, (char*)"Time", (char*)"Error");
  plot_real_setYAxisScale(&psync, -2, +2);

  plot_scatter_addToWindowGrid(&psync, (char*)"srsue", 1, row_count++);
#endif /* SYNC_PLOT_LEN > 0 */

  uint32_t num_tx = worker->get_cell_nof_ports();
  uint32_t num_rx = worker->get_rx_nof_antennas();

  int n;
  int readed_pdsch_re = 0;
  while (!plot_quit) {
    sem_wait(&plot_sem);

    if (readed_pdsch_re < SCATTER_PDSCH_PLOT_LEN) {
      n = worker->read_pdsch_d(&tmp_plot2[readed_pdsch_re]);
      readed_pdsch_re += n;
    } else {
      for (uint32_t tx = 0; tx < num_tx; tx++) {
        for (uint32_t rx = 0; rx < num_rx; rx++) {
          n = worker->read_ce_abs(tmp_plot, tx, rx);
          if (n > 0) {
            plot_real_setNewData(&pce[tx][rx], tmp_plot, n);
          }
        }
      }
      if (readed_pdsch_re > 0) {
        plot_scatter_setNewData(&pconst, tmp_plot2, readed_pdsch_re);
      }
      readed_pdsch_re = 0;
    }

#if CFO_PLOT_LEN > 0
    cfo_buffer[icfo] = worker->get_cfo() * 15000.0f;
    icfo             = (icfo + 1) % CFO_PLOT_LEN;
    plot_real_setNewData(&pcfo, cfo_buffer, CFO_PLOT_LEN);
#endif /* CFO_PLOT_LEN > 0 */
  }
  return nullptr;
}

void init_plots(srsue::lte::sf_worker* worker)
{
  if (sem_init(&plot_sem, 0, 0)) {
    perror("sem_init");
    exit(-1);
  }

  pthread_attr_t     attr;
  struct sched_param param = {};
  param.sched_priority     = 0;
  pthread_attr_init(&attr);
  pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
  pthread_attr_setschedparam(&attr, &param);
  if (pthread_create(&plot_thread, &attr, plot_thread_run, worker)) {
    perror("pthread_create");
    exit(-1);
  }
}
#endif
