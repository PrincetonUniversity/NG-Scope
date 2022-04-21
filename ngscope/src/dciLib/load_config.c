#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <libconfig.h>
#include "ngscope/hdr/dciLib/load_config.h"

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
        if(! config_lookup_bool(cfg, "disable_plot", &config->rf_config[i].disable_plot)){
            printf("ERROR: reading disable_plot\n");
        }else{
            printf("disable plot: %d\n", config->rf_config[i].disable_plot);
        }
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
