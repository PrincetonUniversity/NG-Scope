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

#include "srsepc/hdr/mbms-gw/mbms-gw.h"
#include "srsran/common/config_file.h"
#include "srsran/srslog/srslog.h"
#include <boost/program_options.hpp>
#include <iostream>
#include <signal.h>

using namespace std;
using namespace srsepc;
namespace bpo = boost::program_options;

bool running = true;

void sig_int_handler(int signo)
{
  running = false;
}

typedef struct {
  std::string mbms_gw_level;
  int         mbms_gw_hex_limit;
  std::string all_level;
  int         all_hex_limit;
  std::string filename;
} log_args_t;

typedef struct {
  mbms_gw_args_t mbms_gw_args;
  log_args_t     log_args;
} all_args_t;

/**********************************************************************
 *  Program arguments processing
 ***********************************************************************/
string config_file;

void parse_args(all_args_t* args, int argc, char* argv[])
{
  string mbms_gw_name;
  string mbms_gw_sgi_mb_if_name;
  string mbms_gw_sgi_mb_if_addr;
  string mbms_gw_sgi_mb_if_mask;
  string mbms_gw_m1u_multi_addr;
  string mbms_gw_m1u_multi_if;

  string log_filename;

  // Command line only options
  bpo::options_description general("General options");

  // clang-format off
  general.add_options()
      ("help,h", "Produce help message")
      ("version,v", "Print version information and exit")
      ;

  // Command line or config file options
  bpo::options_description common("Configuration options");
  common.add_options()

    ("mbms_gw.name",      bpo::value<string>(&mbms_gw_name)->default_value("srsmbmsgw01"), "MBMS-GW Name")
    ("mbms_gw.sgi_mb_if_name",      bpo::value<string>(&mbms_gw_sgi_mb_if_name)->default_value("sgi_mb"), "SGi-mb TUN interface Address.")
    ("mbms_gw.sgi_mb_if_addr",      bpo::value<string>(&mbms_gw_sgi_mb_if_addr)->default_value("172.16.1.1"), "SGi-mb TUN interface Address.")
    ("mbms_gw.sgi_mb_if_mask",      bpo::value<string>(&mbms_gw_sgi_mb_if_mask)->default_value("255.255.255.255"), "SGi-mb TUN interface mask.")
    ("mbms_gw.m1u_multi_addr",      bpo::value<string>(&mbms_gw_m1u_multi_addr)->default_value("239.255.0.1"), "M1-u GTPu destination multicast address.")
    ("mbms_gw.m1u_multi_if",        bpo::value<string>(&mbms_gw_m1u_multi_if)->default_value("127.0.1.200"), "Local interface IP for M1-U multicast packets.")
    ("mbms_gw.m1u_multi_ttl",       bpo::value<int>(&args->mbms_gw_args.m1u_multi_ttl)->default_value(1), "TTL for M1-U multicast packets.")

    ("log.all_level",     bpo::value<string>(&args->log_args.all_level)->default_value("info"),   "ALL log level")
    ("log.all_hex_limit", bpo::value<int>(&args->log_args.all_hex_limit)->default_value(32),  "ALL log hex dump limit")

    ("log.filename",      bpo::value<string>(&args->log_args.filename)->default_value("/tmp/mbms.log"),"Log filename")
    ;

  // Positional options - config file location
  bpo::options_description position("Positional options");
  position.add_options()
  ("config_file", bpo::value< string >(&config_file), "MBMS-GW configuration file")
  ;

  // clang-format on
  bpo::positional_options_description p;
  p.add("config_file", -1);

  // these options are allowed on the command line
  bpo::options_description cmdline_options;
  cmdline_options.add(common).add(position).add(general);

  // parse the command line and store result in vm
  bpo::variables_map vm;
  try {
    bpo::store(bpo::command_line_parser(argc, argv).options(cmdline_options).positional(p).run(), vm);
    bpo::notify(vm);
  } catch (bpo::error& e) {
    cerr << e.what() << endl;
    exit(1);
  }

  // help option was given - print usage and exit
  if (vm.count("help")) {
    cout << "Usage: " << argv[0] << " [OPTIONS] config_file" << endl << endl;
    cout << common << endl << general << endl;
    exit(0);
  }

  // if no config file given, check users home path
  if (!vm.count("config_file")) {
    if (!config_exists(config_file, "mbms.conf")) {
      cout << "Failed to read MBMS-GW configuration file " << config_file << " - exiting" << endl;
      exit(1);
    }
  }

  // Parsing Config File
  cout << "Reading configuration file " << config_file << "..." << endl;
  ifstream conf(config_file.c_str(), ios::in);
  if (conf.fail()) {
    cout << "Failed to read configuration file " << config_file << " - exiting" << endl;
    exit(1);
  }
  bpo::store(bpo::parse_config_file(conf, common), vm);
  bpo::notify(vm);

  args->mbms_gw_args.name           = mbms_gw_name;
  args->mbms_gw_args.sgi_mb_if_name = mbms_gw_sgi_mb_if_name;
  args->mbms_gw_args.sgi_mb_if_addr = mbms_gw_sgi_mb_if_addr;
  args->mbms_gw_args.sgi_mb_if_mask = mbms_gw_sgi_mb_if_mask;
  args->mbms_gw_args.m1u_multi_addr = mbms_gw_m1u_multi_addr;
  args->mbms_gw_args.m1u_multi_if   = mbms_gw_m1u_multi_if;

  // Apply all_level to any unset layers
  if (vm.count("log.all_level")) {
    if (!vm.count("log.mbms_gw_level")) {
      args->log_args.mbms_gw_level = args->log_args.all_level;
    }
  }

  // Apply all_hex_limit to any unset layers
  if (vm.count("log.all_hex_limit")) {
    if (!vm.count("log.mbms_gw_hex_limit")) {
      args->log_args.mbms_gw_hex_limit = args->log_args.all_hex_limit;
    }
  }
  return;
}

int main(int argc, char* argv[])
{
  cout << endl << "---  Software Radio Systems MBMS  ---" << endl << endl;
  signal(SIGINT, sig_int_handler);
  signal(SIGTERM, sig_int_handler);
  signal(SIGHUP, sig_int_handler);

  all_args_t args;
  parse_args(&args, argc, argv);

  srslog::sink* log_sink = (args.log_args.filename == "stdout") ? srslog::create_stdout_sink()
                                                                : srslog::create_file_sink(args.log_args.filename);
  if (!log_sink) {
    return SRSRAN_ERROR;
  }
  srslog::log_channel* chan = srslog::create_log_channel("main_channel", *log_sink);
  if (!chan) {
    return SRSRAN_ERROR;
  }
  srslog::set_default_sink(*log_sink);

  // Start the log backend.
  srslog::init();

  if (args.log_args.filename != "stdout") {
    auto& mbms_gw_logger = srslog::fetch_basic_logger("MBMS GW", false);
    mbms_gw_logger.info("\n---  Software Radio Systems MBMS log ---\n\n");
  }

  auto& mbms_gw_logger = srslog::fetch_basic_logger("MBMS", false);
  mbms_gw_logger.set_level(srslog::str_to_basic_level(args.log_args.mbms_gw_level));
  mbms_gw_logger.set_hex_dump_max_size(args.log_args.mbms_gw_hex_limit);

  mbms_gw* mbms_gw = mbms_gw::get_instance();
  if (mbms_gw->init(&args.mbms_gw_args)) {
    cout << "Error initializing MBMS-GW" << endl;
    exit(1);
  }

  mbms_gw->start();
  while (running) {
    sleep(1);
  }

  mbms_gw->stop();
  mbms_gw->cleanup();

  cout << std::endl << "---  exiting  ---" << endl;
  return 0;
}
