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
#include <srslte/phy/common/phy_common.h>
#include "srslte/phy/io/filesink.h"
#include "srslte/srslte.h"
#include "srslte/phy/ue/lte_scope.h"
#include "srslte/phy/ue/ue_list.h"
#include "srslte/phy/ue/ue_cell_status.h"

#define ENABLE_AGC_DEFAULT

#include "srslte/phy/rf/rf.h"
#include "srslte/phy/rf/rf_utils.h"

#include "dci_decode_multi_usrp.h"
#include "read_cfg.h"


#define PRINT_CHANGE_SCHEDULIGN

extern float mean_exec_time;

//enum receiver_state { DECODE_MIB, DECODE_PDSCH} state; 
bool go_exit = false; 
bool exit_heartBeat = false;
srslte_ue_cell_usage ue_cell_usage;
enum receiver_state state[MAX_NOF_USRP]; 
srslte_ue_sync_t ue_sync[MAX_NOF_USRP]; 
prog_args_t prog_args[MAX_NOF_USRP]; 
srslte_ue_list_t ue_list[MAX_NOF_USRP];
srslte_cell_t cell[MAX_NOF_USRP];  
srslte_rf_t rf[MAX_NOF_USRP]; 
uint32_t system_frame_number[MAX_NOF_USRP] = { 0, 0, 0, 0, 0 }; // system frame number

int free_order[MAX_NOF_USRP*4] = {0};
pthread_mutex_t mutex_exit = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_usage = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_free_order = PTHREAD_MUTEX_INITIALIZER;

uint16_t targetRNTI_const = 0;
void sig_int_handler(int signo)
{
  printf("SIGINT received. Exiting...\n");
  if (signo == SIGINT) {
    go_exit = true;
  } else if (signo == SIGSEGV) {
    exit(1);
  }
}

int main(int argc, char **argv) {
    //srslte_debug_handle_crash(argc, argv);
    srslte_config_t main_config;
    read_config_master(&main_config);
    int nof_usrp;
    nof_usrp = main_config.nof_usrp;
    srslte_UeCell_init(&ue_cell_usage);
    srslte_UeCell_set_logFlag(&ue_cell_usage, false);
    targetRNTI_const = 0;
   
    FILE *FD_DCI;
    FD_DCI  = fopen("./dci_log", "w+");
    srslte_UeCell_set_file_descriptor(&ue_cell_usage, FD_DCI);
    srslte_UeCell_set_logFlag(&ue_cell_usage, true); 
    srslte_UeCell_set_printFlag(&ue_cell_usage, true); 
    srslte_UeCell_set_targetRNTI(&ue_cell_usage, targetRNTI_const);
    srslte_UeCell_set_nof_cells(&ue_cell_usage, nof_usrp);
    
    for(int i=0;i<nof_usrp;i++){
	// INIT important structures
	srslte_init_ue_list(&ue_list[i]);  
	args_default(&prog_args[i]);

	prog_args[i].rf_freq = main_config.usrp_config[i].rf_freq;
	prog_args[i].nof_thread = main_config.usrp_config[i].nof_thread;
	prog_args[i].rf_args	= malloc(100 * sizeof(char));
	strcpy(prog_args[i].rf_args, main_config.usrp_config[i].rf_args);
	srslte_UeCell_set_nof_thread(&ue_cell_usage, prog_args[i].nof_thread, i);
    }

    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigprocmask(SIG_UNBLOCK, &sigset, NULL);
    signal(SIGINT, sig_int_handler);

    int usrp_idx[MAX_NOF_USRP];
    pthread_t usrp_thd[MAX_NOF_USRP];

    int count = 0;
    for(int i=0;i<nof_usrp;i++){
	usrp_idx[i] = i;
	pthread_create( &usrp_thd[i], NULL, dci_start_usrp, (void *)&usrp_idx[i]);
	for(int j=0;j<prog_args[i].nof_thread;j++){
	    free_order[count] = 1;
	    count++;
	}
    }	

    for(int i=0;i<nof_usrp;i++){
	pthread_join(usrp_thd[i], NULL);
    }

    printf("\nBye MAIN FUNCTION!\n");
    exit(0);
}

