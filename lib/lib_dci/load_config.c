#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <libconfig.h>
#include "load_config.h"

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
    config->rawLog_config.nof_cell = config->nof_usrp;
    // read raw_log_configurations
    if(! config_lookup_bool(cfg, "rawLog_config.nameFile_time", &config->rawLog_config.nameFile_time)){
		printf("ERROR: reading fileName_time\n");
    }
    if(! config_lookup_bool(cfg, "rawLog_config.log_dl", &config->rawLog_config.log_dl_flag)){
		printf("ERROR: reading log_dl_flag\n");
    }
    if(! config_lookup_bool(cfg, "rawLog_config.dl_single_file", &config->rawLog_config.dl_single_file)){
		printf("ERROR: reading dl_single_file\n");
    } 
    if(! config_lookup_bool(cfg, "rawLog_config.log_ul", &config->rawLog_config.log_ul_flag)){
		printf("ERROR: reading log_ul_flag\n");
    }
    if(! config_lookup_bool(cfg, "rawLog_config.ul_single_file", &config->rawLog_config.ul_single_file)){
		printf("ERROR: reading ul_single_file\n");
    }
	if(! config_lookup_bool(cfg, "rawLog_config.repeat_flag", &config->rawLog_config.repeat_flag)){
		printf("ERROR: reading repeat_flag\n");
    }
	if(! config_lookup_int(cfg, "rawLog_config.repeat_log_time_ms", &config->rawLog_config.repeat_log_time_ms)){
		printf("ERROR: reading repeat_log_time_ms\n");
    }
	if(! config_lookup_int(cfg, "rawLog_config.total_log_time_ms", &config->rawLog_config.total_log_time_ms)){
		printf("ERROR: reading total_log_time_ms\n");
    } 
 
    return 0;
}

