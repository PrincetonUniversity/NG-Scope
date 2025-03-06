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
#include <libconfig.h>

/* Local libs */
#include "cellinspector/headers/cpu.h"
#include "cellinspector/headers/asn_decoder.h"

/* SRS libs */
#include "srsran/common/crash_handler.h"
#include "srsran/common/gen_mch_tables.h"
#include "srsran/phy/io/filesink.h"
#include "srsran/srsran.h"
#include "srsran/phy/rf/rf.h"
#include "srsran/phy/rf/rf_utils.h"

#ifdef ENABLE_ASN4G
#include <libasn4g.h>
#endif


#define DEFAULT_NOF_RX_ANTENNAS 1
#define MIB_SEARCH_MAX_ATTEMPTS 15
#define MAX_SRATE_DELTA 2 // allowable delta (in Hz) between requested and actual sample rate

cell_search_cfg_t cell_detect_config = {.max_frames_pbch      = SRSRAN_DEFAULT_MAX_FRAMES_PBCH,
                                        .max_frames_pss       = SRSRAN_DEFAULT_MAX_FRAMES_PSS,
                                        .nof_valid_pss_frames = SRSRAN_DEFAULT_NOF_VALID_PSS_FRAMES,
                                        .init_agc             = 0,
                                        .force_tdd            = false};

//#define PRINT_CHANGE_SCHEDULING

//#define CORRECT_SAMPLE_OFFSET

/**********************************************************************
 *  Program arguments processing
 ***********************************************************************/
typedef struct {
  char *   rf_args;
  char *   rf_freq;
  char *   output;
  char *   input;
  char *   cell;
} prog_args_t;

void args_default(prog_args_t* args)
{
  args->rf_args        = "";
  args->rf_freq        = NULL;
  args->output         = ".";
  args->input          = NULL;
  args->cell           = NULL;
}

void usage(prog_args_t* args, char* prog)
{
  printf("Usage: %s [oiafn]\n", prog);
  printf("\t-o Output folder\n");
  printf("\t-i Input file (cannot use with -f)\n");
  printf("\t-c Cell info file (use with -i)\n");
  printf("\t-a RF args [Default %s]\n", args->rf_args);
  printf("\t-f rx_frequency (in Hz)\n");
}

void parse_args(prog_args_t* args, int argc, char** argv)
{
  int opt;
  args_default(args);

  while ((opt = getopt(argc, argv, "hafoci")) != -1) {
    switch (opt) {
        case 'a':
            args->rf_args = argv[optind];
            break;
        case 'f':
            args->rf_freq = argv[optind];
            break;
        case 'o':
            args->output = argv[optind];
            break;
        case 'i':
            args->input = argv[optind];
            break;
        case 'c':
            args->cell = argv[optind];
            break;
        case 'h':
            usage(args, argv[0]);
            exit(0);
        default:
            usage(args, argv[0]);
            exit(-1);
    }
  }
  if ((args->rf_freq == NULL && args->input == NULL) ||
        (args->rf_freq != NULL && args->input != NULL) ||
        (args->input != NULL && args->cell == NULL)) {
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

srsran_cell_t      cell;
srsran_ue_dl_t     ue_dl;
srsran_ue_dl_cfg_t ue_dl_cfg;
srsran_dl_sf_cfg_t dl_sf;
srsran_pdsch_cfg_t pdsch_cfg;
srsran_ue_sync_t   ue_sync;
prog_args_t        prog_args;


/* This function will fill the cell structure with the data specified in the cell config file */
int read_cell_from_file(char * file, srsran_cell_t * cell)
{
    config_t cfg;

    config_init(&cfg);
    /* Read the file. If there is an error, report it and exit. */
    if(! config_read_file(&cfg, file))
    {
        fprintf(stdout, "%s:%d - %s\n", config_error_file(&cfg),
                config_error_line(&cfg), config_error_text(&cfg));
        config_destroy(&cfg);
        return 1;
    }
    /* Read cell values */
    config_lookup_int(&cfg, "cell.id", (int *) &(cell->id));
    config_lookup_int(&cfg, "cell.nof_prb", (int *) &(cell->nof_prb));
    config_lookup_int(&cfg, "cell.nof_ports", (int *) &(cell->nof_ports));
    config_lookup_int(&cfg, "cell.cp", (int *) &(cell->cp));
    config_lookup_int(&cfg, "cell.phich_length", (int *) &(cell->phich_length));
    config_lookup_int(&cfg, "cell.phich_resources", (int *) &(cell->phich_resources));
    config_lookup_int(&cfg, "cell.frame_type", (int *) &(cell->frame_type));
    /* Free config struct */
    config_destroy(&cfg);

    printf("CELL Struct: cell.id=%d, cell.nof_prb=%d, cell.nof_ports=%d, cell.cp=%d, cell.phich_length=%d, cell.phich_resources=%d, cell.frame_type=%d\n",
                            cell->id,
                            cell->nof_prb,
                            cell->nof_ports,
                            cell->cp,
                            cell->phich_length,
                            cell->phich_resources,
                            cell->frame_type);
    
    return 0;
}


#define MAX_SCAN_CELLS 128

/********/
/* Main */
/********/
int main(int argc, char** argv)
{
    int ret;
    srsran_rf_t rf; /* RF structure */
    float search_cell_cfo = 0;
    ASNDecoder * decoder;

    srsran_debug_handle_crash(argc, argv);

    /* Parse command line arguments */
    parse_args(&prog_args, argc, argv);

    /* Start ASN decoder thread */
    if(prog_args.rf_freq) {
        decoder = init_asn_decoder(prog_args.output, basename(prog_args.rf_freq));
    }
    else {
        decoder = init_asn_decoder(prog_args.output, basename(prog_args.input));
    }

    srsran_use_standard_symbol_size(false);

    for (int i = 0; i < SRSRAN_MAX_CODEWORDS; i++) {
        data[i] = srsran_vec_u8_malloc(2000 * 8);
        if (!data[i]) {
            ERROR("Allocating data");
            go_exit = true;
        }
    }

    /* Pin current thread to list of cpus */
    //if(pin_thread(4))
    //    printf("Error pinning to CPU\n");


    /* Using RF */
    if(!prog_args.input) {
        /* Initialize radio */
        printf("Opening RF device with %d RX antennas...\n", DEFAULT_NOF_RX_ANTENNAS);
        if (srsran_rf_open_devname(&rf, "", prog_args.rf_args, DEFAULT_NOF_RX_ANTENNAS)) {
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

        /* Configure signal other signal handlers. This will show a stack dump is something crashes */
        sigset_t sigset;
        sigemptyset(&sigset);
        sigaddset(&sigset, SIGINT);
        sigprocmask(SIG_UNBLOCK, &sigset, NULL);
        signal(SIGINT, sig_int_handler);

        /* set receiver frequency */
        printf("Tuning receiver to %.3f MHz\n", (strtod(prog_args.rf_freq, NULL)) / 1000000);
        srsran_rf_set_rx_freq(&rf, DEFAULT_NOF_RX_ANTENNAS, strtod(prog_args.rf_freq, NULL));
        
        /* Search for cell (decode the MIB and fill the cell structure) */
        uint32_t ntrial = 0;
        do {
        ret = rf_search_and_decode_mib(
            &rf, DEFAULT_NOF_RX_ANTENNAS, &cell_detect_config, -1, &cell, &search_cell_cfo);
        if (ret < 0) {
            ERROR("Error searching for cell");
            exit(-1);
        } else if (ret == 0 && !go_exit) {
            printf("Cell not found after [%4d] attempts. Trying again... (Ctrl+C to exit)\n", ntrial++);
        }
        } while (ret == 0 && !go_exit);
        /* Print cell info */
        srsran_cell_fprint(stdout, &cell, 0);
        printf("cell.id %d, cell.nof_prb %d, cell.nof_ports %d, cell.cp %d, cell.phich_length %d, cell.phich_resources %d, cell.frame_type %d\n",
                            cell.id,
                            cell.nof_prb,
                            cell.nof_ports,
                            cell.cp,
                            cell.phich_length,
                            cell.phich_resources,
                            cell.frame_type);
                            
        FILE *file = fopen("celloutput.txt", "a");
        if (file == NULL) {
            perror("Error openning the file.\n");
        } else {
            fprintf(file, "cell.id %d, cell.nof_prb %d, cell.nof_ports %d, cell.cp %d, cell.phich_length %d, cell.phich_resources %d, cell.frame_type %d\n",
                            cell.id,
                            cell.nof_prb,
                            cell.nof_ports,
                            cell.cp,
                            cell.phich_length,
                            cell.phich_resources,
                            cell.frame_type);
        }
        fclose(file);

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
    }
    else { /* File based decoding */

        /* Read cell structure */
        if(read_cell_from_file(prog_args.cell, &cell)) {
            exit(1);
        }
        /* Initialize structures to get IQs from file */
        if (srsran_ue_sync_init_file_multi(&ue_sync,
                                        cell.nof_prb,
                                        prog_args.input,
                                        0, /* Default value */
                                        0, /* Default value */
                                        1)) { /* Default value */
            ERROR("Error initiating ue_sync");
            exit(-1);
        }
        /* Disable file wrapping */
        srsran_ue_sync_file_wrap(&ue_sync, false);
    }

    uint32_t max_num_samples = 3 * SRSRAN_SF_LEN_PRB(cell.nof_prb); /// Length in complex samples
    for (int i = 0; i < DEFAULT_NOF_RX_ANTENNAS; i++) {
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
    if (srsran_ue_dl_init(&ue_dl, sf_buffer, cell.nof_prb, DEFAULT_NOF_RX_ANTENNAS)) {
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

    pdsch_cfg.rnti = SRSRAN_SIRNTI; /* Broadcast RNTI */


    if(!prog_args.input) {
        /* Start RX streaming */
        srsran_rf_start_rx_stream(&rf, false);

        /* Sync with AGC */
        srsran_rf_info_t* rf_info = srsran_rf_get_info(&rf);
        srsran_ue_sync_start_agc(&ue_sync,
                                srsran_rf_set_rx_gain_th_wrapper_,
                                rf_info->min_rx_gain,
                                rf_info->max_rx_gain,
                                cell_detect_config.init_agc);
    }

    ue_sync.cfo_correct_enable_track = true;

    srsran_pbch_decode_reset(&ue_mib.pbch);

    printf("Entering main loop...\n");

    /* Main loop */
    uint64_t sf_cnt          = 0;
    uint32_t sfn             = 0;
    int n;

    while (!go_exit) {

        cf_t* buffers[SRSRAN_MAX_CHANNELS] = {};
        for (int p = 0; p < SRSRAN_MAX_PORTS; p++) {
            buffers[p] = sf_buffer[p];
        }

        /* Get IQs and copy them to the buffer */
        ret = srsran_ue_sync_zerocopy(&ue_sync, buffers, max_num_samples);
        if (ret < 0) {
            if(prog_args.input) {
                printf("End of the trace!\n");
                go_exit = true;
                continue;
            }
            ERROR("Error calling srsran_ue_sync_work()");
        }
        else if (ret == 0) { /* srsran_ue_sync_zerocopy returns 1 if successfully read 1 aligned subframe */
            printf("Finding PSS... Peak: %8.1f, FrameCnt: %d, State: %d\n", srsran_sync_get_peak_value(&ue_sync.sfind), ue_sync.frame_total_cnt, ue_sync.state);
        }
        else if (ret == 1) {
            bool acks[SRSRAN_MAX_CODEWORDS] = {false};
            uint32_t sf_idx;

            /* SubFrame index */
            sf_idx = srsran_ue_sync_get_sfidx(&ue_sync);


            /* DECODE MIB */
            if (sf_idx == 0) {
                uint8_t bch_payload[SRSRAN_BCH_PAYLOAD_LEN];
                int     sfn_offset;
                bzero(bch_payload, SRSRAN_BCH_PAYLOAD_LEN);
                n = srsran_ue_mib_decode(&ue_mib, bch_payload, NULL, &sfn_offset);
                if (n < 0) {
                    ERROR("Error decoding UE MIB");
                    exit(-1);
                } else if (n == SRSRAN_UE_MIB_FOUND) {
                    srsran_pbch_mib_unpack(bch_payload, &cell, &sfn);
                    cell.phich_resources = SRSRAN_PHICH_R_1;
                    sfn   = (sfn + sfn_offset) % 1024;
                    if(push_asn_payload(decoder, bch_payload, SRSRAN_BCH_PAYLOAD_LEN, MIB_4G, sfn * 10 + sf_idx)) {
                        printf("Error pushing SIB payload\n");
                    }
                }
            }

            uint32_t tti = sfn * 10 + sf_idx;


            /* Decode PDSCH */
            n = 0;
            for (uint32_t tm = 0; tm < 4 && !n; tm++) {
                dl_sf.tti                             = tti;
                dl_sf.sf_type                         = SRSRAN_SF_NORM;
                ue_dl_cfg.cfg.tm                      = (srsran_tm_t)tm; /* Transmission mode */
                ue_dl_cfg.cfg.pdsch.use_tbs_index_alt = false; /*enable_256qam*/
                ue_dl_cfg.chest_cfg = chest_pdsch_cfg; /* Set PDSCH channel estimation */


                /* DCIs */
                /*
                ngscope_tree_t tree;
                ngscope_dci_per_sub_t   dci_per_sub;
                empty_dci_persub(&dci_per_sub);
                n = srsran_ngscope_search_all_space_array_yx(&dci_decoder->ue_dl, &dci_decoder->dl_sf, \
                    &dci_decoder->ue_dl_cfg, &dci_decoder->pdsch_cfg, dci_per_sub, &tree, targetRNTI);
                // filter the dci 
                filter_dci_from_tree(&tree, &ue_tracker[rf_idx], dci_per_sub);

                // update the dci per subframe--> mainly decrease the tti
                update_ue_dci_per_tti(&tree, &ue_tracker[rf_idx], dci_per_sub, tti);

                // update the ue_tracker at subframe level, mainly remove those inactive UE
                ngscope_ue_tracker_update_per_tti(&ue_tracker[rf_idx], tti);

                */
                
                //ngscope_tree_t tree;
                //ngscope_dci_per_sub_t   dci_per_sub;
                //empty_dci_persub(&dci_per_sub);
                //n = srsran_ngscope_search_all_space_array_yx(&ue_dl, &dl_sf, &ue_dl_cfg, &pdsch_cfg, &dci_per_sub, &tree, SRSRAN_SIRNTI); /* 0x1315 */
                ///* filter the dci  */
                //filter_dci_from_tree(&tree, &ue_tracker, dci_per_sub);
                ///* update the dci per subframe--> mainly decrease the tti */
                //update_ue_dci_per_tti(&tree, &ue_tracker, dci_per_sub, tti);
                ///* update the ue_tracker at subframe level, mainly remove those inactive UE */
                //ngscope_ue_tracker_update_per_tti(&ue_tracker, tti);


                /* SIBs */
                if ((ue_dl_cfg.cfg.tm == SRSRAN_TM1 && cell.nof_ports == 1) ||
                    (ue_dl_cfg.cfg.tm > SRSRAN_TM1 && cell.nof_ports > 1)) {
                    n = srsran_ue_dl_find_and_decode(&ue_dl, &dl_sf, &ue_dl_cfg, &pdsch_cfg, data, acks);
                    if (n > 0) {
                        for (uint32_t tb = 0; tb < SRSRAN_MAX_CODEWORDS; tb++) {
                            if (pdsch_cfg.grant.tb[tb].enabled) {
                                if (acks[tb]) { 
                                    int len = pdsch_cfg.grant.tb[tb].tbs;
                                    uint8_t* payload 	= data[tb];
                                    if(push_asn_payload(decoder, payload, len, SIB_4G, tti)) {
                                        printf("Error pushing SIB payload\n");
                                    }
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
            if (sf_idx == 9) {
                sfn++;
                if (sfn == 1024) {
                    sfn = 0;
                }
            }
        }
        sf_cnt++;
    } // Main loop

    terminate_asn_decoder(decoder);
    srsran_ue_dl_free(&ue_dl);
    srsran_ue_sync_free(&ue_sync);

    for (int i = 0; i < SRSRAN_MAX_CODEWORDS; i++) {
      if (data[i]) {
        free(data[i]);
      }
    }
    for (int i = 0; i < DEFAULT_NOF_RX_ANTENNAS; i++) {
      if (sf_buffer[i]) {
        free(sf_buffer[i]);
      }
    }
    
    srsran_ue_mib_free(&ue_mib);
    
    if(prog_args.rf_freq) {
        srsran_rf_close(&rf);
    }

    printf("\nBye\n");
    exit(0);
  }
