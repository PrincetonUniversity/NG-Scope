#ifndef _ARGS_H_
#define _ARGS_H_

#include <iostream>
#include <memory>
#include <string>
#include <libconfig.h>

#include "srsran/srsran.h"

#include "runtimeConfig.h"
#include "common_type_define.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_USRP_NUM 4

class args
{
public:
    args() { }
    args(int argc, char** argv);
    void default_args();
    inline char *get_device_name() {return rf_args[0];}
    inline char *get_device_name_ul() {return rf_args[1];}
    inline int get_nof_rx_ant() {return nof_rf_rx_ant;}
    inline double get_rf_gain() {return rf_gain;}
    inline int get_ngscope_mode() {return ngscope_mode;}
    inline long long get_dl_freq() {return dl_freq;}
    inline int get_ul_freq() {return ul_freq;}
    inline int get_multi_worker() {return multi_worker;}
    void init_ngscope_config(std::shared_ptr<ngscope_config_t> ngscope_config);
    ~args() { }
private:
    char configFile[50]; // name of the config file
    int cpu_affinity;
    int multi_worker;
    int ngscope_mode; // DL_MODE or UL_MODE
    int nof_usrp; // number of USRPs
    int nof_rf_rx_ant;
    char rf_args[MAX_USRP_NUM][50]; //serials of USRP
    int N_id_2[MAX_USRP_NUM];
    long long dl_freq; //downlink frequency
    long long ul_freq; //uplink frequency
    double rf_gain;
    int rnti;

    // std::shared_ptr<ngscope_config_t> ngscope_config;
};


#ifdef __cplusplus
}
#endif

#endif