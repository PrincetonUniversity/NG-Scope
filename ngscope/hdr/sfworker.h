#ifndef _SFWORKER_H_
#define _SFWORKER_H_

#include <iostream>
#include <thread>
#include <mutex>
#include <queue>
#include <functional>
#include <condition_variable>
#include <atomic>
#include <sched.h>
#include <pthread.h>

#include "srsran/srsran.h"
#include "srsran/asn1/rrc/si.h"
#include "srsran/asn1/rrc.h"
#include "srsran/asn1/rrc_utils.h"

#include "sfTask.h"
#include "runtimeConfig.h"
#include "common_type_define.h"

#ifdef __cplusplus
extern "C" {
#endif

class SFWorker
{
public:
    SFWorker(uint32_t workerIdx, std::shared_ptr<ngscope_config_t> &ngscope_config);
    SFWorker(const SFWorker&) = delete;
    SFWorker &operator=(const SFWorker &) = delete;

    SFWorker(SFWorker &&) = default;
    SFWorker &operator=(SFWorker &&) = default;

    ~SFWorker();

    void sfWorkerThread();

    bool available() const {return isAvailable;}

    cf_t** getBuffer() {return sf_buffer;}

    void assignTask(std::unique_ptr<sfTask> task);

    void config(asn1::rrc::sib_type2_s sib2);

    inline bool isConfigured() {return isConfig;}

    void setConfigured() {isConfig = true;}

    inline uint32_t getWorkerIdx() {return workerIdx;}

    cf_t** sf_buffer;
    cf_t** ul_buffer;
    srsran_dl_cfg_t dl_cfg;
    srsran_ul_cfg_t ul_cfg;
    srsran_cell_t cell;
    srsran_ue_dl_t ue_dl;
    srsran_ue_ul_t ue_ul;
    srsran_ue_dl_cfg_t ue_dl_cfg;
    srsran_ue_ul_cfg_t ue_ul_cfg;
    srsran_dl_sf_cfg_t dl_sf_cfg;
    srsran_ul_sf_cfg_t ul_sf_cfg;
    srsran_pdsch_cfg_t pdsch_cfg;
    srsran_pusch_cfg_t pusch_cfg;
    srsran_pucch_cfg_t pucch_cfg;
    srsran_enb_ul_t enb_ul;

    srsran_ue_dl_cfg_t ue_dl_cfg_ded;
    srsran_ue_ul_cfg_t ue_ul_cfg_ded;
    bool have_cfg_ded;

    
private:
    std::thread sfThread;
    // std::queue<std::unique_ptr<sfTask>> &sfTaskQueue;
    std::unique_ptr<sfTask> task;
    // std::mutex &queueMutex;
    std::mutex mutex;
    std::condition_variable cv;
    std::atomic<bool> stop;
    std::shared_ptr<ngscope_config_t> &ngscope_config;

    srsran_chest_dl_cfg_t chest_pdsch_cfg;

    
    srsran_refsignal_dmrs_pusch_cfg_t dmrs_pusch_cfg;
    srsran_refsignal_srs_cfg_t srs_cfg;
    asn1::rrc::sib_type2_s sib2;

    uint32_t workerIdx;
    bool isAvailable;
    bool ngscopeMode;
    bool have_sib1;
    bool have_sib2;
    bool enb_ul_init;
    bool isConfig;
    
    void setThreadAffinity(std::thread& thread, int cpu_id);
};

#ifdef __cplusplus
}
#endif


#endif