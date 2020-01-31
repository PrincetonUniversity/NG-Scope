#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <libconfig.h>
#include "read_cfg.h"

int read_config_master(srslte_config_t* config){
    config_t* cfg = (config_t *)malloc(sizeof(config_t));
    config_init(cfg);

    printf("read config!\n");
    if(! config_read_file(cfg, "./config.cfg")){
        fprintf(stderr, "%s:%d - %s\n", config_error_file(cfg),
                config_error_line(cfg), config_error_text(cfg));
        config_destroy(cfg);
        return(EXIT_FAILURE);
    }

    const char* servIP;
    if(! config_lookup_string(cfg, "servIP", &servIP)){
	printf("ERROR: reading servIP\n");
    } 
    strcpy(config->servIP, servIP);

    if(! config_lookup_int(cfg, "nof_usrp", &config->nof_usrp)){
        printf("ERROR: reading nof_usrp\n");
    }
    printf("read nof usrp:%d\n", config->nof_usrp);
    
    if(! config_lookup_int(cfg, "con_time_s", &config->con_time_s)){
        printf("ERROR: reading con_time_s\n");
    }
    printf("read connection time:%d\n", config->con_time_s);
    
    for(int i=0;i<config->nof_usrp;i++){
	char name[50];

	sprintf(name, "usrp_config%d.rf_freq",i);
	if(! config_lookup_int64(cfg, name, &config->usrp_config[i].rf_freq)){
	    printf("ERROR: reading nof_usrp\n");
	}else{
	    printf("rf_freq:%lld ",config->usrp_config[i].rf_freq);
	}
	    
	sprintf(name, "usrp_config%d.N_id_2",i);
	if(! config_lookup_int(cfg, name, &config->usrp_config[i].N_id_2)){
	    printf("ERROR: reading nof_usrp\n");
	}else{
	    printf("N_id_2:%d ",config->usrp_config[i].N_id_2);
	}

	sprintf(name, "usrp_config%d.nof_thread",i);
	if(! config_lookup_int(cfg, name, &config->usrp_config[i].nof_thread)){
	    printf("ERROR: reading nof_usrp\n");
	}else{
	    printf("nof_thread:%d ",config->usrp_config[i].nof_thread);
	}

	sprintf(name, "usrp_config%d.rf_args",i);
	const char* rf_args;
	if(! config_lookup_string(cfg, name, &rf_args)){
	    printf("ERROR: reading rf_args!\n");
	}else{
	    strcpy(config->usrp_config[i].rf_args, rf_args);
	    printf("rf_args:%s ",config->usrp_config[i].rf_args);
	}
	printf("\n");
    }
    return 0;
}

int read_cca_config(cca_cfg_t* config){
    config_t* cfg = (config_t *)malloc(sizeof(config_t));
    config_init(cfg);

    printf("read config!\n");
    if(! config_read_file(cfg, "./config.cfg")){
        fprintf(stderr, "%s:%d - %s\n", config_error_file(cfg),
                config_error_line(cfg), config_error_text(cfg));
        config_destroy(cfg);
        return(EXIT_FAILURE);
    }
    
    const config_setting_t *tmp_setting;
    /* connection time in seconds */
    tmp_setting         = config_lookup(cfg, "cca_idx");
    config->nof_cca     = config_setting_length(tmp_setting);
    config->cca_idx	= (int*)calloc(config->nof_cca, sizeof(int));

    for(int i=0; i<config->nof_cca; i++){
        config->cca_idx[i] =config_setting_get_int_elem(tmp_setting,i);
    }
    return 0;
}

int read_iperf_config(iperf_cfg_t* config){
    config_t* cfg = (config_t *)malloc(sizeof(config_t));
    config_init(cfg);

    printf("read config!\n");
    if(! config_read_file(cfg, "./config.cfg")){
        fprintf(stderr, "%s:%d - %s\n", config_error_file(cfg),
                config_error_line(cfg), config_error_text(cfg));
        config_destroy(cfg);
        return(EXIT_FAILURE);
    }
    if(! config_lookup_bool(cfg, "iperf_remote", &config->remote)){
        printf("ERROR: reading iperf\n");
    } 
    return 0;
}

int read_sock_config(sock_cfg_t* config){
    config_t* cfg = (config_t *)malloc(sizeof(config_t));
    config_init(cfg);

    printf("read sock config!\n");
    if(! config_read_file(cfg, "./config.cfg")){
	printf("ERROR");
        fprintf(stderr, "%s:%d - %s\n", config_error_file(cfg),
                config_error_line(cfg), config_error_text(cfg));
	printf("ERROR");
        config_destroy(cfg);
        return(EXIT_FAILURE);
    }
    
    if(! config_lookup_int(cfg, "nof_pkt", &config->nof_pkt)){
        printf("ERROR: reading nof_pkt\n");
    }
    if(! config_lookup_int(cfg, "nof_test", &config->nof_test)){
        printf("ERROR: reading nof_test\n");
    }
    const config_setting_t *tmp_setting;
    /* connection time in seconds */
    tmp_setting         = config_lookup(cfg, "pkt_intval");
    config->nof_pkt_intval  = config_setting_length(tmp_setting);
    config->pkt_intval	    = (int*)calloc(config->nof_pkt_intval, sizeof(int));
    for(int i=0; i<config->nof_pkt_intval; i++){
        config->pkt_intval[i] =config_setting_get_int_elem(tmp_setting,i);
    }
    
    /* connection time in seconds */
    tmp_setting		    = config_lookup(cfg, "con_time");
    config->nof_con_time    = config_setting_length(tmp_setting);
    config->con_time	    = (int*)calloc(config->nof_con_time, sizeof(int));
    for(int i=0; i<config->nof_con_time; i++){
        config->con_time[i] =config_setting_get_int_elem(tmp_setting,i);
    }
    printf("B");
    return 0;
}
