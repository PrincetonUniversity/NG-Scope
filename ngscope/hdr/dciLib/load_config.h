#ifndef _CONFIG_H_
#define _CONFIG_H_
#include <stdio.h>
#include "task_scheduler.h"

typedef struct{
    long long   rf_freq;
    int         N_id_2;
    char        rf_args[100];
    int         nof_thread;
    int         disable_plot;
	int 		log_dl;
	int			log_ul;
}rf_dev_config_t;

typedef struct{
    int     nof_cell;
    int     log_ul;
    int     log_dl; 
}dci_log_config_t;


typedef struct{
    int                 nof_rf_dev;
    int                 rnti;
    int                 remote_enable;
	int 				decode_single_ue;

    dci_log_config_t    dci_log_config;
    rf_dev_config_t     rf_config[MAX_NOF_RF_DEV];
}ngscope_config_t;

int ngscope_read_config(ngscope_config_t* config);
bool ngscope_config_check_log(ngscope_config_t* config);
#endif
