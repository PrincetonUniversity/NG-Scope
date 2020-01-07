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
#include "srslte/srslte.h"

#define ENABLE_AGC_DEFAULT

#include "srslte/phy/rf/rf.h"
#include "srslte/phy/rf/rf_utils.h"

#include "dci_decode_multi_usrp.h"

extern bool go_exit;
extern bool exit_heartBeat;
extern uint16_t targetRNTI_const;

extern enum receiver_state state[MAX_NOF_USRP];
extern srslte_ue_sync_t ue_sync[MAX_NOF_USRP];
extern prog_args_t prog_args[MAX_NOF_USRP];
extern srslte_ue_list_t ue_list[MAX_NOF_USRP];
extern srslte_cell_t cell[MAX_NOF_USRP];
extern srslte_rf_t rf[MAX_NOF_USRP];
extern uint32_t system_frame_number[MAX_NOF_USRP];
extern int free_order[MAX_NOF_USRP*4];
extern srslte_ue_cell_usage ue_cell_usage;


extern pthread_mutex_t mutex_exit;
extern pthread_mutex_t mutex_usage;
extern pthread_mutex_t mutex_free_order;


//pthread_mutex_t mutex_exit = PTHREAD_MUTEX_INITIALIZER;
//pthread_mutex_t mutex_usage = PTHREAD_MUTEX_INITIALIZER;
//pthread_mutex_t mutex_free_order = PTHREAD_MUTEX_INITIALIZER;
//
pthread_mutex_t mutex_cell[MAX_NOF_USRP] =
{
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER
}; 

pthread_mutex_t mutex_sfn[MAX_NOF_USRP] =
{
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER
}; 

pthread_mutex_t mutex_list[MAX_NOF_USRP] =
{
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER
}; 

pthread_mutex_t mutex_ue_sync[MAX_NOF_USRP] =
{
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER
}; 

bool check_free_order(int* array, int arraySize, int order){
    bool time_to_free=true;
    for(int i=order;i<arraySize;i++){
	if(array[i] == 1){
	    time_to_free = false;
	}
    }
    return time_to_free;
} 

/* Useful macros for printing lines which will disappear */

//#define PRINT_LINE_INIT() int this_nof_lines = 0; static int prev_nof_lines = 0
//#define PRINT_LINE(_fmt, ...) printf("\033[K" _fmt "\n", ##__VA_ARGS__); this_nof_lines++
//#define PRINT_LINE_RESET_CURSOR() printf("\033[%dA", this_nof_lines); prev_nof_lines = this_nof_lines
//#define PRINT_LINE_ADVANCE_CURSOR() printf("\033[%dB", prev_nof_lines + 1)
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
  args->rf_dev  = "";
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
  printf("\t-I RF dev [Default %s]\n", args->rf_dev);
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
    case 'I':
      args->rf_dev = argv[optind];
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


void* dci_decode_multi(void* p){
    int64_t sf_cnt;
    int n, ret;
    uint8_t bch_payload[SRSLTE_BCH_PAYLOAD_LEN];
    int sfn_offset;
    uint32_t nof_prb;
    uint32_t sfn = 0;
    uint16_t targetRNTI = targetRNTI_const;
    dci_usrp_id_t dci_usrp_id = (dci_usrp_id_t)*(dci_usrp_id_t *)p;
     
    bool time_flag, dl_flag, ul_flag;
    time_flag	= false;
    dl_flag	= false;
    ul_flag	= false; 

    int thd_id, usrp_idx, order;
    order	= dci_usrp_id.free_order;
    thd_id	= dci_usrp_id.dci_thd_id;
    usrp_idx	= dci_usrp_id.usrp_thd_id;

    FILE *FD_TIME, *FD_DCI_DL, *FD_DCI_UL;
    char fileName[30];
    sprintf(fileName, "./time_usrp_%d_dci_%d.txt",usrp_idx,thd_id);
    if(time_flag){
	FD_TIME = fopen(fileName,"w+");
    }
    sprintf(fileName, "./dci_dl_usrp_%d_dci_%d.txt",usrp_idx,thd_id);
    if(dl_flag){
	FD_DCI_DL = fopen(fileName,"w+");
    }
    sprintf(fileName, "./dci_ul_usrp_%d_dci_%d.txt",usrp_idx,thd_id);
    if(ul_flag){
	FD_DCI_UL = fopen(fileName,"w+");
    }

    pthread_mutex_lock(&mutex_cell[usrp_idx]);
    nof_prb = cell[usrp_idx].nof_prb;
    pthread_mutex_unlock(&mutex_cell[usrp_idx]);


    cf_t *sf_buffer[SRSLTE_MAX_PORTS] = {NULL};
    for (int i=0;i<prog_args[usrp_idx].rf_nof_rx_ant;i++) {
        sf_buffer[i] = srslte_vec_malloc(3*sizeof(cf_t)*SRSLTE_SF_LEN_PRB(nof_prb));
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
			//	PRINT_LINE("         RSRP: %+5.1f dBm | %+5.1f dBm", 10 * log10(rsrp0)+30, 10 * log10(rsrp1)+30);
			//	PRINT_LINE("          SNR: %+5.1f dB | %+5.1f dB", 10 * log10(rsrp0 / noise), 10 * log10(rsrp1 / noise));
			//	PRINT_LINE(" TIME ELAPSED: %+5.1f us | %+5.1f us", elapsed1, elapsed2);
			//	PRINT_LINE("       THD ID: %d", thd_id);
			//	PRINT_LINE("");
			//	//PRINT_LINE_RESET_CURSOR();
			//    }else{
			//	printf("USRP: %d Thead ID: %d -- RSRP: %+5.1f dBm | SNR: %+5.1f dB | TIME ELAPSED: %+5.1f us %+5.1f\n", 
			//	    usrp_idx, thd_id, 10 * log10(rsrp0)+30,10 * log10(rsrp0 / noise), elapsed1, elapsed2);
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
    if(dl_flag){
	fclose(FD_DCI_DL);
    }
    if(ul_flag){
	fclose(FD_DCI_UL);
    }
    printf("Bye -- USRP:%d DCI-Thread:%d\n", usrp_idx, thd_id);
    pthread_exit(NULL);
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
    printf("Tunning receiver to %.3f MHz\n", (prog_args[usrp_idx].rf_freq + prog_args[usrp_idx].file_offset_freq)/1000000);
    srslte_rf_set_rx_freq(&rf[usrp_idx], prog_args[usrp_idx].rf_freq + prog_args[usrp_idx].file_offset_freq);
    srslte_rf_rx_wait_lo_locked(&rf[usrp_idx]);

    uint32_t ntrial=0;
    do {
        ret = rf_search_and_decode_mib(&rf[usrp_idx], prog_args[usrp_idx].rf_nof_rx_ant, &cell_detect_config, prog_args[usrp_idx].force_N_id_2, &cell[usrp_idx], &cfo);
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

    //	set the prb for cell ue usage status
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

    dci_usrp_id_t dci_usrp_id[10];
    pthread_t dci_thd[10];
    for(int i=0;i<prog_args[usrp_idx].nof_thread;i++){
	thd_count += 1;
        dci_usrp_id[i].free_order  = thd_count;
        dci_usrp_id[i].dci_thd_id  = i+1;
        dci_usrp_id[i].usrp_thd_id = usrp_idx;
        pthread_create( &dci_thd[i], NULL, dci_decode_multi, (void *)&dci_usrp_id[i]);
    }
    for(int i=0;i<prog_args[usrp_idx].nof_thread;i++){
        pthread_join(dci_thd[i], NULL);
    }
    srslte_ue_sync_free(&ue_sync[usrp_idx]);
    srslte_rf_close(&rf[usrp_idx]);
    printf("Bye -- USRP:%d \n", usrp_idx);
    pthread_exit(NULL);
}
void* heart_beat(void* p){
    int count = 0;
    while (true){
	pthread_mutex_lock( &mutex_exit);
        if(exit_heartBeat){
            pthread_mutex_unlock( &mutex_exit);
            break;
        }
        pthread_mutex_unlock( &mutex_exit);
	count++;
	if(count == 5){ 
	    system("./heart_beat.sh");
	    count = 0;
	}
	sleep(1);
    }
    pthread_exit(NULL);
}
