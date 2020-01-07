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
#include "srslte/phy/ue/rnti_prune.h"

#define ENABLE_AGC_DEFAULT
#define MAX_NOF_USRP
#include "srslte/phy/rf/rf.h"
#include "srslte/phy/rf/rf_utils.h"

#include "dci_decode_main.h"

extern bool go_exit;
extern enum receiver_state state;
extern srslte_ue_sync_t ue_sync;
extern prog_args_t prog_args;
extern srslte_ue_list_t ue_list;
extern srslte_cell_t cell;
extern srslte_rf_t rf;
extern uint32_t system_frame_number;

pthread_mutex_t mutex_exit = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cell = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_sfn = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_list = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_ue_sync = PTHREAD_MUTEX_INITIALIZER;


/* Useful macros for printing lines which will disappear */

#define PRINT_LINE_INIT() int this_nof_lines = 0; static int prev_nof_lines = 0
#define PRINT_LINE(_fmt, ...) printf("\033[K" _fmt "\n", ##__VA_ARGS__); this_nof_lines++
#define PRINT_LINE_RESET_CURSOR() printf("\033[%dA", this_nof_lines); prev_nof_lines = this_nof_lines
#define PRINT_LINE_ADVANCE_CURSOR() printf("\033[%dB", prev_nof_lines + 1)
void args_default(prog_args_t *args) {
  args->disable_plots = false;
  args->disable_plots_except_constellation = false;
  args->nof_subframes = -1;
  args->rnti = SRSLTE_SIRNTI;
  args->force_N_id_2 = -1; // Pick the best
  args->nof_thread = 1; // Pick the best
  args->input_file_name = NULL;
  args->disable_cfo = false;
  args->time_offset = 0;
  args->file_nof_prb = 25;
  args->file_nof_ports = 1;
  args->file_cell_id = 0;
  args->file_offset_time = 0;
  args->file_offset_freq = 0;
  args->rf_args = "";
  args->rf_freq = -1.0;
  args->rf_nof_rx_ant = 1;
  args->enable_cfo_ref = false;
  args->average_subframe = false;
#ifdef ENABLE_AGC_DEFAULT
  args->rf_gain = -1.0;
#else
  args->rf_gain = 50.0;
#endif
  args->net_port = -1;
  args->net_address = "127.0.0.1";
  args->net_port_signal = -1;
  args->net_address_signal = "127.0.0.1";
  args->decimate = 0;
  args->cpu_affinity = -1;
  args->mbsfn_area_id = -1;
  args->non_mbsfn_region = 2;
  args->mbsfn_sf_mask = 32;
}

void usage(prog_args_t *args, char *prog) {
  printf("Usage: %s [agpPoOcildFRDnruMNv] -f rx_frequency (in Hz) | -i input_file\n", prog);
#ifndef DISABLE_RF
  printf("\t-a RF args [Default %s]\n", args->rf_args);
  printf("\t-A Number of RX antennas [Default %d]\n", args->rf_nof_rx_ant);
#ifdef ENABLE_AGC_DEFAULT
  printf("\t-g RF fix RX gain [Default AGC]\n");
#else
  printf("\t-g Set RX gain [Default %.1f dB]\n", args->rf_gain);
#endif
#else
  printf("\t   RF is disabled.\n");
#endif
  printf("\t-i input_file [Default use RF board]\n");
  printf("\t-e number of decoding threads [Default %d]\n", args->nof_thread);
  printf("\t-o offset frequency correction (in Hz) for input file [Default %.1f Hz]\n", args->file_offset_freq);
  printf("\t-O offset samples for input file [Default %d]\n", args->file_offset_time);
  printf("\t-p nof_prb for input file [Default %d]\n", args->file_nof_prb);
  printf("\t-P nof_ports for input file [Default %d]\n", args->file_nof_ports);
  printf("\t-c cell_id for input file [Default %d]\n", args->file_cell_id);
  printf("\t-r RNTI in Hex [Default 0x%x]\n",args->rnti);
  printf("\t-l Force N_id_2 [Default best]\n");
  printf("\t-C Disable CFO correction [Default %s]\n", args->disable_cfo?"Disabled":"Enabled");
  printf("\t-F Enable RS-based CFO correction [Default %s]\n", !args->enable_cfo_ref?"Disabled":"Enabled");
  printf("\t-R Average channel estimates on 1 ms [Default %s]\n", !args->average_subframe?"Disabled":"Enabled");
  printf("\t-t Add time offset [Default %d]\n", args->time_offset);
  printf("\t plots are disabled. Graphics library not available\n");
  printf("\t-y set the cpu affinity mask [Default %d] \n  ",args->cpu_affinity);
  printf("\t-n nof_subframes [Default %d]\n", args->nof_subframes);
  printf("\t-s remote UDP port to send input signal (-1 does nothing with it) [Default %d]\n", args->net_port_signal);
  printf("\t-S remote UDP address to send input signal [Default %s]\n", args->net_address_signal);
  printf("\t-u remote TCP port to send data (-1 does nothing with it) [Default %d]\n", args->net_port);
  printf("\t-U remote TCP address to send data [Default %s]\n", args->net_address);
  printf("\t-M MBSFN area id [Default %d]\n", args->mbsfn_area_id);
  printf("\t-N Non-MBSFN region [Default %d]\n", args->non_mbsfn_region);
  printf("\t-v [set srslte_verbose to debug, default none]\n");
}

void parse_args(prog_args_t *args, int argc, char **argv) {
  int opt;
  args_default(args);
  while ((opt = getopt(argc, argv, "aeAoglipPcOCtdDFRnvrfuUsSZyWMNB")) != -1) {
    switch (opt) {
    case 'i':
      args->input_file_name = argv[optind];
      break;
    case 'p':
      args->file_nof_prb = atoi(argv[optind]);
      break;
    case 'P':
      args->file_nof_ports = atoi(argv[optind]);
      break;
    case 'o':
      args->file_offset_freq = atof(argv[optind]);
      break;
    case 'O':
      args->file_offset_time = atoi(argv[optind]);
      break;
    case 'c':
      args->file_cell_id = atoi(argv[optind]);
      break;
    case 'a':
      args->rf_args = argv[optind];
      break;
    case 'A':
      args->rf_nof_rx_ant = atoi(argv[optind]);
      break;
    case 'g':
      args->rf_gain = atof(argv[optind]);
      break;
    case 'C':
      args->disable_cfo = true;
      break;
    case 'F':
      args->enable_cfo_ref = true;
      break;

    case 'R':
      args->average_subframe = true;
      break;
    case 't':
      args->time_offset = atoi(argv[optind]);
      break;
    case 'f':
      args->rf_freq = strtod(argv[optind], NULL);
      break;
    case 'n':
      args->nof_subframes = atoi(argv[optind]);
      break;
    case 'r':
      args->rnti = strtol(argv[optind], NULL, 16);
      break;
    case 'l':
      args->force_N_id_2 = atoi(argv[optind]);
      break;
    case 'e':
      args->nof_thread = atoi(argv[optind]);
      break;
    case 'u':
      args->net_port = atoi(argv[optind]);
      break;
    case 'U':
      args->net_address = argv[optind];
      break;
    case 's':
      args->net_port_signal = atoi(argv[optind]);
      break;
    case 'S':
      args->net_address_signal = argv[optind];
      break;
    case 'd':
      args->disable_plots = true;
      break;
    case 'D':
      args->disable_plots_except_constellation = true;
      break;
    case 'v':
      srslte_verbose++;
      args->verbose = srslte_verbose;
      break;
    case 'Z':
      args->decimate = atoi(argv[optind]);
      break;
    case 'y':
      args->cpu_affinity = atoi(argv[optind]);
      break;
   case 'M':
      args->mbsfn_area_id = atoi(argv[optind]);
      break;
    case 'N':
      args->non_mbsfn_region = atoi(argv[optind]);
      break;
    case 'B':
      args->mbsfn_sf_mask = atoi(argv[optind]);
      break;
    default:
      usage(args, argv[0]);
      exit(-1);
    }
  }
  if (args->rf_freq < 0 && args->input_file_name == NULL) {
    usage(args, argv[0]);
    exit(-1);
  }
}


void dci_decode_main(){
    int64_t sf_cnt;
    int n, ret;
    uint8_t bch_payload[SRSLTE_BCH_PAYLOAD_LEN];
    int sfn_offset;
    uint32_t nof_prb;
    uint32_t sfn = 0;

    pthread_mutex_lock(&mutex_cell);
    nof_prb = cell.nof_prb;
    pthread_mutex_unlock(&mutex_cell);


    cf_t *sf_buffer[SRSLTE_MAX_PORTS] = {NULL};
    for (int i=0;i<prog_args.rf_nof_rx_ant;i++) {
        sf_buffer[i] = srslte_vec_malloc(3*sizeof(cf_t)*SRSLTE_SF_LEN_PRB(nof_prb));
    }

    srslte_ue_mib_t ue_mib;
    srslte_ue_dl_t ue_dl;

    if (srslte_ue_mib_init(&ue_mib, sf_buffer, nof_prb)) {
        fprintf(stderr, "Error initaiting UE MIB decoder\n");
        exit(-1);
    }
    if (srslte_ue_dl_init(&ue_dl, sf_buffer, nof_prb, prog_args.rf_nof_rx_ant)) {
        fprintf(stderr, "Error initiating UE downlink processing module\n");
        exit(-1);
    }

    pthread_mutex_lock(&mutex_cell);
    if (srslte_ue_mib_set_cell(&ue_mib, cell)) {
        fprintf(stderr, "Error initaiting UE MIB decoder\n");
        exit(-1);
    }
    if (srslte_ue_dl_set_cell(&ue_dl, cell)) {
        fprintf(stderr, "Error initiating UE downlink processing module\n");
        exit(-1);
    }

    pthread_mutex_unlock(&mutex_cell);

    srslte_chest_dl_cfo_estimate_enable(&ue_dl.chest, prog_args.enable_cfo_ref, 1023);
    srslte_chest_dl_average_subframe(&ue_dl.chest, prog_args.average_subframe);


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
    INFO("\nEntering main loop...\n\n");
    /* Main loop */

    while (true) {
        PRINT_LINE_INIT();
        pthread_mutex_lock( &mutex_exit);
        if(go_exit){
            pthread_mutex_unlock( &mutex_exit);
            break;
        }
        pthread_mutex_unlock( &mutex_exit);

	dci_msg_subframe.dl_msg_cnt = 0;	
	dci_msg_subframe.ul_msg_cnt = 0;	
	
        pthread_mutex_lock( &mutex_ue_sync);
        ret = srslte_ue_sync_zerocopy_multi(&ue_sync, sf_buffer);
        uint32_t sfidx = srslte_ue_sync_get_sfidx(&ue_sync);
        pthread_mutex_unlock( &mutex_ue_sync);

        pthread_mutex_lock( &mutex_sfn);
        sfn = system_frame_number;
        if (sfidx == 9) { system_frame_number++; }
        pthread_mutex_unlock( &mutex_sfn);

        uint32_t tti   = sfn*10 + sfidx;

        if (ret < 0) {
            fprintf(stderr, "Error calling srslte_ue_sync_work()\n");
        }
	/* srslte_ue_sync_get_buffer returns 1 if successfully read 1 aligned subframe */
        if (ret == 1) {

            switch (state) {
                case DECODE_MIB:
                    if (sfidx == 0) {
                        n = srslte_ue_mib_decode(&ue_mib, bch_payload, NULL, &sfn_offset);
                        if (n < 0) {
                            fprintf(stderr, "Error decoding UE MIB\n");
                            exit(-1);
                        } else if (n == SRSLTE_UE_MIB_FOUND) {
                            pthread_mutex_lock(&mutex_cell);
                            srslte_pbch_mib_unpack(bch_payload, &cell, &sfn);
                            srslte_cell_fprint(stdout, &cell, sfn);
                            pthread_mutex_unlock(&mutex_cell);

                            printf("Decoded MIB. SFN: %d, offset: %d\n", sfn, sfn_offset);
                            sfn = (sfn + sfn_offset)%1024;
                            state = DECODE_PDSCH;
                            pthread_mutex_lock( &mutex_sfn);
                            system_frame_number = sfn;
                            pthread_mutex_unlock( &mutex_sfn);
                        }
                    }
                    break;
		case DECODE_PDSCH:
                    decode_pdsch = true;
                    if (decode_pdsch) {
                        gettimeofday(&t[1], NULL);
			pthread_mutex_lock( &mutex_list);
                        srslte_copy_active_ue_list(&ue_list, &active_ue_list);
			pthread_mutex_unlock( &mutex_list);

                        n = srslte_dci_decoder_yx(&ue_dl, &active_ue_list, &dci_msg_subframe, tti);
                        gettimeofday(&t[2], NULL);
                        get_time_interval(t);
			
			pthread_mutex_lock( &mutex_list);
                        if (n > 0) {
			    srslte_enqueue_subframe_msg(&dci_msg_subframe, &ue_list, tti);
			    //dci_msg_list_display(dci_msg_subframe.downlink_msg, dci_msg_subframe.dl_msg_cnt);
			    //dci_msg_list_display(dci_msg_subframe.uplink_msg, dci_msg_subframe.ul_msg_cnt);
                        }
                        srslte_update_ue_list_every_subframe(&ue_list, tti);
			pthread_mutex_unlock( &mutex_list);

                        rsrq = SRSLTE_VEC_EMA(srslte_chest_dl_get_rsrq(&ue_dl.chest), rsrq, 0.1f);
                        rsrp0 = SRSLTE_VEC_EMA(srslte_chest_dl_get_rsrp_port(&ue_dl.chest, 0), rsrp0, 0.05f);
                        rsrp1 = SRSLTE_VEC_EMA(srslte_chest_dl_get_rsrp_port(&ue_dl.chest, 1), rsrp1, 0.05f);
                        noise = SRSLTE_VEC_EMA(srslte_chest_dl_get_noise_estimate(&ue_dl.chest), noise, 0.05f);
                        float elapsed = (float) t[0].tv_usec + t[0].tv_sec*1.0e+6f;

                        nframes++;
                        if (isnan(rsrq)) { rsrq = 0; }
                        if (isnan(noise)) { noise = 0; }
                        if (isnan(rsrp0)) { rsrp0 = 0; }
                        if (isnan(rsrp1)) { rsrp1 = 0; }

                        /* Print basic Parameters */
                        if(0){
                            PRINT_LINE("         RSRP: %+5.1f dBm | %+5.1f dBm", 10 * log10(rsrp0)+30, 10 * log10(rsrp1)+30);
                            PRINT_LINE("          SNR: %+5.1f dB | %+5.1f dB", 10 * log10(rsrp0 / noise), 10 * log10(rsrp1 / noise));
                            PRINT_LINE(" TIME ELAPSED: %+5.1f us", elapsed);
                            PRINT_LINE("");
                            //PRINT_LINE_RESET_CURSOR();
                        }else{
			    printf("RSRP: %+5.1f dBm | SNR: %+5.1f dB | TIME ELAPSED: %+5.1f us\n",
						    10 * log10(rsrp0)+30,10 * log10(rsrp0 / noise), elapsed);
			}
                    }
                    break;
            }

        } else if (ret == 0) {
            pthread_mutex_lock( &mutex_ue_sync);
            printf("Finding PSS... Peak: %8.1f, FrameCnt: %d, State: %d\r",
            srslte_sync_get_peak_value(&ue_sync.sfind),
            ue_sync.frame_total_cnt, ue_sync.state);
            pthread_mutex_unlock( &mutex_ue_sync);
        }
        sf_cnt++;
    } // Main loop
    srslte_ue_dl_free(&ue_dl);
    srslte_ue_mib_free(&ue_mib);
    for (int i = 0; i < prog_args.rf_nof_rx_ant; i++) {
        if (sf_buffer[i]) {
            free(sf_buffer[i]);
        }
    }
}

void* dci_decode_multi(void* p){
    int64_t sf_cnt;
    int n, ret;
    uint8_t bch_payload[SRSLTE_BCH_PAYLOAD_LEN];
    int sfn_offset;
    uint32_t nof_prb;
    uint32_t sfn = 0;
    int thd_id = (int)*(int *)p;
   
    bool time_flag, dl_flag, ul_flag;
    time_flag   = true;
    dl_flag     = false;
    ul_flag     = false;
 
    FILE *FD_TIME, *FD_DCI_DL, *FD_DCI_UL;
    char fileName[30];
    sprintf(fileName, "./time_dci_%d.txt",thd_id);
    FD_TIME = fopen(fileName,"w+");
    sprintf(fileName, "./dci_dl_dci_%d.txt",thd_id);
    FD_DCI_DL = fopen(fileName,"w+");
    sprintf(fileName, "./dci_ul_dci_%d.txt",thd_id);
    FD_DCI_UL = fopen(fileName,"w+");


    pthread_mutex_lock(&mutex_cell);
    nof_prb = cell.nof_prb;
    pthread_mutex_unlock(&mutex_cell);


    cf_t *sf_buffer[SRSLTE_MAX_PORTS] = {NULL};
    for (int i=0;i<prog_args.rf_nof_rx_ant;i++) {
        sf_buffer[i] = srslte_vec_malloc(3*sizeof(cf_t)*SRSLTE_SF_LEN_PRB(nof_prb));
    }

    srslte_ue_mib_t ue_mib;
    srslte_ue_dl_t ue_dl;

    if (srslte_ue_mib_init(&ue_mib, sf_buffer, nof_prb)) {
        fprintf(stderr, "Error initaiting UE MIB decoder\n");
        exit(-1);
    }
    if (srslte_ue_dl_init(&ue_dl, sf_buffer, nof_prb, prog_args.rf_nof_rx_ant)) {
        fprintf(stderr, "Error initiating UE downlink processing module\n");
        exit(-1);
    }

    pthread_mutex_lock(&mutex_cell);
    if (srslte_ue_mib_set_cell(&ue_mib, cell)) {
        fprintf(stderr, "Error initaiting UE MIB decoder\n");
        exit(-1);
    }
    if (srslte_ue_dl_set_cell(&ue_dl, cell)) {
        fprintf(stderr, "Error initiating UE downlink processing module\n");
        exit(-1);
    }

    pthread_mutex_unlock(&mutex_cell);

    srslte_chest_dl_cfo_estimate_enable(&ue_dl.chest, prog_args.enable_cfo_ref, 1023);
    srslte_chest_dl_average_subframe(&ue_dl.chest, prog_args.average_subframe);


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
    INFO("\nEntering main loop...\n\n");
    /* Main loop */
    float elapsed1, elapsed2;
    long timestamp1, timestamp2;
    while (true) {
        PRINT_LINE_INIT();
        pthread_mutex_lock( &mutex_exit);
        if(go_exit){
            pthread_mutex_unlock( &mutex_exit);
            break;
        }
        pthread_mutex_unlock( &mutex_exit);

	dci_msg_subframe.dl_msg_cnt = 0;	
	dci_msg_subframe.ul_msg_cnt = 0;	

	if(time_flag){
            gettimeofday(&t[1], NULL);
            timestamp1 =  t[1].tv_usec + t[1].tv_sec*1e6;
        }
	pthread_mutex_lock( &mutex_ue_sync);
        ret = srslte_ue_sync_zerocopy_multi(&ue_sync, sf_buffer);
        uint32_t sfidx = srslte_ue_sync_get_sfidx(&ue_sync);
        pthread_mutex_unlock( &mutex_ue_sync);
	
        pthread_mutex_lock( &mutex_sfn);
        sfn = system_frame_number;
        if (sfidx == 9) {   
	    system_frame_number++;
	    if(system_frame_number == 1024){
		system_frame_number = 0;
	    }
	}
        pthread_mutex_unlock( &mutex_sfn);

        uint32_t tti   = sfn*10 + sfidx;

        if (ret < 0) {
            fprintf(stderr, "Error calling srslte_ue_sync_work()\n");
        }
	/* srslte_ue_sync_get_buffer returns 1 if successfully read 1 aligned subframe */
        if (ret == 1) {
	    if( (sfn % 1000 == 0) && (sfidx == 0) ){
		n = srslte_ue_mib_decode(&ue_mib, bch_payload, NULL, &sfn_offset);
		if (n == SRSLTE_UE_MIB_FOUND) {
		    pthread_mutex_lock(&mutex_cell);
		    srslte_pbch_mib_unpack(bch_payload, &cell, &sfn);
		    //srslte_cell_fprint(stdout, &cell, sfn);
		    pthread_mutex_unlock(&mutex_cell);
		    
		    //printf("Decoded MIB. SFN: %d, offset: %d\n", sfn, sfn_offset);
		    sfn = (sfn + sfn_offset)%1024;
		    pthread_mutex_lock( &mutex_sfn);
		    system_frame_number = sfn;
		    pthread_mutex_unlock( &mutex_sfn);
		}
	    }
            switch (state) {
                case DECODE_MIB:
                    if (sfidx == 0) {
                        n = srslte_ue_mib_decode(&ue_mib, bch_payload, NULL, &sfn_offset);
                        if (n < 0) {
                            fprintf(stderr, "Error decoding UE MIB\n");
                            exit(-1);
                        } else if (n == SRSLTE_UE_MIB_FOUND) {
                            pthread_mutex_lock(&mutex_cell);
                            srslte_pbch_mib_unpack(bch_payload, &cell, &sfn);
                            srslte_cell_fprint(stdout, &cell, sfn);
                            pthread_mutex_unlock(&mutex_cell);

                            printf("Decoded MIB. SFN: %d, offset: %d\n", sfn, sfn_offset);
                            sfn = (sfn + sfn_offset)%1024;
                            state = DECODE_PDSCH;
                            pthread_mutex_lock( &mutex_sfn);
                            system_frame_number = sfn;
                            pthread_mutex_unlock( &mutex_sfn);
                        }
                    }
                    break;
		case DECODE_PDSCH:
                    decode_pdsch = true;
                    if (decode_pdsch) {
                        pthread_mutex_lock( &mutex_list);
                        srslte_copy_active_ue_list(&ue_list, &active_ue_list);
			pthread_mutex_unlock( &mutex_list);
			
			if(time_flag){
                            gettimeofday(&t[2], NULL);
                            timestamp2 =  t[2].tv_usec + t[2].tv_sec*1e6;
                            fprintf(FD_TIME,"%d %ld %ld %ld ",tti, timestamp1, timestamp2, timestamp2-timestamp1);
                            get_time_interval(t);
                            elapsed1 = (float) t[0].tv_usec + t[0].tv_sec*1.0e+6f;
                            gettimeofday(&t[1], NULL);
                            timestamp1 =  t[1].tv_usec + t[1].tv_sec*1e6;
                        }
			n = srslte_dci_decoder_yx(&ue_dl, &active_ue_list, &dci_msg_subframe, tti);

                        pthread_mutex_lock( &mutex_list);
                        if (n > 0) {
			    srslte_enqueue_subframe_msg(&dci_msg_subframe, &ue_list, tti);
			    //dci_msg_list_display(dci_msg_subframe.downlink_msg, dci_msg_subframe.dl_msg_cnt);
			    //dci_msg_list_display(dci_msg_subframe.uplink_msg, dci_msg_subframe.ul_msg_cnt);
			    //printf("\n");
			    if(dl_flag && dci_msg_subframe.dl_msg_cnt > 0){
                                record_dci_msg_log(FD_DCI_DL, dci_msg_subframe.downlink_msg, dci_msg_subframe.dl_msg_cnt);
                            }
                            if(ul_flag && dci_msg_subframe.ul_msg_cnt > 0){
                                record_dci_msg_log(FD_DCI_UL, dci_msg_subframe.uplink_msg, dci_msg_subframe.ul_msg_cnt);
                            }
                        }
                        srslte_update_ue_list_every_subframe(&ue_list, tti);
			pthread_mutex_unlock( &mutex_list);

                        rsrq = SRSLTE_VEC_EMA(srslte_chest_dl_get_rsrq(&ue_dl.chest), rsrq, 0.1f);
                        rsrp0 = SRSLTE_VEC_EMA(srslte_chest_dl_get_rsrp_port(&ue_dl.chest, 0), rsrp0, 0.05f);
                        rsrp1 = SRSLTE_VEC_EMA(srslte_chest_dl_get_rsrp_port(&ue_dl.chest, 1), rsrp1, 0.05f);
                        noise = SRSLTE_VEC_EMA(srslte_chest_dl_get_noise_estimate(&ue_dl.chest), noise, 0.05f);
			if(time_flag){
                            gettimeofday(&t[2], NULL);
                            timestamp2 =  t[2].tv_usec + t[2].tv_sec*1e6;
                            fprintf(FD_TIME, "%ld %ld %ld\n", timestamp1, timestamp2, timestamp2-timestamp1);
                            get_time_interval(t);
                            elapsed2 = (float) t[0].tv_usec + t[0].tv_sec*1.0e+6f;
                        }   	
		        nframes++;
                        if (isnan(rsrq)) { rsrq = 0; }
                        if (isnan(noise)) { noise = 0; }
                        if (isnan(rsrp0)) { rsrp0 = 0; }
                        if (isnan(rsrp1)) { rsrp1 = 0; }    
    
			/* Print basic Parameters */
			if(0){
			    if(0){
				PRINT_LINE("         RSRP: %+5.1f dBm | %+5.1f dBm", 10 * log10(rsrp0)+30, 10 * log10(rsrp1)+30);
				PRINT_LINE("          SNR: %+5.1f dB | %+5.1f dB", 10 * log10(rsrp0 / noise), 10 * log10(rsrp1 / noise));
				PRINT_LINE(" TIME ELAPSED: %+5.1f us | %+5.1f us", elapsed1, elapsed2);
				PRINT_LINE("       THD ID: %d", thd_id);
				PRINT_LINE("");
				//PRINT_LINE_RESET_CURSOR();
			    }else{
				printf("Thead ID: %d -- RSRP: %+5.1f dBm | SNR: %+5.1f dB | TIME ELAPSED: %+5.1f us %+5.1f\n", thd_id,
							10 * log10(rsrp0)+30,10 * log10(rsrp0 / noise), elapsed1, elapsed2);
			    }
			}
                    }
                    break;
            }

        } else if (ret == 0) {
            pthread_mutex_lock( &mutex_ue_sync);
            printf("Finding PSS... Peak: %8.1f, FrameCnt: %d, State: %d\n",
			srslte_sync_get_peak_value(&ue_sync.sfind),
			ue_sync.frame_total_cnt, ue_sync.state);
            pthread_mutex_unlock( &mutex_ue_sync);
        }
        sf_cnt++;
    } // Main loop

    srslte_ue_dl_free(&ue_dl);
    srslte_ue_mib_free(&ue_mib);
    for (int i = 0; i < prog_args.rf_nof_rx_ant; i++) {
        if (sf_buffer[i]) {
            free(sf_buffer[i]);
        }
    }
    fclose(FD_TIME);
    fclose(FD_DCI_DL);
    fclose(FD_DCI_UL);
    pthread_exit(NULL);
}

