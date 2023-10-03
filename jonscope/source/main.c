/**
 * Copyright 2013-2023 Software Radio Systems Limited
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

#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef ENABLE_ASN4G
#include <libasn4g.h>
#endif

/* Local libs */
#include "jonscope/headers/cpu.h"
#include "jonscope/headers/cell_scan.h"

/* SRS libs */
#include "srsran/common/crash_handler.h"
#include "srsran/common/gen_mch_tables.h"
#include "srsran/phy/io/filesink.h"
#include "srsran/srsran.h"
#include "srsran/phy/rf/rf.h"
#include "srsran/phy/rf/rf_utils.h"

#define ENABLE_AGC_DEFAULT

#define MAX_SRATE_DELTA 2 // allowable delta (in Hz) between requested and actual sample rate

cell_search_cfg_t cell_detect_config = {.max_frames_pbch      = SRSRAN_DEFAULT_MAX_FRAMES_PBCH,
                                        .max_frames_pss       = SRSRAN_DEFAULT_MAX_FRAMES_PSS,
                                        .nof_valid_pss_frames = SRSRAN_DEFAULT_NOF_VALID_PSS_FRAMES,
                                        .init_agc             = 0,
                                        .force_tdd            = false};

//#define STDOUT_COMPACT

char* output_file_name;
//#define PRINT_CHANGE_SCHEDULING

//#define CORRECT_SAMPLE_OFFSET

/**********************************************************************
 *  Program arguments processing
 ***********************************************************************/
typedef struct {
  int      nof_subframes;
  int      cpu_affinity;
  bool     disable_plots;
  bool     disable_plots_except_constellation;
  bool     disable_cfo;
  uint32_t time_offset;
  int      force_N_id_2;
  uint16_t rnti;
  char*    input_file_name;
  int      file_offset_time;
  float    file_offset_freq;
  uint32_t file_nof_prb;
  uint32_t file_nof_ports;
  uint32_t file_cell_id;
  bool     enable_cfo_ref;
  char*    estimator_alg;
  char*    rf_dev;
  char*    rf_args;
  uint32_t rf_nof_rx_ant;
  double   rf_freq;
  float    rf_gain;
  int      net_port;
  char*    net_address;
  int      net_port_signal;
  char*    net_address_signal;
  int      decimate;
  int32_t  mbsfn_area_id;
  uint8_t  non_mbsfn_region;
  uint8_t  mbsfn_sf_mask;
  int      tdd_special_sf;
  int      sf_config;
  int      verbose;
  bool     enable_256qam;
  bool     use_standard_lte_rate;
} prog_args_t;

void args_default(prog_args_t* args)
{
  args->disable_plots                      = false;
  args->disable_plots_except_constellation = false;
  args->nof_subframes                      = -1;
  args->rnti                               = SRSRAN_SIRNTI;
  args->force_N_id_2                       = -1; // Pick the best
  args->tdd_special_sf                     = -1;
  args->sf_config                          = -1;
  args->input_file_name                    = NULL;
  args->disable_cfo                        = false;
  args->time_offset                        = 0;
  args->file_nof_prb                       = 25;
  args->file_nof_ports                     = 1;
  args->file_cell_id                       = 0;
  args->file_offset_time                   = 0;
  args->file_offset_freq                   = 0;
  args->rf_dev                             = "";
  args->rf_args                            = "";
  args->rf_freq                            = -1.0;
  args->rf_nof_rx_ant                      = 1;
  args->enable_cfo_ref                     = false;
  args->estimator_alg                      = "interpolate";
  args->enable_256qam                      = false;
#ifdef ENABLE_AGC_DEFAULT
  args->rf_gain = -1.0;
#else
  args->rf_gain = 50.0;
#endif
  args->net_port           = -1;
  args->net_address        = "127.0.0.1";
  args->net_port_signal    = -1;
  args->net_address_signal = "127.0.0.1";
  args->decimate           = 0;
  args->cpu_affinity       = -1;
  args->mbsfn_area_id      = -1;
  args->non_mbsfn_region   = 2;
  args->mbsfn_sf_mask      = 32;
}

void usage(prog_args_t* args, char* prog)
{
  printf("Usage: %s [adgpPoOcildFRDnruMNvTG] -f rx_frequency (in Hz) | -i input_file\n", prog);
#ifndef DISABLE_RF
  printf("\t-I RF dev [Default %s]\n", args->rf_dev);
  printf("\t-a RF args [Default %s]\n", args->rf_args);
  printf("\t-A Number of RX antennas [Default %d]\n", args->rf_nof_rx_ant);
#ifdef ENABLE_AGC_DEFAULT
  printf("\t-g RF fix RX gain [Default AGC]\n");
#else
  printf("\t-g Set RX gain [Default %.1f dB]\n", args->rf_gain);
#endif
#else
  printf("\t   RF is disabled.\n");
#endif
  printf("\t-i input_file [Default use RF board]\n");
  printf("\t-o offset frequency correction (in Hz) for input file [Default %.1f Hz]\n", args->file_offset_freq);
  printf("\t-O offset samples for input file [Default %d]\n", args->file_offset_time);
  printf("\t-p nof_prb for input file [Default %d]\n", args->file_nof_prb);
  printf("\t-P nof_ports for input file [Default %d]\n", args->file_nof_ports);
  printf("\t-c cell_id for input file [Default %d]\n", args->file_cell_id);
  printf("\t-r RNTI in Hex [Default 0x%x]\n", args->rnti);
  printf("\t-l Force N_id_2 [Default best]\n");
  printf("\t-C Disable CFO correction [Default %s]\n", args->disable_cfo ? "Disabled" : "Enabled");
  printf("\t-F Enable RS-based CFO correction [Default %s]\n", !args->enable_cfo_ref ? "Disabled" : "Enabled");
  printf("\t-R Channel estimates algorithm (average, interpolate, wiener) [Default %s]\n", args->estimator_alg);
  printf("\t-t Add time offset [Default %d]\n", args->time_offset);
  printf("\t-T Set TDD special subframe configuration [Default %d]\n", args->tdd_special_sf);
  printf("\t-G Set TDD uplink/downlink configuration [Default %d]\n", args->sf_config);
  printf("\t-y set the cpu affinity mask [Default %d] \n  ", args->cpu_affinity);
  printf("\t-n nof_subframes [Default %d]\n", args->nof_subframes);
  printf("\t-s remote UDP port to send input signal (-1 does nothing with it) [Default %d]\n", args->net_port_signal);
  printf("\t-S remote UDP address to send input signal [Default %s]\n", args->net_address_signal);
  printf("\t-u remote TCP port to send data (-1 does nothing with it) [Default %d]\n", args->net_port);
  printf("\t-U remote TCP address to send data [Default %s]\n", args->net_address);
  printf("\t-M MBSFN area id [Default %d]\n", args->mbsfn_area_id);
  printf("\t-N Non-MBSFN region [Default %d]\n", args->non_mbsfn_region);
  printf("\t-q Enable/Disable 256QAM modulation (default %s)\n", args->enable_256qam ? "enabled" : "disabled");
  printf("\t-Q Use standard LTE sample rates (default %s)\n", args->use_standard_lte_rate ? "enabled" : "disabled");
  printf("\t-v [set srsran_verbose to debug, default none]\n");
}

void parse_args(prog_args_t* args, int argc, char** argv)
{
  int opt;
  args_default(args);

  while ((opt = getopt(argc, argv, "adAogliIpPcOCtdDFRqnvrfuUsSZyWMNBTGQ")) != -1) {
    switch (opt) {
      case 'i':
        args->input_file_name = argv[optind];
        break;
      case 'p':
        args->file_nof_prb = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'P':
        args->file_nof_ports = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'o':
        args->file_offset_freq = strtof(argv[optind], NULL);
        break;
      case 'O':
        args->file_offset_time = (int)strtol(argv[optind], NULL, 10);
        break;
      case 'c':
        args->file_cell_id = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'I':
        args->rf_dev = argv[optind];
        break;
      case 'a':
        args->rf_args = argv[optind];
        break;
      case 'A':
        args->rf_nof_rx_ant = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'g':
        args->rf_gain = strtof(argv[optind], NULL);
        break;
      case 'C':
        args->disable_cfo = true;
        break;
      case 'F':
        args->enable_cfo_ref = true;
        break;
      case 'R':
        args->estimator_alg = argv[optind];
        break;
      case 't':
        args->time_offset = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'f':
        args->rf_freq = strtod(argv[optind], NULL);
        break;
      case 'T':
        args->tdd_special_sf = (int)strtol(argv[optind], NULL, 10);
        break;
      case 'G':
        args->sf_config = (int)strtol(argv[optind], NULL, 10);
        break;
      case 'n':
        args->nof_subframes = (int)strtol(argv[optind], NULL, 10);
        break;
      case 'r':
        args->rnti = strtol(argv[optind], NULL, 16);
        break;
      case 'l':
        args->force_N_id_2 = (int)strtol(argv[optind], NULL, 10);
        break;
      case 'u':
        args->net_port = (int)strtol(argv[optind], NULL, 10);
        break;
      case 'U':
        args->net_address = argv[optind];
        break;
      case 's':
        args->net_port_signal = (int)strtol(argv[optind], NULL, 10);
        break;
      case 'S':
        args->net_address_signal = argv[optind];
        break;
      case 'd':
        args->disable_plots = true;
        break;
      case 'D':
        args->disable_plots_except_constellation = true;
        break;
      case 'v':
        increase_srsran_verbose_level();
        args->verbose = get_srsran_verbose_level();
        break;
      case 'Z':
        args->decimate = (int)strtol(argv[optind], NULL, 10);
        break;
      case 'y':
        args->cpu_affinity = (int)strtol(argv[optind], NULL, 10);
        break;
      case 'W':
        output_file_name = argv[optind];
        break;
      case 'M':
        args->mbsfn_area_id = (int32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'N':
        args->non_mbsfn_region = (uint8_t)strtol(argv[optind], NULL, 10);
        break;
      case 'B':
        args->mbsfn_sf_mask = (uint8_t)strtol(argv[optind], NULL, 10);
        break;
      case 'q':
        args->enable_256qam ^= true;
        break;
      case 'Q':
        args->use_standard_lte_rate ^= true;
        break;
      default:
        usage(args, argv[0]);
        exit(-1);
    }
  }
  if (args->rf_freq < 0 && args->input_file_name == NULL) {
    usage(args, argv[0]);
    exit(-1);
  }
}

/**********************************************************************/

uint8_t* data[SRSRAN_MAX_CODEWORDS];

bool go_exit = false;

void sig_int_handler(int signo)
{
  printf("SIGINT received. Exiting...\n");
  if (signo == SIGINT) {
    go_exit = true;
  } else if (signo == SIGSEGV) {
    exit(1);
  }
}

cf_t* sf_buffer[SRSRAN_MAX_PORTS] = {NULL};

int srsran_rf_recv_wrapper(void* h, cf_t* data_[SRSRAN_MAX_PORTS], uint32_t nsamples, srsran_timestamp_t* t)
{
  DEBUG(" ----  Receive %d samples  ----", nsamples);
  void* ptr[SRSRAN_MAX_PORTS];
  for (int i = 0; i < SRSRAN_MAX_PORTS; i++) {
    ptr[i] = data_[i];
  }
  return srsran_rf_recv_with_time_multi(h, ptr, nsamples, true, NULL, NULL);
}

static SRSRAN_AGC_CALLBACK(srsran_rf_set_rx_gain_th_wrapper_)
{
  srsran_rf_set_rx_gain_th((srsran_rf_t*)h, gain_db);
}

extern float mean_exec_time;

enum receiver_state { DECODE_MIB, DECODE_PDSCH } state;

srsran_cell_t      cell;
srsran_ue_dl_t     ue_dl;
srsran_ue_dl_cfg_t ue_dl_cfg;
srsran_dl_sf_cfg_t dl_sf;
srsran_pdsch_cfg_t pdsch_cfg;
srsran_ue_sync_t   ue_sync;
prog_args_t        prog_args;

uint32_t pkt_errors = 0, pkt_total = 0, nof_detected = 0, pmch_pkt_errors = 0, pmch_pkt_total = 0, nof_trials = 0;
/* Useful macros for printing lines which will disappear */

#define PRINT_LINE_INIT()                                                                                              \
  int        this_nof_lines = 0;                                                                                       \
  static int prev_nof_lines = 0
#define PRINT_LINE(_fmt, ...)                                                                                          \
  printf("\033[K" _fmt "\n", ##__VA_ARGS__);                                                                           \
  this_nof_lines++
#define PRINT_LINE_RESET_CURSOR()                                                                                      \
  printf("\033[%dA", this_nof_lines);                                                                                  \
  prev_nof_lines = this_nof_lines
#define PRINT_LINE_ADVANCE_CURSOR() printf("\033[%dB", prev_nof_lines + 1)


#define MAX_SCAN_CELLS 128

int bands_length = 32;
int bands[] = {1, 2, 3, 4, 5, 8, 10, 12, 13, 14, 17, 24, 25, 26, 26, 29, 30, 31, 35, 36, 37, 41, 46, 47, 48, 49, 65, 66, 70, 71, 74, 85};

/********/
/* Main */
/********/
int main(int argc, char** argv)
{
    int ret, i, band, j;
    srsran_rf_t rf; /* RF structure */
    struct cells scanned_cells[MAX_SCAN_CELLS]; /* List of available cells */
    float search_cell_cfo = 0;

    srsran_debug_handle_crash(argc, argv);

    parse_args(&prog_args, argc, argv);

    srsran_use_standard_symbol_size(prog_args.use_standard_lte_rate);

    uint8_t mch_table[10];
    bzero(&mch_table[0], sizeof(uint8_t) * 10);
    if (prog_args.mbsfn_area_id > -1) {
        generate_mcch_table(mch_table, prog_args.mbsfn_sf_mask);
    }


    /* Initialize radio */
    printf("Opening RF device with %d RX antennas...\n", prog_args.rf_nof_rx_ant);
    if (srsran_rf_open_devname(&rf, prog_args.rf_dev, prog_args.rf_args, prog_args.rf_nof_rx_ant)) {
        fprintf(stderr, "Error opening rf\n");
        exit(-1);
    }
    /* Set receiver gain */
    if (prog_args.rf_gain > 0) {
        srsran_rf_set_rx_gain(&rf, prog_args.rf_gain);
    } else {
        printf("Starting AGC thread...\n");
        if (srsran_rf_start_gain_thread(&rf, false)) {
            ERROR("Error opening rf");
            exit(-1);
        }
        srsran_rf_set_rx_gain(&rf, srsran_rf_get_rx_gain(&rf));
        cell_detect_config.init_agc = srsran_rf_get_rx_gain(&rf);
    }


    /* Set signal handler */
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigprocmask(SIG_UNBLOCK, &sigset, NULL);
    signal(SIGINT, sig_int_handler);

    /* set receiver frequency */
    printf("Tuning receiver to %.3f MHz\n", (prog_args.rf_freq + prog_args.file_offset_freq) / 1000000);
    srsran_rf_set_rx_freq(&rf, prog_args.rf_nof_rx_ant, prog_args.rf_freq + prog_args.file_offset_freq);

    for(j=0; j < bands_length; j++) {
      band = bands[j];
      /* Scan for cells in the selected band */
      ret = cell_scan(&rf, &cell_detect_config, scanned_cells, MAX_SCAN_CELLS, 2);
      if(ret < 0) {
          printf("Error scanning for cells");
          exit(1);
      }
      else if(ret == 0) {
          printf("0 cell found. Try in a different location.\n");
          return 1;
      }

      printf("\n\nFound %d cells:\n", ret);
      for (i = 0; i < ret; i++) {
          printf("CELL %d:\n\tCell ID: %d\n\tEARFCN(DL): %d\n\tFreq: %.1f MHz\n\tPRBs: %d\n\tPSS Power: %.1f dBm\n",
              i+1,
              scanned_cells[i].cell.id,
              scanned_cells[i].dl_earfcn,
              scanned_cells[i].freq,
              scanned_cells[i].cell.nof_prb,
              srsran_convert_power_to_dB(scanned_cells[i].power));
              srsran_cell_fprint(stdout, &(scanned_cells[i].cell), 0);
      }
    }

    if (!prog_args.input_file_name) {
      srsran_ue_mib_free(&ue_mib);
      srsran_rf_close(&rf);
    }

    printf("\nBye\n");
    exit(0);
  }