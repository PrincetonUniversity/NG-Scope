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

#include "srsran/common/crash_handler.h"
#include "srsran/common/gen_mch_tables.h"
#include "srsran/phy/io/filesink.h"
#include "srsran/srsran.h"

#include "srsran/phy/rf/rf.h"
#include "srsran/phy/ue/ue_cell_search_nbiot.h"

#include "ngscope/hdr/dciLib/radio.h"

extern bool go_exit;

int radio_init_and_start(srsran_rf_t* rf, 
                    srsran_cell_t* cell, 
                    prog_args_t prog_args, 
                    cell_search_cfg_t* cell_detect_config, 
                    float* search_cell_cfo){
    int ret;

    printf("Opening RF device with %d RX antennas...\n", prog_args.rf_nof_rx_ant);
    if (srsran_rf_open_devname(rf, prog_args.rf_dev, prog_args.rf_args, prog_args.rf_nof_rx_ant)) {
      fprintf(stderr, "Error opening rf\n");
      exit(-1);
    }
    /* Set receiver gain */
    if (prog_args.rf_gain > 0) {
      srsran_rf_set_rx_gain(rf, prog_args.rf_gain);
    } else {
      printf("Starting AGC thread...\n");
      if (srsran_rf_start_gain_thread(rf, false)) {
        ERROR("Error opening rf");
        exit(-1);
      }
      srsran_rf_set_rx_gain(rf, srsran_rf_get_rx_gain(rf));
      cell_detect_config->init_agc = srsran_rf_get_rx_gain(rf);
    }

    /* set receiver frequency */
    printf("Tunning receiver to %.3f MHz\n", (prog_args.rf_freq + prog_args.file_offset_freq) / 1000000);
    srsran_rf_set_rx_freq(rf, prog_args.rf_nof_rx_ant, prog_args.rf_freq + prog_args.file_offset_freq);

    uint32_t ntrial = 0;
    do {
      ret = rf_search_and_decode_mib(
          rf, prog_args.rf_nof_rx_ant, cell_detect_config, prog_args.force_N_id_2, cell, search_cell_cfo);
      if (ret < 0) {
        ERROR("Error searching for cell");
        exit(-1);
      } else if (ret == 0 && !go_exit) {
        printf("Cell not found after %d trials. Trying again (Press Ctrl+C to exit)\n", ntrial++);
      }
    } while (ret == 0 && !go_exit);

    if (go_exit) {
      srsran_rf_close(rf);
      exit(0);
    }

    /* set sampling frequency */
    int srate = srsran_sampling_freq_hz(cell->nof_prb);
    if (srate != -1) {
      printf("Setting sampling rate %.2f MHz\n", (float)srate / 1000000);
      float srate_rf = srsran_rf_set_rx_srate(rf, (double)srate);
      printf("srate_rf:%f\n",srate_rf);
      if (srate_rf != srate) {
        ERROR("Could not set sampling rate");
        exit(-1);
      }
    } else {
       ERROR("Invalid number of PRB %d", cell->nof_prb);
      exit(-1);
    }
    // start rx stream
    srsran_rf_start_rx_stream(rf, false);
    return SRSRAN_SUCCESS;
}

int radio_stop(srsran_rf_t* rf){
    srsran_rf_stop_rx_stream(rf);
    srsran_rf_close(rf);
    return SRSRAN_SUCCESS;
}
