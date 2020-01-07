#ifndef _CONFIG_H_ 
#define _CONFIG_H_ 
#include <stdio.h>
#include "serv_sock.h"
typedef struct{
    //char *remote_servIP;
    char remote_servIP[100];
    int *con_time_s;
    int len_time;
    
    int *nof_pkt;
    int len_pkt;
    
    int *pkt_intval;
    int len_pkt_intval;
    int con_sleep_s;

    long long  rf_freq;
    int  N_id_2;
    int  usrp_idx;
    int  remote_serv_enable;
    int  remote_usrp_enable;
    int  local_rf_enable;

}m_config_t;

typedef struct{
    char remote_usrpIP[100];
    long long  rf_freq;
    int  N_id_2;
    int  usrp_idx;
}s_config_t;

int read_config_master(m_config_t* config);
int read_config_slave(s_config_t* config);

int get_len_of_pkt_time(m_config_t* config);
int set_serv_sock_param(m_config_t* config, sock_parm_t *sock_config, int index);

void log_master_config(m_config_t* config);
void log_slave_config(s_config_t* config);

void print_master_config(m_config_t* config);
void print_slave_config(s_config_t* config);
#endif
