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

#include "srsran/interfaces/phy_interface_types.h"
#include "srsran/srslog/bundled/fmt/chrono.h"
#include "srsran/srslog/srslog.h"
#include "srsue/hdr/phy/scell/intra_measure_lte.h"
#include <boost/program_options.hpp>
#include <boost/program_options/parsers.hpp>
#include <iostream>
#include <map>
#include <memory>
#include <srsran/common/string_helpers.h>
#include <srsran/phy/utils/random.h>
#include <vector>

// Common execution parameters
static uint32_t          duration_execution_s;
static srsran_cell_t     cell_base = {.nof_prb         = 6,
                                  .nof_ports       = 1,
                                  .id              = 0,
                                  .cp              = SRSRAN_CP_NORM,
                                  .phich_length    = SRSRAN_PHICH_NORM,
                                  .phich_resources = SRSRAN_PHICH_R_1_6,
                                  .frame_type      = SRSRAN_FDD};
static std::string       intra_meas_log_level;
static std::string       active_cell_list;
static std::string       simulation_cell_list;
static int               phy_lib_log_level;
static srsue::phy_args_t phy_args;
static bool              enable_json_report;
static std::string       json_report_filename;

// On the Fly parameters
static int         earfcn_dl;
static std::string radio_device_args;
static std::string radio_device_name;
static std::string radio_log_level;
static float       rx_gain;

// Simulation parameters
static float       channel_period_s;
static uint32_t    cfi;
static float       ncell_attenuation_dB;
static float       channel_hst_fd_hz;
static float       channel_delay_max_us;
static float       channel_snr_db;
static std::string channel_log_level;

// Simulation Serving cell PDSCH parameters
static bool        serving_cell_pdsch_enable;
static uint16_t    serving_cell_pdsch_rnti;
static srsran_tm_t serving_cell_pdsch_tm;
static uint16_t    serving_cell_pdsch_mcs;

// Parsed PCI lists
static std::set<uint32_t> pcis_to_meas     = {};
static std::set<uint32_t> pcis_to_simulate = {};

// PRB allocation helpers
static uint32_t prbset_num = 1, last_prbset_num = 1;
static uint32_t prbset_orig = 0;

unsigned int reverse(unsigned int x)
{
  x = (((x & (uint32_t)0xaaaaaaaa) >> (uint32_t)1) | ((x & (uint32_t)0x55555555) << (uint32_t)1));
  x = (((x & (uint32_t)0xcccccccc) >> (uint32_t)2) | ((x & (uint32_t)0x33333333) << (uint32_t)2));
  x = (((x & (uint32_t)0xf0f0f0f0) >> (uint32_t)4) | ((x & (uint32_t)0x0f0f0f0f) << (uint32_t)4));
  x = (((x & (uint32_t)0xff00ff00) >> (uint32_t)8) | ((x & (uint32_t)0x00ff00ff) << (uint32_t)8));
  return ((x >> (uint32_t)16) | (x << (uint32_t)16));
}

uint32_t prbset_to_bitmask()
{
  uint32_t mask = 0;
  auto     nb   = (uint32_t)ceilf((float)cell_base.nof_prb / srsran_ra_type0_P(cell_base.nof_prb));
  for (uint32_t i = 0; i < nb; i++) {
    if (i >= prbset_orig && i < prbset_orig + prbset_num) {
      mask = mask | ((uint32_t)0x1 << i);
    }
  }
  return reverse(mask) >> (uint32_t)(32 - nb);
}

// Test eNb class
class test_enb
{
private:
  srsran_enb_dl_t       enb_dl;
  srsran::channel_ptr   channel;
  cf_t*                 signal_buffer[SRSRAN_MAX_PORTS] = {};
  srslog::basic_logger& logger;

public:
  test_enb(const srsran_cell_t& cell, const srsran::channel::args_t& channel_args) :
    enb_dl(), logger(srslog::fetch_basic_logger("Channel pci=" + std::to_string(cell.id)))
  {
    logger.set_level(srslog::str_to_basic_level(channel_log_level));

    channel = srsran::channel_ptr(new srsran::channel(channel_args, cell_base.nof_ports, logger));
    channel->set_srate(srsran_sampling_freq_hz(cell.nof_prb));

    // Allocate buffer for eNb
    for (uint32_t i = 0; i < cell_base.nof_ports; i++) {
      signal_buffer[i] = srsran_vec_cf_malloc(SRSRAN_SF_LEN_PRB(cell_base.nof_prb));
      if (!signal_buffer[i]) {
        ERROR("Error allocating buffer");
      }
    }

    if (srsran_enb_dl_init(&enb_dl, signal_buffer, cell.nof_prb)) {
      ERROR("Error initiating eNb downlink");
    }

    if (srsran_enb_dl_set_cell(&enb_dl, cell)) {
      ERROR("Error setting eNb DL cell");
    }
  }

  int work(srsran_dl_sf_cfg_t*           dl_sf,
           srsran_dci_cfg_t*             dci_cfg,
           srsran_dci_dl_t*              dci,
           srsran_softbuffer_tx_t**      softbuffer_tx,
           uint8_t**                     data_tx,
           cf_t*                         baseband_buffer,
           const srsran::rf_timestamp_t& ts)
  {
    int      ret    = SRSRAN_SUCCESS;
    uint32_t sf_len = SRSRAN_SF_LEN_PRB(enb_dl.cell.nof_prb);

    srsran_enb_dl_put_base(&enb_dl, dl_sf);

    // Put PDSCH only if it is required
    if (dci && dci_cfg && softbuffer_tx && data_tx) {
      if (srsran_enb_dl_put_pdcch_dl(&enb_dl, dci_cfg, dci)) {
        ERROR("Error putting PDCCH sf_idx=%d", dl_sf->tti);
        ret = SRSRAN_ERROR;
      }

      // Create pdsch config
      srsran_pdsch_cfg_t pdsch_cfg;
      if (srsran_ra_dl_dci_to_grant(&enb_dl.cell, dl_sf, serving_cell_pdsch_tm, false, dci, &pdsch_cfg.grant)) {
        ERROR("Computing DL grant sf_idx=%d", dl_sf->tti);
        ret = SRSRAN_ERROR;
      }
      char str[512];
      srsran_dci_dl_info(dci, str, 512);
      INFO("eNb PDCCH: rnti=0x%x, %s", serving_cell_pdsch_rnti, str);

      for (uint32_t i = 0; i < SRSRAN_MAX_CODEWORDS; i++) {
        pdsch_cfg.softbuffers.tx[i] = softbuffer_tx[i];
      }

      // Enable power allocation
      pdsch_cfg.power_scale  = true;
      pdsch_cfg.p_a          = 0.0f;                                         // 0 dB
      pdsch_cfg.p_b          = (serving_cell_pdsch_tm > SRSRAN_TM1) ? 1 : 0; // 0 dB
      pdsch_cfg.rnti         = serving_cell_pdsch_rnti;
      pdsch_cfg.meas_time_en = false;

      if (srsran_enb_dl_put_pdsch(&enb_dl, &pdsch_cfg, data_tx) < 0) {
        ERROR("Error putting PDSCH sf_idx=%d", dl_sf->tti);
        ret = SRSRAN_ERROR;
      }
      srsran_pdsch_tx_info(&pdsch_cfg, str, 512);
      INFO("eNb PDSCH: rnti=0x%x, %s", serving_cell_pdsch_rnti, str);
    }

    srsran_enb_dl_gen_signal(&enb_dl);

    // Apply channel
    channel->run(signal_buffer, signal_buffer, sf_len, ts.get(0));

    // Combine Tx ports
    for (uint32_t i = 1; i < enb_dl.cell.nof_ports; i++) {
      srsran_vec_sum_ccc(signal_buffer[0], signal_buffer[i], signal_buffer[0], sf_len);
    }

    // Undo srsran_enb_dl_gen_signal scaling
    float scale = sqrtf(cell_base.nof_prb) / 0.05f / enb_dl.ifft->cfg.symbol_sz;

    // Apply Neighbour cell attenuation
    if (enb_dl.cell.id != *pcis_to_simulate.begin()) {
      scale *= srsran_convert_dB_to_amplitude(-ncell_attenuation_dB);
    }

    // Scale signal
    srsran_vec_sc_prod_cfc(signal_buffer[0], scale, signal_buffer[0], sf_len);

    // Add signal to baseband buffer
    srsran_vec_sum_ccc(signal_buffer[0], baseband_buffer, baseband_buffer, sf_len);

    return ret;
  }

  ~test_enb()
  {
    for (uint32_t i = 0; i < enb_dl.cell.nof_ports; i++) {
      if (signal_buffer[i]) {
        free(signal_buffer[i]);
        signal_buffer[i] = nullptr;
      }
    }

    srsran_enb_dl_free(&enb_dl);
  }
};

/// Cell container metrics.
DECLARE_METRIC("earfcn_dl", metric_earfcn, int, "");
DECLARE_METRIC("PCI", metric_pci, uint32_t, "");
DECLARE_METRIC("RSRP", metric_rsrp, float, "");
DECLARE_METRIC("RSRQ", metric_rsrq, float, "");
DECLARE_METRIC("false_alarm", metric_false_alarm, std::string, "");
DECLARE_METRIC_SET("cell_container",
                   mset_cell_container,
                   metric_earfcn,
                   metric_pci,
                   metric_rsrp,
                   metric_rsrq,
                   metric_false_alarm);

/// Report root object.
DECLARE_METRIC("timestamp", metric_timestamp_tag, std::string, "");
DECLARE_METRIC_LIST("cell_list", mlist_cell, std::vector<mset_cell_container>);

/// Report context.
using report_context_t = srslog::build_context_type<metric_timestamp_tag, mlist_cell>;

class meas_itf_listener : public srsue::scell::intra_measure_base::meas_itf
{
  srslog::log_channel& json_channel;

public:
  typedef struct {
    float    rsrp_avg;
    float    rsrp_min;
    float    rsrp_max;
    float    rsrq_avg;
    float    rsrq_min;
    float    rsrq_max;
    uint32_t count;
  } cell_meas_t;

  explicit meas_itf_listener(srslog::log_channel& json_channel) : json_channel(json_channel) {}

  std::map<uint32_t, cell_meas_t> cells;

  void cell_meas_reset(uint32_t cc_idx) override {}
  void new_cell_meas(uint32_t cc_idx, const std::vector<srsue::phy_meas_t>& meas) override
  {
    for (auto& m : meas) {
      uint32_t pci = m.pci;
      if (!cells.count(pci)) {
        cells[pci].rsrp_min = m.rsrp;
        cells[pci].rsrp_max = m.rsrp;
        cells[pci].rsrp_avg = m.rsrp;
        cells[pci].rsrq_min = m.rsrq;
        cells[pci].rsrq_max = m.rsrq;
        cells[pci].rsrq_avg = m.rsrq;
        cells[pci].count    = 1;
      } else {
        cells[pci].rsrp_min = SRSRAN_MIN(cells[pci].rsrp_min, m.rsrp);
        cells[pci].rsrp_max = SRSRAN_MAX(cells[pci].rsrp_max, m.rsrp);
        cells[pci].rsrp_avg = (m.rsrp + cells[pci].rsrp_avg * cells[pci].count) / (cells[pci].count + 1);

        cells[pci].rsrq_min = SRSRAN_MIN(cells[pci].rsrq_min, m.rsrq);
        cells[pci].rsrq_max = SRSRAN_MAX(cells[pci].rsrq_max, m.rsrq);
        cells[pci].rsrq_avg = (m.rsrq + cells[pci].rsrq_avg * cells[pci].count) / (cells[pci].count + 1);
        cells[pci].count++;
      }
    }
  }

  bool print_stats()
  {
    printf("\n-- Statistics:\n");
    uint32_t true_counts        = 0;
    uint32_t false_counts       = 0;
    uint32_t tti_count          = (1000 * duration_execution_s) / phy_args.intra_freq_meas_period_ms;
    uint32_t ideal_true_counts  = (pcis_to_simulate.size() - 1) * tti_count;
    uint32_t ideal_false_counts = tti_count * cells.size() - ideal_true_counts;

    report_context_t ctx("JSON Report");
    auto current_time = fmt::localtime(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    fmt::memory_buffer ts_buffer;
    fmt::format_to(ts_buffer, "{:%F}T{:%T}", current_time, current_time);
    ctx.write<metric_timestamp_tag>(srsran::to_c_str(ts_buffer));

    auto& cell_list = ctx.get<mlist_cell>();

    for (auto& e : cells) {
      bool false_alarm = pcis_to_simulate.find(e.first) == pcis_to_simulate.end();

      if (false_alarm) {
        false_counts += e.second.count;
      } else {
        true_counts += e.second.count;
      }

      printf("  pci=%03d; count=%3d; false=%s; rsrp=%+.1f|%+.1f|%+.1fdBfs;  rsrq=%+.1f|%+.1f|%+.1fdB;\n",
             e.first,
             e.second.count,
             false_alarm ? "y" : "n",
             e.second.rsrp_min,
             e.second.rsrp_avg,
             e.second.rsrp_max,
             e.second.rsrq_min,
             e.second.rsrq_avg,
             e.second.rsrq_max);

      cell_list.emplace_back();
      auto& cell_container = cell_list.back();

      cell_container.write<metric_pci>(e.first);
      cell_container.write<metric_earfcn>(earfcn_dl);
      cell_container.write<metric_rsrp>(e.second.rsrp_avg);
      cell_container.write<metric_rsrq>(e.second.rsrq_avg);
      cell_container.write<metric_false_alarm>(false_alarm ? "yes" : "no");
    }

    json_channel(ctx);

    float prob_detection   = (ideal_true_counts) ? (float)true_counts / (float)ideal_true_counts : 0.0f;
    float prob_false_alarm = (ideal_false_counts) ? (float)false_counts / (float)ideal_false_counts : 0.0f;
    printf("\n");
    printf("    Probability of detection: %.6f\n", prob_detection);
    printf("  Probability of false alarm: %.6f\n", prob_false_alarm);

    return (prob_detection >= 0.9f && prob_false_alarm <= 0.1f);
  }
};

// shorten boost program options namespace
namespace bpo = boost::program_options;

int parse_args(int argc, char** argv)
{
  int ret = SRSRAN_SUCCESS;

  bpo::options_description options;
  bpo::options_description common("Common execution options");
  bpo::options_description over_the_air("Over the air execution options");
  bpo::options_description simulation("Over the air execution options");

  // clang-format off
  common.add_options()
      ("duration",                  bpo::value<uint32_t>(&duration_execution_s)->default_value(60),                "Duration of the execution in seconds")
      ("cell.nof_prb",              bpo::value<uint32_t>(&cell_base.nof_prb)->default_value(100),                  "Cell Number of PRB")
      ("cell.nof_ports",            bpo::value<uint32_t>(&cell_base.nof_ports)->default_value(1),                  "Cell Number of Tx ports")
      ("intra_meas_log_level",      bpo::value<std::string>(&intra_meas_log_level)->default_value("none"),         "Intra measurement log level (none, warning, info, debug)")
      ("intra_freq_meas_len_ms",    bpo::value<uint32_t>(&phy_args.intra_freq_meas_len_ms)->default_value(20),     "Intra measurement measurement length")
      ("intra_freq_meas_period_ms", bpo::value<uint32_t>(&phy_args.intra_freq_meas_period_ms)->default_value(200), "Intra measurement measurement period")
      ("phy_lib_log_level",         bpo::value<int>(&phy_lib_log_level)->default_value(SRSRAN_VERBOSE_NONE),       "Phy lib log level (0: none, 1: info, 2: debug)")
      ("active_cell_list",          bpo::value<std::string>(&active_cell_list)->default_value("10,17,24,31,38,45,52"),    "Comma separated neighbour PCI cell list")
      ("enable_json_report",        bpo::value<bool>(&enable_json_report)->default_value(false),                   "Enable JSON file reporting")
      ("json_report_filename",      bpo::value<std::string>(&json_report_filename)->default_value("/tmp/scell_search.json"), "JSON report filename")
      ;

  over_the_air.add_options()
      ("rf.dl_earfcn",              bpo::value<int>(&earfcn_dl)->default_value(-1),                                "DL EARFCN (setting this param enables over-the-air execution)")
      ("rf.device_name",            bpo::value<std::string>(&radio_device_name)->default_value("auto"),            "RF Device Name")
      ("rf.device_args",            bpo::value<std::string>(&radio_device_args)->default_value("auto"),            "RF Device arguments")
      ("rf.log_level",              bpo::value<std::string>(&radio_log_level)->default_value("info"),              "RF Log level (none, warning, info, debug)")
      ("rf.rx_gain",                bpo::value<float>(&rx_gain)->default_value(30.0f),                             "RF Receiver gain in dB")
      ("radio_log_level",           bpo::value<std::string>(&radio_log_level)->default_value("info"),              "RF Log level")
      ;

  simulation.add_options()
      ("simulation_cell_list",      bpo::value<std::string>(&simulation_cell_list)->default_value("10,17,24,31,38,45,52"),    "Comma separated neighbour PCI cell list")
      ("cell_cfi",                  bpo::value<uint32_t >(&cfi)->default_value(1),                                 "Cell CFI")
      ("channel_period_s",          bpo::value<float>(&channel_period_s)->default_value(16.8),                     "Channel period for HST and delay")
      ("ncell_attenuation",         bpo::value<float>(&ncell_attenuation_dB)->default_value(3.0f),                 "Neighbour cell attenuation relative to serving cell in dB")
      ("channel.hst.fd",            bpo::value<float>(&channel_hst_fd_hz)->default_value(750.0f),                  "Channel High Speed Train doppler in Hz. Set to 0 for disabling")
      ("channel.delay_max",         bpo::value<float>(&channel_delay_max_us)->default_value(4.7f),                 "Maximum simulated delay in microseconds. Set to 0 for disabling")
      ("channel.snr",               bpo::value<float>(&channel_snr_db)->default_value(NAN),                        "Channel simulator SNR in dB")
      ("channel.log_level",         bpo::value<std::string>(&channel_log_level)->default_value("info"),            "Channel simulator logging level")
      ("serving_cell_pdsch_enable", bpo::value<bool>(&serving_cell_pdsch_enable)->default_value(true),             "Enable simulated PDSCH in serving cell")
      ("serving_cell_pdsch_rnti",   bpo::value<uint16_t >(&serving_cell_pdsch_rnti)->default_value(0x1234),        "Simulated PDSCH RNTI")
      ("serving_cell_pdsch_tm",     bpo::value<int>((int*) &serving_cell_pdsch_tm)->default_value(SRSRAN_TM1),     "Simulated Transmission mode 0: TM1, 1: TM2, 2: TM3, 3: TM4")
      ("serving_cell_pdsch_mcs",    bpo::value<uint16_t >(&serving_cell_pdsch_mcs)->default_value(20),             "Simulated PDSCH MCS")
      ;

  options.add(common).add(over_the_air).add(simulation).add_options()
      ("help",                      "Show this message")
      ;
  // clang-format on

  bpo::variables_map vm;
  try {
    bpo::store(bpo::command_line_parser(argc, argv).options(options).run(), vm);
    bpo::notify(vm);
  } catch (bpo::error& e) {
    std::cerr << e.what() << std::endl;
    ret = SRSRAN_ERROR;
  }

  // help option was given or error - print usage and exit
  if (vm.count("help") || ret) {
    std::cout << "Usage: " << argv[0] << " [OPTIONS] config_file" << std::endl << std::endl;
    std::cout << options << std::endl << std::endl;
    ret = SRSRAN_ERROR;
  }

  return ret;
}

static void pci_list_parse_helper(std::string& list_str, std::set<uint32_t>& list)
{
  if (list_str == "all") {
    // Add all possible cells
    for (int i = 0; i < 504; i++) {
      list.insert(i);
    }
  } else if (list_str == "none") {
    // Do nothing
  } else if (not list_str.empty()) {
    // Remove spaces from neightbour cell list
    list_str = srsran::string_remove_char(list_str, ' ');

    // Add cell to known cells
    srsran::string_parse_list(list_str, ',', list);
  }
}

int main(int argc, char** argv)
{
  int ret;

  // Parse args
  if (parse_args(argc, argv)) {
    return SRSRAN_ERROR;
  }

  // Common for simulation and over-the-air
  srslog::basic_logger& logger = srslog::fetch_basic_logger("intra_measure");
  srslog::sink& json_sink = srslog::fetch_file_sink(json_report_filename, 0, false, srslog::create_json_formatter());
  srslog::log_channel& json_channel = srslog::fetch_log_channel("JSON_channel", json_sink, {});
  json_channel.set_enabled(enable_json_report);
  srslog::init();

  cf_t*                           baseband_buffer = srsran_vec_cf_malloc(SRSRAN_SF_LEN_MAX);
  srsran::rf_timestamp_t          ts              = {};
  meas_itf_listener               rrc(json_channel);
  srsue::scell::intra_measure_lte intra_measure(logger, rrc);

  // Simulation only
  std::vector<std::unique_ptr<test_enb> > test_enb_v;
  uint8_t*                                data_tx[SRSRAN_MAX_TB]       = {};
  srsran_softbuffer_tx_t*                 softbuffer_tx[SRSRAN_MAX_TB] = {};

  // Over-the-air only
  std::unique_ptr<srsran::radio> radio = nullptr;

  // Set Receiver args
  phy_args.estimator_fil_auto           = false;
  phy_args.estimator_fil_order          = 4;
  phy_args.estimator_fil_stddev         = 1.0f;
  phy_args.interpolate_subframe_enabled = false;
  phy_args.nof_rx_ant                   = 1;
  phy_args.cfo_is_doppler               = true;
  phy_args.cfo_integer_enabled          = false;
  phy_args.cfo_correct_tol_hz           = 1.0f;
  phy_args.cfo_pss_ema                  = DEFAULT_CFO_EMA_TRACK;
  phy_args.cfo_ref_mask                 = 1023;
  phy_args.cfo_loop_bw_pss              = DEFAULT_CFO_BW_PSS;
  phy_args.cfo_loop_bw_ref              = DEFAULT_CFO_BW_REF;
  phy_args.cfo_loop_pss_tol             = DEFAULT_CFO_PSS_MIN;
  phy_args.cfo_loop_ref_min             = DEFAULT_CFO_REF_MIN;
  phy_args.cfo_loop_pss_conv            = DEFAULT_PSS_STABLE_TIMEOUT;
  phy_args.snr_estim_alg                = "refs";
  phy_args.snr_ema_coeff                = 0.1f;
  phy_args.rx_gain_offset               = rx_gain + 62.0f;

  // Set phy-lib logging level
  set_srsran_verbose_level(phy_lib_log_level);

  // Allocate PDSCH data and tx-soft-buffers only if pdsch is enabled and radio is not available
  for (int i = 0; i < SRSRAN_MAX_TB && serving_cell_pdsch_enable && radio == nullptr; i++) {
    srsran_random_t random_gen = srsran_random_init(serving_cell_pdsch_rnti);

    const size_t nof_bytes = (6144 * 16 * 3 / 8);
    softbuffer_tx[i]       = (srsran_softbuffer_tx_t*)calloc(sizeof(srsran_softbuffer_tx_t), 1);
    if (!softbuffer_tx[i]) {
      ERROR("Error allocating softbuffer_tx");
    }

    if (srsran_softbuffer_tx_init(softbuffer_tx[i], cell_base.nof_prb)) {
      ERROR("Error initiating softbuffer_tx");
    }

    data_tx[i] = srsran_vec_u8_malloc(nof_bytes);
    if (!data_tx[i]) {
      ERROR("Error allocating data tx");
    } else {
      for (uint32_t j = 0; j < nof_bytes; j++) {
        data_tx[i][j] = (uint8_t)srsran_random_uniform_int_dist(random_gen, 0, 255);
      }
    }

    srsran_random_free(random_gen);
  }

  pci_list_parse_helper(active_cell_list, pcis_to_meas);
  pci_list_parse_helper(simulation_cell_list, pcis_to_simulate);

  // Set cell_base id with the serving cell
  uint32_t serving_cell_id = *pcis_to_simulate.begin();
  cell_base.id             = serving_cell_id;

  logger.set_level(srslog::str_to_basic_level(intra_meas_log_level));

  srsue::scell::intra_measure_base::args_t args = {};
  args.len_ms                                   = phy_args.intra_freq_meas_len_ms;
  args.period_ms                                = phy_args.intra_freq_meas_period_ms;
  args.rx_gain_offset_db                        = phy_args.rx_gain_offset;

  intra_measure.init(0, args);
  intra_measure.set_primary_cell(SRSRAN_MAX(earfcn_dl, 0), cell_base);

  if (earfcn_dl >= 0) {
    // Create radio log
    auto& radio_logger = srslog::fetch_basic_logger("RF", false);
    radio_logger.set_level(srslog::str_to_basic_level(radio_log_level));

    // Create radio
    radio = std::unique_ptr<srsran::radio>(new srsran::radio);

    // Init radio
    srsran::rf_args_t radio_args = {};
    radio_args.device_args       = radio_device_args;
    radio_args.device_name       = radio_device_name;
    radio_args.nof_carriers      = 1;
    radio_args.nof_antennas      = 1;
    radio->init(radio_args, nullptr);

    // Set sampling rate
    radio->set_rx_srate(srsran_sampling_freq_hz(cell_base.nof_prb));

    // Set frequency
    radio->set_rx_freq(0, srsran_band_fd(earfcn_dl) * 1e6);

  } else {
    // Create test eNb's if radio is not available
    float channel_init_time_s = 0;
    float channel_delay_us    = 0;
    for (const auto& pci : pcis_to_simulate) {
      // Initialise cell
      srsran_cell_t cell = cell_base;
      cell.id            = pci;

      // Initialise channel and push back
      srsran::channel::args_t channel_args;
      channel_args.enable                 = (channel_period_s != 0);
      channel_args.hst_enable             = std::isnormal(channel_hst_fd_hz);
      channel_args.hst_init_time_s        = channel_init_time_s;
      channel_args.hst_period_s           = (float)channel_period_s;
      channel_args.hst_fd_hz              = channel_hst_fd_hz;
      channel_args.delay_enable           = std::isnormal(channel_delay_max_us);
      channel_args.delay_min_us           = channel_delay_us;
      channel_args.delay_max_us           = channel_delay_us;
      channel_args.delay_period_s         = (uint32_t)channel_period_s;
      channel_args.delay_init_time_s      = channel_init_time_s;
      channel_args.awgn_enable            = std::isnormal(channel_snr_db) and (pci == *pcis_to_simulate.begin());
      channel_args.awgn_signal_power_dBfs = srsran_enb_dl_get_maximum_signal_power_dBfs(cell.nof_prb);
      channel_args.awgn_snr_dB            = channel_snr_db;
      test_enb_v.push_back(std::unique_ptr<test_enb>(new test_enb(cell, channel_args)));

      // Add cell to known cells
      if (active_cell_list.empty()) {
        pcis_to_meas.insert(cell.id);
      }

      // Increase init time
      channel_init_time_s += channel_period_s / (float)pcis_to_simulate.size();
      channel_delay_us += channel_delay_max_us / (float)pcis_to_simulate.size();
    }
  }

  // pass cells to measure to intra_measure object

  intra_measure.set_cells_to_meas(pcis_to_meas);

  // Run loop
  for (uint32_t sf_idx = 0; sf_idx < duration_execution_s * 1000; sf_idx++) {
    srsran_dl_sf_cfg_t sf_cfg_dl = {};
    sf_cfg_dl.tti                = sf_idx % 10240;
    sf_cfg_dl.cfi                = cfi;
    sf_cfg_dl.sf_type            = SRSRAN_SF_NORM;

    // Clean buffer
    srsran_vec_cf_zero(baseband_buffer, SRSRAN_SF_LEN_MAX);

    if (radio) {
      // Receive radio
      srsran::rf_buffer_t radio_buffer(baseband_buffer, SRSRAN_SF_LEN_PRB(cell_base.nof_prb));
      radio->rx_now(radio_buffer, ts);
    } else {
      // Run eNb simulator
      bool put_pdsch = serving_cell_pdsch_enable;

      for (auto& enb : test_enb_v) {
        if (put_pdsch) {
          // Reset pdsch put flag
          put_pdsch = false;

          // DCI Configuration
          srsran_dci_dl_t  dci;
          srsran_dci_cfg_t dci_cfg;
          dci_cfg.srs_request_enabled          = false;
          dci_cfg.ra_format_enabled            = false;
          dci_cfg.multiple_csi_request_enabled = false;
          dci_cfg.is_not_ue_ss                 = false;

          // DCI Fixed values
          dci.pid                 = 0;
          dci.pinfo               = 0;
          dci.rnti                = serving_cell_pdsch_rnti;
          dci.is_tdd              = false;
          dci.is_dwpts            = false;
          dci.is_ra_order         = false;
          dci.tb_cw_swap          = false;
          dci.pconf               = false;
          dci.power_offset        = false;
          dci.tpc_pucch           = 0;
          dci.ra_preamble         = 0;
          dci.ra_mask_idx         = 0;
          dci.srs_request         = false;
          dci.srs_request_present = false;
          dci.cif_present         = false;
          dci_cfg.cif_enabled     = false;

          // Set PRB Allocation type
          dci.alloc_type              = SRSRAN_RA_ALLOC_TYPE0;
          prbset_num                  = (int)ceilf((float)cell_base.nof_prb / srsran_ra_type0_P(cell_base.nof_prb));
          last_prbset_num             = prbset_num;
          dci.type0_alloc.rbg_bitmask = prbset_to_bitmask();
          dci.location.L              = 0;
          dci.location.ncce           = 0;

          // Set TB
          if (serving_cell_pdsch_tm < SRSRAN_TM3) {
            dci.format        = SRSRAN_DCI_FORMAT1;
            dci.tb[0].mcs_idx = serving_cell_pdsch_mcs;
            dci.tb[0].rv      = 0;
            dci.tb[0].ndi     = false;
            dci.tb[0].cw_idx  = 0;
            dci.tb[1].mcs_idx = 0;
            dci.tb[1].rv      = 1;
          } else if (serving_cell_pdsch_tm == SRSRAN_TM3) {
            dci.format = SRSRAN_DCI_FORMAT2A;
            for (uint32_t i = 0; i < SRSRAN_MAX_TB; i++) {
              dci.tb[i].mcs_idx = serving_cell_pdsch_mcs;
              dci.tb[i].rv      = 0;
              dci.tb[i].ndi     = false;
              dci.tb[i].cw_idx  = i;
            }
          } else if (serving_cell_pdsch_tm == SRSRAN_TM4) {
            dci.format = SRSRAN_DCI_FORMAT2;
            dci.pinfo  = 0;
            for (uint32_t i = 0; i < SRSRAN_MAX_TB; i++) {
              dci.tb[i].mcs_idx = serving_cell_pdsch_mcs;
              dci.tb[i].rv      = 0;
              dci.tb[i].ndi     = false;
              dci.tb[i].cw_idx  = i;
            }
          } else {
            ERROR("Wrong transmission mode (%d)", serving_cell_pdsch_tm);
          }
          enb->work(&sf_cfg_dl, &dci_cfg, &dci, softbuffer_tx, data_tx, baseband_buffer, ts);
        } else {
          enb->work(&sf_cfg_dl, nullptr, nullptr, nullptr, nullptr, baseband_buffer, ts);
        }
      }
      // if it measuring, wait for avoiding overflowing
      intra_measure.wait_meas();
    }

    // Increase Time counter
    ts.add(0.001);

    // Give data to intra measure component
    intra_measure.run_tti(sf_idx % 10240, baseband_buffer, SRSRAN_SF_LEN_PRB(cell_base.nof_prb));
    if (sf_idx % 1000 == 0) {
      printf("Done %.1f%%\n", (double)sf_idx * 100.0 / ((double)duration_execution_s * 1000.0));
    }
  }

  // make sure last measurement has been received before stopping
  if (not radio) {
    intra_measure.wait_meas();
  }

  // Stop, it will block until the asynchronous thread quits
  intra_measure.stop();

  ret = rrc.print_stats() ? SRSRAN_SUCCESS : SRSRAN_ERROR;

  if (radio) {
    radio->stop();
  }

  if (baseband_buffer) {
    free(baseband_buffer);
  }

  for (auto& ptr : data_tx) {
    if (ptr) {
      free(ptr);
    }
  }
  for (auto& sb : softbuffer_tx) {
    if (sb) {
      srsran_softbuffer_tx_free(sb);
      free(sb);
    }
  }

  srslog::flush();

  if (ret && radio == nullptr) {
    printf("Error\n");
  } else {
    printf("Ok\n");
  }

  return ret;
}
