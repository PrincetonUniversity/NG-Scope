#ifndef _COMMON_TYPE_DEFINE_H_
#define _COMMON_TYPE_DEFINE_H_

#ifdef __cplusplus
extern "C" {
#endif

#define DL_MODE 0
#define UL_MODE 1

#define SFWORKER_NUM 8

#define RF_ANT_NUM 2

int srsran_ue_dl_find_dl_dci_sirnti(srsran_ue_dl_t *q, srsran_dl_sf_cfg_t *sf, srsran_ue_dl_cfg_t *dl_cfg, uint16_t rnti, srsran_dci_dl_t dci_dl[SRSRAN_MAX_DCI_MSG]);

int srsran_ue_dl_find_dl_dci_rarnti(srsran_ue_dl_t* q, srsran_dl_sf_cfg_t* sf, srsran_ue_dl_cfg_t* dl_cfg, uint16_t rnti, srsran_dci_dl_t dci_dl[SRSRAN_MAX_DCI_MSG]);

int srsran_ue_dl_find_dl_ul_dci(srsran_ue_dl_t* q, srsran_dl_sf_cfg_t* dl_sf_cfg, srsran_ul_sf_cfg_t* ul_sf_cfg, srsran_ue_dl_cfg_t* dl_cfg, srsran_ue_ul_cfg_t* ul_cfg, uint16_t rnti, 
srsran_dci_dl_t dci_dl[SRSRAN_MAX_DCI_MSG], srsran_dci_ul_t dci_ul[SRSRAN_MAX_DCI_MSG], srsran_pdsch_grant_t* pdsch_grant, srsran_pusch_grant_t* pusch_grant, int* nof_dlmsg, int* nof_ulmsg);

int srsran_ue_dl_find_and_decode_pdsch_ul_grant(srsran_ue_dl_t *q, srsran_ue_ul_t *q_ul, srsran_dl_sf_cfg_t *sf, srsran_ul_sf_cfg_t *sf_ul, srsran_ue_dl_cfg_t *cfg, srsran_ue_ul_cfg_t *cfg_ul, 
srsran_pdsch_cfg_t *pdsch_cfg, srsran_pusch_cfg_t *pusch_cfg, uint8_t *data[SRSRAN_MAX_CODEWORDS], bool acks[SRSRAN_MAX_CODEWORDS]);

int decode_pdcch_and_pdsch(srsran_ue_dl_t* ue_dl, srsran_ue_ul_t* ue_ul, srsran_dl_sf_cfg_t* dl_sf_cfg, srsran_ul_sf_cfg_t* ul_sf_cfg, srsran_ue_dl_cfg_t* ue_dl_cfg, srsran_ue_ul_cfg_t* ue_ul_cfg,
    srsran_dl_cfg_t* dl_cfg, srsran_ul_cfg_t* ul_cfg, int* ret_dl, int* ret_ul);

uint32_t grantTargetTti(uint32_t curtti, int tddconfig);

int decode_pdsch(srsran_ue_dl_t* q, srsran_dl_sf_cfg_t* sf, srsran_ue_dl_cfg_t* cfg, srsran_pdsch_cfg_t* pdsch_cfg, uint8_t* data[SRSRAN_MAX_CODEWORDS], bool acks[SRSRAN_MAX_CODEWORDS]);

int decode_pusch(srsran_ul_cfg_t* ul_cfg, srsran_pusch_grant_t pusch_grant, srsran_enb_ul_t* enb_ul, srsran_ul_sf_cfg_t* ul_sf, uint32_t rnti);

int decode_pucch(srsran_enb_ul_t* enb_ul, srsran_ul_sf_cfg_t* ul_sf, srsran_pucch_cfg_t* pucch_cfg, srsran_pucch_res_t* pucch_res);


#ifdef __cplusplus
}
#endif

#endif