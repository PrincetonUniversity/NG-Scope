#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <libconfig.h>
#include "ngscope/hdr/dciLib/load_config.h"

int compar(const void* a,const void* b)
{
    return (*(long long*)a - *(long long*)b);
}

bool containsDuplicate(long long* nums, int numsSize){
    int i,j;
    qsort(nums, numsSize,sizeof(long long),compar);
    for(i = 0,j = 1;j < numsSize;i++,j++)
    {
        if(nums[i] == nums[j])
        {
            return true;
        }
    }
    return false;
}

int ngscope_read_config(ngscope_config_t* config){
    config_t* cfg = (config_t *)malloc(sizeof(config_t));
    config_init(cfg);

    printf("read config!\n");
    if(! config_read_file(cfg, "./config.cfg")){
        fprintf(stderr, "%s:%d - %s\n", config_error_file(cfg),
                config_error_line(cfg), config_error_text(cfg));
        config_destroy(cfg);
        return(EXIT_FAILURE);
    }

//    const char* servIP;
//    if(! config_lookup_string(cfg, "servIP", &servIP)){
//    printf("ERROR: reading servIP\n");
//    }
//    strcpy(config->servIP, servIP);
//
    if(! config_lookup_int(cfg, "nof_rf_dev", &config->nof_rf_dev)){
        printf("ERROR: reading nof_rf_dev\n");
    }
    printf("read nof rf_dev:%d\n", config->nof_rf_dev);
    
    if(! config_lookup_int(cfg, "rnti", &config->rnti)){
        printf("ERROR: reading rnti\n");
    }
    printf("read nof rf_dev:%d\n", config->rnti);
     
    if(! config_lookup_bool(cfg, "remote_enable", &config->remote_enable)){
        printf("ERROR: reading remote_enable\n");
    }
    printf("read remote_enable:%d\n", config->remote_enable);
    
	if(! config_lookup_bool(cfg, "decode_single_ue", &config->decode_single_ue)){
        printf("ERROR: reading decode_single_ue\n");
    }
    printf("read decode_single_ue:%d\n", config->decode_single_ue);

	long long* freq_vec = (long long*) malloc(config->nof_rf_dev * sizeof(long long));

//    if(! config_lookup_int(cfg, "con_time_s", &config->con_time_s)){
//        printf("ERROR: reading con_time_s\n");
//    }
//    printf("read connection time:%d\n", config->con_time_s);
//

    for(int i=0;i<config->nof_rf_dev;i++){

        char name[50];
        sprintf(name, "rf_config%d.rf_freq",i);
        if(! config_lookup_int64(cfg, name, &config->rf_config[i].rf_freq)){
            printf("ERROR: reading nof_rf_dev\n");
        }else{
            printf("rf_freq:%lld ",config->rf_config[i].rf_freq);
        }
		// store the data inside the vector
		freq_vec[i] = config->rf_config[i].rf_freq;

        sprintf(name, "rf_config%d.N_id_2",i);
        if(! config_lookup_int(cfg, name, &config->rf_config[i].N_id_2)){
            printf("ERROR: reading N_id_2\n");
        }else{
            printf("N_id_2:%d ",config->rf_config[i].N_id_2);
        }

        sprintf(name, "rf_config%d.nof_thread",i);
        if(! config_lookup_int(cfg, name, &config->rf_config[i].nof_thread)){
            printf("ERROR: reading nof_thread\n");
        }else{
            printf("nof_thread:%d ",config->rf_config[i].nof_thread);
        }

        sprintf(name, "rf_config%d.rf_args",i);
        const char* rf_args;
        if(! config_lookup_string(cfg, name, &rf_args)){
            printf("ERROR: reading rf_args!\n");
        }else{
            strcpy(config->rf_config[i].rf_args, rf_args);
            printf("rf_args:%s ",config->rf_config[i].rf_args);
        }
        printf("\n");

        sprintf(name, "rf_config%d.disable_plot",i);
        if(! config_lookup_bool(cfg, name, &config->rf_config[i].disable_plot)){
            printf("ERROR: reading disable_plot\n");
        }else{
            printf("disable plot: %d\n", config->rf_config[i].disable_plot);
        }

        sprintf(name, "rf_config%d.log_dl",i);
		if(! config_lookup_bool(cfg, name, &config->rf_config[i].log_dl)){
            printf("ERROR: reading log_dl\n");
        }else{
            printf("log dl dci: %d\n", config->rf_config[i].log_dl);
        }

        sprintf(name, "rf_config%d.log_ul",i);
		if(! config_lookup_bool(cfg, name, &config->rf_config[i].log_ul)){
            printf("ERROR: reading log_ul\n");
        }else{
            printf("log dl dci: %d\n", config->rf_config[i].log_dl);
        }

    }

	if(containsDuplicate(freq_vec, config->nof_rf_dev)){
		printf("Two USRP is listening to the same base station (with the same frequency), which results in unpredictable behavior when logging the DCI messages. \n \
		So, we currently doesn't support it! Please check your configuration files to fix it.\n");
		exit(0);
	}

    // DCI log config
    config->dci_log_config.nof_cell = config->nof_rf_dev;
    if(! config_lookup_bool(cfg, "dci_log_config.log_dl", &config->dci_log_config.log_dl)){
        printf("ERROR: reading log_ul_flag\n");
    }
    if(! config_lookup_bool(cfg, "dci_log_config.log_ul", &config->dci_log_config.log_ul)){
        printf("ERROR: reading log_ul_flag\n");
    }


    return 0;
}

bool ngscope_config_check_log(ngscope_config_t* config){
	for(int i=0; i<config->nof_rf_dev; i++){
		if(config->rf_config[i].log_dl || config->rf_config[i].log_ul){
			return true;
		}	
	}
	return false;
}
