/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2015 Software Radio Systems Limited
 *
 * \section LICENSE
 *
 * This file is part of the srsLTE library.
 *
 * srsLTE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsLTE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include "srslte/common/gen_mch_tables.h"
#include "srslte/common/crash_handler.h"
#include <srslte/phy/common/phy_common.h>
#include "srslte/phy/io/filesink.h"
#include "srslte/srslte.h"
#include "srslte/phy/ue/lte_scope.h"
#include "srslte/phy/ue/ue_list.h"

#define ENABLE_AGC_DEFAULT

#include "srslte/phy/rf/rf.h"
#include "srslte/phy/rf/rf_utils.h"

#include "dci_decode_main.h"

cell_search_cfg_t cell_detect_config = {
  SRSLTE_DEFAULT_MAX_FRAMES_PBCH,
  SRSLTE_DEFAULT_MAX_FRAMES_PSS, 
  SRSLTE_DEFAULT_NOF_VALID_PSS_FRAMES,
  0
};

//#define STDOUT_COMPACT

#define PRINT_CHANGE_SCHEDULIGN

//#define CORRECT_SAMPLE_OFFSET

extern float mean_exec_time;

//enum receiver_state { DECODE_MIB, DECODE_PDSCH} state; 
bool go_exit = false; 
enum receiver_state state; 
srslte_ue_sync_t ue_sync; 
prog_args_t prog_args; 
srslte_ue_list_t ue_list;
srslte_cell_t cell;  
srslte_rf_t rf; 
uint32_t system_frame_number = 0; // system frame number


void sig_int_handler(int signo)
{
  printf("SIGINT received. Exiting...\n");
  if (signo == SIGINT) {
    go_exit = true;
  } else if (signo == SIGSEGV) {
    exit(1);
  }
}

#ifndef DISABLE_RF
int srslte_rf_recv_wrapper(void *h, cf_t *data[SRSLTE_MAX_PORTS], uint32_t nsamples, srslte_timestamp_t *t) {
  DEBUG(" ----  Receive %d samples  ---- \n", nsamples);
  void *ptr[SRSLTE_MAX_PORTS];
  for (int i=0;i<SRSLTE_MAX_PORTS;i++) {
    ptr[i] = data[i];
  }
  return srslte_rf_recv_with_time_multi(h, ptr, nsamples, true, NULL, NULL);
}

double srslte_rf_set_rx_gain_th_wrapper_(void *h, double f) {
  return srslte_rf_set_rx_gain_th((srslte_rf_t*) h, f);
}

#endif


int main(int argc, char **argv) {
    int ret;

    float cfo = 0; 
    srslte_debug_handle_crash(argc, argv);
    parse_args(&prog_args, argc, argv);
    srslte_init_ue_list(&ue_list);  

    printf("Opening RF device with %d RX antennas...\n", prog_args.rf_nof_rx_ant);
    if (srslte_rf_open_multi(&rf, prog_args.rf_args, prog_args.rf_nof_rx_ant)) {
	fprintf(stderr, "Error opening rf\n");
	exit(-1);
    }
    /* Set receiver gain */
    if (prog_args.rf_gain > 0) {
	srslte_rf_set_rx_gain(&rf, prog_args.rf_gain);      
    } else {
	printf("Starting AGC thread...\n");
	if (srslte_rf_start_gain_thread(&rf, false)) {
	    fprintf(stderr, "Error opening rf\n");
	    exit(-1);
	}
	srslte_rf_set_rx_gain(&rf, srslte_rf_get_rx_gain(&rf));
	cell_detect_config.init_agc = srslte_rf_get_rx_gain(&rf);
    }

    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigprocmask(SIG_UNBLOCK, &sigset, NULL);
    signal(SIGINT, sig_int_handler);

    srslte_rf_set_master_clock_rate(&rf, 30.72e6);        

    /* set receiver frequency */
    printf("Tunning receiver to %.3f MHz\n", (prog_args.rf_freq + prog_args.file_offset_freq)/1000000);
    srslte_rf_set_rx_freq(&rf, prog_args.rf_freq + prog_args.file_offset_freq);
    srslte_rf_rx_wait_lo_locked(&rf);


    uint32_t ntrial=0; 
    do {
	ret = rf_search_and_decode_mib(&rf, prog_args.rf_nof_rx_ant, &cell_detect_config, prog_args.force_N_id_2, &cell, &cfo);
	if (ret < 0) {
	    fprintf(stderr, "Error searching for cell\n");
	    exit(-1); 
	} else if (ret == 0 && !go_exit) {
	    printf("Cell not found after %d trials. Trying again (Press Ctrl+C to exit)\n", ntrial++);
	}      
    } while (ret == 0 && !go_exit); 

    if (go_exit) {
	srslte_rf_close(&rf);    
	exit(0);
    }
    srslte_rf_stop_rx_stream(&rf);
    srslte_rf_flush_buffer(&rf);
    /* set sampling frequency */
    int srate = srslte_sampling_freq_hz(cell.nof_prb);    
    if (srate != -1) {  
	if (srate < 10e6) {          
	    srslte_rf_set_master_clock_rate(&rf, 4*srate);        
	} else {
	    srslte_rf_set_master_clock_rate(&rf, srate);        
	}
	printf("Setting sampling rate %.2f MHz\n", (float) srate/1000000);
	float srate_rf = srslte_rf_set_rx_srate(&rf, (double) srate);
	if (srate_rf != srate) {
	    fprintf(stderr, "Could not set sampling rate\n");
	    exit(-1);
	}
    } else {
	fprintf(stderr, "Invalid number of PRB %d\n", cell.nof_prb);
	exit(-1);
    }

    INFO("Stopping RF and flushing buffer...\r");

    int decimate = 1;
    if (prog_args.decimate) {
	if (prog_args.decimate > 4 || prog_args.decimate < 0) {
	    printf("Invalid decimation factor, setting to 1 \n");
	} else {
	    decimate = prog_args.decimate;
	}
    }

    if (srslte_ue_sync_init_multi_decim(&ue_sync, cell.nof_prb, cell.id == 1000, srslte_rf_recv_wrapper,
			    prog_args.rf_nof_rx_ant, (void*)&rf, decimate)) {
	fprintf(stderr, "Error initiating ue_sync\n");
	exit(-1); 
    }

    if (srslte_ue_sync_set_cell(&ue_sync, cell)) {
	fprintf(stderr, "Error initiating ue_sync\n");
	exit(-1);
    }

    // Disable CP based CFO estimation during find
    ue_sync.cfo_current_value = cfo/15000;
    ue_sync.cfo_is_copied = true;
    ue_sync.cfo_correct_enable_find = true;

    srslte_sync_set_cfo_cp_enable(&ue_sync.sfind, false, 0);
    srslte_rf_info_t *rf_info = srslte_rf_get_info(&rf);
    srslte_ue_sync_start_agc(&ue_sync,
		 srslte_rf_set_rx_gain_th_wrapper_,
		 rf_info->min_rx_gain,
		 rf_info->max_rx_gain,
		 cell_detect_config.init_agc);

    ue_sync.cfo_correct_enable_track = !prog_args.disable_cfo;
    srslte_rf_start_rx_stream(&rf, false);

    int thd_id[10];
    pthread_t dci_thd[10];
    for(int i=0;i<prog_args.nof_thread;i++){
	thd_id[i] = i+1;
	pthread_create( &dci_thd[i], NULL, dci_decode_multi, (void *)&thd_id[i]);
    }
    for(int i=0;i<prog_args.nof_thread;i++){
	pthread_join(dci_thd[i], NULL); 
    }
    srslte_ue_sync_free(&ue_sync);
    srslte_rf_close(&rf);    
    printf("\nBye\n");
    exit(0);
}

