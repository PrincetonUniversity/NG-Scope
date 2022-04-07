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

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <unistd.h>

#include "srsran/phy/rf/rf_utils.h"
#include "srsran/phy/ue/ue_cell_search_nbiot.h"

#ifndef DISABLE_RF
#include "srsran/phy/rf/rf.h"
#endif

#define MHZ 1000000
#define SAMP_FREQ 1920000
#define FLEN 9600
#define FLEN_PERIOD 0.005

#define MAX_EARFCN 1000
#define RASTER_OFFSET 2500

#define NUM_RASTER_OFFSET 5
double raster_offset[NUM_RASTER_OFFSET] = {0.0, 2500.0, -2500.0, 7500.0, -7500.0};

int band         = -1;
int earfcn_start = -1, earfcn_end = -1;

cell_search_cfg_t cell_detect_config = {.max_frames_pbch      = SRSRAN_DEFAULT_MAX_FRAMES_NPBCH,
                                        .max_frames_pss       = SRSRAN_DEFAULT_MAX_FRAMES_NPSS,
                                        .nof_valid_pss_frames = SRSRAN_DEFAULT_NOF_VALID_NPSS_FRAMES,
                                        .init_agc             = 0.0,
                                        .force_tdd            = false};

struct cells {
  srsran_nbiot_cell_t cell;
  float               freq;
  int                 dl_earfcn;
  float               power;
};
struct cells results[1024];

float rf_gain            = 70.0;
char* rf_args            = "";
bool  scan_raster_offset = false;

void usage(char* prog)
{
  printf("Usage: %s [agsendtvb] -b band\n", prog);
  printf("\t-a RF args [Default %s]\n", rf_args);
  printf("\t-g RF gain [Default %.2f dB]\n", rf_gain);
  printf("\t-s earfcn_start [Default All]\n");
  printf("\t-e earfcn_end [Default All]\n");
  printf("\t-r Also scan frequencies with raster offset [Default %s]\n", scan_raster_offset ? "Yes" : "No");
  printf("\t-n nof_frames_total [Default 100]\n");
  printf("\t-v [set srsran_verbose to debug, default none]\n");
}

void parse_args(int argc, char** argv)
{
  int opt;
  while ((opt = getopt(argc, argv, "agserndvb")) != -1) {
    switch (opt) {
      case 'a':
        rf_args = argv[optind];
        break;
      case 'b':
        band = (int)strtol(argv[optind], NULL, 10);
        break;
      case 's':
        earfcn_start = (int)strtol(argv[optind], NULL, 10);
        break;
      case 'e':
        earfcn_end = (int)strtol(argv[optind], NULL, 10);
        break;
      case 'n':
        cell_detect_config.max_frames_pss = (int)strtol(argv[optind], NULL, 10);
        break;
      case 'g':
        rf_gain = strtof(argv[optind], NULL);
        break;
      case 'r':
        scan_raster_offset = true;
        break;
      case 'v':
        increase_srsran_verbose_level();
        break;
      default:
        usage(argv[0]);
        exit(-1);
    }
  }
  if (band == -1) {
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
  return srsran_rf_recv_with_time_multi(h, ptr, nsamples, true, NULL, NULL);
}

bool go_exit = false;

void sig_int_handler(int signo)
{
  printf("SIGINT received. Exiting...\n");
  if (signo == SIGINT) {
    go_exit = true;
  }
}

static SRSRAN_AGC_CALLBACK(srsran_rf_set_rx_gain_wrapper)
{
  srsran_rf_set_rx_gain((srsran_rf_t*)h, gain_db);
}

int main(int argc, char** argv)
{
  int                                 n;
  srsran_rf_t                         rf;
  srsran_ue_cellsearch_nbiot_t        cs;
  srsran_nbiot_ue_cellsearch_result_t found_cells[3];
  int                                 nof_freqs;
  srsran_earfcn_t                     channels[MAX_EARFCN];
  uint32_t                            freq;
  uint32_t                            n_found_cells = 0;

  parse_args(argc, argv);

  printf("Opening RF device...\n");
  if (srsran_rf_open(&rf, rf_args)) {
    fprintf(stderr, "Error opening rf\n");
    exit(-1);
  }
  if (!cell_detect_config.init_agc) {
    srsran_rf_set_rx_gain(&rf, rf_gain);
  } else {
    printf("Starting AGC thread...\n");
    if (srsran_rf_start_gain_thread(&rf, false)) {
      fprintf(stderr, "Error opening rf\n");
      exit(-1);
    }
    srsran_rf_set_rx_gain(&rf, 50);
  }

  // Supress RF messages
  srsran_rf_suppress_stdout(&rf);

  nof_freqs = srsran_band_get_fd_band(band, channels, earfcn_start, earfcn_end, MAX_EARFCN);
  if (nof_freqs < 0) {
    fprintf(stderr, "Error getting EARFCN list\n");
    exit(-1);
  }

  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGINT);
  sigprocmask(SIG_UNBLOCK, &sigset, NULL);
  signal(SIGINT, sig_int_handler);

  for (freq = 0; freq < nof_freqs && !go_exit; freq++) {
    for (int i = 0; i < (scan_raster_offset ? NUM_RASTER_OFFSET : 1); i++) {
      // set rf_freq
      double rf_freq = channels[freq].fd * MHZ + raster_offset[i];
      srsran_rf_set_rx_freq(&rf, 0, rf_freq);
      INFO("Set rf_freq to %.3f Hz", rf_freq);

      printf("[%3d/%d]: EARFCN %d, %.2f MHz looking for NPSS.\n", freq, nof_freqs, channels[freq].id, rf_freq / 1e6);
      fflush(stdout);

      if (SRSRAN_VERBOSE_ISINFO()) {
        printf("\n");
      }

      bzero(found_cells, 3 * sizeof(srsran_nbiot_ue_cellsearch_result_t));

      if (srsran_ue_cellsearch_nbiot_init(&cs, cell_detect_config.max_frames_pss, srsran_rf_recv_wrapper, (void*)&rf)) {
        fprintf(stderr, "Error initiating UE cell detect\n");
        exit(-1);
      }

      if (cell_detect_config.max_frames_pss) {
        srsran_ue_cellsearch_nbiot_set_nof_valid_frames(&cs, cell_detect_config.nof_valid_pss_frames);
      }
      if (cell_detect_config.init_agc) {
        srsran_ue_sync_nbiot_start_agc(&cs.ue_sync, srsran_rf_set_rx_gain_wrapper, cell_detect_config.init_agc);
      }

      INFO("Setting sampling frequency %.2f MHz for NPSS search", SRSRAN_CS_SAMP_FREQ / 1000000);
      srsran_rf_set_rx_srate(&rf, SRSRAN_CS_SAMP_FREQ);
      INFO("Starting receiver...");
      srsran_rf_start_rx_stream(&rf, false);

      n = srsran_ue_cellsearch_nbiot_scan(&cs);
      if (n == SRSRAN_SUCCESS) {
        srsran_rf_stop_rx_stream(&rf);
        n = srsran_ue_cellsearch_nbiot_detect(&cs, found_cells);
        if (n == SRSRAN_SUCCESS) {
          srsran_nbiot_cell_t cell;
          cell.n_id_ncell = found_cells[0].n_id_ncell;
          cell.base.cp    = SRSRAN_CP_NORM;

          // TODO: add MIB decoding
          printf("Found CELL ID %d.\n", cell.n_id_ncell);
          memcpy(&results[n_found_cells].cell, &cell, sizeof(srsran_nbiot_cell_t));
          results[n_found_cells].freq      = channels[freq].fd;
          results[n_found_cells].dl_earfcn = channels[freq].id;
          results[n_found_cells].power     = found_cells[0].peak;
          n_found_cells++;
        } else {
          printf("Cell found but couldn't detect ID.\n");
        }
      }
      srsran_ue_cellsearch_nbiot_free(&cs);
    }
  }

  printf("\n\nFound %d cells\n", n_found_cells);
  for (int i = 0; i < n_found_cells; i++) {
    printf("Found CELL %.1f MHz, EARFCN=%d, PHYID=%d, NPSS power=%.1f dBm\n",
           results[i].freq,
           results[i].dl_earfcn,
           results[i].cell.n_id_ncell,
           10 * log10(results[i].power));
  }

  printf("\nBye\n");

  srsran_rf_close(&rf);
  exit(0);
}
