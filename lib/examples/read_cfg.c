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
    
    if(! config_lookup_int(cfg, "nof_usrp", &config->nof_usrp)){
        printf("ERROR: reading nof_usrp\n");
    }
    printf("read nof usrp:%d\n", config->nof_usrp);
    for(int i=0;i<config->nof_usrp;i++){
	char name[50];

	sprintf(name, "usrp_config%d.rf_freq",i);
	if(! config_lookup_int64(cfg, name, &config->usrp_config[i].rf_freq)){
	    printf("ERROR: reading nof_usrp\n");
	}else{
	    printf("rf_freq:%ld ",config->usrp_config[i].rf_freq);
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
	    printf("ERROR: reading servIP!\n");
	}else{
	    strcpy(config->usrp_config[i].rf_args, rf_args);
	    printf("rf_args:%s ",config->usrp_config[i].rf_args);
	}
	printf("\n");
    }
}
