#include "../../hdr/dciLib/decode_sib.h"

#include "srsran/asn1/asn1_utils.h"
#include "srsran/asn1/rrc/si.h"
#include "srsran/asn1/rrc.h"

int srsran_ue_dl_find_and_decode_sib1
(
srsran_ue_dl_t *q,
srsran_dl_sf_cfg_t *sf,
srsran_ue_dl_cfg_t *cfg,
srsran_pdsch_cfg_t *pdsch_cfg,
uint8_t *data[SRSRAN_MAX_CODEWORDS],
bool acks[SRSRAN_MAX_CODEWORDS]
)
{
  int ret = SRSRAN_ERROR;

  srsran_dci_dl_t    dci_dl[SRSRAN_MAX_DCI_MSG] = {};
  srsran_pmch_cfg_t  pmch_cfg;
  srsran_pdsch_res_t pdsch_res[SRSRAN_MAX_CODEWORDS];

  // Use default values for PDSCH decoder
  ZERO_OBJECT(pmch_cfg);

  uint32_t mi_set_len;
  if (q->cell.frame_type == SRSRAN_TDD && !sf->tdd_config.configured) {
    mi_set_len = 3;
  } else {
    mi_set_len = 1;
  }

  // Blind search PHICH mi value
  ret = 0;
  for (uint32_t i = 0; i < mi_set_len && !ret; i++) {
    if (mi_set_len == 1) {
      srsran_ue_dl_set_mi_auto(q);
    } else {
      srsran_ue_dl_set_mi_manual(q, i);
    }

    if ((ret = srsran_ue_dl_decode_fft_estimate(q, sf, cfg)) < 0) {
      return ret;
    }
    // printf("Finding DCI...\n");
    ret = srsran_ue_dl_find_dl_dci_sirnti(q, sf, cfg, SRSRAN_SIRNTI, dci_dl);
    printf("ret = %d\n", ret);
  }

  if (ret == 1) {
    char str[512];
    srsran_dci_dl_info(&dci_dl[0], str, 512);
    printf("SIB1 Decoder found DCI: PDCCH: %s, snr = %.af dB\n", str, q->chest_res.snr_db);

    // Convert DCI message to DL grant
    if (srsran_ue_dl_dci_to_pdsch_grant(q, sf, cfg, &dci_dl[0], &pdsch_cfg->grant)) {
      ERROR("Error unpacking DCI");
      return SRSRAN_ERROR;
    }

    // Calculate RV if not provided in the grant and reset softbuffer
    for (int i = 0; i < SRSRAN_MAX_CODEWORDS; i++) {
      if (pdsch_cfg->grant.tb[i].enabled) {
        if (pdsch_cfg->grant.tb[i].rv < 0) {
          uint32_t sfn              = sf->tti / 10;
          uint32_t k                = (sfn / 2) % 4;
          pdsch_cfg->grant.tb[i].rv = ((uint32_t)ceilf((float)1.5 * k)) % 4;
        }
        srsran_softbuffer_rx_reset_tbs(pdsch_cfg->softbuffers.rx[i], (uint32_t)pdsch_cfg->grant.tb[i].tbs);
      }
    }

    bool decode_enable = false;
    for (uint32_t tb = 0; tb < SRSRAN_MAX_CODEWORDS; tb++) {
      if (pdsch_cfg->grant.tb[tb].enabled) {
        decode_enable         = true;
        pdsch_res[tb].payload = data[tb];
        pdsch_res[tb].crc     = false;
      }
    }

    if (decode_enable) {
      if (sf->sf_type == SRSRAN_SF_NORM) {
        if (srsran_ue_dl_decode_pdsch(q, sf, pdsch_cfg, pdsch_res)) {
          ERROR("ERROR: Decoding PDSCH");
          ret = -1;
        }
      } else {
        pmch_cfg.pdsch_cfg = *pdsch_cfg;
        if (srsran_ue_dl_decode_pmch(q, sf, &pmch_cfg, pdsch_res)) {
          ERROR("Decoding PMCH");
          ret = -1;
        }
      }
    }

    for (uint32_t tb = 0; tb < SRSRAN_MAX_CODEWORDS; tb++) {
      if (pdsch_cfg->grant.tb[tb].enabled) {
        acks[tb] = pdsch_res[tb].crc;
      }
    }
    asn1::rrc::bcch_dl_sch_msg_s dlsch;
    asn1::rrc::sib_type1_s sib1;
    asn1::cbit_ref dlsch_bref(pdsch_res->payload, pdsch_cfg->grant.tb[0].tbs / 8);
    asn1::json_writer js_sib1;
    asn1::SRSASN_CODE err = dlsch.unpack(dlsch_bref);
    printf("sib type: %d\n", dlsch.msg.c1().type());
    sib1 = dlsch.msg.c1().sib_type1();
    sib1.to_json(js_sib1);
    printf("Decoded SIB1: %s\n", js_sib1.to_string().c_str());
	  FILE *sib1out = fopen("sib1out.txt", "w");
	  fprintf(sib1out, "%s\n", js_sib1.to_string().c_str());
	  fclose(sib1out);
  }
  return ret;
}

int srsran_ue_dl_find_and_decode_sib2
(
srsran_ue_dl_t *q,
srsran_dl_sf_cfg_t *sf,
srsran_ue_dl_cfg_t *cfg,
srsran_pdsch_cfg_t *pdsch_cfg,
uint8_t *data[SRSRAN_MAX_CODEWORDS],
bool acks[SRSRAN_MAX_CODEWORDS]
)
{
  int ret = SRSRAN_ERROR;

  srsran_dci_dl_t    dci_dl[SRSRAN_MAX_DCI_MSG] = {};
  srsran_pmch_cfg_t  pmch_cfg;
  srsran_pdsch_res_t pdsch_res[SRSRAN_MAX_CODEWORDS];

  // Use default values for PDSCH decoder
  ZERO_OBJECT(pmch_cfg);

  uint32_t mi_set_len;
  if (q->cell.frame_type == SRSRAN_TDD && !sf->tdd_config.configured) {
    mi_set_len = 3;
  } else {
    mi_set_len = 1;
  }

  // Blind search PHICH mi value
  ret = 0;
  for (uint32_t i = 0; i < mi_set_len && !ret; i++) {
    if (mi_set_len == 1) {
      srsran_ue_dl_set_mi_auto(q);
    } else {
      srsran_ue_dl_set_mi_manual(q, i);
    }

    if ((ret = srsran_ue_dl_decode_fft_estimate(q, sf, cfg)) < 0) {
      return ret;
    }
    // printf("Finding DCI...\n");
    ret = srsran_ue_dl_find_dl_dci_sirnti(q, sf, cfg, SRSRAN_SIRNTI, dci_dl);
    // printf("ret = %d\n", ret);
  }

  if (ret == 1) {
    char str[512];
    srsran_dci_dl_info(&dci_dl[0], str, 512);
    // printf("SIB2 Decoder found DCI: PDCCH: %s, snr = %.af dB\n", str, q->chest_res.snr_db);

    // Convert DCI message to DL grant
    if (srsran_ue_dl_dci_to_pdsch_grant(q, sf, cfg, &dci_dl[0], &pdsch_cfg->grant)) {
      ERROR("Error unpacking DCI");
      return SRSRAN_ERROR;
    }

    // Calculate RV if not provided in the grant and reset softbuffer
    for (int i = 0; i < SRSRAN_MAX_CODEWORDS; i++) {
      if (pdsch_cfg->grant.tb[i].enabled) {
        if (pdsch_cfg->grant.tb[i].rv < 0) {
          uint32_t sfn              = sf->tti / 10;
          uint32_t k                = (sfn / 2) % 4;
          pdsch_cfg->grant.tb[i].rv = ((uint32_t)ceilf((float)1.5 * k)) % 4;
        }
        srsran_softbuffer_rx_reset_tbs(pdsch_cfg->softbuffers.rx[i], (uint32_t)pdsch_cfg->grant.tb[i].tbs);
      }
    }

    bool decode_enable = false;
    for (uint32_t tb = 0; tb < SRSRAN_MAX_CODEWORDS; tb++) {
      if (pdsch_cfg->grant.tb[tb].enabled) {
        decode_enable         = true;
        pdsch_res[tb].payload = data[tb];
        pdsch_res[tb].crc     = false;
      }
    }

    if (decode_enable) {
      if (sf->sf_type == SRSRAN_SF_NORM) {
        if (srsran_ue_dl_decode_pdsch(q, sf, pdsch_cfg, pdsch_res)) {
          ERROR("ERROR: Decoding PDSCH");
          ret = -1;
        }
      } else {
        pmch_cfg.pdsch_cfg = *pdsch_cfg;
        if (srsran_ue_dl_decode_pmch(q, sf, &pmch_cfg, pdsch_res)) {
          ERROR("Decoding PMCH");
          ret = -1;
        }
      }
    }

    for (uint32_t tb = 0; tb < SRSRAN_MAX_CODEWORDS; tb++) {
      if (pdsch_cfg->grant.tb[tb].enabled) {
        acks[tb] = pdsch_res[tb].crc;
      }
    }
    asn1::rrc::bcch_dl_sch_msg_s dlsch;
    asn1::rrc::sys_info_s sib2;
    asn1::cbit_ref dlsch_bref(pdsch_res->payload, pdsch_cfg->grant.tb[0].tbs / 8);
    asn1::json_writer js_sib2;
    asn1::SRSASN_CODE err = dlsch.unpack(dlsch_bref);
    // int si_type = dlsch.msg.c1().set_sys_info().crit_exts.sys_info_r8().sib_type_and_info[0].type();
    // printf("sib type: %d\n", si_type);
    // if (si_type != 2) {
    //   return -1;
    // }
    FILE *sib2out = fopen("sib2out.txt", "w");
    
    sib2 = dlsch.msg.c1().sys_info();
    sib2.to_json(js_sib2);
    printf("Decoded SIB2 %s\n", js_sib2.to_string().c_str());
	  fprintf(sib2out, "s\n", js_sib2.to_string().c_str());
  }
  return ret;
}