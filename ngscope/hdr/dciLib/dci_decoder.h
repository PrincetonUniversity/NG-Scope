#ifndef NGSCOPE_DCI_DECODER_H
#define NGSCOPE_DCI_DECODER_H

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


#include "radio.h"

typedef struct{
    srsran_ue_dl_t     ue_dl;
    srsran_ue_dl_cfg_t ue_dl_cfg;
    srsran_dl_sf_cfg_t dl_sf;
    srsran_pdsch_cfg_t pdsch_cfg;
    srsran_cell_t      cell;
    prog_args_t        prog_args;
    int                decoder_idx;
}ngscope_dci_decoder_t;

typedef struct{
	int decoder_idx;
	int nof_pdcch_sample;
	int nof_prb;
	int size;
}decoder_plot_t;

int dci_decoder_init(ngscope_dci_decoder_t*     dci_decoder,
                        prog_args_t             prog_args,
                        srsran_cell_t*          cell,
                        cf_t*                   sf_buffer[SRSRAN_MAX_PORTS],
                        srsran_softbuffer_rx_t* rx_softbuffers,
                        int                     decoder_idx);

int dci_decoder_decode(ngscope_dci_decoder_t*       dci_decoder,
                            uint32_t                sf_idx,
                            uint32_t                sfn,
                            ngscope_dci_per_sub_t*  dci_per_sub);

void* dci_decoder_thread(void* p);
#ifdef __cplusplus
}
#endif

#endif
