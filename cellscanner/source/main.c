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
#include "cellscanner/headers/cell_scan.h"

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

srsran_cell_t      cell;
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

// ALL BANDS
int all_bands_length = 57;
int all_bands[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 65, 66, 67, 68, 69, 70, 71};

// NORTH AMERICA BANDS
int na_bands_length = 35;
int na_bands[] = {1, 2, 3, 4, 5, 8, 10, 12, 13, 14, 17, 23, 24, 25, 26, 27, 29, 30, 35, 36, 37, 41, 42, 43, 46, 47, 48, 49, 50, 51, 52, 65, 66, 70, 71};

int eu_bands_length = 8;
int eu_bands[] = {7, 20, 22, 28, 68, 72, 87, 88};




/********/
/* Main */
/********/
int main(int argc, char** argv)
{
    int ret, i, band, j, region, bands_length;
    int * bands;
    srsran_rf_t rf; /* RF structure */
    struct cells scanned_cells[MAX_SCAN_CELLS]; /* List of available cells */
    FILE * file;


    if(argc < 2 || argc > 4) {
      printf("USAGE: %s <Output file> <Region (0: All, 1: North America, 2: Europe)> <USRP Args (Optional)>\n", argv[0]);
      exit(1);
    }

    if((file = fopen(argv[1], "w")) == NULL) {
      printf("Error openning %s\n", argv[1]);
      exit(1);
    }

    region = atoi(argv[2]);
    if(region == 0) {
      bands_length = all_bands_length;
      bands = all_bands;
    }
    else if(region == 1) {
      bands_length = na_bands_length;
      bands = na_bands;
    }
    else if(region == 2) {
      bands_length = eu_bands_length;
      bands = eu_bands;
    }
    else {
      printf("Invalid region value. 0: All, 1: North America, 2: Europe\n");
      exit(1);
    }
    

    srsran_debug_handle_crash(argc, argv);

    srsran_use_standard_symbol_size(true);

    /* Initialize radio */
    if(argc == 2) {
      printf("Opening RF device with 1 RX antennas...\n");
      if (srsran_rf_open_devname(&rf, "", "", 1)) {
          fprintf(stderr, "Error opening rf\n");
          exit(-1);
      }
    }
    else {
      printf("Opening RF device with 1 RX antennas and \"%s\" as arguments...\n", argv[3]);
      if (srsran_rf_open_devname(&rf, "", argv[3], 1)) {
          fprintf(stderr, "Error opening rf\n");
          exit(-1);
      }
    }

    
    printf("Starting AGC thread...\n");
    if (srsran_rf_start_gain_thread(&rf, false)) {
        ERROR("Error opening rf");
        exit(-1);
    }
    srsran_rf_set_rx_gain(&rf, srsran_rf_get_rx_gain(&rf));
    cell_detect_config.init_agc = srsran_rf_get_rx_gain(&rf);


    /* Set signal handler */
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigprocmask(SIG_UNBLOCK, &sigset, NULL);
    signal(SIGINT, sig_int_handler);

    fprintf(file, "band,cell_id,dl_earfcn,freq_mhz,prbs,pss_power_dbm\n");
    /* Scanning loop */
    for(j=0; j < bands_length; j++) {
      band = bands[j];

      printf("Searching in band %d\n", band);
      /* Scan for cells in the selected band */
      ret = cell_scan(&rf, &cell_detect_config, scanned_cells, MAX_SCAN_CELLS, band);
      if(ret < 0) {
          printf("Error scanning for cells");
          exit(1);
      }
      else if(ret == 0) {
          printf("0 cell found in band %d\n", band);
          continue;
      }

      printf("\n\nFound %d cells in band %d\n", ret, band);
      for (i = 0; i < ret; i++) {
          fprintf(file, "%d,%d,%d,%.1f,%d,%.1f\n", band, scanned_cells[i].cell.id, scanned_cells[i].dl_earfcn, scanned_cells[i].freq, scanned_cells[i].cell.nof_prb, srsran_convert_power_to_dB(scanned_cells[i].power));
          printf("CELL %d:\n\tCell ID: %d\n\tEARFCN(DL): %d\n\tFreq: %.1f MHz\n\tPRBs: %d\n\tPSS Power: %.1f dBm\n",
              i+1,
              scanned_cells[i].cell.id,
              scanned_cells[i].dl_earfcn,
              scanned_cells[i].freq,
              scanned_cells[i].cell.nof_prb,
              srsran_convert_power_to_dB(scanned_cells[i].power));
              //srsran_cell_fprint(stdout, &(scanned_cells[i].cell), 0);
      }
    }

    fclose(file);

    srsran_rf_close(&rf);

    printf("\nBye\n");
    exit(0);
  }