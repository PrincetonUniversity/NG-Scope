#ifndef _CONFIG_H_
#define _CONFIG_H_
#include <stdio.h>
#include "srslte/phy/ue/LTESCOPE_GLOBAL.h"
typedef struct{
    long long  rf_freq;
    int  N_id_2;
    int	 rx_ant;
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
    int	nof_cell;
    int format;
    int log_amp_flag;
    int log_phase_flag;
    int down_sample_subcarrier;
    int down_sample_subframe;
    int	rx_ant[MAX_NOF_USRP];

    int repeat_flag;
    int repeat_log_intval_s;
    int repeat_pause_intval_s;

}csiLog_config_t;
typedef struct{
    int forward_flag;
    char remoteIP[100];
}csi_forward_config_t;


typedef struct{
    int nof_usrp;
    char servIP[100];
    int  con_time_s;
    usrp_config_t usrp_config[MAX_NOF_USRP];
    rawLog_config_t  rawLog_config;
    csiLog_config_t  csiLog_config;
    csi_forward_config_t forward_config;

}srslte_config_t;


int read_config_master(srslte_config_t* config);

#endif

