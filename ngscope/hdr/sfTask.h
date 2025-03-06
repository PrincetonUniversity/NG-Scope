#ifndef _SFTASK_H_
#define _SFTASK_H_

#include <iostream>
#include <thread>
#include <mutex>
#include <functional>

#include "srsran/srsran.h"

#include "common_type_define.h"

#ifdef __cplusplus
extern "C" {
#endif

class SFWorker;

class sfTask
{
public:
    sfTask() { }
    virtual ~sfTask() = default;

    virtual int run_ul_mode(SFWorker* curworker) = 0;

    //execute function type1: downlink processing
    virtual int execute(srsran_ue_dl_t* ue_dl, cf_t** sf_buffer, srsran_ue_dl_cfg_t* ue_dl_cfg, srsran_dl_sf_cfg_t* dl_sf_cfg, srsran_pdsch_cfg_t* pdsch_cfg) = 0;

    //execute function type2: downlink and uplink processing
    virtual int execute_ul(srsran_cell_t cell, srsran_ue_dl_t* ue_dl, srsran_ue_ul_t* ue_ul, cf_t** sf_buffer, srsran_ue_dl_cfg_t* ue_dl_cfg, srsran_dl_sf_cfg_t* dl_sf_cfg, srsran_ul_sf_cfg_t *sf_ul, 
    srsran_ue_ul_cfg_t *ue_ul_cfg, srsran_pdsch_cfg_t* pdsch_cfg, srsran_dl_cfg_t* dl_cfg, srsran_ul_cfg_t* ul_cfg, srsran_enb_ul_t* enb_ul, bool* have_sib1, bool* have_sib2) = 0;

private:
    cf_t *sf_buffer[SRSRAN_MAX_PORTS];

    SFWorker* curworker;
};


#ifdef __cplusplus
}
#endif

#endif