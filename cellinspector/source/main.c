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
#include "cellinspector/headers/cpu.h"
#include "cellinspector/headers/cell_scan.h"

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
  args->rf_args                            = "";
  args->rf_freq                            = -1.0;
  args->rf_nof_rx_ant                      = 1;
}

void usage(prog_args_t* args, char* prog)
{
  printf("Usage: %s [afA]\n", prog);
  printf("\t-a RF args [Default %s]\n", args->rf_args);
  printf("\t-f rx_frequency (in Hz)\n");
  printf("\t-A Number of RX antennas [Default %d]\n", args->rf_nof_rx_ant);
}

void parse_args(prog_args_t* args, int argc, char** argv)
{
  int opt;
  args_default(args);

  while ((opt = getopt(argc, argv, "af")) != -1) {
    switch (opt) {
      case 'a':
        args->rf_args = argv[optind];
        break;
      case 'f':
        args->rf_freq = strtod(argv[optind], NULL);
        break;
      default:
        usage(args, argv[0]);
        exit(-1);
    }
  }
  if (args->rf_freq < 0) {
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
/* Useful macros for printing lines which will disappear */


#define MAX_SCAN_CELLS 128

/********/
/* Main */
/********/
int main(int argc, char** argv)
{
    int ret;
    srsran_rf_t rf; /* RF structure */
    float search_cell_cfo = 0;

    srsran_debug_handle_crash(argc, argv);

    parse_args(&prog_args, argc, argv);

    srsran_use_standard_symbol_size(prog_args.use_standard_lte_rate);

    for (int i = 0; i < SRSRAN_MAX_CODEWORDS; i++) {
        data[i] = srsran_vec_u8_malloc(2000 * 8);
        if (!data[i]) {
            ERROR("Allocating data");
            go_exit = true;
        }
    }

    uint8_t mch_table[10];
    bzero(&mch_table[0], sizeof(uint8_t) * 10);
    if (prog_args.mbsfn_area_id > -1) {
        generate_mcch_table(mch_table, prog_args.mbsfn_sf_mask);
    }

    /* Pin current thread to list of cpus */
    //if(pin_thread(4))
    //    printf("Error pinning to CPU\n");


    /* Initialize radio */
    printf("Opening RF device with %d RX antennas...\n", prog_args.rf_nof_rx_ant);
    if (srsran_rf_open_devname(&rf, "", prog_args.rf_args, prog_args.rf_nof_rx_ant)) {
        fprintf(stderr, "Error opening rf\n");
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


    /* Set signal handler */
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigprocmask(SIG_UNBLOCK, &sigset, NULL);
    signal(SIGINT, sig_int_handler);

    /* set receiver frequency */
    printf("Tuning receiver to %.3f MHz\n", (prog_args.rf_freq) / 1000000);
    srsran_rf_set_rx_freq(&rf, prog_args.rf_nof_rx_ant, prog_args.rf_freq);
    

    uint32_t ntrial = 0;
    do {
      ret = rf_search_and_decode_mib(
          &rf, prog_args.rf_nof_rx_ant, &cell_detect_config, -1, &cell, &search_cell_cfo);
      if (ret < 0) {
        ERROR("Error searching for cell");
        exit(-1);
      } else if (ret == 0 && !go_exit) {
        printf("Cell not found after [%4d] attempts. Trying again... (Ctrl+C to exit)\n", ntrial++);
      }
    } while (ret == 0 && !go_exit);

    srsran_cell_fprint(stdout, &cell, 0);

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


    /* Initialize sync struct for syncing with the cell */
    if (srsran_ue_sync_init_multi_decim(&ue_sync,
                                        cell.nof_prb,
                                        false,
                                        srsran_rf_recv_wrapper,
                                        prog_args.rf_nof_rx_ant,
                                        (void*)&rf,
                                        0)) {
        ERROR("Error initiating ue_sync");
        exit(-1);
    }
    if (srsran_ue_sync_set_cell(&ue_sync, cell)) {
        ERROR("Error initiating ue_sync");
        exit(-1);
    }

    uint32_t max_num_samples = 3 * SRSRAN_SF_LEN_PRB(cell.nof_prb); /// Length in complex samples
    for (int i = 0; i < prog_args.rf_nof_rx_ant; i++) {
        sf_buffer[i] = srsran_vec_cf_malloc(max_num_samples);
    }

    /* Initialize MIB structure and set the cell */
    srsran_ue_mib_t ue_mib;
    if (srsran_ue_mib_init(&ue_mib, sf_buffer[0], cell.nof_prb)) {
        ERROR("Error initaiting UE MIB decoder");
        exit(-1);
    }
    if (srsran_ue_mib_set_cell(&ue_mib, cell)) {
        ERROR("Error initaiting UE MIB decoder");
        exit(-1);
    }


    /* Initialize UE downlink processing module */
    if (srsran_ue_dl_init(&ue_dl, sf_buffer, cell.nof_prb, prog_args.rf_nof_rx_ant)) {
        ERROR("Error initiating UE downlink processing module");
        exit(-1);
    }
    if (srsran_ue_dl_set_cell(&ue_dl, cell)) {
        ERROR("Error initiating UE downlink processing module");
        exit(-1);
    }

    // Disable CP based CFO estimation during find
    ue_sync.cfo_current_value       = search_cell_cfo / 15000;
    ue_sync.cfo_is_copied           = true;
    ue_sync.cfo_correct_enable_find = true;
    srsran_sync_set_cfo_cp_enable(&ue_sync.sfind, false, 0);

    ZERO_OBJECT(ue_dl_cfg);
    ZERO_OBJECT(dl_sf);
    ZERO_OBJECT(pdsch_cfg);

    pdsch_cfg.meas_evm_en = true;

    srsran_chest_dl_cfg_t chest_pdsch_cfg = {};
    chest_pdsch_cfg.cfo_estimate_enable   = false;
    chest_pdsch_cfg.cfo_estimate_sf_mask  = 1023;
    chest_pdsch_cfg.estimator_alg         = srsran_chest_dl_str2estimator_alg("interpolate");
    chest_pdsch_cfg.sync_error_enable     = true;

    // Special configuration for MBSFN channel estimation
    srsran_chest_dl_cfg_t chest_mbsfn_cfg = {};
    chest_mbsfn_cfg.filter_type           = SRSRAN_CHEST_FILTER_TRIANGLE;
    chest_mbsfn_cfg.filter_coef[0]        = 0.1;
    chest_mbsfn_cfg.estimator_alg         = SRSRAN_ESTIMATOR_ALG_INTERPOLATE;
    chest_mbsfn_cfg.noise_alg             = SRSRAN_NOISE_ALG_PSS;

    // Allocate softbuffer buffers
    srsran_softbuffer_rx_t rx_softbuffers[SRSRAN_MAX_CODEWORDS];
    for (uint32_t i = 0; i < SRSRAN_MAX_CODEWORDS; i++) {
        pdsch_cfg.softbuffers.rx[i] = &rx_softbuffers[i];
        srsran_softbuffer_rx_init(pdsch_cfg.softbuffers.rx[i], cell.nof_prb);
    }

    pdsch_cfg.rnti = SRSRAN_SIRNTI;






    /**********************/
    /* Start RX streaming */
    /**********************/
    srsran_rf_start_rx_stream(&rf, false);

    /* Sync with AGC */
    srsran_rf_info_t* rf_info = srsran_rf_get_info(&rf);
    srsran_ue_sync_start_agc(&ue_sync,
                            srsran_rf_set_rx_gain_th_wrapper_,
                            rf_info->min_rx_gain,
                            rf_info->max_rx_gain,
                            cell_detect_config.init_agc);

  #ifdef PRINT_CHANGE_SCHEDULING
    srsran_ra_dl_grant_t old_dl_dci;
    bzero(&old_dl_dci, sizeof(srsran_ra_dl_grant_t));
  #endif

    ue_sync.cfo_correct_enable_track = true;

    srsran_pbch_decode_reset(&ue_mib.pbch);

    printf("Entering main loop...\n");

    // Variables for measurements
    bool decode_pdsch = false;

    /* Main loop */
    uint64_t sf_cnt          = 0;
    uint32_t sfn             = 0;
    int n;

    while (!go_exit) {

        cf_t* buffers[SRSRAN_MAX_CHANNELS] = {};
        for (int p = 0; p < SRSRAN_MAX_PORTS; p++) {
            buffers[p] = sf_buffer[p];
        }
        ret = srsran_ue_sync_zerocopy(&ue_sync, buffers, max_num_samples);
        if (ret < 0) {
            ERROR("Error calling srsran_ue_sync_work()");
        }

        /* srsran_ue_sync_zerocopy returns 1 if successfully read 1 aligned subframe */
        if (ret == 0) {
            printf("Finding PSS... Peak: %8.1f, FrameCnt: %d, State: %d\r",
                srsran_sync_get_peak_value(&ue_sync.sfind),
                ue_sync.frame_total_cnt,
                ue_sync.state);
        }
        else if (ret == 1) {
            bool acks[SRSRAN_MAX_CODEWORDS] = {false};
            uint32_t sf_idx;

            sf_idx = srsran_ue_sync_get_sfidx(&ue_sync);


            /* DECODE MIB */
            if (sf_idx == 0) {
                uint8_t bch_payload[SRSRAN_BCH_PAYLOAD_LEN];
                int     sfn_offset;
                n = srsran_ue_mib_decode(&ue_mib, bch_payload, NULL, &sfn_offset);
                if (n < 0) {
                    ERROR("Error decoding UE MIB");
                    exit(-1);
                } else if (n == SRSRAN_UE_MIB_FOUND) {
                    srsran_pbch_mib_unpack(bch_payload, &cell, &sfn);
                    srsran_cell_fprint(stdout, &cell, sfn);
                    printf("Decoded MIB. SFN: %d, offset: %d\n", sfn, sfn_offset);
                    sfn   = (sfn + sfn_offset) % 1024;
                    state = DECODE_PDSCH;
                }
            }


            /* DECODE PDSCH (SIBS) */
            if (prog_args.rnti != SRSRAN_SIRNTI) {
                decode_pdsch = true;
                if (srsran_sfidx_tdd_type(dl_sf.tdd_config, sf_idx) == SRSRAN_TDD_SF_U) {
                    decode_pdsch = false;
                }
            } else {
                /* We are looking for SIB1 Blocks, search only in appropiate places */
                if ((sf_idx == 5 && (sfn % 2) == 0) || mch_table[sf_idx] == 1) {
                    decode_pdsch = true;
                } else {
                    decode_pdsch = false;
                }
            }

            uint32_t tti = sfn * 10 + sf_idx;

            if (decode_pdsch) {
                srsran_sf_t sf_type;
                /* Check if this is a MBSFN subframe (Multicast Broadcase Single Frequency Network) */
                if (mch_table[sf_idx] == 0 || prog_args.mbsfn_area_id < 0) { // Not an MBSFN subframe
                    sf_type = SRSRAN_SF_NORM;
                    // Set PDSCH channel estimation
                    ue_dl_cfg.chest_cfg = chest_pdsch_cfg;
                } else {
                    sf_type = SRSRAN_SF_MBSFN;
                    // Set MBSFN channel estimation
                    ue_dl_cfg.chest_cfg = chest_mbsfn_cfg;
                }

                n = 0;
                for (uint32_t tm = 0; tm < 4 && !n; tm++) {
                    dl_sf.tti                             = tti;
                    dl_sf.sf_type                         = sf_type;
                    ue_dl_cfg.cfg.tm                      = (srsran_tm_t)tm; /* Transmission mode */
                    ue_dl_cfg.cfg.pdsch.use_tbs_index_alt = false; /*enable_256qam*/

                    if ((ue_dl_cfg.cfg.tm == SRSRAN_TM1 && cell.nof_ports == 1) ||
                        (ue_dl_cfg.cfg.tm > SRSRAN_TM1 && cell.nof_ports > 1)) {
                        n = srsran_ue_dl_find_and_decode(&ue_dl, &dl_sf, &ue_dl_cfg, &pdsch_cfg, data, acks);
                        if (n > 0) {
                            for (uint32_t tb = 0; tb < SRSRAN_MAX_CODEWORDS; tb++) {
                                if (pdsch_cfg.grant.tb[tb].enabled) {
                                    if (acks[tb]) { 
                                        int len = pdsch_cfg.grant.tb[tb].tbs;
                                        uint8_t* payload 	= data[tb];
#ifdef ENABLE_ASN4G
                                        sib_decode_4g(stdout, payload, len);
#endif
                                    }
                                }
                            }
                        }
                    }
                }
                // Feed-back ue_sync with chest_dl CFO estimation
                if (sf_idx == 5 && false) {
                    srsran_ue_sync_set_cfo_ref(&ue_sync, ue_dl.chest_res.cfo);
                }
            }



            if (sf_idx == 9) {
                sfn++;
                if (sfn == 1024) {
                    sfn = 0;
                }
            }
        }

        sf_cnt++;
    } // Main loop

    srsran_ue_dl_free(&ue_dl);
    srsran_ue_sync_free(&ue_sync);
    for (int i = 0; i < SRSRAN_MAX_CODEWORDS; i++) {
      if (data[i]) {
        free(data[i]);
      }
    }
    for (int i = 0; i < prog_args.rf_nof_rx_ant; i++) {
      if (sf_buffer[i]) {
        free(sf_buffer[i]);
      }
    }
    if (!prog_args.input_file_name) {
      srsran_ue_mib_free(&ue_mib);
      srsran_rf_close(&rf);
    }

    printf("\nBye\n");
    exit(0);
  }