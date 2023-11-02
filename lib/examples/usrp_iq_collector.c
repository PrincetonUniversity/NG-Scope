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

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include <stdbool.h>

#include "srsran/phy/rf/rf.h"
#include "srsran/srsran.h"
#include "srsran/phy/rf/rf_utils.h"

#define DEFAULT_NOF_RX_ANTENNAS 1
#define MIB_SEARCH_MAX_ATTEMPTS 15
#define MAX_SRATE_DELTA 2 // allowable delta (in Hz) between requested and actual sample rate


/*
static bool           keep_running     = true;
char*                 output_file_name = NULL;
static char           rf_devname[64]   = "";
static char           rf_args[64]      = "auto";
float                 rf_gain = 60.0, rf_freq = -1.0;
int                   nof_prb                = 6;
int                   nof_subframes          = -1;
int                   N_id_2                 = -1;
uint32_t              nof_rx_antennas        = 1;
bool                  use_standard_lte_rates = false;
srsran_ue_sync_mode_t sync_mode              = SYNC_MODE_PSS;
*/

cell_search_cfg_t cell_detect_config = {.max_frames_pbch      = SRSRAN_DEFAULT_MAX_FRAMES_PBCH,
                                        .max_frames_pss       = SRSRAN_DEFAULT_MAX_FRAMES_PSS,
                                        .nof_valid_pss_frames = SRSRAN_DEFAULT_NOF_VALID_PSS_FRAMES,
                                        .init_agc             = 0,
                                        .force_tdd            = false};

static bool keep_running = true;

void int_handler(int dummy)
{
  keep_running = false;
}

typedef struct {
  char *   rf_args;
  double   rf_freq;
  char *   output;
} prog_args_t;

void usage(char* prog)
{
  printf("Usage: %s <afo>\n", prog);
  printf("\t-a RF args [Default ""]\n");
  printf("\t-f rx_frequency (in Hz)\n");
  printf("\t-o Output file\n");
}

void args_default(prog_args_t* args)
{
  args->rf_args        = "";
  args->rf_freq        = -1.0;
  args->output         = NULL;
}

void parse_args(prog_args_t* args, int argc, char** argv)
{
  int opt;
  while ((opt = getopt(argc, argv, "afoh")) != -1) {
    switch (opt) {
      case 'a':
            args->rf_args = argv[optind];
            break;
        case 'f':
            args->rf_freq = strtod(argv[optind], NULL);
            break;
        case 'o':
            args->output = argv[optind];
            break;
        case 'h':
            usage(argv[0]);
            exit(0);
            break;
      default:
        usage(argv[0]);
        exit(-1);
    }
  }
  if (args->rf_freq < 0 && args->output == NULL ) {
    usage(argv[0]);
    exit(-1);
  }
}

int srsran_rf_recv_wrapper(void* h, cf_t* data[SRSRAN_MAX_PORTS], uint32_t nsamples, srsran_timestamp_t* t)
{
  DEBUG(" ----  Receive %d samples  ----", nsamples);
  void* ptr[SRSRAN_MAX_PORTS];
  for (int i = 0; i < SRSRAN_MAX_PORTS; i++) {
    ptr[i] = data[i];
  }
  return srsran_rf_recv_with_time_multi(h, ptr, nsamples, true, &t->full_secs, &t->frac_secs);
}

static SRSRAN_AGC_CALLBACK(srsran_rf_set_rx_gain_th_wrapper_)
{
  srsran_rf_set_rx_gain_th((srsran_rf_t*)h, gain_db);
}


prog_args_t prog_args;


int main(int argc, char** argv)
{
  cf_t*             buffer[SRSRAN_MAX_CHANNELS] = {NULL};
  int               n                           = 0;
  srsran_rf_t       rf                          = {};
  srsran_ue_sync_t  ue_sync                     = {};

  int ret;
  float search_cell_cfo = 0;
  int ntrial = 0;
  srsran_cell_t cell;
  srsran_filesink_t sink;

  signal(SIGINT, int_handler);

  parse_args(&prog_args, argc, argv);

  srsran_use_standard_symbol_size(false);

  srsran_filesink_init(&sink, prog_args.output, SRSRAN_COMPLEX_FLOAT_BIN);

  printf("Opening RF device...\n");
  if (srsran_rf_open_multi(&rf, prog_args.rf_args, DEFAULT_NOF_RX_ANTENNAS)) {
    ERROR("Error opening rf");
    exit(-1);
  }


  /* Start Automatic Gain Control thread */
  printf("Starting AGC thread...\n");
  if (srsran_rf_start_gain_thread(&rf, false)) {
    ERROR("Error opening rf");
    exit(-1);
  }
  srsran_rf_set_rx_gain(&rf, srsran_rf_get_rx_gain(&rf));
  cell_detect_config.init_agc = srsran_rf_get_rx_gain(&rf);

  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGINT);
  sigprocmask(SIG_UNBLOCK, &sigset, NULL);

  /* set receiver frequency */
  printf("Tuning receiver to %.3f MHz\n", (prog_args.rf_freq) / 1000000);
  srsran_rf_set_rx_freq(&rf, DEFAULT_NOF_RX_ANTENNAS, prog_args.rf_freq);

  /* Decode MIB to get Cell Info */
  do {
    ret = rf_search_and_decode_mib(
        &rf, DEFAULT_NOF_RX_ANTENNAS, &cell_detect_config, -1, &cell, &search_cell_cfo);
    if (ret < 0 || ntrial == MIB_SEARCH_MAX_ATTEMPTS) {
        ERROR("Error searching for cell");
        exit(-1);
    } else if (ret == 0) {
        printf("Cell not found after [%d/%d] attempts. Trying again... (Ctrl+C to exit)\n", ntrial++, MIB_SEARCH_MAX_ATTEMPTS);
    }
  } while (ret == 0);

  srsran_cell_fprint(stdout, &cell, 0);
  printf("CELL Struct: cell.id %d, cell.nof_prb %d, cell.nof_ports %d, cell.cp %d, cell.phich_length %d, cell.phich_resources %d, cell.frame_type %d\n",
                            cell.id,
                            cell.nof_prb,
                            cell.nof_ports,
                            cell.cp,
                            cell.phich_length,
                            cell.phich_resources,
                            cell.frame_type);
  

  /* Set sampling rate base on the number of PRBs of the selected cell */
  int srate = srsran_sampling_freq_hz(cell.nof_prb);
  if (srate != -1) {
      printf("Setting rx sampling rate %.2f MHz\n", (float)srate / 1000000);
      float srate_rf = srsran_rf_set_rx_srate(&rf, (double)srate);
      if (abs(srate - (int)srate_rf) > MAX_SRATE_DELTA) {
          ERROR("Could not set rx sampling rate : wanted %d got %f", srate, srate_rf);
          exit(-1);
      }
  } else {
      ERROR("Invalid number of PRB %d", cell.nof_prb);
      exit(-1);
  }

  if (srsran_ue_sync_init_multi_decim(&ue_sync,
                                      cell.nof_prb,
                                      false,
                                      srsran_rf_recv_wrapper,
                                      DEFAULT_NOF_RX_ANTENNAS,
                                      (void*)&rf,
                                      0)) {
      ERROR("Error initiating ue_sync");
      exit(-1);
  }
  if (srsran_ue_sync_set_cell(&ue_sync, cell)) {
      ERROR("Error initiating ue_sync");
      exit(-1);
  }

  /* Allocate memory for the buffers */
  uint32_t max_num_samples = 3 * SRSRAN_SF_LEN_PRB(cell.nof_prb); /// Length in complex samples
  for (int i = 0; i < DEFAULT_NOF_RX_ANTENNAS; i++) {
      buffer[i] = srsran_vec_cf_malloc(max_num_samples);
  }

  /* Start RX streaming */
  srsran_rf_start_rx_stream(&rf, false);

  /* Sync with AGC */
  srsran_rf_info_t* rf_info = srsran_rf_get_info(&rf);
  srsran_ue_sync_start_agc(&ue_sync,
    srsran_rf_set_rx_gain_th_wrapper_,
    rf_info->min_rx_gain,
    rf_info->max_rx_gain,
    cell_detect_config.init_agc);





  uint32_t           subframe_count = 0;
  bool               start_capture  = false;
  bool               stop_capture   = false;
  srsran_timestamp_t ts_rx_start    = {};




  while (!stop_capture) {
    n = srsran_ue_sync_zerocopy(&ue_sync, buffer, max_num_samples);
    if (n < 0) {
      ERROR("Error receiving samples");
      exit(-1);
    }
    if (n == 1) {
      if (!start_capture) {
        if (srsran_ue_sync_get_sfidx(&ue_sync) == 9) {
          start_capture = true;
        }
      } else {
        printf("Writing to file %6d subframes...\r", subframe_count);
        srsran_filesink_write_multi(&sink, (void**)buffer, SRSRAN_SF_LEN_PRB(cell.nof_prb), DEFAULT_NOF_RX_ANTENNAS);

        // store time stamp of first subframe
        if (subframe_count == 0) {
          srsran_ue_sync_get_last_timestamp(&ue_sync, &ts_rx_start);
        }
        subframe_count++;
      }
    }
    if (!keep_running) {
      if (!start_capture || (start_capture && srsran_ue_sync_get_sfidx(&ue_sync) == 9)) {
        stop_capture = true;
      }
    }
  }

  srsran_filesink_free(&sink);
  srsran_rf_close(&rf);
  srsran_ue_sync_free(&ue_sync);

  for (int i = 0; i < DEFAULT_NOF_RX_ANTENNAS; i++) {
    if (buffer[i]) {
      free(buffer[i]);
    }
  }

  printf("\nOk - wrote %d subframes\n", subframe_count);

  srsran_ue_sync_set_tti_from_timestamp(&ue_sync, &ts_rx_start);
  printf("Start of capture at %ld+%.3f. TTI=%d.%d\n",
         ts_rx_start.full_secs,
         ts_rx_start.frac_secs,
         srsran_ue_sync_get_sfn(&ue_sync),
         srsran_ue_sync_get_sfidx(&ue_sync));

  return SRSRAN_SUCCESS;
}
