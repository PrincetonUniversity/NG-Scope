#ifndef NGSCOPE_STATUS_PLOT_H
#define NGSCOPE_STATUS_PLOT_H

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
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "srsran/srsran.h"
#include "ngscope_def.h"
#include "dci_decoder.h"

typedef struct{
    float       csi_amp[100 * 12];
    uint32_t    tti;
    int         cell_dl_prb;
    int         cell_ul_prb;
}ngscope_plot_sf_t;

typedef struct{
    int header;
    //handle multi-thread synchronization
    uint16_t    dci_touched;
    uint16_t    token[PLOT_SF];

    ngscope_plot_sf_t plot_data_sf[PLOT_SF];
}ngscope_plot_cell_t;

typedef struct{
    uint16_t    targetRNTI;
    int         nof_cell;
    int         header;
    int         cell_prb[MAX_NOF_RF_DEV];
    ngscope_plot_cell_t plot_data_cell[MAX_NOF_RF_DEV];
}ngscope_plot_t;



void* plot_thread_run(void* arg);
void  plot_init_thread(pthread_t* plot_thread);
void  plot_init_pdcch_thread(pthread_t* plot_thread, decoder_plot_t* arg);

#ifdef __cplusplus
}
#endif
#endif
