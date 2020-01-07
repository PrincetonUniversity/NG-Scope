#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <libconfig.h>
#include "config.h"
int read_config_master(m_config_t* config){
    config_t* cfg = (config_t *)malloc(sizeof(config_t));
    config_init(cfg);
    printf("read master!\n");
    if(! config_read_file(cfg, "./master.cfg")){
	fprintf(stderr, "%s:%d - %s\n", config_error_file(cfg),
		config_error_line(cfg), config_error_text(cfg));
	config_destroy(cfg);
	return(EXIT_FAILURE);
    }
    const char* servIP;
    if(! config_lookup_string(cfg, "servIP", &servIP)){
	printf("ERROR: reading servIP!\n");
    }
    //char servIP_tmp[100];
    //strcpy(servIP_tmp,servIP);
    strcpy(config->remote_servIP,servIP);
    //config->remote_servIP = servIP_tmp;
   
    if(! config_lookup_bool(cfg, "remote_serv_enable", &config->remote_serv_enable)){
	printf("ERROR: reading remote_serv_enable\n");
    }
    
    if(! config_lookup_bool(cfg, "remote_usrp_enable", &config->remote_usrp_enable)){
	printf("ERROR: reading remote_usrp_enable\n");
    }
    printf("remote usrp enable:%d \n", config->remote_usrp_enable);
    if(! config_lookup_bool(cfg, "local_rf_enable", &config->local_rf_enable)){
	printf("ERROR: reading local_rf_enable\n");
    }
    printf("local rf enable:%d \n", config->local_rf_enable);

    if(! config_lookup_int(cfg, "connection_config.sleep", &config->con_sleep_s)){
	printf("ERROR: reading sleep time\n");
    }

    const config_setting_t *tmp_setting; 
    /* connection time in seconds */ 
    tmp_setting		= config_lookup(cfg, "connection_config.con_time_s");
    config->len_time	= config_setting_length(tmp_setting);
    config->con_time_s	= (int*)calloc(config->len_time, sizeof(int));
    for(int i=0; i<config->len_time; i++){
	config->con_time_s[i] =config_setting_get_int_elem(tmp_setting,i);  
    }
    
    /* number of packets to be transmitted*/ 
    tmp_setting		= config_lookup(cfg, "connection_config.nof_pkt");
    config->len_pkt	= config_setting_length(tmp_setting);
    config->nof_pkt	= (int*)calloc(config->len_pkt, sizeof(int));
    for(int i=0; i<config->len_pkt; i++){
	config->nof_pkt[i] =config_setting_get_int_elem(tmp_setting,i);  
    }
    
    /* number of packets to be transmitted*/ 
    tmp_setting		= config_lookup(cfg, "connection_config.pkt_intval");
    config->len_pkt_intval	= config_setting_length(tmp_setting);
    config->pkt_intval	= (int*)calloc(config->len_pkt_intval, sizeof(int));
    for(int i=0; i<config->len_pkt_intval; i++){
	config->pkt_intval[i] =config_setting_get_int_elem(tmp_setting,i);  
    }
    
    if(! config_lookup_int64(cfg, "rf_config.rf_freq", &config->rf_freq)){
	printf("ERROR: reading freq\n");
    }
    
    if(! config_lookup_int(cfg, "rf_config.N_id_2", &config->N_id_2)){
	printf("ERROR: reading N_id_2\n");
    }
    
    if(! config_lookup_int(cfg, "rf_config.usrp_idx", &config->usrp_idx)){
	printf("ERROR: reading usrp_idx\n");
    }

    config_destroy(cfg);
    free(cfg);
    return 0;
}

int read_config_slave(s_config_t* config){
    config_t* cfg = (config_t *)malloc(sizeof(config_t));
    config_init(cfg);
    
    if(! config_read_file(cfg, "./slave.cfg")){
	fprintf(stderr, "%s:%d - %s\n", config_error_file(cfg),
		config_error_line(cfg), config_error_text(cfg));
	config_destroy(cfg);
	return(EXIT_FAILURE);
    }
    
    const char* servIP;
    if(! config_lookup_string(cfg, "servIP", &servIP)){
	    printf("ERROR: reading servIP!\n");
    }
    char servIP_tmp[100];
    strcpy(servIP_tmp,servIP);
    strcpy(config->remote_usrpIP,servIP_tmp);
    //config->remote_usrpIP = servIP_tmp;

    if(! config_lookup_int64(cfg, "rf_freq", &config->rf_freq)){
	printf("ERROR: reading freq\n");
    }
    printf("rf_freq: %ld\n",config->rf_freq);
    if(! config_lookup_int(cfg, "N_id_2", &config->N_id_2)){
	printf("ERROR: reading N_id_2\n");
    }
    
    if(! config_lookup_int(cfg, "usrp_idx", &config->usrp_idx)){
        printf("ERROR: reading usrp_idx\n");
    }

    config_destroy(cfg);
    free(cfg);
    return 0;
}
int get_type_of_pkt_time(m_config_t* config){
    if( (config->len_pkt >1) &&  (config->len_pkt >1) ){
    printf("ERROR: nof_pkt and con_time cannot be set simultaneously!\n");
    return -1;
    }else if((config->len_pkt == 1) &&  (config->len_pkt == 1)){
	int nof_pkt = config->nof_pkt[0]; 
	int time_s  = config->con_time_s[0]; 
	if( (nof_pkt == 0) && (time_s == 0)){
	    return 0;
	}else if((nof_pkt > 0) && (time_s == 0)){
	    return 1;
	}else if((nof_pkt == 0) && (time_s > 0)){
	    return 2;
	}else{
	    return -1;
	}
    }else{
	if(config->len_pkt >1){
	    return 1;
	}
	if(config->len_time >1){
	    return 2;
	}
	return -1;
    }
}

int get_len_of_pkt_time(m_config_t* config){
    int type = get_type_of_pkt_time( config);
    switch (type){
    case -1:
        return -1;
    case 0:
        return 1;
    case 1:
        return config->len_pkt;
    case 2:
        return config->len_time;
    default:
        printf("Configuration is not supported!\n");
        return -1;
    }
}
int set_serv_sock_param(m_config_t* config, sock_parm_t *sock_config, int index){
    int type = get_type_of_pkt_time(config);
    switch (type){
    case -1:
        return -1;
    case 0:
        sock_config->nof_pkt = 0;       
        sock_config->con_time_s = 0;	    
        return 1;
    case 1:
        sock_config->nof_pkt = config->nof_pkt[index];
        sock_config->con_time_s = 0;	    
        return 1;
    case 2:
        sock_config->nof_pkt = 0;       
        sock_config->con_time_s = config->con_time_s[index];        
        return 1;
    default:
        printf("Configuration is not supported!\n");
        return -1;
    }
}


void print_master_config(m_config_t* config){ 
    printf(" \n Master configuation \n");
    printf("server IP: %s\n", config->remote_servIP);
    printf("Connection time (s): ");
    for(int i = 0;i<config->len_time;i++){
        printf("%d\t",config->con_time_s[i]);
    }
    printf("\nNof pkts: ");
    for(int i = 0;i<config->len_pkt;i++){
        printf("%d\t",config->nof_pkt[i]);
    }
    printf("\npkt intval: ");
    for(int i = 0;i<config->len_pkt_intval;i++){
        printf("%d\t", config->pkt_intval[i]);
    }	
    printf("\nrf freq: %ld\n",config->rf_freq); 
    printf("N_id_2: %d\n",config->N_id_2); 
    printf("usrp_idx: %d\n",config->usrp_idx); 

    printf("\nEND of print!\n");
    return;
} 
void log_master_config(m_config_t* config){
    /* clean the log */
    char clean_cmd[256];
    //strcpy(clean_cmd, "./rm_log");
   // if(system(clean_cmd) == -1){
   //     printf("clean log failed\n");
   // }

    FILE* fp;
    fp = fopen("./data/sock_parameter.log","w+");
    fclose(fp);

    fp = fopen("./data/master_config.log","w+");
    fprintf(fp, "Master configuation \n");
    fprintf(fp, "server IP: %s\n", config->remote_servIP);
    fprintf(fp, "Connection time (s)\n");
    for(int i = 0;i<config->len_time;i++){
        fprintf(fp, "%d\t",config->con_time_s[i]);
    }
    fprintf(fp, "\n\nNumber of packets for each transmission\n");
    for(int i = 0;i<config->len_pkt;i++){
        fprintf(fp, "%d\t",config->nof_pkt[i]);
    }
    fprintf(fp, "\n\nInter packet interva\n");
    for(int i = 0;i<config->len_pkt_intval;i++){
        fprintf(fp, "%d\t", config->pkt_intval[i]);
    }	
    fprintf(fp, "\n\nrf freq: %ld\n",config->rf_freq); 
    fprintf(fp, "N_id_2: %d\n",config->N_id_2); 
    fprintf(fp, "usrp_idx: %d\n",config->usrp_idx); 

    fprintf(fp, "\nEND of LOG\n");
    fclose(fp);
    return;
}
void print_slave_config(s_config_t* config){ 
    printf(" \n Slave configuation \n");
    printf("server IP: %s\n", config->remote_usrpIP);
    printf("rf freq: %ld\n",config->rf_freq); 
    printf("N_id_2: %d\n",config->N_id_2); 
    printf("usrp_idx: %d\n",config->usrp_idx); 
}

void log_slave_config(s_config_t* config){
    //char clean_cmd[256];
    //strcpy(clean_cmd, "./rm_log");
    //if(system(clean_cmd) == -1){
    //    printf("clean log failed\n");
    //}
    FILE* fp;
    fp = fopen("./data/slave_config.log","w+"); 
    fprintf(fp, "  Slave configuation \n");
    fprintf(fp, "server IP: %s\n", config->remote_usrpIP);
    fprintf(fp, "rf freq: %ld\n",config->rf_freq); 
    fprintf(fp, "N_id_2: %d\n",config->N_id_2); 
    fclose(fp);
}
