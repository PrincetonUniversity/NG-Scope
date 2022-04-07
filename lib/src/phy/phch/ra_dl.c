/**
 * Copyright 2013-2021 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 *
 * srsRAN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsRAN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#include "srsran/phy/phch/ra_dl.h"
#include "srsran/phy/common/phy_common.h"
#include "srsran/phy/phch/ra.h"
#include "srsran/phy/utils/bit.h"
#include "srsran/phy/utils/debug.h"
#include "srsran/phy/utils/vector.h"
#include "srsran/srsran.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#define min(a, b) (a < b ? a : b)

const int tbs_format1c_table[32] = {40,  56,   72,   120,  136,  144,  176,  208,  224,  256, 280,
                                    296, 328,  336,  392,  488,  552,  600,  632,  696,  776, 840,
                                    904, 1000, 1064, 1128, 1224, 1288, 1384, 1480, 1608, 1736};

/* Returns the number of RE in a PRB in a slot and subframe */
uint32_t ra_re_x_prb(const srsran_cell_t* cell, srsran_dl_sf_cfg_t* sf, uint32_t slot, uint32_t prb_idx)
{
  uint32_t subframe         = sf->tti % 10;
  uint32_t nof_ctrl_symbols = SRSRAN_NOF_CTRL_SYMBOLS((*cell), sf->cfi);

  uint32_t    re;
  bool        skip_refs = true;
  srsran_cp_t cp_       = cell->cp;
  if (SRSRAN_SF_MBSFN == sf->sf_type) {
    cp_ = SRSRAN_CP_EXT;
  }

  uint32_t nof_symbols = SRSRAN_CP_NSYMB(cp_);
  if (cell->frame_type == SRSRAN_TDD && srsran_sfidx_tdd_type(sf->tdd_config, subframe) == SRSRAN_TDD_SF_S) {
    nof_symbols = srsran_sfidx_tdd_nof_dw_slot(sf->tdd_config, slot, cp_);
  }

  if (slot == 0) {
    re = (nof_symbols - nof_ctrl_symbols) * SRSRAN_NRE;
  } else {
    re = nof_symbols * SRSRAN_NRE;
  }

  /* if it's the prb in the middle, there are less RE due to PBCH and PSS/SSS */
  if (cell->frame_type == SRSRAN_FDD) {
    if ((subframe == 0 || subframe == 5) &&
        (prb_idx >= cell->nof_prb / 2 - 3 && prb_idx < cell->nof_prb / 2 + 3 + (cell->nof_prb % 2))) {
      if (subframe == 0) {
        if (slot == 0) {
          re = (nof_symbols - nof_ctrl_symbols - 2) * SRSRAN_NRE;
        } else {
          if (SRSRAN_CP_ISEXT(cp_)) {
            re        = (nof_symbols - 4) * SRSRAN_NRE;
            skip_refs = false;
          } else {
            re = (nof_symbols - 4) * SRSRAN_NRE + 2 * cell->nof_ports;
          }
        }
      } else if (subframe == 5) {
        if (slot == 0) {
          re = (nof_symbols - nof_ctrl_symbols - 2) * SRSRAN_NRE;
        }
      }
      if ((cell->nof_prb % 2) && (prb_idx == cell->nof_prb / 2 - 3 || prb_idx == cell->nof_prb / 2 + 3)) {
        if (slot == 0) {
          re += 2 * SRSRAN_NRE / 2;
        } else if (subframe == 0) {
          re += 4 * SRSRAN_NRE / 2 - cell->nof_ports;
          if (SRSRAN_CP_ISEXT(cp_)) {
            re -= cell->nof_ports > 2 ? 2 : cell->nof_ports;
          }
        }
      }
    }
  } else {
    if ((((subframe == 0 || subframe == 5) && slot == 1) || ((subframe == 1 || subframe == 6) && slot == 0)) &&
        (prb_idx >= cell->nof_prb / 2 - 3 && prb_idx < cell->nof_prb / 2 + 3 + (cell->nof_prb % 2))) {
      if (subframe == 0) {
        if (SRSRAN_CP_ISEXT(cp_)) {
          re        = (nof_symbols - 5) * SRSRAN_NRE;
          skip_refs = false;
        } else {
          re = (nof_symbols - 5) * SRSRAN_NRE + 2 * cell->nof_ports;
        }
      } else if (subframe == 5) {
        re = (nof_symbols - 1) * SRSRAN_NRE;
      } else if (subframe == 1) {
        re = (nof_symbols - nof_ctrl_symbols - 1) * SRSRAN_NRE;
      } else if (subframe == 6) {
        re = (nof_symbols - nof_ctrl_symbols - 1) * SRSRAN_NRE;
      }
      if ((cell->nof_prb % 2) && (prb_idx == cell->nof_prb / 2 - 3 || prb_idx == cell->nof_prb / 2 + 3)) {
        re += SRSRAN_NRE / 2;
        if (subframe == 0) {
          re += 4 * SRSRAN_NRE / 2 - cell->nof_ports;
          if (SRSRAN_CP_ISEXT(cp_)) {
            re -= cell->nof_ports > 2 ? 2 : cell->nof_ports;
          }
        }
      }
    }
  }

  // remove references
  if (skip_refs) {
    if (sf->sf_type == SRSRAN_SF_NORM) {
      switch (cell->nof_ports) {
        case 1:
        case 2:
          if ((cp_ == SRSRAN_CP_NORM && nof_symbols >= 5) || (cp_ == SRSRAN_CP_EXT && nof_symbols >= 4)) {
            re -= 2 * (slot + 1) * cell->nof_ports;
          } else if (slot == 1) {
            if (nof_symbols >= 1) {
              re -= 2 * cell->nof_ports;
            }
          }
          break;
        case 4:
          if (slot == 1) {
            if ((cp_ == SRSRAN_CP_NORM && nof_symbols >= 5) || (cp_ == SRSRAN_CP_EXT && nof_symbols >= 4)) {
              re -= 12;
            } else if (nof_symbols >= 2) {
              re -= 8;
            }
          } else {
            if ((cp_ == SRSRAN_CP_NORM && nof_symbols >= 5) || (cp_ == SRSRAN_CP_EXT && nof_symbols >= 4)) {
              re -= 4;
              if (nof_ctrl_symbols == 1) {
                re -= 4;
              }
            }
          }
          break;
      }
    }
    if (sf->sf_type == SRSRAN_SF_MBSFN) {
      re -= 6 * (slot + 1);
    }
  }
  return re;
}

/** Compute PRB allocation for Downlink as defined in 7.1.6 of 36.213
 * Decode grant->type?_alloc to grant
 * This function only reads dci->type?_alloc (e.g. rbg_bitmask, mode, riv) and dci->alloc_type fields.
 * This function only writes grant->prb_idx and grant->nof_prb.
 */
/** Compute PRB allocation for Downlink as defined in 7.1.6 of 36.213 */
int srsran_ra_dl_grant_to_grant_prb_allocation(const srsran_dci_dl_t* dci,
                                               srsran_pdsch_grant_t*  grant,
                                               uint32_t               nof_prb)
{
  int      i, j;
  uint32_t bitmask;
  uint32_t P = srsran_ra_type0_P(nof_prb);
  uint32_t n_rb_rbg_subset, n_rb_type1;
  uint32_t L_crb = 0, RB_start = 0, nof_vrb = 0, nof_prb_t2 = 0, n_step = 0;

  switch (dci->alloc_type) {
    case SRSRAN_RA_ALLOC_TYPE0:
      bitmask = dci->type0_alloc.rbg_bitmask;
      int nb  = (int)ceilf((float)nof_prb / P);
      for (i = 0; i < nb; i++) {
        if (bitmask & (1 << (nb - i - 1))) {
          for (j = 0; j < P; j++) {
            if (i * P + j < nof_prb) {
              grant->prb_idx[0][i * P + j] = true;
              grant->nof_prb++;
            }
          }
        }
      }
      memcpy(&grant->prb_idx[1], &grant->prb_idx[0], SRSRAN_MAX_PRB * sizeof(bool));
      break;
    case SRSRAN_RA_ALLOC_TYPE1:
      // Make sure the rbg_subset is valid
      if (dci->type1_alloc.rbg_subset >= P) {
        //ERROR("Invalid RBG subset=%d for nof_prb=%d where P=%d", dci->type1_alloc.rbg_subset, nof_prb, P);
        return SRSRAN_ERROR;
      }
      n_rb_type1    = srsran_ra_type1_N_rb(nof_prb);
      uint32_t temp = ((nof_prb - 1) / P) % P;
      if (dci->type1_alloc.rbg_subset < temp) {
        n_rb_rbg_subset = ((nof_prb - 1) / (P * P)) * P + P;
      } else if (dci->type1_alloc.rbg_subset == temp) {
        n_rb_rbg_subset = ((nof_prb - 1) / (P * P)) * P + ((nof_prb - 1) % P) + 1;
      } else {
        n_rb_rbg_subset = ((nof_prb - 1) / (P * P)) * P;
      }
      int shift = dci->type1_alloc.shift ? (n_rb_rbg_subset - n_rb_type1) : 0;
      bitmask   = dci->type1_alloc.vrb_bitmask;
      for (i = 0; i < n_rb_type1; i++) {
        if (bitmask & (1 << (n_rb_type1 - i - 1))) {
          uint32_t idx = (((i + shift) / P) * P * P + dci->type1_alloc.rbg_subset * P + (i + shift) % P);
          if (idx < nof_prb) {
            grant->prb_idx[0][idx] = true;
            grant->nof_prb++;
          } else {
            ERROR("Invalid idx=%d in Type1 RA, nof_prb=%d", idx, nof_prb);
            return SRSRAN_ERROR;
          }
        }
      }
      memcpy(&grant->prb_idx[1], &grant->prb_idx[0], SRSRAN_MAX_PRB * sizeof(bool));
      break;
    case SRSRAN_RA_ALLOC_TYPE2:

      if (dci->type2_alloc.mode == SRSRAN_RA_TYPE2_LOC) {
        nof_vrb = nof_prb;
      } else {
        nof_vrb = srsran_ra_type2_n_vrb_dl(nof_prb, dci->type2_alloc.n_gap == SRSRAN_RA_TYPE2_NG1);
      }
      if (dci->format == SRSRAN_DCI_FORMAT1C) {
        n_step = srsran_ra_type2_n_rb_step(nof_prb);
        nof_vrb /= n_step;
        nof_prb_t2 = nof_vrb;
      } else {
        nof_prb_t2 = nof_prb;
      }
      srsran_ra_type2_from_riv(dci->type2_alloc.riv, &L_crb, &RB_start, nof_prb_t2, nof_vrb);

      if (dci->format == SRSRAN_DCI_FORMAT1C) {
        L_crb *= n_step;
        RB_start *= n_step;
      }

      if (dci->type2_alloc.mode == SRSRAN_RA_TYPE2_LOC) {
        for (i = 0; i < L_crb; i++) {
          grant->prb_idx[0][i + RB_start] = true;
          grant->nof_prb++;
        }
        memcpy(&grant->prb_idx[1], &grant->prb_idx[0], SRSRAN_MAX_PRB * sizeof(bool));
      } else {
        /* Mapping of Virtual to Physical RB for distributed type is defined in
         * 6.2.3.2 of 36.211
         */
        int N_gap, N_tilde_vrb, n_tilde_vrb, n_tilde_prb, n_tilde2_prb, N_null, N_row, n_vrb;
        int n_tilde_prb_odd, n_tilde_prb_even;
        if (dci->type2_alloc.n_gap == SRSRAN_RA_TYPE2_NG1) {
          N_tilde_vrb = srsran_ra_type2_n_vrb_dl(nof_prb, true);
          N_gap       = srsran_ra_type2_ngap(nof_prb, true);
        } else {
          N_tilde_vrb = 2 * srsran_ra_type2_n_vrb_dl(nof_prb, true);
          N_gap       = srsran_ra_type2_ngap(nof_prb, false);
        }
        N_row  = (int)ceilf((float)N_tilde_vrb / (4 * P)) * P;
        N_null = 4 * N_row - N_tilde_vrb;
        for (i = 0; i < L_crb; i++) {
          n_vrb        = i + RB_start;
          n_tilde_vrb  = n_vrb % N_tilde_vrb;
          n_tilde_prb  = 2 * N_row * (n_tilde_vrb % 2) + n_tilde_vrb / 2 + N_tilde_vrb * (n_vrb / N_tilde_vrb);
          n_tilde2_prb = N_row * (n_tilde_vrb % 4) + n_tilde_vrb / 4 + N_tilde_vrb * (n_vrb / N_tilde_vrb);

          if (N_null != 0 && n_tilde_vrb >= (N_tilde_vrb - N_null) && (n_tilde_vrb % 2) == 1) {
            n_tilde_prb_odd = n_tilde_prb - N_row;
          } else if (N_null != 0 && n_tilde_vrb >= (N_tilde_vrb - N_null) && (n_tilde_vrb % 2) == 0) {
            n_tilde_prb_odd = n_tilde_prb - N_row + N_null / 2;
          } else if (N_null != 0 && n_tilde_vrb < (N_tilde_vrb - N_null) && (n_tilde_vrb % 4) >= 2) {
            n_tilde_prb_odd = n_tilde2_prb - N_null / 2;
          } else {
            n_tilde_prb_odd = n_tilde2_prb;
          }
          n_tilde_prb_even = (n_tilde_prb_odd + N_tilde_vrb / 2) % N_tilde_vrb + N_tilde_vrb * (n_vrb / N_tilde_vrb);

          if (n_tilde_prb_odd < N_tilde_vrb / 2) {
            if (n_tilde_prb_odd < nof_prb) {
              grant->prb_idx[0][n_tilde_prb_odd] = true;
            } else {
              return SRSRAN_ERROR;
            }
          } else {
            if (n_tilde_prb_odd + N_gap - N_tilde_vrb / 2 < nof_prb) {
              grant->prb_idx[0][n_tilde_prb_odd + N_gap - N_tilde_vrb / 2] = true;
            } else {
              return SRSRAN_ERROR;
            }
          }
          grant->nof_prb++;
          if (n_tilde_prb_even < N_tilde_vrb / 2) {
            if (n_tilde_prb_even < nof_prb) {
              grant->prb_idx[1][n_tilde_prb_even] = true;
            } else {
              return SRSRAN_ERROR;
            }
          } else {
            if (n_tilde_prb_even + N_gap - N_tilde_vrb / 2 < nof_prb) {
              grant->prb_idx[1][n_tilde_prb_even + N_gap - N_tilde_vrb / 2] = true;
            } else {
              return SRSRAN_ERROR;
            }
          }
        }
      }
      break;
    default:
      return SRSRAN_ERROR;
  }

  return SRSRAN_SUCCESS;
}

int srsran_dl_fill_ra_mcs(srsran_ra_tb_t* tb, int last_tbs, uint32_t nprb, bool pdsch_use_tbs_index_alt)
{
  // Get modulation
  tb->mod = srsran_ra_dl_mod_from_mcs(tb->mcs_idx, pdsch_use_tbs_index_alt);

  // Get Transport block size index
  int i_tbs = srsran_ra_tbs_idx_from_mcs(tb->mcs_idx, pdsch_use_tbs_index_alt, false);

  // If i_tbs = -1, TBS is determined from the latest PDCCH for this TB (7.1.7.2 36.213)
  int tbs = 0;
  if (i_tbs >= 0) {
    tbs     = srsran_ra_tbs_from_idx((uint32_t)i_tbs, nprb);
    tb->tbs = tbs;
  } else {
    tb->tbs = last_tbs;
  }

  return tbs;
}

/* Modulation order and transport block size determination 7.1.7 in 36.213
 * */
static int dl_dci_compute_tb(bool pdsch_use_tbs_index_alt, const srsran_dci_dl_t* dci, srsran_pdsch_grant_t* grant)
{
  uint32_t n_prb = 0;
  int      tbs   = -1;
  uint32_t i_tbs = 0;

  // Copy info and Enable/Disable TB
  for (uint32_t i = 0; i < SRSRAN_MAX_CODEWORDS; i++) {
    grant->tb[i].mcs_idx = dci->tb[i].mcs_idx;
    grant->tb[i].rv      = dci->tb[i].rv;
    grant->tb[i].cw_idx  = dci->tb[i].cw_idx;
    if ((SRSRAN_DCI_IS_TB_EN(dci->tb[i]) && dci->format >= SRSRAN_DCI_FORMAT2) ||
        (dci->format < SRSRAN_DCI_FORMAT2 && i == 0)) {
      grant->tb[i].enabled = true;
      grant->nof_tb++;
    } else {
      grant->tb[i].enabled = false;
    }
  }

  // 256QAM table is allowed if:
  // - if the higher layer parameter altCQI-Table-r12 is configured, and
  // - if the PDSCH is assigned by a PDCCH/EPDCCH with DCI format 1/1B/1D/2/2A/2B/2C/2D with
  // - CRC scrambled by C-RNTI,
  // Otherwise, use 64QAM table (default table for R8).
  if (dci->format == SRSRAN_DCI_FORMAT1A || !SRSRAN_RNTI_ISUSER(dci->rnti)) {
    pdsch_use_tbs_index_alt = false;
  }

  if (!SRSRAN_RNTI_ISUSER(dci->rnti) && !SRSRAN_RNTI_ISMBSFN(dci->rnti)) {
    if (dci->format == SRSRAN_DCI_FORMAT1A) {
      n_prb = dci->type2_alloc.n_prb1a == SRSRAN_RA_TYPE2_NPRB1A_2 ? 2 : 3;
      i_tbs = dci->tb[0].mcs_idx;
      tbs   = srsran_ra_tbs_from_idx(i_tbs, n_prb);
      if (tbs < 0) {
        ERROR("Invalid TBS_index=%d or n_prb=%d", i_tbs, n_prb);
        return SRSRAN_ERROR;
      }
    } else if (dci->format == SRSRAN_DCI_FORMAT1C) {
      if (dci->tb[0].mcs_idx < 32) {
        tbs = tbs_format1c_table[dci->tb[0].mcs_idx];
      } else {
        ERROR("Error decoding DCI: Invalid mcs_idx=%d in Format1C", dci->tb[0].mcs_idx);
        return SRSRAN_ERROR;
      }
    } else {
      //ERROR("Error decoding DCI: P/SI/RA-RNTI supports Format1A/1C only");
      return SRSRAN_ERROR;
    }
    grant->tb[0].mod = SRSRAN_MOD_QPSK;
    if (tbs >= 0) {
      grant->tb[0].tbs = (uint32_t)tbs;
    } else {
      ERROR("Invalid TBS=%d", tbs);
      return SRSRAN_ERROR;
    }
  } else {
    if (dci->is_dwpts) {
      n_prb = SRSRAN_MAX(1, 0.75 * grant->nof_prb);
    } else {
      n_prb = grant->nof_prb;
    }
    for (uint32_t i = 0; i < SRSRAN_MAX_CODEWORDS; i++) {
      if (grant->tb[i].enabled) {
        grant->tb[i].tbs = srsran_dl_fill_ra_mcs(&grant->tb[i], grant->last_tbs[i], n_prb, pdsch_use_tbs_index_alt);
        if (grant->tb[i].tbs < 0) {
          char str[128];
          srsran_dci_dl_info(dci, str, sizeof(str));
          INFO("Error computing TBS from %s", str);
          return SRSRAN_ERROR;
        }
      } else {
        grant->tb[i].tbs = 0;
      }
    }
  }
  return SRSRAN_SUCCESS;
}

void srsran_ra_dl_compute_nof_re(const srsran_cell_t* cell, srsran_dl_sf_cfg_t* sf, srsran_pdsch_grant_t* grant)
{
  // Compute number of RE
  grant->nof_re   = srsran_ra_dl_grant_nof_re(cell, sf, grant);
  srsran_cp_t cp_ = SRSRAN_SF_NORM == sf->sf_type ? cell->cp : SRSRAN_CP_EXT;
  if (cell->frame_type == SRSRAN_FDD) {
    grant->nof_symb_slot[0] = SRSRAN_CP_NSYMB(cp_);
    grant->nof_symb_slot[1] = SRSRAN_CP_NSYMB(cp_);
  } else {
    if (srsran_sfidx_tdd_type(sf->tdd_config, sf->tti % 10) == SRSRAN_TDD_SF_S) {
      grant->nof_symb_slot[0] = srsran_sfidx_tdd_nof_dw_slot(sf->tdd_config, 0, cp_);
      grant->nof_symb_slot[1] = srsran_sfidx_tdd_nof_dw_slot(sf->tdd_config, 1, cp_);
    } else {
      grant->nof_symb_slot[0] = SRSRAN_CP_NSYMB(cp_);
      grant->nof_symb_slot[1] = SRSRAN_CP_NSYMB(cp_);
    }
  }

  for (int i = 0; i < SRSRAN_MAX_TB; i++) {
    /* Compute number of RE for first transport block */
    if (grant->tb[i].enabled) {
      grant->tb[i].nof_bits = grant->nof_re * srsran_mod_bits_x_symbol(grant->tb[i].mod);
    }
  }
}

/* Determine MIMO type based on number of cell ports and receive antennas, transport blocks and pinfo */
static int
config_mimo_type(const srsran_cell_t* cell, srsran_tm_t tm, const srsran_dci_dl_t* dci, srsran_pdsch_grant_t* grant)
{
  grant->tx_scheme  = SRSRAN_TXSCHEME_PORT0;
  bool valid_config = true;

  uint32_t nof_tb = grant->nof_tb;
  switch (tm) {
    /* Implemented Tx Modes */
    case SRSRAN_TM1:
    case SRSRAN_TM2:
      if (cell->nof_ports > 1) {
        grant->tx_scheme = SRSRAN_TXSCHEME_DIVERSITY;
      } else {
        grant->tx_scheme = SRSRAN_TXSCHEME_PORT0;
      }
      if (nof_tb != 1) {
        ERROR("Wrong number of transport blocks (%d) for %s.", nof_tb, srsran_mimotype2str(grant->tx_scheme));
        valid_config = false;
      }
      break;
    case SRSRAN_TM3:
      if (nof_tb == 1) {
        grant->tx_scheme = SRSRAN_TXSCHEME_DIVERSITY;
      } else if (nof_tb == 2) {
        grant->tx_scheme = SRSRAN_TXSCHEME_CDD;
      } else {
        ERROR("Invalid number of transport blocks (%d) for TM3", nof_tb);
        valid_config = false;
      }
      break;
    case SRSRAN_TM4:
      if (nof_tb == 1) {
        grant->tx_scheme = (dci->pinfo == 0) ? SRSRAN_TXSCHEME_DIVERSITY : SRSRAN_TXSCHEME_SPATIALMUX;
      } else if (nof_tb == 2) {
        grant->tx_scheme = SRSRAN_TXSCHEME_SPATIALMUX;
      } else {
        ERROR("Invalid number of transport blocks (%d) for TM4", nof_tb);
        valid_config = false;
      }
      break;

      /* Not implemented cases */
    case SRSRAN_TM5:
    case SRSRAN_TM6:
    case SRSRAN_TM7:
    case SRSRAN_TM8:
      ERROR("Not implemented Tx mode (%d)", tm + 1);
      break;

      /* Error cases */
    default:
      ERROR("Wrong Tx mode (%d)", tm + 1);
  }
  return valid_config ? SRSRAN_SUCCESS : SRSRAN_ERROR;
}

/* Translates Precoding Information (pinfo) to Precoding matrix Index (pmi) as 3GPP 36.212 Table 5.3.3.1.5-4 */
static int config_mimo_pmi(const srsran_cell_t* cell, const srsran_dci_dl_t* dci, srsran_pdsch_grant_t* grant)
{
  uint32_t nof_tb = grant->nof_tb;
  if (grant->tx_scheme == SRSRAN_TXSCHEME_SPATIALMUX) {
    if (nof_tb == 1) {
      if (dci->pinfo > 0 && dci->pinfo < 5) {
        grant->pmi = dci->pinfo - 1;
      } else {
        ERROR("Not Implemented (nof_tb=%d, pinfo=%d)", nof_tb, dci->pinfo);
        return -1;
      }
    } else {
      if (dci->pinfo == 2) {
        ERROR("Not implemented codebook index (nof_tb=%d (%d/%d), pinfo=%d)",
              nof_tb,
              SRSRAN_DCI_IS_TB_EN(grant->tb[0]),
              SRSRAN_DCI_IS_TB_EN(grant->tb[1]),
              dci->pinfo);
        return -1;
      } else if (dci->pinfo > 2) {
        ERROR("Reserved codebook index (nof_tb=%d, pinfo=%d)", nof_tb, dci->pinfo);
        return -1;
      }
      grant->pmi = dci->pinfo % 2;
    }
  }

  return 0;
}

/* Determine number of MIMO layers */
static int config_mimo_layers(const srsran_cell_t* cell, const srsran_dci_dl_t* dci, srsran_pdsch_grant_t* grant)
{
  uint32_t nof_tb = grant->nof_tb;
  switch (grant->tx_scheme) {
    case SRSRAN_TXSCHEME_PORT0:
      if (nof_tb != 1) {
        ERROR("Wrong number of transport blocks (%d) for single antenna.", nof_tb);
        return SRSRAN_ERROR;
      }
      grant->nof_layers = 1;
      break;
    case SRSRAN_TXSCHEME_DIVERSITY:
      if (nof_tb != 1) {
        ERROR("Wrong number of transport blocks (%d) for transmit diversity.", nof_tb);
        return SRSRAN_ERROR;
      }
      grant->nof_layers = cell->nof_ports;
      break;
    case SRSRAN_TXSCHEME_SPATIALMUX:
      if (nof_tb == 1) {
        grant->nof_layers = 1;
      } else if (nof_tb == 2) {
        grant->nof_layers = 2;
      } else {
        ERROR("Wrong number of transport blocks (%d) for spatial multiplexing.", nof_tb);
        return SRSRAN_ERROR;
      }
      INFO("PDSCH configured for Spatial Multiplex; nof_codewords=%d; nof_layers=%d; pmi=%d",
           nof_tb,
           grant->nof_layers,
           grant->pmi);
      break;
    case SRSRAN_TXSCHEME_CDD:
      if (nof_tb != 2) {
        ERROR("Wrong number of transport blocks (%d) for CDD.", nof_tb);
        return SRSRAN_ERROR;
      }
      grant->nof_layers = 2;
      break;
  }
  return 0;
}

static int
config_mimo(const srsran_cell_t* cell, srsran_tm_t tm, const srsran_dci_dl_t* dci, srsran_pdsch_grant_t* grant)
{
  if (config_mimo_type(cell, tm, dci, grant)) {
    ERROR("Configuring MIMO type");
    return -1;
  }

  if (config_mimo_pmi(cell, dci, grant)) {
    ERROR("Configuring MIMO PMI");
    return -1;
  }

  if (config_mimo_layers(cell, dci, grant)) {
    ERROR("Configuring MIMO layers");
    return -1;
  }

  return 0;
}

/**********
 * NON-STATIC FUNCTIONS
 *
 **********/

/** Compute the DL grant parameters  */
int srsran_ra_dl_dci_to_grant(const srsran_cell_t*   cell,
                              srsran_dl_sf_cfg_t*    sf,
                              srsran_tm_t            tm,
                              bool                   pdsch_use_tbs_index_alt,
                              const srsran_dci_dl_t* dci,
                              srsran_pdsch_grant_t*  grant)
{
  bzero(grant, sizeof(srsran_pdsch_grant_t));

  // Compute PRB allocation
  int ret = srsran_ra_dl_grant_to_grant_prb_allocation(dci, grant, cell->nof_prb);
  if (ret == SRSRAN_SUCCESS) {
    // Compute MCS
    ret = dl_dci_compute_tb(pdsch_use_tbs_index_alt, dci, grant);
    if (ret == SRSRAN_SUCCESS) {
      // Compute number of RE and number of ack_value in grant
      srsran_ra_dl_compute_nof_re(cell, sf, grant);

      // Apply Section 7.1.7.3. If RA-RNTI and Format1C rv_idx=0
      if (dci->format == SRSRAN_DCI_FORMAT1C) {
        if ((SRSRAN_RNTI_ISRAR(dci->rnti)) || dci->rnti == SRSRAN_PRNTI) {
          for (uint32_t i = 0; i < SRSRAN_MAX_CODEWORDS; i++) {
            grant->tb[i].rv = 0;
          }
        }
      }
    } else {
      INFO("Configuring TB Info");
      return SRSRAN_ERROR;
    }
  } else {
    ERROR("Configuring resource allocation");
    return SRSRAN_ERROR;
  }

  // Configure MIMO for this TM
  return config_mimo(cell, tm, dci, grant);
}

/** Compute the DL grant parameters  
 *  In NG-Scope, we igore the mimo configuration
 ***/
int srsran_ra_dl_dci_to_grant_wo_mimo_yx(const srsran_cell_t*   cell,
                              srsran_dl_sf_cfg_t*    sf,
                              srsran_tm_t            tm,
                              bool                   pdsch_use_tbs_index_alt,
                              const srsran_dci_dl_t* dci,
                              srsran_pdsch_grant_t*  grant)
{
  bzero(grant, sizeof(srsran_pdsch_grant_t));

  // Compute PRB allocation
  int ret = srsran_ra_dl_grant_to_grant_prb_allocation(dci, grant, cell->nof_prb);
  if (ret == SRSRAN_SUCCESS) {
    // Compute MCS
    ret = dl_dci_compute_tb(pdsch_use_tbs_index_alt, dci, grant);
    if (ret == SRSRAN_SUCCESS) {
      // Compute number of RE and number of ack_value in grant
      srsran_ra_dl_compute_nof_re(cell, sf, grant);

      // Apply Section 7.1.7.3. If RA-RNTI and Format1C rv_idx=0
      if (dci->format == SRSRAN_DCI_FORMAT1C) {
        if ((SRSRAN_RNTI_ISRAR(dci->rnti)) || dci->rnti == SRSRAN_PRNTI) {
          for (uint32_t i = 0; i < SRSRAN_MAX_CODEWORDS; i++) {
            grant->tb[i].rv = 0;
          }
        }
      }
    } else {
      INFO("Configuring TB Info");
      return SRSRAN_ERROR;
    }
  } else {
    //ERROR("Configuring resource allocation");
    return SRSRAN_ERROR;
  }

  // Configure MIMO for this TM
  //return config_mimo(cell, tm, dci, grant);
  return 0;
}


uint32_t srsran_ra_dl_approx_nof_re(const srsran_cell_t* cell, uint32_t nof_prb, uint32_t nof_ctrl_symbols)
{
  uint32_t nof_refs = 0;
  uint32_t nof_symb = 2 * SRSRAN_CP_NSYMB(cell->cp) - nof_ctrl_symbols;
  switch (cell->nof_ports) {
    case 1:
      nof_refs = 2 * 3;
      break;
    case 2:
      nof_refs = 4 * 3;
      break;
    case 4:
      nof_refs = 4 * 4;
      break;
  }
  return nof_prb * (nof_symb * SRSRAN_NRE - nof_refs);
}

/* Computes the number of RE for each PRB in the prb_dist structure */
uint32_t srsran_ra_dl_grant_nof_re(const srsran_cell_t* cell, srsran_dl_sf_cfg_t* sf, srsran_pdsch_grant_t* grant)
{
  uint32_t j, s;
  // Compute number of RE per PRB
  uint32_t nof_re = 0;
  for (s = 0; s < 2; s++) {
    for (j = 0; j < cell->nof_prb; j++) {
      if (grant->prb_idx[s][j]) {
        nof_re += ra_re_x_prb(cell, sf, s, j);
      }
    }
  }
  return nof_re;
}

static uint32_t print_multi(char* info_str, uint32_t n, uint32_t len, srsran_pdsch_grant_t* grant, uint32_t value_id)
{
  for (uint32_t i = 0; i < SRSRAN_MAX_CODEWORDS; i++) {
    if (grant->tb[i].enabled) {
      switch (value_id) {
        case 0:
          n = srsran_print_check(info_str, len, n, "%d", grant->tb[i].rv);
          break;
        case 1:
          n = srsran_print_check(info_str, len, n, "%d", grant->tb[i].tbs / 8);
          break;
        case 2:
          n = srsran_print_check(info_str, len, n, "%d", srsran_mod_bits_x_symbol(grant->tb[i].mod));
          break;
      }
      if (i < SRSRAN_MAX_CODEWORDS - 1) {
        if (grant->tb[i + 1].enabled) {
          n = srsran_print_check(info_str, len, n, "/");
        }
      }
    }
  }
  return n;
}

uint32_t srsran_ra_dl_info(srsran_pdsch_grant_t* grant, char* info_str, uint32_t len)
{
  int n = 0;

  n = srsran_print_check(info_str, len, n, ", nof_prb=%d, nof_re=%d", grant->nof_prb, grant->nof_re);

  n = srsran_print_check(info_str, len, n, ", tbs={");
  n = print_multi(info_str, n, len, grant, 1);
  n = srsran_print_check(info_str, len, n, "}");
  n = srsran_print_check(info_str, len, n, ", mod={");
  n = print_multi(info_str, n, len, grant, 2);
  n = srsran_print_check(info_str, len, n, "}");
  n = srsran_print_check(info_str, len, n, ", rv={");
  n = print_multi(info_str, n, len, grant, 0);
  n = srsran_print_check(info_str, len, n, "}");

  if (grant->tx_scheme != SRSRAN_TXSCHEME_PORT0) {
    n = srsran_print_check(info_str,
                           len,
                           n,
                           ", tx=%s, nof_tb=%d, nof_l=%d",
                           srsran_mimotype2str(grant->tx_scheme),
                           grant->nof_tb,
                           grant->nof_layers);
    if (grant->tx_scheme == SRSRAN_TXSCHEME_SPATIALMUX) {
      n = srsran_print_check(info_str, len, n, ", pmi=%d", grant->pmi);
    }
  }
  return n;
}
