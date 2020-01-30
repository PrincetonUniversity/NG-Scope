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
    char servIP[100];
    usrp_config_t usrp_config[5];
}srslte_config_t;


typedef struct{
    int nof_cca;
    int* cca_idx;
}cca_cfg_t;

typedef struct{
    int remote;
}iperf_cfg_t;

typedef struct{
    int nof_pkt;
    int nof_test;
    int nof_pkt_intval;
    int *pkt_intval;
    int nof_con_time;
    int *con_time;
}sock_cfg_t;

int read_config_master(srslte_config_t* config);
int read_cca_config(cca_cfg_t* config);
int read_iperf_config(iperf_cfg_t* config);
int read_sock_config(sock_cfg_t* config);

#endif

