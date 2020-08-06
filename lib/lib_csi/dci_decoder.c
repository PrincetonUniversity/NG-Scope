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

#include "srslte/phy/rf/rf.h"
#include "srslte/phy/rf/rf_utils.h"

#include "dci_decoder.h"
#include "arg_parser.h"
#include "ue_cell_status.h"

#include "srsgui/srsgui.h"

#define cesymb(i) csi[SRSLTE_RE_IDX(nof_prb,i,0)]

extern bool go_exit;

extern uint16_t targetRNTI_const;

extern enum receiver_state state[MAX_NOF_USRP];

extern srslte_ue_sync_t	    ue_sync[MAX_NOF_USRP];
extern prog_args_t	    prog_args[MAX_NOF_USRP];
extern srslte_cell_t	    cell[MAX_NOF_USRP];
extern srslte_rf_t	    rf[MAX_NOF_USRP];

extern srslte_ue_cell_usage ue_cell_usage;

extern uint32_t system_frame_number[MAX_NOF_USRP];
extern int	free_order[MAX_NOF_USRP*4];

extern pthread_mutex_t mutex_csi;
extern cf_t csi[1200];
extern cf_t csi_mat[MAX_NOF_USRP][1200];

extern pthread_mutex_t mutex_exit;
extern pthread_mutex_t mutex_usage;
extern pthread_mutex_t mutex_free_order;

extern pthread_mutex_t mutex_cell[MAX_NOF_USRP];
extern pthread_mutex_t mutex_sfn[MAX_NOF_USRP];
extern pthread_mutex_t mutex_ue_sync[MAX_NOF_USRP];

bool check_free_order(int* array, int arraySize, int order){
    bool time_to_free=true;
    for(int i=order;i<arraySize;i++){
        if(array[i] == 1){
            time_to_free = false;
        }
    }
    return time_to_free;
}

void* dci_decoder(void* p){
    int64_t sf_cnt;
    int n, ret;
    uint8_t bch_payload[SRSLTE_BCH_PAYLOAD_LEN];
    int sfn_offset;
    uint32_t nof_prb;
    uint32_t sfn = 0;
    uint16_t targetRNTI = targetRNTI_const;
    dci_usrp_id_t dci_usrp_id = (dci_usrp_id_t)*(dci_usrp_id_t *)p;

    int thd_id, usrp_idx, order;
    order       = dci_usrp_id.free_order;
    thd_id      = dci_usrp_id.dci_thd_id;
    usrp_idx    = dci_usrp_id.usrp_thd_id;

    char fileName[50];

//    sprintf(fileName, "./csi_dl_usrp_%d_dci_%d_amp.txt",usrp_idx,thd_id);
//    FILE *FD_CSI_AMP = fopen(fileName,"w+");
//
//    sprintf(fileName, "./csi_dl_usrp_%d_dci_%d_phase.txt",usrp_idx,thd_id);
//    FILE *FD_CSI_PHA = fopen(fileName,"w+");
//

    pthread_mutex_lock(&mutex_cell[usrp_idx]);
    nof_prb = cell[usrp_idx].nof_prb;
    pthread_mutex_unlock(&mutex_cell[usrp_idx]);


    cf_t *sf_buffer[SRSLTE_MAX_PORTS] = {NULL};
    for (int i=0;i<prog_args[usrp_idx].rf_nof_rx_ant;i++) {
        sf_buffer[i] = (cf_t *) srslte_vec_malloc(3*sizeof(cf_t)*SRSLTE_SF_LEN_PRB(nof_prb));
    }

    srslte_ue_mib_t ue_mib;
    srslte_ue_dl_t ue_dl;

    if (srslte_ue_mib_init(&ue_mib, sf_buffer, nof_prb)) {
        fprintf(stderr, "Error initaiting UE MIB decoder\n");
        exit(-1);
    }
    if (srslte_ue_dl_init(&ue_dl, sf_buffer, nof_prb, prog_args[usrp_idx].rf_nof_rx_ant)) {
        fprintf(stderr, "Error initiating UE downlink processing module\n");
        exit(-1);
    }

    pthread_mutex_lock(&mutex_cell[usrp_idx]);
    if (srslte_ue_mib_set_cell(&ue_mib, cell[usrp_idx])) {
        fprintf(stderr, "Error initaiting UE MIB decoder\n");
        exit(-1);
    }
    if (srslte_ue_dl_set_cell(&ue_dl, cell[usrp_idx])) {
        fprintf(stderr, "Error initiating UE downlink processing module\n");
        exit(-1);
    }

    pthread_mutex_unlock(&mutex_cell[usrp_idx]);

    srslte_chest_dl_cfo_estimate_enable(&ue_dl.chest, prog_args[usrp_idx].enable_cfo_ref, 1023);
    srslte_chest_dl_average_subframe(&ue_dl.chest, prog_args[usrp_idx].average_subframe);


    srslte_pbch_decode_reset(&ue_mib.pbch);
    /* Initialize subframe counter */
    sf_cnt = 0;

    // Variables for measurements
    uint32_t nframes=0;
    float rsrp0=0.0, rsrp1=0.0, rsrq=0.0, noise=0.0;
    float sinr[SRSLTE_MAX_LAYERS][SRSLTE_MAX_CODEBOOKS];
    bool decode_pdsch = false;
    for (int i = 0; i < SRSLTE_MAX_LAYERS; i++) {
        bzero(sinr[i], sizeof(float)*SRSLTE_MAX_CODEBOOKS);
    }

    struct timeval t[3];
    srslte_dci_subframe_t dci_msg_subframe;
    srslte_subframe_bw_usage sf_bw_usage;
    srslte_dci_msg_paws ue_dci;

    INFO("\nEntering main loop...\n\n");
    /* Main loop */
    //float elapsed1=0, elapsed2=0;
    long timestamp1, timestamp2;
    while (true) {
        //PRINT_LINE_INIT();
        pthread_mutex_lock( &mutex_exit);
        if(go_exit){
            pthread_mutex_unlock( &mutex_exit);
            break;
        }
        pthread_mutex_unlock( &mutex_exit);

        dci_msg_subframe.dl_msg_cnt = 0;
        dci_msg_subframe.ul_msg_cnt = 0;

        bzero(&sf_bw_usage, sizeof(srslte_subframe_bw_usage));
        bzero(&ue_dci, sizeof(srslte_dci_msg_paws));

        pthread_mutex_lock( &mutex_ue_sync[usrp_idx]);
        ret = srslte_ue_sync_zerocopy_multi(&ue_sync[usrp_idx], sf_buffer);
        uint32_t sfidx = srslte_ue_sync_get_sfidx(&ue_sync[usrp_idx]);
        pthread_mutex_unlock( &mutex_ue_sync[usrp_idx]);

        pthread_mutex_lock( &mutex_sfn[usrp_idx]);
        sfn = system_frame_number[usrp_idx];
        if (sfidx == 9) {
            system_frame_number[usrp_idx]++;
            if(system_frame_number[usrp_idx] == 1024){
                system_frame_number[usrp_idx] = 0;
            }
        }
        pthread_mutex_unlock( &mutex_sfn[usrp_idx]);

        uint32_t tti   = sfn*10 + sfidx;

        pthread_mutex_lock(&mutex_usage);
        cell_status_ask_for_dci_token(&ue_cell_usage, usrp_idx, tti);
        //targetRNTI = srslte_UeCell_get_targetRNTI(&ue_cell_usage);
        pthread_mutex_unlock(&mutex_usage);


        if (ret < 0) {
            fprintf(stderr, "Error calling srslte_ue_sync_work()\n");
        }
        /* srslte_ue_sync_get_buffer returns 1 if successfully read 1 aligned subframe */
        if (ret == 1) {
            if( (sfn % 1000 == 0) && (sfidx == 0) ){
                n = srslte_ue_mib_decode(&ue_mib, bch_payload, NULL, &sfn_offset);
                if (n == SRSLTE_UE_MIB_FOUND) {
                    pthread_mutex_lock(&mutex_cell[usrp_idx]);
                    srslte_pbch_mib_unpack(bch_payload, &cell[usrp_idx], &sfn);
                    //srslte_cell_fprint(stdout, &cell, sfn);
                    pthread_mutex_unlock(&mutex_cell[usrp_idx]);

                    //printf("Decoded MIB. SFN: %d, offset: %d\n", sfn, sfn_offset);
                    sfn = (sfn + sfn_offset)%1024;
                    pthread_mutex_lock( &mutex_sfn[usrp_idx]);
                    system_frame_number[usrp_idx] = sfn;
                    pthread_mutex_unlock( &mutex_sfn[usrp_idx]);
                }
            }
            switch (state[usrp_idx]) {
                case DECODE_MIB:
                    if (sfidx == 0) {
                        n = srslte_ue_mib_decode(&ue_mib, bch_payload, NULL, &sfn_offset);
                        if (n < 0) {
                            fprintf(stderr, "Error decoding UE MIB\n");
                            exit(-1);
                        } else if (n == SRSLTE_UE_MIB_FOUND) {
                            pthread_mutex_lock(&mutex_cell[usrp_idx]);
                            srslte_pbch_mib_unpack(bch_payload, &cell[usrp_idx], &sfn);
                            srslte_cell_fprint(stdout, &cell[usrp_idx], sfn);
                            pthread_mutex_unlock(&mutex_cell[usrp_idx]);

                            printf("Decoded MIB. SFN: %d, offset: %d\n", sfn, sfn_offset);
                            sfn = (sfn + sfn_offset)%1024;
                            state[usrp_idx] = DECODE_PDSCH;
                            pthread_mutex_lock( &mutex_sfn[usrp_idx]);
                            system_frame_number[usrp_idx] = sfn;
                            pthread_mutex_unlock( &mutex_sfn[usrp_idx]);
                        }
                    }
                    break;
                case DECODE_PDSCH:
                    decode_pdsch = true;
                    if (decode_pdsch) {
			uint32_t cfi;	
			srslte_ue_dl_decode_fft_estimate_mbsfn(&ue_dl, tti%10, &cfi, SRSLTE_SF_NORM);
		//	
		//	if(tti%1 == 0){	
		//	    
		//	    int nof_subcarrier = nof_prb * SRSLTE_NRE;
		//	    cf_t* csi_ptr	= ue_dl.ce_m[0][0];
		//	    
		//	    // for plotting 
		//	    //pthread_mutex_lock( &mutex_csi);
		//	    //for(int i=0;i<nof_subcarrier;i++){
		//	    //    //csi[i] = csi_ptr[i];
		//	    //    csi_mat[usrp_idx][i] = csi_ptr[i];
		//	    //    //csi[i] = creal(csi_ptr[i]) + cimag(csi_ptr[i]) * I;
		//	    //}
		//	    //pthread_mutex_unlock( &mutex_csi);


		//	    fprintf(FD_CSI_AMP, "%d ",tti); 
		//	    fprintf(FD_CSI_PHA, "%d ",tti); 
		//	    for(int idx=0;idx< nof_subcarrier;idx++){
		//	        cf_t csi_v	= csi_ptr[idx];
		//	        fprintf(FD_CSI_AMP, "%f ",cabsf(csi_v));
		//	        fprintf(FD_CSI_PHA, "%f ",cargf(csi_v));
		//	    }
		//	    fprintf(FD_CSI_AMP,"\n");
		//	    fprintf(FD_CSI_PHA,"\n");
		//	}	
		//	
			//fprintf(FD_CSI, "%d ",tti); 
			//for(int idx=0;idx< nof_subcarrier;idx++){
			//    csi[idx]	= csi_ptr[idx];
			//    fprintf(FD_CSI, "%f ",absv);
			//}
			//fprintf(FD_CSI,"\n");
                        nframes++;
                                            
		    }
                    break;
            }

        } else if (ret == 0) {
            //pthread_mutex_lock( &mutex_ue_sync[usrp_idx]);
            //printf("USRP:%d-THD-%d  Finding PSS... Peak: %8.1f, FrameCnt: %d, State: %d\n",usrp_idx, thd_id,
            //            srslte_sync_get_peak_value(&ue_sync[usrp_idx].sfind),
            //            ue_sync[usrp_idx].frame_total_cnt, ue_sync[usrp_idx].state);
            //pthread_mutex_unlock( &mutex_ue_sync[usrp_idx]);
        }
        pthread_mutex_lock( &mutex_usage );
        cell_status_return_dci_token(&ue_cell_usage, &ue_dl, usrp_idx, tti);
        pthread_mutex_unlock( &mutex_usage );

        sf_cnt++;
    } // Main loop

    while(true){
        pthread_mutex_lock( &mutex_free_order );
        if( check_free_order(free_order, MAX_NOF_USRP*4, order)){
            pthread_mutex_unlock( &mutex_free_order );
            break;
        }else{
            pthread_mutex_unlock( &mutex_free_order );
            usleep(2e5);
        }
    }

    srslte_ue_dl_free(&ue_dl);
    srslte_ue_mib_free(&ue_mib);

    if(order > 1){
        pthread_mutex_lock( &mutex_free_order );
        free_order[order-1] = 0;
        pthread_mutex_unlock( &mutex_free_order );
    }

    for (int i = 0; i < prog_args[usrp_idx].rf_nof_rx_ant; i++) {
        if (sf_buffer[i]) {
            free(sf_buffer[i]);
        }
    }

    //fclose(FD_CSI_AMP); 
    //fclose(FD_CSI_PHA); 

    printf("Bye -- USRP:%d DCI-Thread:%d\n", usrp_idx, thd_id);
    pthread_exit(NULL);
}


