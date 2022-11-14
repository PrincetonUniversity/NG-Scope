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

#include "srsgui/srsgui.h"

#include "srsran/srsran.h"
#include "ngscope/hdr/dciLib/radio.h"
#include "ngscope/hdr/dciLib/task_scheduler.h"
#include "ngscope/hdr/dciLib/dci_decoder.h"
#include "ngscope/hdr/dciLib/phich_decoder.h"
#include "ngscope/hdr/dciLib/time_stamp.h"
#include "ngscope/hdr/dciLib/status_plot.h"
#include "ngscope/hdr/dciLib/thread_exit.h"

extern bool                 go_exit;

extern ngscope_sf_buffer_t  sf_buffer[MAX_NOF_RF_DEV][MAX_NOF_DCI_DECODER];
extern bool                 sf_token[MAX_NOF_RF_DEV][MAX_NOF_DCI_DECODER];
extern pthread_mutex_t      token_mutex[MAX_NOF_RF_DEV]; 

extern dci_ready_t         dci_ready;
extern ngscope_status_buffer_t    dci_buffer[MAX_DCI_BUFFER];

// For decoding phich
extern pend_ack_list       ack_list;
extern pthread_mutex_t     ack_mutex;

extern bool dci_decoder_up[MAX_NOF_RF_DEV][MAX_NOF_DCI_DECODER];
extern bool task_scheduler_up[MAX_NOF_RF_DEV];

pthread_mutex_t dci_plot_mutex[MAX_NOF_RF_DEV] = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER,
					 								PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER};
pthread_cond_t 	dci_plot_cond[MAX_NOF_RF_DEV] = {PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER,
													PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER};

cf_t* pdcch_buf[MAX_NOF_RF_DEV];
float csi_amp[MAX_NOF_RF_DEV][110 * 15 * 2048];

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

	// test: enable cif 
	//dci_decoder->ue_dl_cfg.cfg.dci.cif_enabled = true;
	//dci_decoder->ue_dl_cfg.cfg.dci.cif_present = true;
	dci_decoder->ue_dl_cfg.cfg.dci.multiple_csi_request_enabled = true;

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
                            //ngscope_dci_msg_t       dci_array[][MAX_CANDIDATES_ALL],
                            //srsran_dci_location_t   dci_location[MAX_CANDIDATES_ALL],
                            ngscope_dci_per_sub_t*  dci_per_sub)
{
    uint32_t tti = sfn * 10 + sf_idx;

    bool decode_pdsch = false;
    uint16_t targetRNTI = dci_decoder->prog_args.rnti;  

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

    //decode_pdsch = false;
//	uint64_t t1=0, t2=0;

	//char fileName[100];
	//sprintf(fileName,"decoder_%d.txt", dci_decoder->decoder_idx);
	//FILE* fd = fopen(fileName,"a+");

//	if(dci_decoder->decoder_idx == 0){
//    	t1 = timestamp_us();        
//	}

    // Now decode the PDSCH
    if(decode_pdsch){
        uint32_t tm = 3;
        dci_decoder->dl_sf.tti                             = tti;
        dci_decoder->dl_sf.sf_type                         = SRSRAN_SF_NORM; //Ingore the MBSFN
        dci_decoder->ue_dl_cfg.cfg.tm                      = (srsran_tm_t)tm;
        dci_decoder->ue_dl_cfg.cfg.pdsch.use_tbs_index_alt = true;

    	n = srsran_ngscope_search_all_space_array_yx(&dci_decoder->ue_dl, &dci_decoder->dl_sf, \
								&dci_decoder->ue_dl_cfg, &dci_decoder->pdsch_cfg, dci_per_sub, targetRNTI);

		//printf("end of decoding!\n");
        //t2 = timestamp_us();        
        //printf("time_spend:%ld (us)\n", t2-t1);
        //printf("decoder:%d finish decoding. time_spend:%ld (us)\n", dci_decoder->decoder_idx, t2-t1);
    } 
//	if(dci_decoder->decoder_idx == 0){
//    	t2 = timestamp_us();        
//    	printf("time_spend:%ld (us)\n", t2-t1);
//	}

	//printf("going to print t2\n");
	//printf("%ld\n", t2);
    //printf("time_spend:%ld (us)\n", t2-t1);
	//fprintf(fd, "%d\t%ld\n", tti, tdif);
	//fclose(fd);

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
        for(int j=0; j<MAX_CANDIDATES_ALL; j++){
            ZERO_OBJECT(dci_array[i][j]);
        }
    }
   	 
    for(int i=0; i<MAX_CANDIDATES_ALL; i++){
		ZERO_OBJECT(dci_location[i]);
	}

    dci_per_sub->nof_dl_dci = 0;
    dci_per_sub->nof_ul_dci = 0;
    for(int i=0; i<MAX_DCI_PER_SUB; i++){
        ZERO_OBJECT(dci_per_sub->dl_msg[i]); 
        ZERO_OBJECT(dci_per_sub->ul_msg[i]); 
    }

    return;
}
void empty_dci_persub(ngscope_dci_per_sub_t*  dci_per_sub){
    dci_per_sub->nof_dl_dci = 0;
    dci_per_sub->nof_ul_dci = 0;
    for(int i=0; i<MAX_DCI_PER_SUB; i++){
        ZERO_OBJECT(dci_per_sub->dl_msg[i]); 
        ZERO_OBJECT(dci_per_sub->ul_msg[i]); 
    }
    return;
}
    
void* dci_decoder_thread(void* p){
	ngscope_dci_decoder_t* dci_decoder 	= (ngscope_dci_decoder_t* )p;

	int decoder_idx = dci_decoder->decoder_idx;
    int rf_idx     	= dci_decoder->prog_args.rf_index;


	printf("decoder idx :%d \n", decoder_idx);
    ngscope_dci_per_sub_t   dci_per_sub; 
    ngscope_status_buffer_t dci_ret;
//
//    pthread_mutex_lock(&sf_buffer[decoder_idx].sf_mutex);
//	dci_decoder_init(&dci_decoder, prog_args, &cell, \
//                           sf_buffer[decoder_idx].IQ_buffer, rx_softbuffers, decoder_idx);
//    pthread_mutex_unlock(&sf_buffer[decoder_idx].sf_mutex);

	int nof_pdcch_sample = 36 * dci_decoder->ue_dl.pdcch.nof_cce[0];
	int nof_prb = dci_decoder->cell.nof_prb;
	int sz = srsran_symbol_sz(nof_prb);
	bool enable_plot = !dci_decoder->prog_args.disable_plots;	
    pthread_t plot_thread;
	if(enable_plot){
		if(decoder_idx == 0){
			pdcch_buf[rf_idx] = srsran_vec_cf_malloc(36*80);
			srsran_vec_cf_zero(pdcch_buf[rf_idx], nof_pdcch_sample);

			decoder_plot_t decoder_plot;
			decoder_plot.decoder_idx 		= decoder_idx;
			decoder_plot.nof_pdcch_sample 	= nof_pdcch_sample;
			decoder_plot.nof_prb 			= nof_prb;
			decoder_plot.size 				= sz;
			plot_init_pdcch_thread(&plot_thread, &decoder_plot);
		}
	}
//    uint64_t t1=0, t2=0, t3=0, t4=0;  
	char fileName[100];
	sprintf(fileName,"decoder_%d.txt", decoder_idx);
	FILE* fd = fopen(fileName,"w+");

    printf("Decoder thread idx:%d\n\n\n",decoder_idx);

    while(!go_exit){
                
		//empty_dci_array(dci_array, dci_location, &dci_per_sub);
		empty_dci_persub(&dci_per_sub);

//--->  Lock the buffer
        pthread_mutex_lock(&sf_buffer[rf_idx][decoder_idx].sf_mutex);
    
        // We release the token in the last minute just before the waiting of the condition signal 
        pthread_mutex_lock(&token_mutex[rf_idx]);
        if(sf_token[rf_idx][decoder_idx] == true){
            sf_token[rf_idx][decoder_idx] = false;
        }
        pthread_mutex_unlock(&token_mutex[rf_idx]);

//--->  Wait the signal 
        //printf("%d-th decoder is waiting for conditional signal!\n", dci_decoder->decoder_idx);
		//t1 = timestamp_us();        
        pthread_cond_wait(&sf_buffer[rf_idx][decoder_idx].sf_cond, 
                          &sf_buffer[rf_idx][decoder_idx].sf_mutex);
    
        uint32_t sfn    = sf_buffer[rf_idx][decoder_idx].sfn;
        uint32_t sf_idx = sf_buffer[rf_idx][decoder_idx].sf_idx;

        uint32_t tti    = sfn * 10 + sf_idx;
		bool   empty_sf = sf_buffer[rf_idx][decoder_idx].empty_sf;
        //printf("%d-th decoder Get the conditional signal! empty:%d\n", dci_decoder->decoder_idx, empty_sf);
    
 
		fprintf(fd,"%d\n", tti);

        //printf("decoder:%d Get the signal! sfn:%d sf_idx:%d tti:%d\n", decoder_idx, sfn, sf_idx, sfn * 10 + sf_idx);
		// We only decode when the subframe is not empty
		if(empty_sf){
			pthread_mutex_unlock(&sf_buffer[rf_idx][decoder_idx].sf_mutex);	
		}else{
			//usleep(1000);
			dci_decoder_decode(dci_decoder, sf_idx, sfn, &dci_per_sub);
			//t3 = timestamp_us();        
	//--->  Unlock the buffer
			pthread_mutex_unlock(&sf_buffer[rf_idx][decoder_idx].sf_mutex);	

			if(enable_plot){
				if(decoder_idx == 0){
					pthread_mutex_lock(&dci_plot_mutex[rf_idx]);    
					srsran_vec_cf_copy(pdcch_buf[rf_idx], dci_decoder->ue_dl.pdcch.d, nof_pdcch_sample);

					if (sz > 0) {
						srsran_vec_f_zero(&(csi_amp[rf_idx][0]), sz);
					}
					int g = (sz - 12 * nof_prb) / 2;
					for (int i = 0; i < 12 * nof_prb; i++) {
						csi_amp[rf_idx][g + i] = srsran_convert_amplitude_to_dB(cabsf(dci_decoder->ue_dl.chest_res.ce[0][0][i]));
						if (isinf(csi_amp[rf_idx][g + i])) {
							csi_amp[rf_idx][g + i] = -80;
						}
					}
					pthread_cond_signal(&dci_plot_cond[rf_idx]);
					pthread_mutex_unlock(&dci_plot_mutex[rf_idx]);    
				}
			}
		}
        //t4 = timestamp_us();        

        //printf("End of decoding decoder_idx:%d sfn:%d sf_idx:%d tti:%d\n", 
        //                    dci_decoder->decoder_idx, sfn, sf_idx, sfn * 10 + sf_idx);

//        dci_decoder_phich_decode(dci_decoder, tti, &dci_per_sub);

        dci_ret.dci_per_sub  = dci_per_sub;
        dci_ret.tti          = sfn *10 + sf_idx;
        dci_ret.cell_idx     = rf_idx;

        // put the dci into the dci buffer
        pthread_mutex_lock(&dci_ready.mutex);
        dci_buffer[dci_ready.header] = dci_ret;
        dci_ready.header = (dci_ready.header + 1) % MAX_DCI_BUFFER;
        if(dci_ready.nof_dci < MAX_DCI_BUFFER){
            dci_ready.nof_dci++;
        }else{
			printf("DCI-buffer between decoder and status tracker is full! Considering increase its side!\n");
		}

        //printf("TTI :%d ul_dci: %d dl_dci:%d nof_dci:%d\n", dci_ret.tti, dci_per_sub.nof_ul_dci, 
        //                                        dci_per_sub.nof_dl_dci, dci_ready.nof_dci);
        pthread_cond_signal(&dci_ready.cond);
        pthread_mutex_unlock(&dci_ready.mutex);

		//fprintf(fd, "%d\t%ld\t%ld\n",tti, t4-t1, t3-t2);
    }
	//fprintf(fd, "%ld\t%ld\n", t4-t1, t3-t2);

	// we free the ue dl in task scheduler
    //srsran_ue_dl_free(&dci_decoder->ue_dl);

	wait_for_decoder_ready_to_close(rf_idx, decoder_idx);
	// free the ue dl and the related buffer
    //srsran_ue_dl_free(&dci_decoder->ue_dl);
	
    printf("Going to Close %d-th DCI decoder!\n",decoder_idx);
	if(enable_plot){
		if(decoder_idx == 0){
			pthread_cond_signal(&dci_plot_cond[rf_idx]);
			pthread_join(plot_thread, NULL);
			free(pdcch_buf[rf_idx]);
		}
	}
	fclose(fd);
	dci_decoder_up[rf_idx][decoder_idx] = false;

    printf("%d-th RF-DEV %d-th DCI decoder CLOSED!\n",rf_idx, decoder_idx);

    return NULL;
}

