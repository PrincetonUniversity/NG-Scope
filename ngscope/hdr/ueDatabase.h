#ifndef _UE_DATABASE_H_
#define _UE_DATABASE_H_

#include <iostream>
#include <mutex>
#include <vector>
#include <memory>
#include <map>
#include <algorithm>

#include "srsran/srsran.h"

#ifdef __cplusplus
extern "C" {
#endif

class tempRNTI {
public:
    tempRNTI() { }
    ~tempRNTI() { }

    void push_tcRNTI(uint16_t t_crnti);
    std::vector<uint16_t> get_tcRNTI();
    void delete_tcRNTI(uint16_t t_crnti);

private:
    std::mutex tcrntiMutex;
    std::vector<uint16_t> tcrntiVec;
};


class ueDatabase {
public:
    ueDatabase() {have_cfg = false; }
    ~ueDatabase() { }

    void pushUE(uint16_t rnti);
    void set_cfg_ded(srsran_ue_dl_cfg_t ue_dl_cfg, srsran_ue_ul_cfg_t ue_ul_cfg);
    bool get_cfg_ded(srsran_ue_dl_cfg_t* ue_dl_cfg, srsran_ue_ul_cfg_t* ue_ul_cfg);
    bool updateUE(uint16_t rnti, bool have_dci);
    bool have_cfg_ded();
    std::map<uint16_t, int> getUETrack();

private:
    std::mutex ueDatabaseMutex;
    srsran_ue_dl_cfg_t ue_dl_cfg_ded;
    srsran_ue_ul_cfg_t ue_ul_cfg_ded;
    std::map<uint16_t, int> ueTracker;
    bool have_cfg;
};

#ifdef __cplusplus
}
#endif

#endif