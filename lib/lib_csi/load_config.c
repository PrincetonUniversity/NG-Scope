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
    }else{
	printf("read nof usrp:%d\n", config->nof_usrp);
    }
    
    if(! config_lookup_int(cfg, "con_time_s", &config->con_time_s)){
        printf("ERROR: reading con_time_s\n");
    }else{
	printf("read connection time:%d\n", config->con_time_s);
    }
    
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
	
	sprintf(name, "usrp_config%d.rx_ant",i);
	if(! config_lookup_int(cfg, name, &config->usrp_config[i].rx_ant)){
	    printf("ERROR: reading nof_rx_ant\n");
	}else{
	    printf("rx_ant:%d ",config->usrp_config[i].rx_ant);
	}

	config->csiLog_config.rx_ant[i] = config->usrp_config[i].rx_ant;

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
    }else{
	printf("nameFile_time:%d\n", config->rawLog_config.nameFile_time);
    }

    if(! config_lookup_bool(cfg, "rawLog_config.log_dl", &config->rawLog_config.log_dl_flag)){
	printf("ERROR: reading log_dl_flag\n");
    }else{
	printf("log_dl_flag%d\n", config->rawLog_config.log_dl_flag);
    }

    if(! config_lookup_bool(cfg, "rawLog_config.dl_single_file", &config->rawLog_config.dl_single_file)){
	printf("ERROR: reading dl_single_file\n");
    }else{
	printf("dl_single_file%d\n", config->rawLog_config.dl_single_file);
    }

    if(! config_lookup_bool(cfg, "rawLog_config.log_ul", &config->rawLog_config.log_ul_flag)){
	printf("ERROR: reading log_ul_flag\n");
    }else{
	printf("log_ul_flag%d\n", config->rawLog_config.log_ul_flag);
    }

    if(! config_lookup_bool(cfg, "rawLog_config.ul_single_file", &config->rawLog_config.ul_single_file)){
	printf("ERROR: reading ul_single_file\n");
    }else{
	printf("ul_single_file%d\n", config->rawLog_config.ul_single_file);
    }

    
    // read csi_log_configurations

    config->csiLog_config.nof_cell = config->nof_usrp;

    if(! config_lookup_int(cfg, "csiLog_config.format", &config->csiLog_config.format)){
	printf("ERROR: reading csi save file format\n");
    }else{
	printf("csi save file format:%d\n", config->csiLog_config.format);
    }

    if(! config_lookup_bool(cfg, "csiLog_config.log_amp_flag", &config->csiLog_config.log_amp_flag)){
	printf("ERROR: reading log_amp_flag\n");
    }else{
	printf("log_amp_flag:%d\n", config->csiLog_config.log_amp_flag);
    }

    if(! config_lookup_bool(cfg, "csiLog_config.log_phase_flag", &config->csiLog_config.log_phase_flag)){
	printf("ERROR: reading log_phase_flag\n");
    }else{
	printf("log_phase_flag:%d\n", config->csiLog_config.log_phase_flag);
    }
    
    if(! config_lookup_int(cfg, "csiLog_config.down_sample_subcarrier", &config->csiLog_config.down_sample_subcarrier)){
	printf("ERROR: reading down_sample_subcarrier\n");
    }else{
	printf("down_sample_subcarrier:%d\n", config->csiLog_config.down_sample_subcarrier);
    }

    if(! config_lookup_int(cfg, "csiLog_config.down_sample_subframe", &config->csiLog_config.down_sample_subframe)){
	printf("ERROR: reading down_sample_subframe\n");
    }else{
	printf("down_sample_subframe:%d\n", config->csiLog_config.down_sample_subframe);
    }

    if(! config_lookup_bool(cfg, "csiLog_config.repeat_flag", &config->csiLog_config.repeat_flag)){
	printf("ERROR: reading repeat_flag\n");
    }else{
	printf("repeat_flag:%d\n", config->csiLog_config.repeat_flag);
    }

    if(! config_lookup_int(cfg, "csiLog_config.repeat_log_intval_s", &config->csiLog_config.repeat_log_intval_s)){
	printf("ERROR: reading repeat_log_intval_s\n");
    }else{
	printf("repeat_log_intval_s:%d\n", config->csiLog_config.repeat_log_intval_s);
    }

    if(! config_lookup_int(cfg, "csiLog_config.repeat_pause_intval_s", &config->csiLog_config.repeat_pause_intval_s)){
	printf("ERROR: reading repeat_pause_intval_s\n");
    }else{
	printf("repeat_pause_intval_s:%d\n", config->csiLog_config.repeat_pause_intval_s);
    }

    // read csi_forward_config
    if(! config_lookup_bool(cfg, "csi_forward_config.forward_flag", &config->forward_config.forward_flag)){
	printf("ERROR: reading forward_flag\n");
    }else{
	printf("forward_flag:%d\n", config->forward_config.forward_flag);
    }
    //const char* servIP;
    if(! config_lookup_string(cfg, "csi_forward_config.remoteIP", &servIP)){
	printf("ERROR: reading remoteIP\n");
    } 
    strcpy(config->forward_config.remoteIP, servIP);

    return 0;
}

