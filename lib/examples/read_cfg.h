#ifndef _CONFIG_H_
#define _CONFIG_H_
#include <stdio.h>
typedef struct{
    long long  rf_freq;
    int  N_id_2;
    char rf_args[100];
    int nof_thread;
}usrp_config_t;



typedef struct{
    int nof_usrp;
    usrp_config_t usrp_config[5];
}srslte_config_t;

int read_config_master(srslte_config_t* config);

#endif

