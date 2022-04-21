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

pthread_mutex_t     cell_mutex = PTHREAD_MUTEX_INITIALIZER;
srsran_cell_t       cell_vec[MAX_NOF_RF_DEV];
dci_ready_t         dci_ready = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, 0, 0};

ngscope_status_buffer_t    dci_buffer[MAX_DCI_BUFFER];

int ngscope_main(ngscope_config_t* config, prog_args_t* prog_args){
    int nof_rf_dev;

    // Init the cell
    for(int i=0; i<MAX_NOF_RF_DEV; i++){
        memset(&cell_vec[i], 0, sizeof(srsran_cell_t));
    }

    nof_rf_dev = config->nof_rf_dev;
    prog_args->nof_rf_dev       = nof_rf_dev;

    /* Task scheduler thread */
    pthread_t task_thd[MAX_NOF_RF_DEV];
    for(int i=0; i<nof_rf_dev; i++){
        prog_args->rf_index      = i;
        prog_args->rf_freq       = config->rf_config[i].rf_freq;
        prog_args->force_N_id_2  = config->rf_config[i].N_id_2;
        prog_args->nof_decoder   = config->rf_config[i].nof_thread;
        prog_args->disable_plots = config->rf_config[i].disable_plot;

        prog_args->rf_args    = (char*) malloc(100 * sizeof(char));
        strcpy(prog_args->rf_args, config->rf_config[i].rf_args);
        pthread_create(&task_thd[i], NULL, task_scheduler_thread, (void*)(prog_args));
    }

    pthread_t status_thd;
    prog_args->disable_plots    = config->rf_config[0].disable_plot;
    printf("disable_plots :%d\n", prog_args->disable_plots);

    pthread_create(&status_thd, NULL, status_tracker_thread, (void*)(prog_args));

    /* Now waiting for those threads to end */
    for(int i=0; i<nof_rf_dev; i++){
        pthread_join(task_thd[i], NULL);
    }
    pthread_join(status_thd, NULL);
    return 1;
}
