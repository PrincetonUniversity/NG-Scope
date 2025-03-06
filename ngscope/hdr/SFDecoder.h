#ifndef _NGSCOPE_SFDECODER_H_
#define _NGSCOPE_SFDECODER_H_

#include <iostream>
#include <memory>

#include "srsran/srsran.h"
#include "srsran/mac/pdu.h"

#include "sfworker.h"
#include "sfTask.h"
#include "runtimeConfig.h"
#include "common_type_define.h"
// #include "PRACHDecoder.h"

#ifdef __cplusplus
extern "C" {
#endif

class SFDecoder : public sfTask
{
public:
    SFDecoder(std::shared_ptr<ngscope_config_t> ngscope_config, uint32_t tti, uint32_t sf_idx) : ngscope_config(ngscope_config), tti(tti), sf_idx(sf_idx), rb_power(ngscope_config->cell.nof_prb, 0) {
        puschSche = &ngscope_config->puschSche;
        sibs = &ngscope_config->sibs;
        tcrntiDatabase = &ngscope_config->tcrntiDatabse;
        ueDataBase = &ngscope_config->ueDataBase;
        nof_prb = ngscope_config->cell.nof_prb;
        dupmode = ngscope_config->cell.frame_type;
    }
    ~SFDecoder() { }
    int run_ul_mode(SFWorker* curworker) override;
    int execute(srsran_ue_dl_t* ue_dl, cf_t** sf_buffer, srsran_ue_dl_cfg_t* ue_dl_cfg, srsran_dl_sf_cfg_t* dl_sf_cfg, srsran_pdsch_cfg_t* pdsch_cfg) override;
    int execute_ul(srsran_cell_t cell, srsran_ue_dl_t* ue_dl, srsran_ue_ul_t* ue_ul, cf_t** sf_buffer, srsran_ue_dl_cfg_t* ue_dl_cfg, srsran_dl_sf_cfg_t* dl_sf_cfg, srsran_ul_sf_cfg_t *sf_ul, 
    srsran_ue_ul_cfg_t *cfg_ul, srsran_pdsch_cfg_t* pdsch_cfg, srsran_dl_cfg_t* dl_cfg, srsran_ul_cfg_t* ul_cfg, srsran_enb_ul_t* enb_ul, bool* have_sib1, bool* have_sib2) override;

    int sfDecodePusch(srsran_ue_ul_t* ue_ul, srsran_ue_ul_cfg_t* ue_ul_cfg, srsran_dci_ul_t dci_ul, srsran_enb_ul_t* enb_ul, srsran_ul_sf_cfg_t* ul_sf, uint32_t rnti);
    int sfDecodePuschMultiUE(srsran_ue_ul_t* ue_ul, srsran_ue_ul_cfg_t* ue_ul_cfg, srsran_enb_ul_t* enb_ul, srsran_ul_sf_cfg_t* ul_sf_cfg);
    int sfDecodePucch(srsran_ue_ul_t* ue_ul, srsran_ue_ul_cfg_t* ue_ul_cfg, srsran_enb_ul_t* enb_ul, srsran_ul_sf_cfg_t* ul_sf, uint16_t rnti);
    int sfDecodeRach(srsran_ue_dl_t* ue_dl, srsran_dl_sf_cfg_t* dl_sf_cfg, srsran_ue_dl_cfg_t* ue_dl_cfg, srsran_ue_ul_cfg_t* ue_ul_cfg);
private:
    cf_t** sf_buffer;
    std::shared_ptr<ngscope_config_t> ngscope_config;
    const uint32_t tti;
    const uint32_t sf_idx;
    srsran_chest_dl_cfg_t chest_pdsch_cfg = {};
    int nof_prb;
    
    bool prach_init;
    ULSch *puschSche;
    tempRNTI *tcrntiDatabase;
    sys_info_common *sibs;
    ueDatabase *ueDataBase;
    std::vector<float> rb_power;
    float max_power;
    float min_power;
    int max_power_idx;
    int min_power_idx;
    int dupmode;

    int sfWorkerUlInit(srsran_pdsch_cfg_t pdsch_cfg, srsran_dl_sf_cfg_t dl_sf_cfg, srsran_ue_dl_t ue_dl, srsran_ue_dl_cfg_t ue_dl_cfg, srsran_enb_ul_t* enb_ul, srsran_refsignal_dmrs_pusch_cfg_t* dmrs_pusch_cfg);
    
    int getPuschGrant(srsran_ue_dl_t* ue_dl, srsran_ue_ul_t* ue_ul, srsran_dl_sf_cfg_t* dl_sf_cfg, srsran_ul_sf_cfg_t* ul_sf_cfg, srsran_ue_dl_cfg_t* ue_dl_cfg, srsran_ue_ul_cfg_t* ue_ul_cfg, srsran_dci_ul_t* dci_ul);

    int getPuschGrantMultiUE(srsran_ue_dl_t* ue_dl, srsran_ue_ul_t* ue_ul, srsran_dl_sf_cfg_t* dl_sf_cfg, srsran_ul_sf_cfg_t* ul_sf_cfg, srsran_ue_dl_cfg_t* ue_dl_cfg, srsran_ue_ul_cfg_t* ue_ul_cfg);

    int findDCI(srsran_ue_dl_t* ue_dl, srsran_dl_sf_cfg_t* dl_sf_cfg, srsran_ue_dl_cfg_t* ue_dl_cfg, uint16_t rnti, srsran_dci_dl_t* dci_dl, srsran_dci_ul_t* dci_ul);

    void computerPower(const cf_t *sf_symbols);
};


int srsran_ue_ul_dci_to_pusch_grant_qam256_ngscope(srsran_ue_ul_t* q, srsran_ul_sf_cfg_t* sf, srsran_ue_ul_cfg_t* cfg, srsran_dci_ul_t* dci, srsran_pusch_grant_t* grant);

#ifdef __cplusplus
}
#endif

#endif