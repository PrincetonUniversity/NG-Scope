#ifndef _RUNTIME_CONFIG_H_
#define _RUNTIME_CONFIG_H_

#include <iostream>
#include <mutex>
#include <queue>
#include <vector>
#include <shared_mutex>
#include <atomic>
#include <memory>

#include "srsran/srsran.h"
#include "srsran/asn1/rrc.h"
#include "srsran/asn1/rrc_utils.h"
#include "srsran/asn1/asn1_utils.h"

#include "ulSch.h"
#include "ueDatabase.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int nof_subframes;
    int force_N_id_2;
    int cpu_affinity;
    int file_offset_time;
    int net_port;
    int net_port_signal;
    int decimate;
    int tdd_special_sf;
    int sf_config;
    int verbose;

    bool disable_plots;
    bool disable_plots_except_constellation;
    bool disable_cfo;
    bool enable_cfo_ref;
    bool enable_256qam;
    bool use_standard_lte_rate;
    bool have_sib1;
    bool have_sib2;

    char *input_file_name;
    char *estimator_alg;
    char rf_dev[50];
    char rf_args[50];
    char *net_address;
    char *net_address_signal;

    float file_offset_freq;
    float rf_gain;
    long long rf_freq;

    uint32_t time_offset;
    uint16_t rnti;
    uint32_t file_nof_prb;
    uint32_t file_nof_ports;
    uint32_t file_cell_id;
    uint32_t rf_nof_rx_ant;
    int32_t  mbsfn_area_id;
    uint8_t  non_mbsfn_region;
    uint8_t  mbsfn_sf_mask;
} prog_args_t;


class sys_info_common{
public:
    sys_info_common() {
        have_sib1 = false;
        have_sib2 = false;
    }
    ~sys_info_common() { }

    inline bool have_sys() {
        // std::shared_lock<std::shared_mutex> lock(sysInfoMutex);
        // return (have_sib1 && have_sib2);
        return (have_sib1.load() && have_sib2.load());
    }

    void set_sib1(const asn1::rrc::sib_type1_s& sib1in) {
        std::lock_guard<std::shared_mutex> lock(sysInfoMutex);
        sib1 = sib1in;
        // if (!sib1ptr) {
        //     std::lock_guard<std::mutex> lock(sib1Mutex);
        //     if (!sib1ptr) {
        //         sib1ptr = std::make_shared<asn1::rrc::sib_type1_s>(sib1in);
        //         have_sib1.store(true);
        //     }
        // }
        // sib1ptr.store(std::make_shared<asn1::rrc::sib_type1_s>(sib1in));
        have_sib1.store(true);
    }

    void set_sib2(const asn1::rrc::sib_type2_s& sib2in) {
        std::lock_guard<std::shared_mutex> lock(sysInfoMutex);
        sib2 = sib2in;
        // if (!sib2ptr) {
        //     std::lock_guard<std::mutex> lock(sib2Mutex);
        //     if (!sib2ptr) {
        //         sib2ptr = std::make_shared<asn1::rrc::sib_type2_s>(sib2in);
        //         have_sib2.store(true);
        //     }
        // }
        // sib2ptr.store(std::make_shared<asn1::rrc::sib_type2_s>(sib2in));
        have_sib2.store(true);
    }

    std::shared_ptr<const asn1::rrc::sib_type1_s> getSIB1ptr() const {
        return sib1ptr;
    }

    std::shared_ptr<const asn1::rrc::sib_type2_s> getSIB2ptr() const {
        return sib2ptr;
    }

    inline const asn1::rrc::sib_type1_s& getSIB1() const {
        // std::lock_guard<std::mutex> lock(sysInfoMutex);
        // std::shared_lock<std::shared_mutex> lock(sysInfoMutex);
        std::lock_guard<std::mutex> lock(sib1Mutex);
        return sib1;
    }

    inline const asn1::rrc::sib_type2_s& getSIB2() const {
        // std::lock_guard<std::mutex> lock(sysInfoMutex);
        // std::shared_lock<std::shared_mutex> lock(sysInfoMutex);
        std::lock_guard<std::mutex> lock(sib2Mutex);
        return sib2;
    }

private:
    mutable std::shared_mutex sysInfoMutex;

    mutable std::mutex sib1Mutex;
    mutable std::mutex sib2Mutex;

    asn1::rrc::sib_type1_s sib1;
    asn1::rrc::sib_type2_s sib2;

    std::atomic<bool> have_sib1;
    std::atomic<bool> have_sib2;

    std::shared_ptr<asn1::rrc::sib_type1_s> sib1ptr{nullptr};
    std::shared_ptr<asn1::rrc::sib_type2_s> sib2ptr{nullptr};
};


class ngscope_config_t{
public:
    ngscope_config_t(const ngscope_config_t&) = delete;
    ngscope_config_t& operator=(const ngscope_config_t&) = delete;
    ngscope_config_t(){ have_sib1 = false;
                        have_sib2 = false;
                        prog_args = { };
                        cell = { };
                        RNTI = 0;
                        }
    ~ngscope_config_t( ) { }
    bool ngscopeMode;
    std::mutex ngscope_config_mutex;
    prog_args_t prog_args;
    srsran_cell_t cell;
    bool have_sib1;
    asn1::rrc::sib_type1_s sib1;
    bool have_sib2;
    asn1::rrc::sib_type2_s sib2;

    uint32_t RNTI;
    // void puschGrantPush(puschGrant_t);
    // puschGrant_t puschGrantPop(uint32_t curtti);
    asn1::rrc::sib_type2_s getSib2();
    ULSch puschSche;
    tempRNTI tcrntiDatabse;
    sys_info_common sibs;
    ueDatabase ueDataBase;

private:
    // std::vector<puschGrant_t> puschGrantVec;
    
};


#ifdef __cplusplus
}
#endif

#endif