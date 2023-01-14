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

#include "ngscope/hdr/dciLib/radio.h"
#include "ngscope/hdr/dciLib/task_scheduler.h"
#include "ngscope/hdr/dciLib/dci_decoder.h"
#include "ngscope/hdr/dciLib/ngscope_def.h"
#include "ngscope/hdr/dciLib/phich_decoder.h"
#include "ngscope/hdr/dciLib/time_stamp.h"

#include "ngscope/hdr/dciLib/task_sf_ring_buffer.h"
#include "ngscope/hdr/dciLib/skip_tti.h"
#include "ngscope/hdr/dciLib/thread_exit.h"
#include "ngscope/hdr/dciLib/ue_tracker.h"

extern bool go_exit;

extern pthread_mutex_t     cell_mutex; 
extern srsran_cell_t       cell_vec[MAX_NOF_RF_DEV];

extern dci_ready_t              dci_ready;
extern ngscope_status_buffer_t  dci_buffer[MAX_DCI_BUFFER];

extern bool dci_decoder_up[MAX_NOF_RF_DEV][MAX_NOF_DCI_DECODER];
extern bool task_scheduler_up[MAX_NOF_RF_DEV];
extern bool task_scheduler_closed[MAX_NOF_RF_DEV];
extern pthread_mutex_t     scheduler_close_mutex;

/******************* Global buffer for passing subframe IQ  ******************/ 
ngscope_sf_buffer_t sf_buffer[MAX_NOF_RF_DEV][MAX_NOF_DCI_DECODER] = 
{
{
    {false, 0, 0, {NULL}, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER},
    {false, 0, 0, {NULL}, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER},
    {false, 0, 0, {NULL}, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER},
    {false, 0, 0, {NULL}, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER},
},
{
    {false, 0, 0, {NULL}, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER},
    {false, 0, 0, {NULL}, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER},
    {false, 0, 0, {NULL}, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER},
    {false, 0, 0, {NULL}, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER},
},
{
    {false, 0, 0, {NULL}, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER},
    {false, 0, 0, {NULL}, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER},
    {false, 0, 0, {NULL}, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER},
    {false, 0, 0, {NULL}, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER},
},
{
    {false, 0, 0, {NULL}, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER},
    {false, 0, 0, {NULL}, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER},
    {false, 0, 0, {NULL}, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER},
    {false, 0, 0, {NULL}, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER},
}
};
bool                sf_token[MAX_NOF_RF_DEV][MAX_NOF_DCI_DECODER];

pthread_mutex_t     token_mutex[MAX_NOF_RF_DEV] = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER,
					 								PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER};

pend_ack_list       ack_list;
pthread_mutex_t     ack_mutex = PTHREAD_MUTEX_INITIALIZER;

ngscope_ue_tracker_t ue_tracker[MAX_NOF_RF_DEV];
pthread_mutex_t      ue_tracker_mutex[MAX_NOF_RF_DEV] = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER,
					 								PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER};


// This is the container for dci that are not allocated 
task_skip_tti_t 	skip_tti[MAX_NOF_RF_DEV];

task_tmp_buffer_t   task_tmp_buffer[MAX_NOF_RF_DEV];
pthread_mutex_t     tmp_buf_mutex[MAX_NOF_RF_DEV] = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER,
					 								PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER};

int find_idle_decoder(int rf_idx, int nof_decoder){
    int idle_idx = -1;
    pthread_mutex_lock(&token_mutex[rf_idx]);
    for(int i=0;i<nof_decoder;i++){
        if(sf_token[rf_idx][i] == false){       
            idle_idx = i;
            // Set the token to true to mark that this decoder has been taken
            // Do remember to free the token 
            sf_token[rf_idx][idle_idx] = true;
            break;
        } 
    } 
    pthread_mutex_unlock(&token_mutex[rf_idx]);
    return idle_idx;
}
/*****************************************************************************/


/********************** callback wrapper **********************/ 
int srsran_rf_recv_wrapper(void* h, cf_t* data_[SRSRAN_MAX_PORTS], uint32_t nsamples, srsran_timestamp_t* t)
{
  DEBUG(" ----  Receive %d samples  ----", nsamples);
  void* ptr[SRSRAN_MAX_PORTS];
  for (int i = 0; i < SRSRAN_MAX_PORTS; i++) {
    ptr[i] = data_[i];
  }
  //return srsran_rf_recv_with_time_multi(h, ptr, nsamples, true, NULL, NULL);
  return srsran_rf_recv_with_time_multi(h, ptr, nsamples, true, &t->full_secs, &t->frac_secs);
}

static SRSRAN_AGC_CALLBACK(srsran_rf_set_rx_gain_th_wrapper_)
{
  srsran_rf_set_rx_gain_th((srsran_rf_t*)h, gain_db);
}
/******************** End of callback wrapper ********************/ 

// Init MIB
int mib_init_imp(srsran_ue_mib_t*   ue_mib,
                    cf_t*           sf_buffer[SRSRAN_MAX_PORTS],
                    srsran_cell_t*  cell)
{
    if (srsran_ue_mib_init(ue_mib, sf_buffer[0], cell->nof_prb)) {
        ERROR("Error initaiting UE MIB decoder");
        exit(-1);
    }
    if (srsran_ue_mib_set_cell(ue_mib, *cell)) {
        ERROR("Error initaiting UE MIB decoder");
        exit(-1);
    }
    return SRSRAN_SUCCESS;
}

int ue_mib_decode_sfn(srsran_ue_mib_t*   ue_mib,
                        srsran_cell_t*   cell,
                        uint32_t*        sfn,
                        bool             decode_pdcch)
{
    uint8_t bch_payload[SRSRAN_BCH_PAYLOAD_LEN];
    int     sfn_offset;
    int n = srsran_ue_mib_decode(ue_mib, bch_payload, NULL, &sfn_offset);
    if (n < 0) {
      ERROR("Error decoding UE MIB");
      exit(-1);
    } else if (n == SRSRAN_UE_MIB_FOUND) {
      srsran_pbch_mib_unpack(bch_payload, cell, sfn);
      if(!decode_pdcch){
          srsran_cell_fprint(stdout, cell, *sfn);
          printf("Decoded MIB. SFN: %d, offset: %d\n", *sfn, sfn_offset);
      }
      *sfn   = (*sfn + sfn_offset) % 1024;
    }
    return SRSRAN_SUCCESS;
}

// Initialize UE sync
int ue_sync_init_imp(srsran_ue_sync_t*      ue_sync,
                        srsran_rf_t*        rf, 
                        srsran_cell_t*      cell,
                        cell_search_cfg_t*  cell_detect_config,
                        prog_args_t         prog_args,
                        float               search_cell_cfo)
{
    int decimate = 0;
    if (prog_args.decimate) {
        if (prog_args.decimate > 4 || prog_args.decimate < 0) {
            printf("Invalid decimation factor, setting to 1 \n");
        } else {
            decimate = prog_args.decimate;
        }
    }
    // Init the structure
    if (srsran_ue_sync_init_multi_decim(ue_sync,
                                        cell->nof_prb,
                                        cell->id == 1000,
                                        srsran_rf_recv_wrapper,
                                        prog_args.rf_nof_rx_ant,
                                        (void*)rf,
                                        decimate)) {
      ERROR("Error initiating ue_sync");
      exit(-1);
    }
    // UE sync set cell info
    if (srsran_ue_sync_set_cell(ue_sync, *cell)) {
      ERROR("Error initiating ue_sync");
      exit(-1);
    }

    // Disable CP based CFO estimation during find
    ue_sync->cfo_current_value       = search_cell_cfo / 15000;
    ue_sync->cfo_is_copied           = true;
    ue_sync->cfo_correct_enable_find = true;
    srsran_sync_set_cfo_cp_enable(&ue_sync->sfind, false, 0);
    
    // set AGC
    if (prog_args.rf_gain < 0) {
        srsran_rf_info_t* rf_info = srsran_rf_get_info(rf);
        srsran_ue_sync_start_agc(ue_sync,
                             srsran_rf_set_rx_gain_th_wrapper_,
                             rf_info->min_rx_gain,
                             rf_info->max_rx_gain,
                             cell_detect_config->init_agc);
    }
    ue_sync->cfo_correct_enable_track = !prog_args.disable_cfo;
      
    return SRSRAN_SUCCESS;
}

// Init the task scheduler 
int task_scheduler_init(ngscope_task_scheduler_t* task_scheduler,
                            prog_args_t prog_args){
                            //srsran_rf_t* rf, 
                            //srsran_cell_t* cell, 
                            //srsran_ue_sync_t* ue_sync){
    float search_cell_cfo = 0;
    task_scheduler->prog_args = prog_args;

    cell_search_cfg_t cell_detect_config = {.max_frames_pbch      = SRSRAN_DEFAULT_MAX_FRAMES_PBCH,
                                        .max_frames_pss       = SRSRAN_DEFAULT_MAX_FRAMES_PSS,
                                        .nof_valid_pss_frames = SRSRAN_DEFAULT_NOF_VALID_PSS_FRAMES,
                                        .init_agc             = 0,
                                        .force_tdd            = false};
   
    // Copy the prameters 
    task_scheduler->prog_args = prog_args;
 
    // First of all, start the radio and get the cell information
    radio_init_and_start(&task_scheduler->rf, &task_scheduler->cell, prog_args, 
                                                &cell_detect_config, &search_cell_cfo);
           
    // Copy the cell info to the  
    pthread_mutex_lock(&cell_mutex); 
    memcpy(&cell_vec[prog_args.rf_index], &(task_scheduler->cell), sizeof(srsran_cell_t));
    printf("\n\nFinished copying to cell:%d prb:%d \n", prog_args.rf_index, cell_vec[prog_args.rf_index].nof_prb);
    pthread_mutex_unlock(&cell_mutex); 

    // Next, let's get the ue_sync ready
    ue_sync_init_imp(&task_scheduler->ue_sync, &task_scheduler->rf, &task_scheduler->cell, 
                                          &cell_detect_config, prog_args, search_cell_cfo); 

    pthread_mutex_lock(&ack_mutex); 
    init_pending_ack(&ack_list);
    pthread_mutex_unlock(&ack_mutex); 

    return SRSRAN_SUCCESS;
}

void copy_sf_sync_buffer(cf_t* source[SRSRAN_MAX_PORTS],
                         cf_t* dest[SRSRAN_MAX_PORTS],
                         uint32_t max_num_samples)
{
    for(int p=0; p<SRSRAN_MAX_PORTS; p++){
        memcpy(dest[p], source[p], max_num_samples*sizeof(cf_t));
    }
    return;
}

/* Assign the decoding task to available idle decoder */
void assign_task_to_decoder(int 	 rf_idx,
							int      idle_idx, 
                            uint32_t rf_nof_rx_ant,
                            uint32_t sf_idx, 
                            uint32_t sfn,
                            uint32_t max_num_samples,
                            cf_t*    IQ_buffer[SRSRAN_MAX_PORTS])
{
    //--> Lock sf buffer
    pthread_mutex_lock(&sf_buffer[rf_idx][idle_idx].sf_mutex);

    // Tell the scheduler that we now busy
    pthread_mutex_lock(&token_mutex[rf_idx]);
    sf_token[rf_idx][idle_idx] = true;
    pthread_mutex_unlock(&token_mutex[rf_idx]);

    //printf("TTI:%d --> sfn:%d sf_idx:%d\n", sf_idx + sfn * 10, sfn, sf_idx);
    sf_buffer[rf_idx][idle_idx].sf_idx  	= sf_idx;
    sf_buffer[rf_idx][idle_idx].sfn     	= sfn;
    sf_buffer[rf_idx][idle_idx].empty_sf    = false; // not empty subframe

    // copy the buffer source:sync_buffer dest: IQ_buffer
    //copy_sf_sync_buffer(sync_buffer, sf_buffer[rf_idx][idle_idx].IQ_buffer, max_num_samples);
    for(int p=0; p<rf_nof_rx_ant; p++){
        memcpy(sf_buffer[rf_idx][idle_idx].IQ_buffer[p], IQ_buffer[p], max_num_samples*sizeof(cf_t));
    }

    // Tell the corresponding idle thread to process the signal
    //printf("Send the conditional signal to the %d-th decoder!\n", idle_idx);
    pthread_cond_signal(&sf_buffer[rf_idx][idle_idx].sf_cond);

    //--> Unlock sf buffer
    pthread_mutex_unlock(&sf_buffer[rf_idx][idle_idx].sf_mutex);

    return;
}

/* Assign the empty task to available idle decoder */
void assign_empty_task_to_decoder(int 		 rf_idx,
									int      idle_idx, 
									uint32_t sf_idx, 
									uint32_t sfn)
{
    //--> Lock sf buffer
    pthread_mutex_lock(&sf_buffer[rf_idx][idle_idx].sf_mutex);

    // Tell the scheduler that we now busy
    pthread_mutex_lock(&token_mutex[rf_idx]);
    sf_token[rf_idx][idle_idx] = true;
    pthread_mutex_unlock(&token_mutex[rf_idx]);

    //printf("TTI:%d --> sfn:%d sf_idx:%d\n", sf_idx + sfn * 10, sfn, sf_idx);
    sf_buffer[rf_idx][idle_idx].sf_idx  	= sf_idx;
    sf_buffer[rf_idx][idle_idx].sfn     	= sfn;
    sf_buffer[rf_idx][idle_idx].empty_sf    = true; // not empty subframe

    // Tell the corresponding idle thread to process the signal
    //printf("Send the conditional signal to the %d-th decoder!\n", idle_idx);
    pthread_cond_signal(&sf_buffer[rf_idx][idle_idx].sf_cond);

    //--> Unlock sf buffer
    pthread_mutex_unlock(&sf_buffer[rf_idx][idle_idx].sf_mutex);

    return;
}

typedef struct{
    int         nof_decoder;
    uint32_t    rf_nof_rx_ant;
    uint32_t    max_num_samples;
	int 		rf_idx;
}tmp_para_t;

int  get_nof_buffered_sf(int rf_idx){
	if(task_tmp_buffer[rf_idx].header !=  task_tmp_buffer[rf_idx].tail){
		int idx = task_tmp_buffer[rf_idx].tail;
		int cnt = 0;
		while(idx != task_tmp_buffer[rf_idx].header){
			cnt++;
			idx = (idx+1) % MAX_TMP_BUFFER;
		}
		return cnt;
	}else{
		return 0;
	}
}
void* handle_tmp_buffer_thread(void* p){
    int         nof_decoder     = (*(tmp_para_t *)p).nof_decoder;
    uint32_t    rf_nof_rx_ant   = (*(tmp_para_t *)p).rf_nof_rx_ant;
    uint32_t    max_num_samples = (*(tmp_para_t *)p).max_num_samples;
    int         rf_idx     		= (*(tmp_para_t *)p).rf_idx;

    while(!go_exit){
        /* Before fetching we check if we have sf in tmp buffer*/ 
        /* we give higher priority to subframes stored inside the tmp buffer */

        //uint64_t t1 = timestamp_us();        
        pthread_mutex_lock(&tmp_buf_mutex[rf_idx]);
		if(!skip_tti_empty(&skip_tti[rf_idx])){
			while(!go_exit){
				int idle_idx  =  find_idle_decoder(rf_idx, nof_decoder);
                if(idle_idx < 0){ 
                    break;  
                }else{
					//printf("1-> len:%d sfn:%d sf_idx:%d \n",  skip_tti[rf_idx].nof_tti,\
								skip_tti[rf_idx].sf[skip_tti[rf_idx].nof_tti-1], \
								skip_tti[rf_idx].sf_idx[skip_tti[rf_idx].nof_tti-1]);
					uint32_t tti 	= skip_tti_get(&skip_tti[rf_idx]);
					uint32_t sfn 	= tti / 10;
					uint32_t sf_idx = tti % 10;
					//printf("2-> len:%d sfn:%d sf_idx:%d tti:%d\n",  skip_tti[rf_idx].nof_tti,\
								sfn, sf_idx, tti);
                    assign_empty_task_to_decoder(rf_idx, idle_idx, sf_idx, sfn);
					//printf("after assignment skip tti:%d\n", skip_tti[rf_idx].nof_tti);
               	}  
				if(skip_tti_empty(&skip_tti[rf_idx])){
					break;
				}
			}
		}

        if(!task_sf_ring_buffer_empty(&task_tmp_buffer[rf_idx])){
			//int nof_buf_sf = get_nof_buffered_sf(rf_idx);
            //printf("We have %d subframes the tmp buffer!\n", nof_buf_sf); 
            while(!go_exit){
                int idle_idx  =  find_idle_decoder(rf_idx, nof_decoder);
                if(idle_idx < 0){ 
                    break;  
                }else{
                    // Assign the Task to the corresponding decoder
                    int tmp_buf_idx = task_tmp_buffer[rf_idx].tail;
                    int tmp_sf_idx  = task_tmp_buffer[rf_idx].sf_buf[tmp_buf_idx].sf_idx;
                    int tmp_sfn     = task_tmp_buffer[rf_idx].sf_buf[tmp_buf_idx].sfn;

                    //printf("Assigning tti:%d to the %d-th decoder since it is idle!\n\n", \
                                                    tmp_sfn * 10 + tmp_sf_idx, idle_idx); 
                    assign_task_to_decoder(rf_idx, idle_idx, rf_nof_rx_ant, tmp_sf_idx, tmp_sfn, max_num_samples,
                             task_tmp_buffer[rf_idx].sf_buf[tmp_buf_idx].IQ_buffer);

					// advance the tail 
                   	task_sf_ring_buffer_get(&task_tmp_buffer[rf_idx]);

                    // Exit when the tmp buffer is empty
                    //if(task_tmp_buffer.header == task_tmp_buffer.tail){
                    if(task_sf_ring_buffer_empty(&task_tmp_buffer[rf_idx])){
                        break;
                    }
                }
            }
        }
        pthread_mutex_unlock(&tmp_buf_mutex[rf_idx]);
        //uint64_t t2 = timestamp_us();        
        //printf("Handle tmp buffer time_spend:%ld (us)\n", t2-t1);

        // let's sleep for a while so that we don't fully block
        usleep(100);
    }
	printf("%d-th RF-Dev tmp buffer handler CLOSED!\n", rf_idx);
    return NULL;
}

void* task_scheduler_thread(void* p){

    prog_args_t* prog_args = (prog_args_t*)p;
    ngscope_task_scheduler_t task_scheduler;
    task_scheduler_init(&task_scheduler, *prog_args);

    int ret;
    int nof_decoder = task_scheduler.prog_args.nof_decoder;
    int rf_idx      = task_scheduler.prog_args.rf_index;
    uint32_t rf_nof_rx_ant = task_scheduler.prog_args.rf_nof_rx_ant;
    

    ngscope_dci_per_sub_t       dci_per_sub; // empty place hoder for skipped frames 
    ngscope_status_buffer_t     dci_ret;

    memset(&dci_per_sub, 0, sizeof(ngscope_dci_per_sub_t));
    memset(&dci_ret, 0, sizeof(ngscope_status_buffer_t));

    uint32_t max_num_samples = 3 * SRSRAN_SF_LEN_PRB(task_scheduler.cell.nof_prb); /// Length in complex samples
    printf("nof_prb:%d max_sample:%d\n", task_scheduler.cell.nof_prb, max_num_samples);

    /************** Setting up the UE sync buffer ******************/
    cf_t* sync_buffer[SRSRAN_MAX_PORTS] = {NULL};
    cf_t* buffers[SRSRAN_MAX_CHANNELS] = {};

    for (int j = 0; j < task_scheduler.prog_args.rf_nof_rx_ant; j++) {
        sync_buffer[j] = srsran_vec_cf_malloc(max_num_samples);
    }
    // Set the buffer for ue_sync
    for (int p = 0; p < SRSRAN_MAX_PORTS; p++) {
        buffers[p] = sync_buffer[p];
    }
    /************** END OF setting up the UE sync buffer ******************/

    // init the subframe buffer
    ngscope_dci_decoder_t   dci_decoder[MAX_NOF_DCI_DECODER];
    pthread_t               dci_thd[MAX_NOF_DCI_DECODER];

    // Init the UE MIB decoder
    srsran_ue_mib_t         ue_mib;    
    mib_init_imp(&ue_mib, sync_buffer, &task_scheduler.cell);

    /********************** Set up the tmp buffer **********************/
	task_sf_ring_buffer_init(&task_tmp_buffer[rf_idx], max_num_samples);
	skip_tti_init(&skip_tti[rf_idx]);
    /********** End of setting up the tmp buffer **********************/
   
    // Start the tmp buffer handling thd
    // which allocates the buffered Subframe to corresponding decoder
    pthread_t tmp_buf_thd;
    tmp_para_t tmp_para = {nof_decoder, rf_nof_rx_ant, max_num_samples, rf_idx};
    pthread_create(&tmp_buf_thd, NULL, handle_tmp_buffer_thread, (void*)&tmp_para);
		
	//cell_args_t 		cell_args[MAX_NOF_DCI_DECODER];

  	srsran_softbuffer_rx_t 	rx_softbuffers[SRSRAN_MAX_CODEWORDS];

    for(int i=0;i<nof_decoder;i++){
        // init the subframe buffer 
        for (int j = 0; j < SRSRAN_MAX_PORTS; j++) {
            sf_buffer[rf_idx][i].IQ_buffer[j] = srsran_vec_cf_malloc(max_num_samples);
        }

		dci_decoder_init(&dci_decoder[i], task_scheduler.prog_args, &task_scheduler.cell, \
                           sf_buffer[rf_idx][i].IQ_buffer, rx_softbuffers, i);

        //mib_init_imp(&ue_mib[i], sf_buffer[rf_idx][i].IQ_buffer, &task_scheduler->cell);
        pthread_create( &dci_thd[i], NULL, dci_decoder_thread, (void*)&dci_decoder[i]);

		// fill the dci decoder status
		dci_decoder_up[rf_idx][i] = true;
    }
    
    // Let's sleep for 1 second and wait for the decoder to be ready!
    sleep(1);
    //srsran_ue_mib_t ue_mib;    
    //mib_init_imp(&ue_mib, sf_buffer[rf_idx][i].sf_buffer, cell);

    int         sf_cnt = 0; 
    uint32_t    sfn = 0;
    uint32_t    last_tti = 0;
    uint32_t    tti = 0;
    bool        decode_pdcch = false;
	uint32_t 	sf_idx = 0;

	FILE* 		fd = fopen("task_scheduler.txt","w+");
	//FILE* 		fd_1 = fopen("sf_sfn.txt","w+");
	

	//uint64_t t1=0, t2=0, t3=0;
	//uint64_t t1_sf_idx =0, t2_sf_idx=0;
    while(!go_exit && (sf_cnt < task_scheduler.prog_args.nof_subframes || task_scheduler.prog_args.nof_subframes == -1)) {
    	//fprintf(fd, "%d\t%d\t%d\t%ld\t%ld\t\n", sfn*10+sf_idx, sfn, sf_idx, t2-t1, t3-t1);

    	/*  Get the subframe data and put it into the buffer */
        //t1 = timestamp_us();        
        ret = srsran_ue_sync_zerocopy(&(task_scheduler.ue_sync), buffers, max_num_samples);
        //t2 = timestamp_us();        
        //printf("time_spend:%ld (us)\n", t2-t1);
        //printf("RET is:%d\n", ret); 
        if (ret < 0) {
            ERROR("Error calling srsran_ue_sync_work()");
        }else if(ret == 1){
        	//t1_sf_idx = timestamp_us();        
            sf_idx = srsran_ue_sync_get_sfidx(&task_scheduler.ue_sync);
        	//t2_sf_idx = timestamp_us();        
            //printf("Get %d-th subframe TTI:%d \n", sf_idx, sf_idx+ sfn*10);
			//printf("task -> finish get index!\n");
            sf_cnt ++; 
			//fprintf(fd_1, "%d\t%d\t%d\t", sf_idx + sfn*10, sf_idx, sfn);
            /********************* SFN handling *********************/
            if ( (sf_idx == 0) || (decode_pdcch == false) ) {
                // update SFN when sf_idx is 0 
                uint32_t sfn_tmp = 0;
                ue_mib_decode_sfn(&ue_mib, &task_scheduler.cell, &sfn_tmp, decode_pdcch);

                if(sfn != sfn_tmp){
                    printf("current sfn:%d decoded sfn:%d\n",sfn, sfn_tmp);
                }
                if(sfn_tmp > 0){
                    //printf("decoded sfn from:%d\n",sfn_tmp);
                    sfn = sfn_tmp;
                    decode_pdcch = true;
                }
                //printf("\n");
            }
            //printf("Get %d-th subframe TTI:%d \n", sf_idx, sf_idx+ sfn*10);
			tti = sfn*10 + sf_idx;
			fprintf(fd,"%d\t%d\t%d\t", sfn*10 + sf_idx, sfn, sf_idx);
			//fprintf(fd_1, "%d\t", sfn);
            /******************* END OF SFN handling *******************/

          	//decode_pdcch = false; 
            /***************** Tell the decoder to decode the PDCCH *********/          
            if(decode_pdcch){  // We only decode when we got the SFN
				if((last_tti != 10239) && (last_tti+1 != tti) ){
					printf("Last tti:%d current tti:%d\n", last_tti, tti);
				}
				last_tti = tti;
                /** Now we need to know where shall we put the IQ data of each subframe*/
                int idle_idx  = -1;

                idle_idx  =  find_idle_decoder(rf_idx, nof_decoder);
                
                // If we cannot find any idle decoder (all of them are busy!)
                // store them inside a temporal buffer
                if(idle_idx < 0){
                    //printf("Skiping %d subframe since Decoder Blocked! \
                            We suggest increasing the number deocder per cell.\n", sfn*10 + sf_idx);

                    pthread_mutex_lock(&tmp_buf_mutex[rf_idx]);
                    /* Store the data into a tmp buffer. Later, when we have idle decoder, we will decode it*/ 
					//printf("put %d subframe into the buffer\n", sfn*10+sf_idx);
					if(task_sf_ring_buffer_put(&task_tmp_buffer[rf_idx], buffers, sfn, sf_idx, 
								task_scheduler.prog_args.rf_nof_rx_ant, max_num_samples) == 0){
						int nof_buf_sf = task_sf_ring_buffer_len(&task_tmp_buffer[rf_idx]);
						printf("Skip %d subframe ring buf len:%d \n", sfn*10+sf_idx, nof_buf_sf);
						skip_tti_put(&skip_tti[rf_idx], sfn, sf_idx);			
					}
					//int nof_buf_sf = get_nof_buffered_sf(rf_idx);
					int nof_buf_sf = task_sf_ring_buffer_len(&task_tmp_buffer[rf_idx]);
					fprintf(fd,"%d\t%d\t%d\n",task_tmp_buffer[rf_idx].header, task_tmp_buffer[rf_idx].tail, nof_buf_sf);
                    pthread_mutex_unlock(&tmp_buf_mutex[rf_idx]);
                    
                    if((sf_idx == 9)) {
                        sfn++;  // we increase the sfn incase MIB decoding failed
                        if(sfn == 1024){ sfn = 0; }
                    }
					//printf("task -> finish move signal to tmp buf!\n");
  					//t3 = timestamp_us();        
                    continue;
                }else{
					//printf("Directly Assign TTI: %d \n", sf_idx + sfn*10);
                    // Assign the task to the corresponding idle decoder 
                    assign_task_to_decoder(rf_idx, idle_idx, rf_nof_rx_ant, \
									sf_idx, sfn, max_num_samples, sync_buffer);
                }
            }
           	pthread_mutex_lock(&tmp_buf_mutex[rf_idx]);
			int nof_buf_sf = get_nof_buffered_sf(rf_idx);
			fprintf(fd,"%d\t%d\t%d\n",task_tmp_buffer[rf_idx].header, task_tmp_buffer[rf_idx].tail, nof_buf_sf);
           	pthread_mutex_unlock(&tmp_buf_mutex[rf_idx]);

			//printf("task -> end of while!\n");
            if((sf_idx == 9)) {
                sfn++;  // we increase the sfn incase MIB decoding failed
                if(sfn == 1024){ sfn = 0; }
            }// endof if(decode_pdcch)
        }// end of i(ret)
  		//t3 = timestamp_us();        
        //printf("time_spend:%ld (us)\n", t2-t1);

	}// end of while
		
	fclose(fd);
	//fclose(fd_1);

//--> Deal with the exit and free memory 

	// wait until its our turn to close
	printf("wait for scheduler to be ready!\n");

	wait_for_scheduler_ready_to_close(rf_idx);

	task_scheduler_up[rf_idx] = false;

    /* Wait for the decoder thread to finish*/
    for(int i=0;i<nof_decoder;i++){
        // Tell the decoder thread to exit in case 
        // they are still waiting for the signal
		printf("Signling %d-th decoder!\n",i);
        pthread_cond_signal(&sf_buffer[rf_idx][i].sf_cond);
	}

    for(int i=0;i<nof_decoder;i++){
        pthread_join(dci_thd[i], NULL);
    }

	// free the ue dl and the related buffer
    for(int i=0;i<nof_decoder;i++){
        srsran_ue_dl_free(&dci_decoder[i].ue_dl);
        //free the buffer
        for(int j=0;  j < task_scheduler.prog_args.rf_nof_rx_ant; j++){
            free(sf_buffer[rf_idx][i].IQ_buffer[j]);
        }
    } 
        
    srsran_ue_mib_free(&ue_mib);

    // free the ue_sync
    srsran_ue_sync_free(&task_scheduler.ue_sync);

    for(int j=0; j<task_scheduler.prog_args.rf_nof_rx_ant; j++){
        free(sync_buffer[j]);
    }

    radio_stop(&task_scheduler.rf);

	// close the tmp buffer handling thread
    pthread_join(tmp_buf_thd, NULL);

	task_sf_ring_buffer_free(&task_tmp_buffer[rf_idx]);
//
//    for(int i=0; i<MAX_TMP_BUFFER; i++){
//        for (int j = 0; j < SRSRAN_MAX_PORTS; j++) {
//            free(task_tmp_buffer.sf_buf[i].IQ_buffer[j]);
//        }
//    }
// 
    pthread_mutex_lock(&scheduler_close_mutex);
	task_scheduler_closed[rf_idx] = true;
    pthread_mutex_unlock(&scheduler_close_mutex);

    printf("TASK-Scheduler of %d-th RF devices CLOSED!\n", rf_idx);

    return NULL;
}

