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
#include "ngscope/hdr/dciLib/ngscope_def.h"
#include "ngscope/hdr/dciLib/load_config.h"
#include "ngscope/hdr/dciLib/status_tracker.h"
#include "ngscope/hdr/dciLib/cell_status.h"

pthread_mutex_t     cell_mutex = PTHREAD_MUTEX_INITIALIZER;
srsran_cell_t       cell_vec[MAX_NOF_RF_DEV];


// DCI from dci_decoder -> status_tracker
dci_ready_t         dci_ready = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, 0, 0};
ngscope_status_buffer_t    dci_buffer[MAX_DCI_BUFFER];

// DCI from status_tracker -> cell_status 
cell_status_buffer_t cell_stat_buffer[MAX_DCI_BUFFER];
dci_ready_t          cell_stat_ready = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, 0, 0};

// DCI status
cell_status_t 		cell_status[MAX_NOF_RF_DEV];
CA_status_t   		ca_status;
pthread_mutex_t     cell_status_mutex = PTHREAD_MUTEX_INITIALIZER;

// UE list
ngscope_ue_list_t 	ue_list[MAX_NOF_RF_DEV];

int ngscope_main(ngscope_config_t* config){
    int nof_rf_dev;

	prog_args_t prog_args[MAX_NOF_RF_DEV];
    // default parameters

    // Init the cell
    for(int i=0; i<MAX_NOF_RF_DEV; i++){
    	args_default(&prog_args[i]);
        memset(&cell_vec[i], 0, sizeof(srsran_cell_t));
    }

    nof_rf_dev = config->nof_rf_dev;

    /* Task scheduler thread */
    pthread_t task_thd[MAX_NOF_RF_DEV];
    for(int i=0; i<nof_rf_dev; i++){

		prog_args[i].nof_rf_dev       = nof_rf_dev;
		prog_args[i].log_dl           = config->dci_log_config.log_dl;
		prog_args[i].log_ul           = config->dci_log_config.log_ul;
		prog_args[i].rnti             = (uint16_t)config->rnti;
		prog_args[i].remote_enable    = config->remote_enable;

        prog_args[i].rf_index      = i;
        prog_args[i].rf_freq       = config->rf_config[i].rf_freq;
        prog_args[i].rf_freq_vec[i]= config->rf_config[i].rf_freq;

        prog_args[i].force_N_id_2  = config->rf_config[i].N_id_2;
        prog_args[i].nof_decoder   = config->rf_config[i].nof_thread;
        prog_args[i].disable_plots = config->rf_config[i].disable_plot;
        
        prog_args[i].rf_args    = (char*) malloc(100 * sizeof(char));
        strcpy(prog_args[i].rf_args, config->rf_config[i].rf_args);
        pthread_create(&task_thd[i], NULL, task_scheduler_thread, (void*)( &prog_args[i] ));
    }

    pthread_t status_thd;
    prog_args[0].disable_plots    = config->rf_config[0].disable_plot;
    printf("disable_plots :%d\n", prog_args[0].disable_plots);

    pthread_create(&status_thd, NULL, status_tracker_thread, (void*)(&(prog_args[0])));

    /* Now waiting for those threads to end */
    for(int i=0; i<nof_rf_dev; i++){
        pthread_join(task_thd[i], NULL);
    }
    pthread_join(status_thd, NULL);
    return 1;
}
