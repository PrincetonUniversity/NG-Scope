/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2015 Software Radio Systems Limited
 *
 * \section LICENSE
 *
 * This file is part of the srsLTE library.
 *
 * srsLTE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsLTE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

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
#include "srslte/phy/common/phy_common.h"
#include "srslte/phy/io/filesink.h"
#include "srslte/srslte.h"
#include "srslte/phy/ue/lte_scope.h"
#include "srslte/phy/ue/ue_list.h"

#define ENABLE_AGC_DEFAULT

#include "srslte/phy/rf/rf.h"
#include "srslte/phy/rf/rf_utils.h"

#include "dci_decoder.h"
#include "dci_usrp.h"
#include "dci_ue_status.h"

#include "load_config.h"

#include "ue_cell_status.h"
#include "status_main.h"
#include "arg_parser.h"

#include "srsgui/srsgui.h"
void init_plots();
pthread_t plot_thread;


#define PRINT_CHANGE_SCHEDULIGN

extern bool go_exit; 

srslte_config_t main_config;		// configuration (which is stored inside the configuration file .cfg)
uint16_t targetRNTI_const = 0;		// target RNTI


pthread_t usrp_thd[MAX_NOF_USRP];	// the USRP thread -- one per usrp (multiple decoder thread per usrp)
pthread_t ue_status_thd;		// the ue status update thread

srslte_ue_cell_usage ue_cell_usage;	// UE cell usage status

bool logDL_flag = false;    // Highly likely to be deleted
bool logUL_flag = false;


bool	all_RF_ready = false;
pthread_mutex_t mutex_RF_ready   = PTHREAD_MUTEX_INITIALIZER;

int	cell_prb_G[MAX_NOF_USRP] = {0};
int	nof_tx_ant_G[MAX_NOF_USRP] = {0};
bool	rf_ready_G[MAX_NOF_USRP] = {0};
pthread_mutex_t mutex_RF_config   = PTHREAD_MUTEX_INITIALIZER;


lteCCA_status_t ue_status_t;

enum receiver_state state[MAX_NOF_USRP]; 
srslte_ue_sync_t    ue_sync[MAX_NOF_USRP]; 
prog_args_t	    prog_args[MAX_NOF_USRP]; 
srslte_ue_list_t    ue_list[MAX_NOF_USRP];
srslte_cell_t	    cell[MAX_NOF_USRP];  
srslte_rf_t	    rf[MAX_NOF_USRP]; 

cf_t csi[100*12];
cf_t csi_mat[MAX_NOF_USRP][100*12];


uint32_t system_frame_number[MAX_NOF_USRP] = { 0, 0, 0, 0, 0 }; // system frame number

int free_order[MAX_NOF_USRP*4] = {0};

pthread_mutex_t mutex_csi   = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_exit  = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_usage = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t mutex_free_order = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t mutex_dl_flag;
pthread_mutex_t mutex_ul_flag;

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

void sig_int_handler(int signo)
{
  printf("SIGINT received. Exiting...\n");
  if (signo == SIGINT) {
    go_exit = true;
  } else if (signo == SIGSEGV) {
    exit(1);
  }
}

static void reset_csi(cf_t* csi, int nof_point){
    for(int i=0;i<nof_point;i++){
	csi[i] = 0;
    }   
} 
static void check_RF_ready();

/* Set up the decoder accroding to the configurations 
 * Most of the configurations are stored in the configuration file
 */
int lteScope_init() {
    //srslte_debug_handle_crash(argc, argv);

    // read the configurations from the cfg file
    read_config_master(&main_config);

    int nof_usrp;
    nof_usrp = main_config.nof_usrp;

    // Init the ue cell usage related structure
    cell_status_init(&ue_cell_usage);	    // init the cell status 
    cell_status_set_nof_cells(&ue_cell_usage, nof_usrp);    // set the nof usrp
    for(int i=0;i<nof_usrp;i++){
	cell_status_set_rx_ant(&ue_cell_usage, main_config.usrp_config[i].rx_ant, i);    // set the nof rx antenna 
    }


    targetRNTI_const = 0;
    for(int i=0;i<nof_usrp;i++){
	// INIT important structures
	srslte_init_ue_list(&ue_list[i]);   // init ue list 
	args_default(&prog_args[i]);	    // init args 

	// read the rf configurations of the USRPs
	prog_args[i].rf_freq	= main_config.usrp_config[i].rf_freq;
	prog_args[i].nof_thread = main_config.usrp_config[i].nof_thread;
	prog_args[i].rf_args	= (char*) malloc(100 * sizeof(char));

	prog_args[i].rf_nof_rx_ant	= main_config.usrp_config[i].rx_ant;

	strcpy(prog_args[i].rf_args, main_config.usrp_config[i].rf_args);
	
	// set the nof decoder threads for each usrp
	cell_status_set_nof_thread(&ue_cell_usage, prog_args[i].nof_thread, i);
	
	// reset csi matrix
	reset_csi(csi_mat[i], 1200);
    }
    return 0;
}

/*  Start one thread to handle the decoding of each USRP (one USRP per carrier)
 *  NOTE: each USRP handling thread may create more decoding threads
 */
int lteScope_start() {
    int nof_usrp;
    nof_usrp = main_config.nof_usrp;

    int usrp_idx[MAX_NOF_USRP];
    int count = 0;

    //start plotting threads
    //init_plots();

    for(int i=0;i<nof_usrp;i++){
	usrp_idx[i] = i;
	pthread_create( &usrp_thd[i], NULL, dci_start_usrp, (void *)&usrp_idx[i]);
	for(int j=0;j<prog_args[i].nof_thread;j++){
	    free_order[count] = 1;
	    count++;
	}
    }
    printf("Check RF\n");
    // check if RF is ready or not
    check_RF_ready();
    printf("END of Check RF\n");

    int ue_status_idx = 0;	
    printf("create ue_status_thd\n");
    pthread_create(&ue_status_thd, NULL, dci_ue_status_update, (void *)&ue_status_idx);
    printf("END of create ue_status_thd\n");

    return 0;
}

/* wait until all the decoding threads finish */
int lteScope_wait_to_close() {
    int nof_usrp;
    nof_usrp = main_config.nof_usrp;

    // USRP thread
    for(int i=0;i<nof_usrp;i++){
	pthread_join(usrp_thd[i], NULL);
    }

    // clear the ue_status matrix 
    cell_status_exit_free_csi_matrix(&ue_cell_usage);

    // UE status update thread
    //pthread_join(ue_status_thd, NULL);
    
    printf("\nBye MAIN FUNCTION!\n");
    exit(0);
}

static bool check_RF(){
    int nof_usrp;
    bool ready = true;

    nof_usrp = main_config.nof_usrp;
    printf("Check_RF: nof_usrp: %d\n",nof_usrp);
    pthread_mutex_lock( &mutex_RF_config);
    for(int i=0;i<nof_usrp;i++){
	if(rf_ready_G[i] == false){
	    ready = false;
	    break;
	}
    } 
    pthread_mutex_unlock( &mutex_RF_config);
    return ready;
}

static void set_RF_parameters(){
    int nof_usrp;
    nof_usrp = main_config.nof_usrp;

    // set up the RF parameters for ue_cell_usage
    pthread_mutex_lock(&mutex_usage);
    for(int i=0;i<nof_usrp;i++){
	cell_status_set_prb(&ue_cell_usage, cell_prb_G[i], i);
	cell_status_set_tx_ant(&ue_cell_usage, nof_tx_ant_G[i], i);
    }
    cell_status_allocate_csi_matrix(&ue_cell_usage);
    pthread_mutex_unlock(&mutex_usage);


}
static void check_RF_ready(){
    while(true){
        pthread_mutex_lock( &mutex_exit);
        if(go_exit){
            pthread_mutex_unlock( &mutex_exit);
            break;
        }
        pthread_mutex_unlock( &mutex_exit);

	if(check_RF()){
	    set_RF_parameters();    // set the parameters for ue_cell_usage
	    pthread_mutex_lock(&mutex_RF_ready);
	    all_RF_ready = true;    
	    pthread_mutex_unlock(&mutex_RF_ready);
	    break;
	}else{
	    printf("CHeck RF sleep and wait!\n");
	    sleep(1);
	}
    }
    return;
}
