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

#include "srsran/build_info.h"
#include "srsran/common/tsan_options.h"
#include "srsran/srslog/srslog.h"
#include "srsue/hdr/ue.h"
#include "swappable_sink.h"
#include "ttcn3_syssim.h"
#include <boost/program_options.hpp>
#include <boost/program_options/parsers.hpp>
#include <iostream>

using namespace srsran;
using namespace srsue;
;
using namespace std;
using namespace rapidjson;
namespace bpo = boost::program_options;

typedef struct {
  pcap_args_t mac_pcap;
  pcap_args_t nas_pcap;
  std::string log_filename;
  std::string log_level;
  int32_t     log_hex_level;
} ttcn3_dut_args_t;

all_args_t parse_args(ttcn3_dut_args_t* args, int argc, char* argv[])
{
  // Command line only options
  bpo::options_description general("General options");

  general.add_options()("help,h", "Produce help message")("version,v", "Print version information and exit");

  // Command line or config file options
  bpo::options_description common("Configuration options");
  // clang-format off
  common.add_options()
      ("pcap.enable", bpo::value<bool>(&args->mac_pcap.enable)->default_value(true), "Enable MAC packet captures for wireshark")
      ("pcap.filename", bpo::value<string>(&args->mac_pcap.filename)->default_value("/tmp/ttcn3_ue.pcap"), "MAC layer capture filename")
      ("pcap.nas_enable",   bpo::value<bool>(&args->nas_pcap.enable)->default_value(false), "Enable NAS packet captures for wireshark")
      ("pcap.nas_filename", bpo::value<string>(&args->nas_pcap.filename)->default_value("/tmp/ttcn3_ue_nas.pcap"), "NAS layer capture filename (useful when NAS encryption is enabled)")
      ("logfilename",   bpo::value<std::string>(&args->log_filename)->default_value("/tmp/ttcn3_ue.log"), "Filename of log file")
      ("loglevel",      bpo::value<std::string>(&args->log_level)->default_value("warning"), "Log level (Error,Warning,Info,Debug)")
      ("loghexlevel",   bpo::value<int32_t>(&args->log_hex_level)->default_value(64), "Log hex level (-1 unbounded)");
  // clang-format on

  // these options are allowed on the command line
  bpo::options_description cmdline_options;
  cmdline_options.add(common).add(general);

  // parse the command line and store result in vm
  bpo::variables_map vm;
  bpo::store(bpo::command_line_parser(argc, argv).options(cmdline_options).run(), vm);
  bpo::notify(vm);

  // help option was given - print usage and exit
  if (vm.count("help")) {
    cout << "Usage: " << argv[0] << " [OPTIONS] config_file" << endl << endl;
    cout << common << endl << general << endl;
    exit(0);
  }

  all_args_t all_args = {};

  all_args.stack.pkt_trace.enable = "mac,nas";

  all_args.stack.pkt_trace.mac_pcap.enable   = args->mac_pcap.enable;
  all_args.stack.pkt_trace.mac_pcap.filename = args->mac_pcap.filename;

  all_args.stack.pkt_trace.nas_pcap.enable   = args->nas_pcap.enable;
  all_args.stack.pkt_trace.nas_pcap.filename = args->nas_pcap.filename;

  all_args.log.filename      = args->log_filename;
  all_args.log.all_level     = args->log_level;
  all_args.log.all_hex_limit = args->log_hex_level;

  all_args.phy.log.phy_level        = args->log_level;

  all_args.stack.log.stack_level     = args->log_level;
  all_args.stack.log.mac_level      = args->log_level;
  all_args.stack.log.rlc_level      = args->log_level;
  all_args.stack.log.pdcp_level     = args->log_level;
  all_args.stack.log.rrc_level      = args->log_level;
  all_args.stack.log.nas_level      = args->log_level;
  all_args.stack.log.gw_level       = args->log_level;
  all_args.stack.log.usim_level     = args->log_level;
  all_args.phy.log.phy_hex_limit    = args->log_hex_level;
  all_args.stack.log.stack_hex_limit = args->log_hex_level;
  all_args.stack.log.mac_hex_limit  = args->log_hex_level;
  all_args.stack.log.rlc_hex_limit  = args->log_hex_level;
  all_args.stack.log.pdcp_hex_limit = args->log_hex_level;
  all_args.stack.log.rrc_hex_limit  = args->log_hex_level;
  all_args.stack.log.nas_hex_limit  = args->log_hex_level;
  all_args.stack.log.gw_hex_limit   = args->log_hex_level;
  all_args.stack.log.usim_hex_limit = args->log_hex_level;

  all_args.stack.sync_queue_size = MULTIQUEUE_DEFAULT_CAPACITY;

  return all_args;
}

int main(int argc, char** argv)
{
  std::cout << "Built in " << srsran_get_build_mode() << " mode using " << srsran_get_build_info() << "." << std::endl;

  // we handle OS signals through epoll
  block_signals();

  ttcn3_dut_args_t dut_args = {};
  all_args_t       ue_args  = parse_args(&dut_args, argc, argv);

  // Create a swappable sink, install it and use it as the default one.
  if (!srslog::install_custom_sink(swappable_sink::name(),
                                   std::unique_ptr<swappable_sink>(new swappable_sink(
                                       dut_args.log_filename, srslog::get_default_log_formatter())))) {
    return SRSRAN_ERROR;
  }
  auto* default_sink = srslog::find_sink(swappable_sink::name());
  if (!default_sink) {
    return SRSRAN_ERROR;
  }
  srslog::set_default_sink(*default_sink);

  // Start the log backend.
  srslog::init();

  // Create UE object
  unique_ptr<ttcn3_ue> ue = std::unique_ptr<ttcn3_ue>(new ttcn3_ue());

  // create and init SYSSIM
  ttcn3_syssim syssim(ue.get());
  if (syssim.init(ue_args) != SRSRAN_SUCCESS) {
    fprintf(stderr, "Error: Couldn't initialize system simulator\n");
    return SRSRAN_ERROR;
  }

  return syssim.run();
}
