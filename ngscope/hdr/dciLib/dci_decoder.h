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
#include "ngscope/hdr/dciLib/asn_decoder.h"


#ifdef __cplusplus
extern "C" {
#endif


#include "radio.h"

// Logger thread + Queue
#define NGSCOPE_DECODER_LOG_QUEUE_LEN 100

// binary writing
#pragma pack(push, 1)
typedef struct {
    uint32_t tti;
    uint64_t ts;
    uint16_t nof_ports;
    uint16_t nof_rx_antennas;
    uint32_t nof_re;
} chest_bin_hdr_t;
#pragma pack(pop)

typedef struct{
    uint32_t tti;
    float  rsrp_dbm;
    float* rsrp_per_rb_dbm;
    int    rsrp_nof_rb;
    int    nof_ports;
    int    nof_rx_antennas;
    int    nof_re;
    float* ce_real;
    float* ce_imag;
}ngscope_decoder_log_item_t;

typedef struct{
    pthread_t       thread;
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    bool            stop;
    size_t          head;
    size_t          tail;
    size_t          count;
    size_t          capacity;
    uint64_t        dropped;
    uint64_t        last_write_ms;
    uint64_t        last_chest_write_ms;
    int             csi_downsample_ms;
    ngscope_decoder_log_item_t** items;
    FILE*           rsrp_file;
    FILE*           rsrp_prbs_file;
    FILE*           chest_config_file;
    FILE*           chest_real_file;
    FILE*           chest_imag_file;
}ngscope_decoder_logger_t;

typedef struct{
    srsran_ue_dl_t     ue_dl;
    srsran_ue_dl_cfg_t ue_dl_cfg;
    srsran_dl_sf_cfg_t dl_sf;
    srsran_pdsch_cfg_t pdsch_cfg;
    srsran_cell_t      cell;
    prog_args_t        prog_args;
    int                decoder_idx;
    ASNDecoder * decoder;

    // Logger thread + Queue
    ngscope_decoder_logger_t* logger;
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
							uint8_t*                data[SRSRAN_MAX_CODEWORDS],
                            ngscope_dci_per_sub_t*  dci_per_sub);

void* dci_decoder_thread(void* p);

int ngscope_decoder_logger_init(ngscope_decoder_logger_t* logger, size_t capacity, int csi_downsample_ms);
void ngscope_decoder_logger_close(ngscope_decoder_logger_t* logger);
void ngscope_decoder_logger_enqueue(ngscope_decoder_logger_t* logger,
                                    const ngscope_dci_decoder_t* dci_decoder);

#ifdef __cplusplus
}
#endif

#endif
