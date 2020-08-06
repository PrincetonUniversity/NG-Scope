#ifndef _CONFIG_H_
#define _CONFIG_H_
#include <stdio.h>
#include "srslte/phy/ue/LTESCOPE_GLOBAL.h"
typedef struct{
    long long  rf_freq;
    int  N_id_2;
    char rf_args[100];
    int nof_thread;
}usrp_config_t;
typedef struct{
    int	nof_cell;
    int nameFile_time;

    int log_dl_flag;
    int dl_single_file;
    int log_ul_flag;
    int ul_single_file;
}rawLog_config_t;

typedef struct{
    int nof_usrp;
    char servIP[100];
    int  con_time_s;
    usrp_config_t usrp_config[MAX_NOF_USRP];
    rawLog_config_t  rawLog_config;
 
}srslte_config_t;


int read_config_master(srslte_config_t* config);

#endif

