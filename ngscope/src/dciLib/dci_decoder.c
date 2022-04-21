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

#include "srsran/srsran.h"
#include "ngscope/hdr/dciLib/radio.h"
#include "ngscope/hdr/dciLib/task_scheduler.h"
#include "ngscope/hdr/dciLib/dci_decoder.h"
#include "ngscope/hdr/dciLib/phich_decoder.h"

extern bool                 go_exit;
extern ngscope_sf_buffer_t  sf_buffer[MAX_NOF_DCI_DECODER];
extern bool                 sf_token[MAX_NOF_DCI_DECODER];
extern pthread_mutex_t      token_mutex; 

extern dci_ready_t         dci_ready;
extern ngscope_status_buffer_t    dci_buffer[MAX_DCI_BUFFER];

// For decoding phich
extern pend_ack_list       ack_list;
extern pthread_mutex_t     ack_mutex;

int dci_decoder_init(ngscope_dci_decoder_t*     dci_decoder,
                        prog_args_t             prog_args,
                        srsran_cell_t*          cell,
                        cf_t*                   sf_buffer[SRSRAN_MAX_PORTS],
                        srsran_softbuffer_rx_t* rx_softbuffers,
                        int                     decoder_idx){
    // Init the args
    dci_decoder->prog_args  = prog_args;
    dci_decoder->cell       = *cell;

    if (srsran_ue_dl_init(&dci_decoder->ue_dl, sf_buffer, cell->nof_prb, prog_args.rf_nof_rx_ant)) {
        ERROR("Error initiating UE downlink processing module");
        exit(-1);
    }
    if (srsran_ue_dl_set_cell(&dci_decoder->ue_dl, *cell)) {
        ERROR("Error initiating UE downlink processing module");
        exit(-1);
    }

    ZERO_OBJECT(dci_decoder->ue_dl_cfg);
    ZERO_OBJECT(dci_decoder->dl_sf);
    ZERO_OBJECT(dci_decoder->pdsch_cfg);

    /************************* Init dl_sf **************************/
    if (cell->frame_type == SRSRAN_TDD && prog_args.tdd_special_sf >= 0 && prog_args.sf_config >= 0) {
        dci_decoder->dl_sf.tdd_config.ss_config  = prog_args.tdd_special_sf;
        //dci_decoder->dl_sf.tdd_config.sf_config  = prog_args.sf_config;
        dci_decoder->dl_sf.tdd_config.sf_config  = 2; 
        dci_decoder->dl_sf.tdd_config.configured = true;
    }
    dci_decoder->dl_sf.tdd_config.ss_config  = prog_args.tdd_special_sf;
    //dci_decoder->dl_sf.tdd_config.sf_config  = prog_args.sf_config;
    dci_decoder->dl_sf.tdd_config.sf_config  = 2; 
    dci_decoder->dl_sf.tdd_config.configured = true;

    /************************* Init ue_dl_cfg **************************/
    srsran_chest_dl_cfg_t chest_pdsch_cfg = {};
    chest_pdsch_cfg.cfo_estimate_enable   = prog_args.enable_cfo_ref;
    chest_pdsch_cfg.cfo_estimate_sf_mask  = 1023;
    chest_pdsch_cfg.estimator_alg         = srsran_chest_dl_str2estimator_alg(prog_args.estimator_alg);
    chest_pdsch_cfg.sync_error_enable     = true;

    // Set PDSCH channel estimation (we don't consider MBSFN)
    dci_decoder->ue_dl_cfg.chest_cfg = chest_pdsch_cfg;

    /************************* Init pdsch_cfg **************************/
    dci_decoder->pdsch_cfg.meas_evm_en = true;
    // Allocate softbuffer buffers
    //srsran_softbuffer_rx_t rx_softbuffers[SRSRAN_MAX_CODEWORDS];
    for (uint32_t i = 0; i < SRSRAN_MAX_CODEWORDS; i++) {
        dci_decoder->pdsch_cfg.softbuffers.rx[i] = &rx_softbuffers[i];
        srsran_softbuffer_rx_init(dci_decoder->pdsch_cfg.softbuffers.rx[i], cell->nof_prb);
    }

    dci_decoder->pdsch_cfg.rnti = prog_args.rnti;
    dci_decoder->decoder_idx    = decoder_idx;
    return SRSRAN_SUCCESS;
}

int dci_decoder_decode(ngscope_dci_decoder_t*       dci_decoder,
                            uint32_t                sf_idx,
                            uint32_t                sfn,
                            ngscope_dci_msg_t       dci_array[][MAX_CANDIDATES_ALL],
                            srsran_dci_location_t   dci_location[MAX_CANDIDATES_ALL],
                            ngscope_dci_per_sub_t*  dci_per_sub)
{
    uint32_t tti = sfn * 10 + sf_idx;

    bool decode_pdsch = false;

    // Shall we decode the PDSCH of the current subframe?
    if (dci_decoder->prog_args.rnti != SRSRAN_SIRNTI) {
        decode_pdsch = true;
        if (srsran_sfidx_tdd_type(dci_decoder->dl_sf.tdd_config, sf_idx) == SRSRAN_TDD_SF_U) {
            decode_pdsch = false;
        }
    } else {
        /* We are looking for SIB1 Blocks, search only in appropiate places */
        if ((sf_idx == 5 && (sfn % 2) == 0)) {
            decode_pdsch = true;
        } else {
            decode_pdsch = false;
        }
    }

    //if(sf_idx % 1 == 0)    
    //    decode_pdsch = true;

    decode_pdsch = true;

    if ( (dci_decoder->cell.frame_type == SRSRAN_TDD) && 
        (srsran_sfidx_tdd_type(dci_decoder->dl_sf.tdd_config, sf_idx) == SRSRAN_TDD_SF_U) ){
        printf("TDD uplink subframe skip\n");
        decode_pdsch = false;
    }
 
    int n = 0;
    // Now decode the PDSCH
    if(decode_pdsch){

        uint32_t tm = 3;
        dci_decoder->dl_sf.tti                             = tti;
        dci_decoder->dl_sf.sf_type                         = SRSRAN_SF_NORM; //Ingore the MBSFN
        dci_decoder->ue_dl_cfg.cfg.tm                      = (srsran_tm_t)tm;
        dci_decoder->ue_dl_cfg.cfg.pdsch.use_tbs_index_alt = dci_decoder->prog_args.enable_256qam;

        //n = srsran_ngscope_search_all_space_yx(&dci_decoder->ue_dl, &dci_decoder->dl_sf, 
        //                                    &dci_decoder->ue_dl_cfg, &dci_decoder->pdsch_cfg);
        
        n = srsran_ngscope_search_all_space_array_yx(&dci_decoder->ue_dl, &dci_decoder->dl_sf, &dci_decoder->ue_dl_cfg, 
                                            &dci_decoder->pdsch_cfg, dci_array, dci_location, dci_per_sub);
    } 
    return SRSRAN_SUCCESS;
}
int get_target_dci(ngscope_dci_msg_t* msg, int nof_msg, uint16_t targetRNTI){
    for(int i=0; i<nof_msg; i++){
        if(msg[i].rnti == targetRNTI){
            return i;
        }
    }
    return -1;
}
int dci_decoder_phich_decode(ngscope_dci_decoder_t*       dci_decoder,
                                  uint32_t                tti,
                                  ngscope_dci_per_sub_t*  dci_per_sub)
{
    uint16_t targetRNTI = dci_decoder->prog_args.rnti;
    if(targetRNTI > 0){
        if(dci_per_sub->nof_ul_dci > 0){
            int idx = get_target_dci(dci_per_sub->ul_msg, dci_per_sub->nof_ul_dci, targetRNTI);
            if(idx >= 0){
                uint32_t n_dmrs         = dci_per_sub->ul_msg[idx].phich.n_dmrs;
                uint32_t n_prb_tilde    = dci_per_sub->ul_msg[idx].phich.n_prb_tilde;
                pthread_mutex_lock(&ack_mutex);
                phich_set_pending_ack(&ack_list, TTI_RX_ACK(tti), n_prb_tilde, n_dmrs);
                pthread_mutex_unlock(&ack_mutex);
            }
        }
        srsran_phich_res_t phich_res;
        bool ack_available = false;
        ack_available = decode_phich(&dci_decoder->ue_dl, &dci_decoder->dl_sf, &dci_decoder->ue_dl_cfg, &ack_list, &phich_res);
    }
    return 0;
}


void empty_dci_array(ngscope_dci_msg_t   dci_array[][MAX_CANDIDATES_ALL],
                        srsran_dci_location_t   dci_location[MAX_CANDIDATES_ALL],
                        ngscope_dci_per_sub_t*  dci_per_sub){
    for(int i=0; i<MAX_NOF_FORMAT+1; i++){
        ZERO_OBJECT(dci_location[i]);
        for(int j=0; j<MAX_CANDIDATES_ALL; j++){
            ZERO_OBJECT(dci_array[i][j]);
        }
    }
    
    dci_per_sub->nof_dl_dci = 0;
    dci_per_sub->nof_ul_dci = 0;
    for(int i=0; i<MAX_DCI_PER_SUB; i++){
        ZERO_OBJECT(dci_per_sub->dl_msg[i]); 
        ZERO_OBJECT(dci_per_sub->ul_msg[i]); 
    }
    return;
}

void* dci_decoder_thread(void* p){
    ngscope_dci_decoder_t*  dci_decoder = (ngscope_dci_decoder_t*)p;
    ngscope_dci_msg_t       dci_array[MAX_NOF_FORMAT+1][MAX_CANDIDATES_ALL];
    srsran_dci_location_t   dci_location[MAX_CANDIDATES_ALL];
    ngscope_dci_per_sub_t   dci_per_sub; 
    ngscope_status_buffer_t        dci_ret;
    int rf_idx     = dci_decoder->prog_args.rf_index;

    printf("Decoder thread idx:%d\n\n\n",dci_decoder->decoder_idx);
    while(!go_exit){
        empty_dci_array(dci_array, dci_location, &dci_per_sub);
        //printf("dci decoder -1\n");
//--->  Lock the buffer
        pthread_mutex_lock(&sf_buffer[dci_decoder->decoder_idx].sf_mutex);
    
        // We release the token in the last minute just before the waiting of the condition signal 
        pthread_mutex_lock(&token_mutex);
        sf_token[dci_decoder->decoder_idx] = false;
        pthread_mutex_unlock(&token_mutex);

//--->  Wait the signal 
        //printf("%d-th decoder is waiting for conditional signal!\n", dci_decoder->decoder_idx);

        pthread_cond_wait(&sf_buffer[dci_decoder->decoder_idx].sf_cond, 
                          &sf_buffer[dci_decoder->decoder_idx].sf_mutex);
        //printf("%d-th decoder Get the conditional signal!\n", dci_decoder->decoder_idx);
    
        uint32_t sfn    = sf_buffer[dci_decoder->decoder_idx].sfn;
        uint32_t sf_idx = sf_buffer[dci_decoder->decoder_idx].sf_idx;
        uint32_t tti    = sfn * 10 + sf_idx;
        //printf(" Get the signal! decoder_idx:%d sfn:%d sf_idx:%d\n", 
        //                    dci_decoder->decoder_idx, sfn, sf_idx);
        //dci_decoder_decode(dci_decoder, sf_idx, sfn);
        dci_decoder_decode(dci_decoder, sf_idx, sfn, dci_array, dci_location, &dci_per_sub);
    
//--->  Unlock the buffer
        pthread_mutex_unlock(&sf_buffer[dci_decoder->decoder_idx].sf_mutex);

        dci_decoder_phich_decode(dci_decoder, tti, &dci_per_sub);
        dci_ret.dci_per_sub  = dci_per_sub;
        dci_ret.tti          = sfn *10 + sf_idx;
        dci_ret.cell_idx     = rf_idx;
   
        for(int i=0; i< 12 * dci_decoder->cell.nof_prb; i++){
            dci_ret.csi_amp[i] = srsran_convert_amplitude_to_dB(cabsf(dci_decoder->ue_dl.chest_res.ce[0][0][i])); 
        }
       // // put the dci into the dci buffer
        pthread_mutex_lock(&dci_ready.mutex);
        dci_buffer[dci_ready.header] = dci_ret;
        dci_ready.header = (dci_ready.header + 1) % MAX_DCI_BUFFER;
        if(dci_ready.nof_dci < MAX_DCI_BUFFER){
            dci_ready.nof_dci++;
        }
        printf("TTI :%d ul_dci: %d dl_dci:%d nof_dci:%d\n", dci_ret.tti, dci_per_sub.nof_ul_dci, 
                                                dci_per_sub.nof_dl_dci, dci_ready.nof_dci);
        pthread_cond_signal(&dci_ready.cond);
        pthread_mutex_unlock(&dci_ready.mutex);
    }
    //srsran_ue_dl_free(&dci_decoder->ue_dl);
    printf("Close %d-th DCI decoder!\n",dci_decoder->decoder_idx);
    return NULL;
}
