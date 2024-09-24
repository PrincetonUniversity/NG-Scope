#ifndef _DECODE_SIB_H_
#define _DECODE_SIB_H_

#include <stdio.h>
#include <stdlib.h>

#include "srsran/srsran.h"

#ifdef __cplusplus
extern "C" {
#endif

int srsran_ue_dl_find_and_decode_sib1(srsran_ue_dl_t *q, srsran_dl_sf_cfg_t *sf, srsran_ue_dl_cfg_t *cfg, srsran_pdsch_cfg_t *pdsch_cfg, uint8_t *data[SRSRAN_MAX_CODEWORDS], bool acks[SRSRAN_MAX_CODEWORDS]);

int srsran_ue_dl_find_and_decode_sib2(srsran_ue_dl_t *q, srsran_dl_sf_cfg_t *sf, srsran_ue_dl_cfg_t *cfg, srsran_pdsch_cfg_t *pdsch_cfg, uint8_t *data[SRSRAN_MAX_CODEWORDS], bool acks[SRSRAN_MAX_CODEWORDS]);

int srsran_ue_dl_find_dl_dci_sirnti(srsran_ue_dl_t *q, srsran_dl_sf_cfg_t *sf, srsran_ue_dl_cfg_t *dl_cfg, uint16_t rnti, srsran_dci_dl_t dci_dl[SRSRAN_MAX_DCI_MSG]);

#ifdef __cplusplus
}
#endif

#endif