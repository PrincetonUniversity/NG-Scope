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

extern bool go_exit;
extern bool logDL_flag;
extern bool logUL_flag;

extern uint16_t targetRNTI_const;

extern enum receiver_state state[MAX_NOF_USRP];

extern srslte_ue_sync_t	    ue_sync[MAX_NOF_USRP];
extern prog_args_t	    prog_args[MAX_NOF_USRP];
extern srslte_ue_list_t	    ue_list[MAX_NOF_USRP];
extern srslte_cell_t	    cell[MAX_NOF_USRP];
extern srslte_rf_t	    rf[MAX_NOF_USRP];

extern srslte_ue_cell_usage ue_cell_usage;

extern uint32_t system_frame_number[MAX_NOF_USRP];
extern int	free_order[MAX_NOF_USRP*4];

extern pthread_mutex_t mutex_dl_flag;
extern pthread_mutex_t mutex_ul_flag;

extern pthread_mutex_t mutex_exit;
extern pthread_mutex_t mutex_usage;
extern pthread_mutex_t mutex_free_order;

extern pthread_mutex_t mutex_cell[MAX_NOF_USRP];
extern pthread_mutex_t mutex_sfn[MAX_NOF_USRP];
extern pthread_mutex_t mutex_list[MAX_NOF_USRP];
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

    bool time_flag, dl_flag, ul_flag;
    time_flag   = false;
    dl_flag     = false;
    ul_flag     = false;

    int thd_id, usrp_idx, order;
    order       = dci_usrp_id.free_order;
    thd_id      = dci_usrp_id.dci_thd_id;
    usrp_idx    = dci_usrp_id.usrp_thd_id;

    FILE *FD_TIME, *FD_DCI_DL, *FD_DCI_UL;
    char fileName[30];
    sprintf(fileName, "./time_usrp_%d_dci_%d.txt",usrp_idx,thd_id);
    if(time_flag){
        FD_TIME = fopen(fileName,"w+");
    }
    sprintf(fileName, "./dci_dl_usrp_%d_dci_%d.txt",usrp_idx,thd_id);
    FD_DCI_DL = fopen(fileName,"w+");

    sprintf(fileName, "./dci_ul_usrp_%d_dci_%d.txt",usrp_idx,thd_id);
    FD_DCI_UL = fopen(fileName,"w+");

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
    srslte_active_ue_list_t active_ue_list;
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
        if(time_flag){
            gettimeofday(&t[1], NULL);
            timestamp1 =  t[1].tv_usec + t[1].tv_sec*1e6;
        }

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
        srslte_UeCell_ask_for_dci_token(&ue_cell_usage, usrp_idx, tti);
        targetRNTI = srslte_UeCell_get_targetRNTI(&ue_cell_usage);
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
                        pthread_mutex_lock( &mutex_list[usrp_idx]);
                        srslte_copy_active_ue_list(&ue_list[usrp_idx], &active_ue_list);
                        pthread_mutex_unlock( &mutex_list[usrp_idx]);

                        if(time_flag){
                            gettimeofday(&t[2], NULL);
                            timestamp2 =  t[2].tv_usec + t[2].tv_sec*1e6;
                            fprintf(FD_TIME,"%d %ld %ld %ld ",tti, timestamp1, timestamp2, timestamp2-timestamp1);
                            get_time_interval(t);
                            //elapsed1 = (float) t[0].tv_usec + t[0].tv_sec*1.0e+6f;
                            gettimeofday(&t[1], NULL);
                            timestamp1 =  t[1].tv_usec + t[1].tv_sec*1e6;
                        }

                        n = srslte_dci_decoder_yx(&ue_dl, &active_ue_list, &dci_msg_subframe, tti);
                        pthread_mutex_lock( &mutex_list[usrp_idx]);
                        if (n > 0) {
                            srslte_enqueue_subframe_msg(&dci_msg_subframe, &ue_list[usrp_idx], tti);
                            pthread_mutex_lock(&mutex_dl_flag);
                            dl_flag = logDL_flag;
                            pthread_mutex_unlock(&mutex_dl_flag);

                            pthread_mutex_lock(&mutex_ul_flag);
                            ul_flag = logUL_flag;
                            pthread_mutex_unlock(&mutex_ul_flag);
                            //dci_msg_list_display(dci_msg_subframe.downlink_msg, dci_msg_subframe.dl_msg_cnt);
                            //dci_msg_list_display(dci_msg_subframe.uplink_msg, dci_msg_subframe.ul_msg_cnt);
                            if(dl_flag && dci_msg_subframe.dl_msg_cnt > 0){
                                record_dci_msg_log(FD_DCI_DL, dci_msg_subframe.downlink_msg, dci_msg_subframe.dl_msg_cnt);
                            }
                            if(ul_flag && dci_msg_subframe.ul_msg_cnt > 0){
                                record_dci_msg_log(FD_DCI_UL, dci_msg_subframe.uplink_msg, dci_msg_subframe.ul_msg_cnt);
                            }
                            srslte_subframe_prb_status(&dci_msg_subframe, &sf_bw_usage, &ue_dci, targetRNTI, nof_prb);
                        }
                        srslte_update_ue_list_every_subframe(&ue_list[usrp_idx], tti);
                        pthread_mutex_unlock( &mutex_list[usrp_idx]);

                        rsrq = SRSLTE_VEC_EMA(srslte_chest_dl_get_rsrq(&ue_dl.chest), rsrq, 0.1f);
                        rsrp0 = SRSLTE_VEC_EMA(srslte_chest_dl_get_rsrp_port(&ue_dl.chest, 0), rsrp0, 0.05f);
                        rsrp1 = SRSLTE_VEC_EMA(srslte_chest_dl_get_rsrp_port(&ue_dl.chest, 1), rsrp1, 0.05f);
                        noise = SRSLTE_VEC_EMA(srslte_chest_dl_get_noise_estimate(&ue_dl.chest), noise, 0.05f);
                        if(time_flag){
                            gettimeofday(&t[2], NULL);
                            timestamp2 =  t[2].tv_usec + t[2].tv_sec*1e6;
                            fprintf(FD_TIME, "%ld %ld %ld\n", timestamp1, timestamp2, timestamp2-timestamp1);
                            get_time_interval(t);
                            //elapsed2 = (float) t[0].tv_usec + t[0].tv_sec*1.0e+6f;
                        }
                        nframes++;
                        if (isnan(rsrq)) { rsrq = 0; }
                        if (isnan(noise)) { noise = 0; }
                        if (isnan(rsrp0)) { rsrp0 = 0; }
                        if (isnan(rsrp1)) { rsrp1 = 0; }
                       ///* Print basic Parameters */
                        //if(0){
                        //    if(0){
                        //      PRINT_LINE("         RSRP: %+5.1f dBm | %+5.1f dBm", 10 * log10(rsrp0)+30, 10 * log10(rsrp1)+30);
                        //      PRINT_LINE("          SNR: %+5.1f dB | %+5.1f dB", 10 * log10(rsrp0 / noise), 10 * log10(rsrp1 / noise));
                        //      PRINT_LINE(" TIME ELAPSED: %+5.1f us | %+5.1f us", elapsed1, elapsed2);
                        //      PRINT_LINE("       THD ID: %d", thd_id);
                        //      PRINT_LINE("");
                        //      //PRINT_LINE_RESET_CURSOR();
                        //    }else{
                        //      printf("USRP: %d Thead ID: %d -- RSRP: %+5.1f dBm | SNR: %+5.1f dB | TIME ELAPSED: %+5.1f us %+5.1f\n",
                        //          usrp_idx, thd_id, 10 * log10(rsrp0)+30,10 * log10(rsrp0 / noise), elapsed1, elapsed2);
                        //    }
                        //}
                    }
                    break;
            }

        } else if (ret == 0) {
            pthread_mutex_lock( &mutex_ue_sync[usrp_idx]);
            printf("USRP:%d-THD-%d  Finding PSS... Peak: %8.1f, FrameCnt: %d, State: %d\n",usrp_idx, thd_id,
                        srslte_sync_get_peak_value(&ue_sync[usrp_idx].sfind),
                        ue_sync[usrp_idx].frame_total_cnt, ue_sync[usrp_idx].state);
            pthread_mutex_unlock( &mutex_ue_sync[usrp_idx]);
        }
        pthread_mutex_lock( &mutex_usage );
        srslte_UeCell_return_dci_token(&ue_cell_usage, usrp_idx, tti, &ue_dci, &sf_bw_usage);
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
    if(time_flag){
        fclose(FD_TIME);
    }
    fclose(FD_DCI_DL);
    fclose(FD_DCI_UL);

    printf("Bye -- USRP:%d DCI-Thread:%d\n", usrp_idx, thd_id);
    pthread_exit(NULL);
}


