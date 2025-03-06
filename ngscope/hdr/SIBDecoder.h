#ifndef _SIBDECODER_H_
#define _SIBDECODER_H_

#include <iostream>

#include "srsran/srsran.h"
#include "srsran/mac/pdu.h"
#include "srsran/asn1/rrc/phy_ded.h"
#include "srsran/asn1/rrc/si.h"
#include "srsran/asn1/rrc/bcch_msg.h"
#include "srsran/support/srsran_assert.h"
#include "srsran/asn1/asn1_utils.h"

#include "sfworker.h"
#include "sfTask.h"
#include "runtimeConfig.h"
#include "common_type_define.h"
#include "ulSch.h"
#include "ueDatabase.h"

#ifdef __cplusplus
extern "C" {
#endif

class SIBDecoder : public sfTask
{
public:
    // SIBDecoder(uint32_t tti, uint32_t sf_idx) : tti(tti), sf_idx(sf_idx) { }
    SIBDecoder(std::shared_ptr<ngscope_config_t> ngscope_config, uint32_t tti, uint32_t sf_idx) : ngscope_config(ngscope_config), tti(tti), sf_idx(sf_idx), rb_power(ngscope_config->cell.nof_prb, 0) {
        puschSche = &ngscope_config->puschSche;
        tcrntiDatabase = &ngscope_config->tcrntiDatabse;
        sibs = &ngscope_config->sibs;
        nof_prb = ngscope_config->cell.nof_prb;
    }
    ~SIBDecoder() { }
    int run_ul_mode(SFWorker* curworker) override;
    int execute(srsran_ue_dl_t* ue_dl, cf_t** sf_buffer, srsran_ue_dl_cfg_t* ue_dl_cfg, srsran_dl_sf_cfg_t* dl_sf_cfg, srsran_pdsch_cfg_t* pdsch_cfg) override;
    int execute_ul(srsran_cell_t cell, srsran_ue_dl_t* ue_dl, srsran_ue_ul_t* ue_ul, cf_t** sf_buffer, srsran_ue_dl_cfg_t* ue_dl_cfg, srsran_dl_sf_cfg_t* dl_sf_cfg, srsran_ul_sf_cfg_t *sf_ul, srsran_ue_ul_cfg_t *cfg_ul, 
    srsran_pdsch_cfg_t* pdsch_cfg, srsran_dl_cfg_t* dl_cfg, srsran_ul_cfg_t* ul_cfg, srsran_enb_ul_t* enb_ul, bool* have_sib1, bool* have_sib2) override;
private:
    cf_t** sf_buffer;
    std::shared_ptr<ngscope_config_t> ngscope_config;
    std::mutex SibMutex;
    const uint32_t tti;
    const uint32_t sf_idx;
    asn1::rrc::sib_type1_s sib1;
    asn1::rrc::sib_type2_s sib2;
    ULSch *puschSche;
    tempRNTI *tcrntiDatabase;
    sys_info_common *sibs;

    std::vector<float> rb_power;
    float max_power;
    float min_power;
    int max_power_idx;
    int min_power_idx;
    int nof_prb;

    // asn1::rrc::dl_ccch_msg_s dlccch;
    // asn1::rrc::pucch_cfg_ded_s pucch_cfg_ded;
    // asn1::rrc::dl_ded_msg_segment_r16_s  dl_ded_msg;
    // asn1::cbit_ref bref;

    int decodeSib(asn1::rrc::sib_type1_s* sib1, asn1::rrc::sib_type2_s* sib2, srsran_ue_dl_t* ue_dl, cf_t** sf_buffer, srsran_ue_dl_cfg_t* ue_dl_cfg, srsran_dl_sf_cfg_t* dl_sf_cfg, srsran_pdsch_cfg_t pdsch_cfg);

    int decodeRach(srsran_ue_dl_t* ue_dl, cf_t** sf_buffer, srsran_ue_dl_cfg_t* ue_dl_cfg, srsran_ue_ul_cfg_t* ue_ul_cfg, srsran_dl_sf_cfg_t* dl_sf_cfg);

    int rachInit();

    void computerPower(const cf_t *sf_symbols);
};

#ifdef __cplusplus
}
#endif

#endif