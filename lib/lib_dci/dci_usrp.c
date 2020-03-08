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
#include "srslte/phy/io/filesink.h"
#include <srslte/phy/common/phy_common.h>
#include "srslte/srslte.h"
#include "srslte/phy/ue/lte_scope.h"
#include "srslte/phy/ue/ue_list.h"
#include "srslte/phy/ue/LTESCOPE_GLOBAL.h"

#define ENABLE_AGC_DEFAULT
#include "srslte/phy/rf/rf.h"
#include "srslte/phy/rf/rf_utils.h"

#include "arg_parser.h"
#include "dci_decoder.h"
#include "ue_cell_status.h"

extern bool go_exit;

extern srslte_ue_sync_t	    ue_sync[MAX_NOF_USRP];
extern prog_args_t	    prog_args[MAX_NOF_USRP];
extern srslte_cell_t	    cell[MAX_NOF_USRP];
extern srslte_rf_t	    rf[MAX_NOF_USRP];
extern srslte_ue_cell_usage ue_cell_usage;
extern pthread_mutex_t	    mutex_usage;

int srslte_rf_recv_wrapper(void *h, cf_t *data[SRSLTE_MAX_PORTS], uint32_t nsamples, srslte_timestamp_t *t) {
  DEBUG(" ----  Receive %d samples  ---- \n", nsamples);
  void *ptr[SRSLTE_MAX_PORTS];
  for (int i=0;i<SRSLTE_MAX_PORTS;i++) {
    ptr[i] = data[i];
  }
  return srslte_rf_recv_with_time_multi((srslte_rf_t*) h, ptr, nsamples, true, NULL, NULL);
}

double srslte_rf_set_rx_gain_th_wrapper_(void *h, double f) {
  return srslte_rf_set_rx_gain_th((srslte_rf_t*) h, f);
}

void* dci_start_usrp(void* p)
{
    int ret;
    float cfo = 0;
    int usrp_idx = (int)*(int *)p;

    cell_search_cfg_t cell_detect_config = {
        SRSLTE_DEFAULT_MAX_FRAMES_PBCH,
        SRSLTE_DEFAULT_MAX_FRAMES_PSS,
        SRSLTE_DEFAULT_NOF_VALID_PSS_FRAMES,
        0
    };
    printf("Opening %d-th RF device with %d RX antennas...\n", usrp_idx, prog_args[usrp_idx].rf_nof_rx_ant);
    //if( srslte_rf_open_devname(&rf[usrp_idx], prog_args[usrp_idx].rf_dev, prog_args[usrp_idx].rf_args, prog_args[usrp_idx].rf_nof_rx_ant) ) {
    if (srslte_rf_open_multi(&rf[usrp_idx], prog_args[usrp_idx].rf_args, prog_args[usrp_idx].rf_nof_rx_ant)) {
        fprintf(stderr, "Error opening rf\n");
        exit(-1);
    }
    /* Set receiver gain */
    if (prog_args[usrp_idx].rf_gain > 0) {
        srslte_rf_set_rx_gain(&rf[usrp_idx], prog_args[usrp_idx].rf_gain);
    } else {
        printf("Starting AGC thread...\n");
        if (srslte_rf_start_gain_thread(&rf[usrp_idx], false)) {
            fprintf(stderr, "Error opening rf\n");
            exit(-1);
        }
        srslte_rf_set_rx_gain(&rf[usrp_idx], srslte_rf_get_rx_gain(&rf[usrp_idx]));
        cell_detect_config.init_agc = srslte_rf_get_rx_gain(&rf[usrp_idx]);
    }

    srslte_rf_set_master_clock_rate(&rf[usrp_idx], 30.72e6);

    /* set receiver frequency */
    printf("Tunning receiver to %.3f MHz\n", (prog_args[usrp_idx].rf_freq )/1000000);
    srslte_rf_set_rx_freq(&rf[usrp_idx], prog_args[usrp_idx].rf_freq);
    srslte_rf_rx_wait_lo_locked(&rf[usrp_idx]);

    uint32_t ntrial=0;
    do {
        ret = rf_search_and_decode_mib(&rf[usrp_idx], prog_args[usrp_idx].rf_nof_rx_ant, 
				       &cell_detect_config, prog_args[usrp_idx].force_N_id_2, &cell[usrp_idx], &cfo);
        if (ret < 0) {
            fprintf(stderr, "Error searching for cell\n");
            exit(-1);
        } else if (ret == 0 && !go_exit) {
            printf("Cell not found after %d trials. Trying again (Press Ctrl+C to exit)\n", ntrial++);
        }
    } while (ret == 0 && !go_exit);

    if (go_exit) {
        srslte_rf_close(&rf[usrp_idx]);
        exit(0);
    }
    srslte_rf_stop_rx_stream(&rf[usrp_idx]);
    srslte_rf_flush_buffer(&rf[usrp_idx]);

    //  set the prb for cell ue usage status
    pthread_mutex_lock(&mutex_usage);
    srslte_UeCell_set_prb(&ue_cell_usage, cell[usrp_idx].nof_prb, usrp_idx);
    pthread_mutex_unlock(&mutex_usage);

    /* set sampling frequency */
    int srate = srslte_sampling_freq_hz(cell[usrp_idx].nof_prb);
    if (srate != -1) {
        if (srate < 10e6) {
            srslte_rf_set_master_clock_rate(&rf[usrp_idx], 4*srate);
        } else {
            srslte_rf_set_master_clock_rate(&rf[usrp_idx], srate);
        }
        printf("Setting sampling rate %.2f MHz\n", (float) srate/1000000);
        float srate_rf = srslte_rf_set_rx_srate(&rf[usrp_idx], (double) srate);
        if (srate_rf != srate) {
            fprintf(stderr, "Could not set sampling rate\n");
            exit(-1);
        }
    } else {
        fprintf(stderr, "Invalid number of PRB %d\n", cell[usrp_idx].nof_prb);
        exit(-1);
    }

    INFO("Stopping RF and flushing buffer...\r");

    int decimate = 1;
    if (prog_args[usrp_idx].decimate) {
        if (prog_args[usrp_idx].decimate > 4 || prog_args[usrp_idx].decimate < 0) {
            printf("Invalid decimation factor, setting to 1 \n");
        } else {
            decimate = prog_args[usrp_idx].decimate;
        }
    }

    if (srslte_ue_sync_init_multi_decim(&ue_sync[usrp_idx], cell[usrp_idx].nof_prb, cell[usrp_idx].id == 1000, srslte_rf_recv_wrapper,
                            prog_args[usrp_idx].rf_nof_rx_ant, (void*)&rf[usrp_idx], decimate)) {
        fprintf(stderr, "Error initiating ue_sync\n");
        exit(-1);
    }

    if (srslte_ue_sync_set_cell(&ue_sync[usrp_idx], cell[usrp_idx])) {
        fprintf(stderr, "Error initiating ue_sync\n");
        exit(-1);
    }

    // Disable CP based CFO estimation during find
    ue_sync[usrp_idx].cfo_current_value = cfo/15000;
    ue_sync[usrp_idx].cfo_is_copied = true;
    ue_sync[usrp_idx].cfo_correct_enable_find = true;

    srslte_sync_set_cfo_cp_enable(&ue_sync[usrp_idx].sfind, false, 0);
    srslte_rf_info_t *rf_info = srslte_rf_get_info(&rf[usrp_idx]);
    srslte_ue_sync_start_agc(&ue_sync[usrp_idx],
                 srslte_rf_set_rx_gain_th_wrapper_,
                 rf_info->min_rx_gain,
                 rf_info->max_rx_gain,
                 cell_detect_config.init_agc);

    ue_sync[usrp_idx].cfo_correct_enable_track = !prog_args[usrp_idx].disable_cfo;
    srslte_rf_start_rx_stream(&rf[usrp_idx], false);

    // handle the free order
    int thd_count = 0;
    if( usrp_idx > 0){
        for(int i=0;i<usrp_idx;i++){
            thd_count += prog_args[i].nof_thread;
        }
    }

    dci_usrp_id_t dci_usrp_id[MAX_NOF_USRP];
    pthread_t dci_thd[MAX_NOF_USRP];

    for(int i=0;i<prog_args[usrp_idx].nof_thread;i++){
        thd_count += 1;
        dci_usrp_id[i].free_order  = thd_count;
        dci_usrp_id[i].dci_thd_id  = i+1;
        dci_usrp_id[i].usrp_thd_id = usrp_idx;
        pthread_create( &dci_thd[i], NULL, dci_decoder, (void *)&dci_usrp_id[i]);
    }
    for(int i=0;i<prog_args[usrp_idx].nof_thread;i++){
        pthread_join(dci_thd[i], NULL);
    }

    srslte_ue_sync_free(&ue_sync[usrp_idx]);
    srslte_rf_close(&rf[usrp_idx]);
    printf("Bye -- USRP:%d \n", usrp_idx);
    pthread_exit(NULL);
}

