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

#include "srsran/phy/ue/ue_dl.h"

#include "srsran/srsran.h"
#include <string.h>

#define CURRENT_FFTSIZE srsran_symbol_sz(q->cell.nof_prb)
#define CURRENT_SFLEN_RE SRSRAN_NOF_RE(q->cell)
#define MAX_SFLEN_RE SRSRAN_SF_LEN_RE(max_prb, q->cell.cp)

const static srsran_dci_format_t ue_dci_formats[8][2] = {
    /* Mode 1 */ {SRSRAN_DCI_FORMAT1A, SRSRAN_DCI_FORMAT1},
    /* Mode 2 */ {SRSRAN_DCI_FORMAT1A, SRSRAN_DCI_FORMAT1},
    /* Mode 3 */ {SRSRAN_DCI_FORMAT1A, SRSRAN_DCI_FORMAT2A},
    /* Mode 4 */ {SRSRAN_DCI_FORMAT1A, SRSRAN_DCI_FORMAT2},
    /* Mode 5 */ {SRSRAN_DCI_FORMAT1A, SRSRAN_DCI_FORMAT1D},
    /* Mode 6 */ {SRSRAN_DCI_FORMAT1A, SRSRAN_DCI_FORMAT1B},
    /* Mode 7 */ {SRSRAN_DCI_FORMAT1A, SRSRAN_DCI_FORMAT1},
    /* Mode 8 */ {SRSRAN_DCI_FORMAT1A, SRSRAN_DCI_FORMAT2B}};

const uint32_t nof_ue_dci_formats = 2;

static srsran_dci_format_t common_formats[]   = {SRSRAN_DCI_FORMAT1A, SRSRAN_DCI_FORMAT1C};
const uint32_t             nof_common_formats = 2;

// mi value as in table 6.9-1 36.213 for regs vector. For FDD, uses only 1st
const static uint32_t mi_reg_idx[3]     = {1, 0, 2};
const static uint32_t mi_reg_idx_inv[3] = {1, 0, 2};

// Table 6.9-1: mi value for differnt ul/dl TDD configurations
const static uint32_t mi_tdd_table[7][10] = {{2, 1, 0, 0, 0, 2, 1, 0, 0, 0},  // ul/dl 0
                                             {0, 1, 0, 0, 1, 0, 1, 0, 0, 1},  // ul/dl 1
                                             {0, 0, 0, 1, 0, 0, 0, 0, 1, 0},  // ul/dl 2
                                             {1, 0, 0, 0, 0, 0, 0, 0, 1, 1},  // ul/dl 3
                                             {0, 0, 0, 0, 0, 0, 0, 0, 1, 1},  // ul/dl 4
                                             {0, 0, 0, 0, 0, 0, 0, 0, 1, 0},  // ul/dl 5
                                             {1, 1, 0, 0, 0, 1, 1, 0, 0, 1}}; // ul/dl 6

#define MI_VALUE(sf_idx) ((q->cell.frame_type == SRSRAN_FDD) ? 1 : mi_tdd_table[sf->tdd_config.sf_config][sf_idx])
#define MI_IDX(sf_idx)                                                                                                 \
  (mi_reg_idx_inv[MI_VALUE(sf_idx)] +                                                                                  \
   ((q->cell.frame_type == SRSRAN_TDD && q->cell.phich_length == SRSRAN_PHICH_EXT && (sf_idx == 1 || sf_idx == 6))     \
        ? 3                                                                                                            \
        : 0))

int srsran_ue_dl_init(srsran_ue_dl_t* q, cf_t* in_buffer[SRSRAN_MAX_PORTS], uint32_t max_prb, uint32_t nof_rx_antennas)
{
  int ret = SRSRAN_ERROR_INVALID_INPUTS;

  if (q != NULL && nof_rx_antennas <= SRSRAN_MAX_PORTS) {
    ret = SRSRAN_ERROR;

    bzero(q, sizeof(srsran_ue_dl_t));

    q->pending_ul_dci_count = 0;
    q->nof_rx_antennas      = nof_rx_antennas;
    q->mi_auto              = true;
    q->mi_manual_index      = 0;

    for (int j = 0; j < SRSRAN_MAX_PORTS; j++) {
      q->sf_symbols[j] = srsran_vec_cf_malloc(MAX_SFLEN_RE);
      if (!q->sf_symbols[j]) {
        perror("malloc");
        goto clean_exit;
      }
    }

    srsran_ofdm_cfg_t ofdm_cfg = {};
    ofdm_cfg.nof_prb           = max_prb;
    ofdm_cfg.cp                = SRSRAN_CP_NORM;
    ofdm_cfg.rx_window_offset  = 0.0f;
    ofdm_cfg.normalize         = false;
    for (int i = 0; i < nof_rx_antennas; i++) {
      ofdm_cfg.in_buffer  = in_buffer[i];
      ofdm_cfg.out_buffer = q->sf_symbols[i];
      ofdm_cfg.sf_type    = SRSRAN_SF_NORM;

      if (srsran_ofdm_rx_init_cfg(&q->fft[i], &ofdm_cfg)) {
        ERROR("Error initiating FFT");
        goto clean_exit;
      }
    }

    ofdm_cfg.in_buffer  = in_buffer[0];
    ofdm_cfg.out_buffer = q->sf_symbols[0];
    ofdm_cfg.sf_type    = SRSRAN_SF_MBSFN;
    if (srsran_ofdm_rx_init_cfg(&q->fft_mbsfn, &ofdm_cfg)) {
      ERROR("Error initiating FFT for MBSFN subframes ");
      goto clean_exit;
    }
    srsran_ofdm_set_non_mbsfn_region(&q->fft_mbsfn, 2); // Set a default to init

    if (srsran_chest_dl_init(&q->chest, max_prb, nof_rx_antennas)) {
      ERROR("Error initiating channel estimator");
      goto clean_exit;
    }
    if (srsran_chest_dl_res_init(&q->chest_res, max_prb)) {
      ERROR("Error initiating channel estimator");
      goto clean_exit;
    }
    if (srsran_pcfich_init(&q->pcfich, nof_rx_antennas)) {
      ERROR("Error creating PCFICH object");
      goto clean_exit;
    }
    if (srsran_phich_init(&q->phich, nof_rx_antennas)) {
      ERROR("Error creating PHICH object");
      goto clean_exit;
    }

    if (srsran_pdcch_init_ue(&q->pdcch, max_prb, nof_rx_antennas)) {
      ERROR("Error creating PDCCH object");
      goto clean_exit;
    }

    if (srsran_pdsch_init_ue(&q->pdsch, max_prb, nof_rx_antennas)) {
      ERROR("Error creating PDSCH object");
      goto clean_exit;
    }

    if (srsran_pmch_init(&q->pmch, max_prb, nof_rx_antennas)) {
      ERROR("Error creating PMCH object");
      goto clean_exit;
    }

    ret = SRSRAN_SUCCESS;
  } else {
    ERROR("Invalid parameters");
  }

clean_exit:
  if (ret == SRSRAN_ERROR) {
    srsran_ue_dl_free(q);
  }
  return ret;
}

void srsran_ue_dl_free(srsran_ue_dl_t* q)
{
  if (q) {
    for (int port = 0; port < SRSRAN_MAX_PORTS; port++) {
      srsran_ofdm_rx_free(&q->fft[port]);
    }
    srsran_ofdm_rx_free(&q->fft_mbsfn);
    srsran_chest_dl_free(&q->chest);
    srsran_chest_dl_res_free(&q->chest_res);
    for (int i = 0; i < SRSRAN_MI_NOF_REGS; i++) {
      srsran_regs_free(&q->regs[i]);
    }
    srsran_pcfich_free(&q->pcfich);
    srsran_phich_free(&q->phich);
    srsran_pdcch_free(&q->pdcch);
    srsran_pdsch_free(&q->pdsch);
    srsran_pmch_free(&q->pmch);
    for (int j = 0; j < SRSRAN_MAX_PORTS; j++) {
      if (q->sf_symbols[j]) {
        free(q->sf_symbols[j]);
      }
    }
    bzero(q, sizeof(srsran_ue_dl_t));
  }
}

int srsran_ue_dl_set_cell(srsran_ue_dl_t* q, srsran_cell_t cell)
{
  int ret = SRSRAN_ERROR_INVALID_INPUTS;

  if (q != NULL && srsran_cell_isvalid(&cell)) {
    q->pending_ul_dci_count = 0;

    if (q->cell.id != cell.id || q->cell.nof_prb == 0) {
      if (q->cell.nof_prb != 0) {
        for (int i = 0; i < SRSRAN_MI_NOF_REGS; i++) {
          srsran_regs_free(&q->regs[i]);
        }
      }
      q->cell = cell;
      for (int i = 0; i < SRSRAN_MI_NOF_REGS; i++) {
        if (srsran_regs_init_opts(&q->regs[i], q->cell, mi_reg_idx[i % 3], i > 2)) {
          ERROR("Error resizing REGs");
          return SRSRAN_ERROR;
        }
      }
      for (int port = 0; port < q->nof_rx_antennas; port++) {
        if (srsran_ofdm_rx_set_prb(&q->fft[port], q->cell.cp, q->cell.nof_prb)) {
          ERROR("Error resizing FFT");
          return SRSRAN_ERROR;
        }
      }

      // In TDD, initialize PDCCH and PHICH for the worst case: max ncces and phich groupds respectively
      uint32_t pdcch_init_reg = 0;
      uint32_t phich_init_reg = 0;
      if (q->cell.frame_type == SRSRAN_TDD) {
        pdcch_init_reg = 1; // mi=0
        phich_init_reg = 2; // mi=2
      }

      if (srsran_ofdm_rx_set_prb(&q->fft_mbsfn, SRSRAN_CP_EXT, q->cell.nof_prb)) {
        ERROR("Error resizing MBSFN FFT");
        return SRSRAN_ERROR;
      }

      if (srsran_chest_dl_set_cell(&q->chest, q->cell)) {
        ERROR("Error resizing channel estimator");
        return SRSRAN_ERROR;
      }
      if (srsran_pcfich_set_cell(&q->pcfich, &q->regs[0], q->cell)) {
        ERROR("Error resizing PCFICH object");
        return SRSRAN_ERROR;
      }
      if (srsran_phich_set_cell(&q->phich, &q->regs[phich_init_reg], q->cell)) {
        ERROR("Error resizing PHICH object");
        return SRSRAN_ERROR;
      }

      if (srsran_pdcch_set_cell(&q->pdcch, &q->regs[pdcch_init_reg], q->cell)) {
        ERROR("Error resizing PDCCH object");
        return SRSRAN_ERROR;
      }

      if (srsran_pdsch_set_cell(&q->pdsch, q->cell)) {
        ERROR("Error resizing PDSCH object");
        return SRSRAN_ERROR;
      }

      if (srsran_pmch_set_cell(&q->pmch, q->cell)) {
        ERROR("Error resizing PMCH object");
        return SRSRAN_ERROR;
      }
    }
    ret = SRSRAN_SUCCESS;
  } else {
    ERROR("Invalid cell properties ue_dl: Id=%d, Ports=%d, PRBs=%d", cell.id, cell.nof_ports, cell.nof_prb);
  }
  return ret;
}

void srsran_ue_dl_set_non_mbsfn_region(srsran_ue_dl_t* q, uint8_t non_mbsfn_region_length)
{
  srsran_ofdm_set_non_mbsfn_region(&q->fft_mbsfn, non_mbsfn_region_length);
}

void srsran_ue_dl_set_mi_auto(srsran_ue_dl_t* q)
{
  q->mi_auto = true;
}

void srsran_ue_dl_set_mi_manual(srsran_ue_dl_t* q, uint32_t mi_idx)
{
  q->mi_auto         = false;
  q->mi_manual_index = mi_idx;
}

/* Set the area ID on pmch and chest_dl to generate scrambling sequence and reference
 * signals.
 */
int srsran_ue_dl_set_mbsfn_area_id(srsran_ue_dl_t* q, uint16_t mbsfn_area_id)
{
  int ret = SRSRAN_ERROR_INVALID_INPUTS;
  if (q != NULL) {
    ret = SRSRAN_ERROR;
    if (srsran_chest_dl_set_mbsfn_area_id(&q->chest, mbsfn_area_id)) {
      ERROR("Error setting MBSFN area ID ");
      return ret;
    }
    if (srsran_pmch_set_area_id(&q->pmch, mbsfn_area_id)) {
      ERROR("Error setting MBSFN area ID ");
      return ret;
    }
    q->current_mbsfn_area_id = mbsfn_area_id;
    ret                      = SRSRAN_SUCCESS;
  }
  return ret;
}

static void set_mi_value(srsran_ue_dl_t* q, srsran_dl_sf_cfg_t* sf, srsran_ue_dl_cfg_t* cfg)
{
  uint32_t sf_idx = sf->tti % 10;
  // Set mi value in pdcch region
  if (q->mi_auto) {
    //INFO("Setting PHICH mi value auto. sf_idx=%d, mi=%d, idx=%d", sf_idx, MI_VALUE(sf_idx), MI_IDX(sf_idx));
    //printf("Setting PHICH mi value auto. sf_idx=%d, mi=%d, idx=%d\n", sf_idx, MI_VALUE(sf_idx), MI_IDX(sf_idx));
    srsran_phich_set_regs(&q->phich, &q->regs[MI_IDX(sf_idx)]);
    srsran_pdcch_set_regs(&q->pdcch, &q->regs[MI_IDX(sf_idx)]);
  } else {
    // No subframe 1 or 6 so no need to consider it
    INFO("Setting PHICH mi value manual. sf_idx=%d, mi=%d, idx=%d",
         sf_idx,
         q->mi_manual_index,
         mi_reg_idx_inv[q->mi_manual_index]);
    //printf("Setting PHICH mi value manual. sf_idx=%d, mi=%d, idx=%d\n",
    //     sf_idx,
    //     q->mi_manual_index,
    //     mi_reg_idx_inv[q->mi_manual_index]);

    srsran_phich_set_regs(&q->phich, &q->regs[mi_reg_idx_inv[q->mi_manual_index]]);
    srsran_pdcch_set_regs(&q->pdcch, &q->regs[mi_reg_idx_inv[q->mi_manual_index]]);
  }
}

static int estimate_pdcch_pcfich(srsran_ue_dl_t* q, srsran_dl_sf_cfg_t* sf, srsran_ue_dl_cfg_t* cfg)
{
  if (q) {
    float cfi_corr = 0;

    set_mi_value(q, sf, cfg);

    /* Get channel estimates for each port */
    srsran_chest_dl_estimate_cfg(&q->chest, sf, &cfg->chest_cfg, q->sf_symbols, &q->chest_res);

    /* First decode PCFICH and obtain CFI */
    if (srsran_pcfich_decode(&q->pcfich, sf, &q->chest_res, q->sf_symbols, &cfi_corr) < 0) {
      ERROR("Error decoding PCFICH");
      return SRSRAN_ERROR;
    }

    if (q->cell.frame_type == SRSRAN_TDD && ((sf->tti % 10) == 1 || (sf->tti % 10) == 6) && sf->cfi == 3) {
      sf->cfi = 2;
      INFO("Received CFI=3 in subframe 1 or 6 and TDD. Setting to 2");
    }

    if (srsran_pdcch_extract_llr(&q->pdcch, sf, &q->chest_res, q->sf_symbols)) {
      ERROR("Extracting PDCCH LLR");
      return false;
    }

    INFO("Decoded CFI=%d with correlation %.2f, sf_idx=%d", sf->cfi, cfi_corr, sf->tti % 10);

    return SRSRAN_SUCCESS;
  } else {
    return SRSRAN_ERROR_INVALID_INPUTS;
  }
}

int srsran_ue_dl_decode_fft_estimate(srsran_ue_dl_t* q, srsran_dl_sf_cfg_t* sf, srsran_ue_dl_cfg_t* cfg)
{
  if (q) {
    /* Run FFT for all subframe data */
    for (int j = 0; j < q->nof_rx_antennas; j++) {
      if (sf->sf_type == SRSRAN_SF_MBSFN) {
        srsran_ofdm_rx_sf(&q->fft_mbsfn);
      } else {
        srsran_ofdm_rx_sf(&q->fft[j]);
      }
    }
    return estimate_pdcch_pcfich(q, sf, cfg);
  } else {
    return SRSRAN_ERROR_INVALID_INPUTS;
  }
}

int srsran_ue_dl_decode_fft_estimate_noguru(srsran_ue_dl_t*     q,
                                            srsran_dl_sf_cfg_t* sf,
                                            srsran_ue_dl_cfg_t* cfg,
                                            cf_t*               input[SRSRAN_MAX_PORTS])
{
  if (q && input) {
    /* Run FFT for all subframe data */
    for (int j = 0; j < q->nof_rx_antennas; j++) {
      if (sf->sf_type == SRSRAN_SF_MBSFN) {
        srsran_ofdm_rx_sf_ng(&q->fft_mbsfn, input[j], q->sf_symbols[j]);
      } else {
        srsran_ofdm_rx_sf_ng(&q->fft[j], input[j], q->sf_symbols[j]);
      }
    }
    return estimate_pdcch_pcfich(q, sf, cfg);
  } else {
    return SRSRAN_ERROR_INVALID_INPUTS;
  }
}

static bool find_dci(srsran_dci_msg_t* dci_msg, uint32_t nof_dci_msg, srsran_dci_msg_t* match)
{
  bool     found    = false;
  uint32_t nof_bits = match->nof_bits;

  for (int k = 0; k < nof_dci_msg && !found; k++) {
    if (dci_msg[k].nof_bits == nof_bits) {
      if (memcmp(dci_msg[k].payload, match->payload, nof_bits) == 0) {
        found = true;
      }
    }
  }

  return found;
}

static bool dci_location_is_allocated(srsran_ue_dl_t* q, srsran_dci_location_t new_loc)
{
  for (uint32_t i = 0; i < q->nof_allocated_locations; i++) {
    uint32_t L    = q->allocated_locations[i].L;
    uint32_t ncce = q->allocated_locations[i].ncce;
    if ((ncce <= new_loc.ncce && new_loc.ncce < ncce + L) || // if new location starts in within an existing allocation
        (new_loc.ncce <= ncce &&
         ncce < new_loc.ncce + new_loc.L)) { // or an existing allocation starts within the new location
      return true;
    }
  }
  return false;
}

static int dci_blind_search(srsran_ue_dl_t*     q,
                            srsran_dl_sf_cfg_t* sf,
                            uint16_t            rnti,
                            dci_blind_search_t* search_space,
                            srsran_dci_cfg_t*   dci_cfg,
                            srsran_dci_msg_t    dci_msg[SRSRAN_MAX_DCI_MSG],
                            bool                search_in_common)
{
  uint32_t nof_dci = 0;
  if (rnti) {
    for (int l = 0; l < search_space->nof_locations; l++) {
      if (nof_dci >= SRSRAN_MAX_DCI_MSG) {
        ERROR("Can't store more DCIs in buffer");
        return nof_dci;
      }
      if (dci_location_is_allocated(q, search_space->loc[l])) {
        INFO("Skipping location L=%d, ncce=%d. Already allocated", search_space->loc[l].L, search_space->loc[l].ncce);
        continue;
      }
      for (uint32_t f = 0; f < search_space->nof_formats; f++) {
        INFO("Searching format %s in %d,%d (%d/%d)",
             srsran_dci_format_string(search_space->formats[f]),
             search_space->loc[l].ncce,
             search_space->loc[l].L,
             l,
             search_space->nof_locations);

        // Try to decode a valid DCI msg
        dci_msg[nof_dci].location = search_space->loc[l];
        dci_msg[nof_dci].format   = search_space->formats[f];
        dci_msg[nof_dci].rnti     = 0;
        if (srsran_pdcch_decode_msg(&q->pdcch, sf, dci_cfg, &dci_msg[nof_dci])) {
          ERROR("Error decoding DCI msg");
          return SRSRAN_ERROR;
        }

        // Check if RNTI is matched
        if ((dci_msg[nof_dci].rnti == rnti) && (dci_msg[nof_dci].nof_bits > 0)) {
          // Compute decoded message correlation to drastically reduce false alarm probability
          float corr = srsran_pdcch_msg_corr(&q->pdcch, &dci_msg[nof_dci]);

          // Skip candidate if the threshold is not reached
          // 0.5 is set from pdcch_test
          if (!isnormal(corr) || corr < 0.5f) {
            continue;
          }

          // Look for the messages found and apply the new format if the location is common
          if (search_in_common && (dci_cfg->multiple_csi_request_enabled || dci_cfg->srs_request_enabled)) {
            /*
             * A UE configured to monitor PDCCH candidates whose CRCs are scrambled with C-RNTI or SPS C-RNTI,
             * with a common payload size and with the same first CCE index ncce, but with different sets of DCI
             * information fields in the common and UE-specific search spaces on the primary cell, is required to assume
             * that only the PDCCH in the common search space is transmitted by the primary cell.
             */
            // Find a matching ncce in the common SS
            if (srsran_location_find_location(
                    q->current_ss_common.loc, q->current_ss_common.nof_locations, &dci_msg[nof_dci].location)) {
              srsran_dci_cfg_t cfg = *dci_cfg;
              srsran_dci_cfg_set_common_ss(&cfg);
              // if the payload size is the same that it would have in the common SS (only Format0/1A is allowed there)
              if (dci_msg[nof_dci].nof_bits == srsran_dci_format_sizeof(&q->cell, sf, &cfg, SRSRAN_DCI_FORMAT1A)) {
                // assume that only the PDDCH is transmitted, therefore update the format to 0/1A
                dci_msg[nof_dci].format = dci_msg[nof_dci].payload[0]
                                              ? SRSRAN_DCI_FORMAT1A
                                              : SRSRAN_DCI_FORMAT0; // Format0/1A bit indicator is the MSB
                INFO("DCI msg found in location L=%d, ncce=%d, size=%d belongs to the common SS and is format %s",
                     dci_msg[nof_dci].location.L,
                     dci_msg[nof_dci].location.ncce,
                     dci_msg[nof_dci].nof_bits,
                     srsran_dci_format_string_short(dci_msg[nof_dci].format));
              }
            }
          }

          // If found a Format0, save it for later
          if (dci_msg[nof_dci].format == SRSRAN_DCI_FORMAT0) {
            // If there is space for accumulate another UL DCI dci and it was not detected before, then store it
            if (q->pending_ul_dci_count < SRSRAN_MAX_DCI_MSG &&
                !find_dci(q->pending_ul_dci_msg, q->pending_ul_dci_count, &dci_msg[nof_dci])) {
              srsran_dci_msg_t* pending_ul_dci_msg = &q->pending_ul_dci_msg[q->pending_ul_dci_count];
              *pending_ul_dci_msg                  = dci_msg[nof_dci];
              q->pending_ul_dci_count++;
            }
            /* Check if the DCI is duplicated */
          } else if (!find_dci(dci_msg, (uint32_t)nof_dci, &dci_msg[nof_dci]) &&
                     !find_dci(q->pending_ul_dci_msg, q->pending_ul_dci_count, &dci_msg[nof_dci])) {
            // Save message and continue with next location
            if (q->nof_allocated_locations < SRSRAN_MAX_DCI_MSG) {
              q->allocated_locations[q->nof_allocated_locations] = dci_msg[nof_dci].location;
              q->nof_allocated_locations++;
            }
            nof_dci++;
            break;
          } else {
            INFO("Ignoring message with size %d, already decoded", dci_msg[nof_dci].nof_bits);
          }
        }
      }
    }
  } else {
    ERROR("RNTI not specified");
  }
  return nof_dci;
}

/* Check whether the locations of decoded DCI with RNTI is valid 
 * by valid we mean the location of the DCI follows the 3gpp standard 
 * nof_cce: the total number control channel element (CCE) 
 * nsubframe: subframe index
 * rnti: the RNTI of the decoded dci
 * this_ncce: the ncce of the decoded dci
 */

// Check the UE-specific search space 
static uint32_t srsran_ngscope_ue_locations_ncce_check_ue_specific(uint32_t nof_cce, uint32_t nsubframe, uint16_t rnti, uint32_t this_ncce) {
    int l; // this must be int because of the for(;;--) loop
    uint32_t i, L, m;
    uint32_t Yk, ncce;
    const int nof_candidates[4] = { 6, 6, 2, 2};

    // Compute Yk for this subframe
    Yk = rnti;
    for (m = 0; m < nsubframe+1; m++) {
        Yk = (39827 * Yk) % 65537;
    }

    // All aggregation levels from 8 to 1
    for (l = 3; l >= 0; l--) {
        L = (1 << l);
        // For all candidates as given in table 9.1.1-1
        for (i = 0; i < nof_candidates[l]; i++) {
            if (nof_cce >= L) {
                ncce = L * ((Yk + i) % (nof_cce / L));
                // Check if candidate fits in c vector and in CCE region
                if (ncce + L <= nof_cce)
                {
                    if (ncce==this_ncce) {
                        return 1; // cce matches
                    }
                }
            }
        }
    }
    return 0;
}

// Check the common search space 
static uint32_t srsran_ngscope_ue_locations_ncce_check_common(uint32_t nof_cce, uint32_t nsubframe, uint16_t rnti, uint32_t this_ncce) {
    int l; // this must be int because of the for(;;--) loop
    uint32_t i, L;
    uint32_t ncce;

    // Commom search space
    for (l = 3; l > 1; l--) {
        L = (1 << l);
        for (i = 0; i < SRSRAN_MIN(nof_cce, 16) / (L); i++) {
            ncce = (L) * (i % (nof_cce / (L)));
            if (ncce + L <= nof_cce){
                if (ncce==this_ncce) {
                    return 1; // cce matches
                }
            }
        }
    }
    return 0;
}

// Decide the search space according to the format

static uint32_t srsran_ngscope_is_common_space(srsran_dci_format_t format){
    if( format == SRSRAN_DCI_FORMAT0 || format == SRSRAN_DCI_FORMAT1A ){
        // both common and ue specific space
        return 0;
    }else if(format == SRSRAN_DCI_FORMAT1 || format == SRSRAN_DCI_FORMAT1B
       || format == SRSRAN_DCI_FORMAT1D || format == SRSRAN_DCI_FORMAT2 
       || format == SRSRAN_DCI_FORMAT2A || format == SRSRAN_DCI_FORMAT2B ){
        return 1;
    }else if(format == SRSRAN_DCI_FORMAT1C ){
        return 2;
    }else{
        ERROR("Format not recognized !\n");
        return 3;
    }
}


/* Yaxiong's implementation of the search in one space
 *
 */
int srsran_ngscope_search_in_space_yx(srsran_ue_dl_t*     q,
                            srsran_dl_sf_cfg_t* sf,
                            dci_blind_search_t* search_space,
                            srsran_dci_cfg_t*   dci_cfg,
                            srsran_dci_msg_t    dci_msg[MAX_NOF_FORMAT])
{
  uint32_t nof_dci = 0;
  int nof_cce = srsran_pdcch_get_nof_cce_yx(&q->pdcch, sf->cfi);
    for (int l = 0; l < search_space->nof_locations; l++) {
      if (nof_dci >= SRSRAN_MAX_DCI_MSG) {
        ERROR("Can't store more DCIs in buffer");
        return nof_dci;
      }
      if (dci_location_is_allocated(q, search_space->loc[l])) {
        INFO("Skipping location L=%d, ncce=%d. Already allocated", search_space->loc[l].L, search_space->loc[l].ncce);
        continue;
      }
      for (uint32_t f = 0; f < search_space->nof_formats; f++) {
        INFO("Searching format %s in %d,%d (%d/%d)",
             srsran_dci_format_string(search_space->formats[f]),
             search_space->loc[l].ncce,
             search_space->loc[l].L,
             l,
             search_space->nof_locations);

        // Try to decode a valid DCI msg
        dci_msg[nof_dci].location = search_space->loc[l];
        dci_msg[nof_dci].format   = search_space->formats[f];
        dci_msg[nof_dci].rnti     = 0;
        float decode_prob = 0;
        //if (srsran_pdcch_decode_msg(&q->pdcch, sf, dci_cfg, &dci_msg[nof_dci])) {
        if (srsran_pdcch_decode_msg_yx(&q->pdcch, sf, dci_cfg, &dci_msg[nof_dci], &decode_prob)) {
          ERROR("Error decoding DCI msg");
          return SRSRAN_ERROR;
        }else{
            //printf("PROB:%f\n", decode_prob);
        }
        dci_msg[nof_dci].decode_prob = decode_prob;
        // Check if RNTI is matched
        //if ((dci_msg[nof_dci].nof_bits > 0) && decode_prob > 50 ) {
        if ((dci_msg[nof_dci].nof_bits > 0) ) {
          // Compute decoded message correlation to drastically reduce false alarm probability
          float corr = srsran_pdcch_msg_corr(&q->pdcch, &dci_msg[nof_dci]);
          dci_msg[nof_dci].corr = corr;
          // Skip candidate if the threshold is not reached
          // 0.5 is set from pdcch_test
          if (!isnormal(corr) || corr < 0.5f) {
            //printf("Corr skip!\n");
            //continue;
          }

          // When searching for format 1A, we also need to consider format 0         
          //if(search_space->formats[f]  == SRSRAN_DCI_FORMAT1A){
          //    srsran_dci_format_t decoded_format = (dci_msg[nof_dci].payload[0] == 0) ? SRSRAN_DCI_FORMAT0 : SRSRAN_DCI_FORMAT1A;
          //    //printf("dci_msg format:%d decode format:%d\n", dci_msg[nof_dci].format, decoded_format);
          //    if ( dci_msg[nof_dci].format != decoded_format ){
          //      dci_msg[nof_dci].format = decoded_format; 
          //    }
          // }
          int ue_specific = srsran_ngscope_ue_locations_ncce_check_ue_specific(nof_cce, 
                                        sf->tti % 10, dci_msg[nof_dci].rnti, dci_msg[nof_dci].location.ncce); 
          int common_space = srsran_ngscope_ue_locations_ncce_check_common(nof_cce, 
                                        sf->tti % 10, dci_msg[nof_dci].rnti, dci_msg[nof_dci].location.ncce); 
          
        //  // Skip if the message location doesn't match with its search space
          uint32_t is_common = srsran_ngscope_is_common_space(dci_msg[nof_dci].format);
          if(is_common == 0){
              if( (ue_specific == 0) && (common_space == 0)) {
                //printf("UE specific and common doesn't match!\n");
                //continue;
              }
           }
           if(is_common == 1){
              if( (ue_specific == 1) && (common_space == 0)){
                //printf("Location Matched!\n");
              }else{
                //continue;
              }
           }
           if(is_common == 2){
              if( (ue_specific == 0) && (common_space == 1)){
                //printf("Location Matched!\n");
              }else{
                //continue;
              }
           }
          nof_dci++;
        }
      }
    }
  
  return nof_dci;
}


static int find_dci_ss(srsran_ue_dl_t*            q,
                       srsran_dl_sf_cfg_t*        sf,
                       srsran_ue_dl_cfg_t*        cfg,
                       uint16_t                   rnti,
                       srsran_dci_msg_t*          dci_msg,
                       const srsran_dci_format_t* formats,
                       uint32_t                   nof_formats,
                       bool                       is_ue)
{
  dci_blind_search_t search_space = {};

  uint32_t         cfi     = sf->cfi;
  srsran_dci_cfg_t dci_cfg = cfg->cfg.dci;

  if (!SRSRAN_CFI_ISVALID(cfi)) {
    ERROR("Invalid CFI=%d", cfi);
    return SRSRAN_ERROR_INVALID_INPUTS;
  }

  // Generate Common Search space
  q->current_ss_common.nof_locations =
      srsran_pdcch_common_locations(&q->pdcch, q->current_ss_common.loc, SRSRAN_MAX_CANDIDATES_COM, cfi);

  // Generate Search Space
  if (is_ue) {
    search_space.nof_locations =
        srsran_pdcch_ue_locations(&q->pdcch, sf, search_space.loc, SRSRAN_MAX_CANDIDATES_UE, rnti);
  } else {
    // Disable extended CSI request and SRS request in common SS
    srsran_dci_cfg_set_common_ss(&dci_cfg);
    search_space = q->current_ss_common;
  }

  // Search for DCI in the SS
  search_space.nof_formats = nof_formats;
  memcpy(search_space.formats, formats, nof_formats * sizeof(srsran_dci_format_t));

  INFO("Searching %d formats in %d locations in %s SS, csi=%d",
       nof_formats,
       search_space.nof_locations,
       is_ue ? "ue" : "common",
       dci_cfg.multiple_csi_request_enabled);

  return dci_blind_search(q, sf, rnti, &search_space, &dci_cfg, dci_msg, cfg->cfg.dci_common_ss);
}

/*
 * Note: This function does not perform a DCI search. It just copies the Format0 messages from the
 * pending_ul_dci_msg buffer found during a call to srsran_ue_dl_find_dl_dci().
 * It is assumed that the user called srsran_ue_dl_find_dl_dci() prior to calling this function.
 */
int srsran_ue_dl_find_ul_dci(srsran_ue_dl_t*     q,
                             srsran_dl_sf_cfg_t* sf,
                             srsran_ue_dl_cfg_t* dl_cfg,
                             uint16_t            rnti,
                             srsran_dci_ul_t     dci_ul[SRSRAN_MAX_DCI_MSG])
{
  srsran_dci_msg_t dci_msg[SRSRAN_MAX_DCI_MSG];
  uint32_t         nof_msg = 0;

  if (rnti) {
    // Copy the messages found in the last call to srsran_ue_dl_find_dl_dci()
    nof_msg = SRSRAN_MIN(SRSRAN_MAX_DCI_MSG, q->pending_ul_dci_count);
    memcpy(dci_msg, q->pending_ul_dci_msg, sizeof(srsran_dci_msg_t) * nof_msg);
    q->pending_ul_dci_count = 0;

    // Unpack DCI messages
    for (uint32_t i = 0; i < nof_msg; i++) {
      if (srsran_dci_msg_unpack_pusch(&q->cell, sf, &dl_cfg->cfg.dci, &dci_msg[i], &dci_ul[i])) {
        ERROR("Unpacking UL DCI");
        return SRSRAN_ERROR;
      }
    }

    return nof_msg;

  } else {
    return 0;
  }
}

// Blind search for SI/P/RA-RNTI
static int find_dl_dci_type_siprarnti(srsran_ue_dl_t*     q,
                                      srsran_dl_sf_cfg_t* sf,
                                      srsran_ue_dl_cfg_t* dl_cfg,
                                      uint16_t            rnti,
                                      srsran_dci_msg_t    dci_msg[SRSRAN_MAX_DCI_MSG])
{
  return find_dci_ss(q, sf, dl_cfg, rnti, dci_msg, common_formats, nof_common_formats, false);
}

// Blind search for C-RNTI
static int find_dl_ul_dci_type_crnti(srsran_ue_dl_t*     q,
                                     srsran_dl_sf_cfg_t* sf,
                                     srsran_ue_dl_cfg_t* dl_cfg,
                                     uint16_t            rnti,
                                     srsran_dci_msg_t*   dci_msg)
{
  int ret = SRSRAN_SUCCESS;

  if (dl_cfg->cfg.tm > SRSRAN_TM8) {
    ERROR("Searching DL CRNTI: Invalid TM=%d", dl_cfg->cfg.tm + 1);
    return SRSRAN_ERROR;
  }

  int nof_dci_msg = 0;

  // Although the common SS has higher priority than the UE, we'll start the search with UE space
  // since has the smallest aggregation levels. With the messages found in the UE space, we'll
  // check if they belong to the common SS and change the format if needed

  // Search UE-specific search space
  if ((ret = find_dci_ss(q, sf, dl_cfg, rnti, dci_msg, ue_dci_formats[dl_cfg->cfg.tm], nof_ue_dci_formats, true)) < 0) {
    return ret;
  }

  nof_dci_msg += ret;

  // Search common SS
  if (dl_cfg->cfg.dci_common_ss) {
    // Search only for SRSRAN_DCI_FORMAT1A (1st in common_formats) when looking for C-RNTI
    ret = 0;
    if ((ret = find_dci_ss(q, sf, dl_cfg, rnti, &dci_msg[nof_dci_msg], common_formats, 1, false)) < 0) {
      return ret;
    }
    nof_dci_msg += ret;
  }

  return nof_dci_msg;
}

int srsran_ue_dl_find_dl_dci(srsran_ue_dl_t*     q,
                             srsran_dl_sf_cfg_t* sf,
                             srsran_ue_dl_cfg_t* dl_cfg,
                             uint16_t            rnti,
                             srsran_dci_dl_t     dci_dl[SRSRAN_MAX_DCI_MSG])
{
  set_mi_value(q, sf, dl_cfg);

  srsran_dci_msg_t dci_msg[SRSRAN_MAX_DCI_MSG] = {};

  // Reset pending UL grants on each call
  q->pending_ul_dci_count = 0;

  // Reset allocated DCI locations
  q->nof_allocated_locations = 0;

  int nof_msg = 0;
  if (rnti == SRSRAN_SIRNTI || rnti == SRSRAN_PRNTI || SRSRAN_RNTI_ISRAR(rnti)) {
    nof_msg = find_dl_dci_type_siprarnti(q, sf, dl_cfg, rnti, dci_msg);
  } else {
    nof_msg = find_dl_ul_dci_type_crnti(q, sf, dl_cfg, rnti, dci_msg);
  }

  if (nof_msg < 0) {
    ERROR("Invalid number of DCI messages");
    return SRSRAN_ERROR;
  }

  // Unpack DCI messages
  for (uint32_t i = 0; i < nof_msg; i++) {
    if (srsran_dci_msg_unpack_pdsch(&q->cell, sf, &dl_cfg->cfg.dci, &dci_msg[i], &dci_dl[i])) {
      ERROR("Unpacking DL DCI");
      return SRSRAN_ERROR;
    }
  }
  return nof_msg;
}

int srsran_ue_dl_dci_to_pdsch_grant(srsran_ue_dl_t*       q,
                                    srsran_dl_sf_cfg_t*   sf,
                                    srsran_ue_dl_cfg_t*   cfg,
                                    srsran_dci_dl_t*      dci,
                                    srsran_pdsch_grant_t* grant)
{
  return srsran_ra_dl_dci_to_grant(&q->cell, sf, cfg->cfg.tm, cfg->cfg.pdsch.use_tbs_index_alt, dci, grant);
}

/****************************************************************************
 *In NG-Scope, we igore the MIMO related parameters(such as pmi). 
 * We igore because that the current hasn't implemented some MIMO type (e.g., 4x2) yet  
 * and will report error when decoding those MIMO configurations
 * We don't want that so we igore those errors by not configure the parameters
 *******************************************************************************/ 
int srsran_ue_dl_dci_to_pdsch_grant_wo_mimo_yx(srsran_ue_dl_t*       q,
                                    srsran_dl_sf_cfg_t*   sf,
                                    srsran_ue_dl_cfg_t*   cfg,
                                    srsran_dci_dl_t*      dci,
                                    srsran_pdsch_grant_t* grant)
{
  return srsran_ra_dl_dci_to_grant_wo_mimo_yx(&q->cell, sf, cfg->cfg.tm, cfg->cfg.pdsch.use_tbs_index_alt, dci, grant);
}


int srsran_ue_dl_decode_pdsch(srsran_ue_dl_t*     q,
                              srsran_dl_sf_cfg_t* sf,
                              srsran_pdsch_cfg_t* pdsch_cfg,
                              srsran_pdsch_res_t  data[SRSRAN_MAX_CODEWORDS])
{
  return srsran_pdsch_decode(&q->pdsch, sf, pdsch_cfg, &q->chest_res, q->sf_symbols, data);
}

int srsran_ue_dl_decode_pmch(srsran_ue_dl_t*     q,
                             srsran_dl_sf_cfg_t* sf,
                             srsran_pmch_cfg_t*  pmch_cfg,
                             srsran_pdsch_res_t  data[SRSRAN_MAX_CODEWORDS])
{
  return srsran_pmch_decode(&q->pmch, sf, pmch_cfg, &q->chest_res, q->sf_symbols, &data[0]);
}

int srsran_ue_dl_decode_phich(srsran_ue_dl_t*       q,
                              srsran_dl_sf_cfg_t*   sf,
                              srsran_ue_dl_cfg_t*   cfg,
                              srsran_phich_grant_t* grant,
                              srsran_phich_res_t*   result)
{
  srsran_phich_resource_t n_phich;

  uint32_t sf_idx = sf->tti % 10;

  set_mi_value(q, sf, cfg);

  srsran_phich_calc(&q->phich, grant, &n_phich);
  INFO("Decoding PHICH sf_idx=%d, n_prb_lowest=%d, n_dmrs=%d, I_phich=%d, n_group=%d, n_seq=%d, Ngroups=%d, Nsf=%d",
       sf_idx,
       grant->n_prb_lowest,
       grant->n_dmrs,
       grant->I_phich,
       n_phich.ngroup,
       n_phich.nseq,
       srsran_phich_ngroups(&q->phich),
       srsran_phich_nsf(&q->phich));

  if (!srsran_phich_decode(&q->phich, sf, &q->chest_res, n_phich, q->sf_symbols, result)) {
    INFO("Decoded PHICH %d with distance %f", result->ack_value, result->distance);
    return 0;
  } else {
    ERROR("Error decoding PHICH");
    return -1;
  }
}

/* Compute the Rank Indicator (RI) and Precoder Matrix Indicator (PMI) by computing the Signal to Interference plus
 * Noise Ratio (SINR), valid for TM4 */
static int select_pmi(srsran_ue_dl_t* q, uint32_t ri, uint32_t* pmi, float* sinr_db)
{
  uint32_t best_pmi = 0;
  float    sinr_list[SRSRAN_MAX_CODEBOOKS];

  if (q->cell.nof_ports < 2) {
    /* Do nothing */
    return SRSRAN_SUCCESS;
  } else {
    if (srsran_pdsch_select_pmi(&q->pdsch, &q->chest_res, ri + 1, &best_pmi, sinr_list)) {
      DEBUG("SINR calculation error");
      return SRSRAN_ERROR;
    }

    /* Set PMI */
    if (pmi != NULL) {
      *pmi = best_pmi;
    }

    /* Set PMI */
    if (sinr_db != NULL) {
      *sinr_db = srsran_convert_power_to_dB(sinr_list[best_pmi % SRSRAN_MAX_CODEBOOKS]);
    }
  }

  return SRSRAN_SUCCESS;
}

static int select_ri_pmi(srsran_ue_dl_t* q, uint32_t* ri, uint32_t* pmi, float* sinr_db)
{
  float    best_sinr_db = -INFINITY;
  uint32_t best_pmi = 0, best_ri = 0;
  uint32_t max_ri = SRSRAN_MIN(q->nof_rx_antennas, q->cell.nof_ports);

  if (q->cell.nof_ports < 2) {
    /* Do nothing */
    return SRSRAN_SUCCESS;
  } else {
    /* Select the best Rank indicator (RI) and Precoding Matrix Indicator (PMI) */
    for (uint32_t this_ri = 0; this_ri < max_ri; this_ri++) {
      uint32_t this_pmi     = 0;
      float    this_sinr_db = 0.0f;
      if (select_pmi(q, this_ri, &this_pmi, &this_sinr_db)) {
        DEBUG("SINR calculation error");
        return SRSRAN_ERROR;
      }

      /* Find best SINR, force maximum number of layers if SNR is higher than 30 dB */
      if (this_sinr_db > best_sinr_db + 0.1 || this_sinr_db > 20.0) {
        best_sinr_db = this_sinr_db;
        best_pmi     = this_pmi;
        best_ri      = this_ri;
      }
    }
  }

  /* Set RI */
  if (ri != NULL) {
    *ri = best_ri;
  }

  /* Set PMI */
  if (pmi != NULL) {
    *pmi = best_pmi;
  }

  /* Set SINR */
  if (sinr_db != NULL) {
    *sinr_db = best_sinr_db;
  }

  return SRSRAN_SUCCESS;
}

/* Compute the Rank Indicator (RI) by computing the condition number, valid for TM3 */
int srsran_ue_dl_select_ri(srsran_ue_dl_t* q, uint32_t* ri, float* cn)
{
  float _cn = INFINITY;
  int   ret = srsran_pdsch_compute_cn(&q->pdsch, &q->chest_res, &_cn);

  if (ret == SRSRAN_SUCCESS) {
    /* Set Condition number */
    if (cn) {
      *cn = _cn;
    }

    /* Set rank indicator */
    if (ri) {
      *ri = (uint8_t)((_cn < 17.0f) ? 1 : 0);
    }
  }

  return ret;
}

void srsran_ue_dl_gen_cqi_periodic(srsran_ue_dl_t*     q,
                                   srsran_ue_dl_cfg_t* cfg,
                                   uint32_t            wideband_value,
                                   uint32_t            tti,
                                   srsran_uci_data_t*  uci_data)
{
  if (srsran_cqi_periodic_ri_send(&cfg->cfg.cqi_report, tti, q->cell.frame_type)) {
    /* Compute RI, PMI and SINR */
    if (q->nof_rx_antennas > 1) {
      if (cfg->cfg.tm == SRSRAN_TM3) {
        srsran_ue_dl_select_ri(q, &cfg->last_ri, NULL);
      } else if (cfg->cfg.tm == SRSRAN_TM4) {
        select_ri_pmi(q, &cfg->last_ri, NULL, NULL);
      }
    } else {
      cfg->last_ri = 0;
    }
    uci_data->cfg.cqi.ri_len = 1;
    uci_data->value.ri       = cfg->last_ri;
  } else if (srsran_cqi_periodic_send(&cfg->cfg.cqi_report, tti, q->cell.frame_type)) {
    if (cfg->cfg.cqi_report.format_is_subband &&
        srsran_cqi_periodic_is_subband(&cfg->cfg.cqi_report, tti, q->cell.nof_prb, q->cell.frame_type)) {
      // TODO: Implement subband periodic reports
      uci_data->cfg.cqi.type                       = SRSRAN_CQI_TYPE_SUBBAND_UE;
      uci_data->value.cqi.subband_ue.subband_cqi   = wideband_value;
      uci_data->value.cqi.subband_ue.subband_label = tti / 100 % 2;
      uci_data->cfg.cqi.L                          = srsran_cqi_hl_get_L(q->cell.nof_prb);
      uci_data->cfg.cqi.subband_label_2_bits       = uci_data->cfg.cqi.L > 1;
    } else {
      uci_data->cfg.cqi.type                    = SRSRAN_CQI_TYPE_WIDEBAND;
      uci_data->value.cqi.wideband.wideband_cqi = wideband_value;
      if (cfg->cfg.tm == SRSRAN_TM4) {
        uint32_t pmi = 0;
        select_pmi(q, cfg->last_ri, &pmi, NULL);

        uci_data->cfg.cqi.pmi_present     = true;
        uci_data->cfg.cqi.rank_is_not_one = (cfg->last_ri != 0);
        uci_data->value.cqi.wideband.pmi  = (uint8_t)pmi;
      }
    }
    uci_data->cfg.cqi.data_enable = true;
    uci_data->cfg.cqi.ri_len      = 0;
    uci_data->value.ri            = cfg->last_ri;
  }
}

void srsran_ue_dl_gen_cqi_aperiodic(srsran_ue_dl_t*     q,
                                    srsran_ue_dl_cfg_t* cfg,
                                    uint32_t            wideband_value,
                                    srsran_uci_data_t*  uci_data)
{
  uint32_t pmi     = 0;
  float    sinr_db = 0.0f;

  switch (cfg->cfg.cqi_report.aperiodic_mode) {
    case SRSRAN_CQI_MODE_30:
      /* only Higher Layer-configured subband feedback support right now, according to TS36.213 section 7.2.1
        - A UE shall report a wideband CQI value which is calculated assuming transmission on set S subbands
        - The UE shall also report one subband CQI value for each set S subband. The subband CQI
          value is calculated assuming transmission only in the subband
        - Both the wideband and subband CQI represent channel quality for the first codeword,
          even when RI>1
        - For transmission mode 3 the reported CQI values are calculated conditioned on the
          reported RI. For other transmission modes they are reported conditioned on rank 1.
      */

      uci_data->cfg.cqi.type                          = SRSRAN_CQI_TYPE_SUBBAND_HL;
      uci_data->value.cqi.subband_hl.wideband_cqi_cw0 = wideband_value;

      // TODO: implement subband CQI properly
      uci_data->value.cqi.subband_hl.subband_diff_cqi_cw0 = 0; // Always report zero offset on all subbands
      uci_data->cfg.cqi.N = (q->cell.nof_prb > 7) ? (uint32_t)srsran_cqi_hl_get_no_subbands(q->cell.nof_prb) : 0;
      uci_data->cfg.cqi.data_enable = true;

      /* Set RI = 1 */
      if (cfg->cfg.tm == SRSRAN_TM3 || cfg->cfg.tm == SRSRAN_TM4) {
        if (q->nof_rx_antennas > 1) {
          srsran_ue_dl_select_ri(q, &cfg->last_ri, NULL);
          uci_data->value.ri       = (uint8_t)cfg->last_ri;
          uci_data->cfg.cqi.ri_len = 1;
        } else {
          uci_data->value.ri = 0;
        }
      } else {
        uci_data->cfg.cqi.ri_len = 0;
      }

      break;
    case SRSRAN_CQI_MODE_31:
      /* only Higher Layer-configured subband feedback support right now, according to TS36.213 section 7.2.1
        - A single precoding matrix is selected from the codebook subset assuming transmission on set S subbands
        - A UE shall report one subband CQI value per codeword for each set S subband which are calculated assuming
          the use of the single precoding matrix in all subbands and assuming transmission in the corresponding
          subband.
        - A UE shall report a wideband CQI value per codeword which is calculated assuming the use of the single
          precoding matrix in all subbands and transmission on set S subbands
        - The UE shall report the single selected precoding matrix indicator.
        - For transmission mode 4 the reported PMI and CQI values are calculated conditioned on the reported RI. For
          other transmission modes they are reported conditioned on rank 1.
      */
      /* Loads the latest SINR according to the calculated RI and PMI */
      pmi     = 0;
      sinr_db = 0.0f;
      select_ri_pmi(q, &cfg->last_ri, &pmi, &sinr_db);

      /* Fill CQI Report */
      uci_data->cfg.cqi.type = SRSRAN_CQI_TYPE_SUBBAND_HL;

      uci_data->value.cqi.subband_hl.wideband_cqi_cw0     = srsran_cqi_from_snr(sinr_db + cfg->snr_to_cqi_offset);
      uci_data->value.cqi.subband_hl.subband_diff_cqi_cw0 = 0; // Always report zero offset on all subbands

      if (cfg->last_ri > 0) {
        uci_data->cfg.cqi.rank_is_not_one                   = true;
        uci_data->value.cqi.subband_hl.wideband_cqi_cw1     = srsran_cqi_from_snr(sinr_db + cfg->snr_to_cqi_offset);
        uci_data->value.cqi.subband_hl.subband_diff_cqi_cw1 = 0; // Always report zero offset on all subbands
      }

      uci_data->value.cqi.subband_hl.pmi   = pmi;
      uci_data->cfg.cqi.pmi_present        = true;
      uci_data->cfg.cqi.four_antenna_ports = (q->cell.nof_ports == 4);
      uci_data->cfg.cqi.N = (uint32_t)((q->cell.nof_prb > 7) ? srsran_cqi_hl_get_no_subbands(q->cell.nof_prb) : 0);

      uci_data->cfg.cqi.data_enable = true;
      uci_data->cfg.cqi.ri_len      = 1;
      uci_data->value.ri            = cfg->last_ri;

      break;
    default:
      ERROR("CQI mode %d not supported", cfg->cfg.cqi_report.aperiodic_mode);
      break;
  }
}

static void ue_dl_gen_ack_fdd_none(const srsran_pdsch_ack_t* ack_info, srsran_uci_data_t* uci_data)
{
  // Set all carriers number of ACKs to 0
  for (uint32_t i = 0; i < ack_info->nof_cc; i++) {
    uci_data->cfg.ack[i].nof_acks = 0;
  }
}

static void
ue_dl_gen_ack_fdd_pcell_skip_drx(const srsran_pdsch_ack_t* ack_info, srsran_uci_data_t* uci_data, uint32_t nof_tb)
{
  uint32_t ack_idx = 0;

  // Find ACK/NACK
  if (ack_info->cc[0].m[0].present) {
    for (uint32_t tb = 0; tb < nof_tb; tb++) {
      if (ack_info->cc[0].m[0].value[tb] != 2) {
        uci_data->value.ack.ack_value[ack_idx] = ack_info->cc[0].m[0].value[tb];
        ack_idx++;
      }
    }
  }

  // Set number of ACKs for PCell
  uci_data->cfg.ack[0].nof_acks = ack_idx;

  // Set rest of carriers to 0 ACKs
  for (uint32_t i = 1; i < ack_info->nof_cc; i++) {
    uci_data->cfg.ack[i].nof_acks = 0;
  }
}

static void
ue_dl_gen_ack_fdd_all_keep_drx(const srsran_pdsch_ack_t* ack_info, srsran_uci_data_t* uci_data, uint32_t nof_tb)
{
  for (uint32_t cc_idx = 0; cc_idx < ack_info->nof_cc; cc_idx++) {
    // Find ACK/NACK
    if (ack_info->cc[cc_idx].m[0].present) {
      for (uint32_t tb = 0; tb < nof_tb; tb++) {
        if (ack_info->cc[cc_idx].m[0].value[tb] != 2) {
          uci_data->value.ack.ack_value[cc_idx * nof_tb + tb] = ack_info->cc[cc_idx].m[0].value[tb];
        }
      }
    }

    // Set all carriers to maximum number of TBs
    uci_data->cfg.ack[cc_idx].nof_acks = nof_tb;
  }
}

static void
ue_dl_gen_ack_fdd_all_spatial_bundling(const srsran_pdsch_ack_t* ack_info, srsran_uci_data_t* uci_data, uint32_t nof_tb)
{
  uint32_t nof_ack = 0;

  for (uint32_t cc_idx = 0; cc_idx < ack_info->nof_cc; cc_idx++) {
    if (ack_info->cc[cc_idx].m[0].present) {
      uci_data->value.ack.ack_value[cc_idx] = 1;
      for (uint32_t tb = 0; tb < nof_tb; tb++) {
        if (ack_info->cc[cc_idx].m[0].value[tb] != 2) {
          uci_data->value.ack.ack_value[cc_idx] &= ack_info->cc[cc_idx].m[0].value[tb];
          nof_ack++;
        }
      }
    } else {
      uci_data->value.ack.ack_value[cc_idx] = 2;
    }
  }

  // If no ACK is counted, set all zero, bundle otherwise
  for (uint32_t i = 0; i < SRSRAN_PUCCH_CS_MAX_CARRIERS; i++) {
    uci_data->cfg.ack[i].nof_acks = (nof_ack == 0) ? 0 : 1;
  }
}

/* UE downlink procedure for reporting HARQ-ACK bits in FDD, Section 7.3 36.213
 */
static void gen_ack_fdd(const srsran_pdsch_ack_t* ack_info, srsran_uci_data_t* uci_data)
{
  // Number of transport blocks for the current Transmission Mode
  uint32_t nof_tb = 1;
  if (ack_info->transmission_mode > SRSRAN_TM2) {
    nof_tb = SRSRAN_MAX_CODEWORDS;
  }

  // Count number of transmissions
  uint32_t tb_count     = 0; // All transmissions
  uint32_t tb_count_cc0 = 0; // Transmissions on PCell
  for (uint32_t cc_idx = 0; cc_idx < ack_info->nof_cc; cc_idx++) {
    for (uint32_t tb = 0; tb < nof_tb; tb++) {
      if (ack_info->cc[cc_idx].m[0].present && ack_info->cc[cc_idx].m[0].value[tb] != 2) {
        tb_count++;
      }

      // Save primary cell number of TB
      if (cc_idx == 0) {
        tb_count_cc0 = tb_count;
      }
    }
  }

  // if no transmission counted return without reporting any ACK/NACK
  if (tb_count == 0) {
    ue_dl_gen_ack_fdd_none(ack_info, uci_data);
    return;
  }

  // Count total of Uplink Control Bits
  uint32_t total_uci_bits =
      tb_count + srsran_cqi_size(&uci_data->cfg.cqi) + (uci_data->value.scheduling_request ? 1 : 0);

  // Does CSI report need to be transmitted?
  bool csi_report = uci_data->cfg.cqi.data_enable || uci_data->cfg.cqi.ri_len;

  // Logic for dropping CSI report if required
  if (csi_report && !ack_info->is_pusch_available) {
    bool drop_csi_report = true; ///< CSI report shall be dropped by default

    // 3GPP 36.213 R.15 Section 10.1.1:
    // For FDD or for FDD-TDD and primary cell frame structure type 1 and for a UE that is configured with more than
    // one serving cell, in case of collision between a periodic CSI report and an HARQ-ACK in a same subframe without
    // PUSCH,

    // - if the parameter simultaneousAckNackAndCQI provided by higher layers is set TRUE and if the HARQ-ACK
    //   corresponds to a PDSCH transmission or PDCCH/EPDCCH indicating downlink SPS release only on the
    //   primary cell, then the periodic CSI report is multiplexed with HARQ-ACK on PUCCH using PUCCH format 2/2a/2b
    drop_csi_report &= !(tb_count_cc0 == tb_count && ack_info->simul_cqi_ack);

    // - else if the UE is configured with PUCCH format 3 and if the parameter simultaneousAckNackAndCQI-Format3-
    //   r11 provided by higher layers is set TRUE, and if PUCCH resource is determined according to subclause
    //   10.1.2.2.2, and
    //   - if the total number of bits in the subframe corresponding to HARQ-ACKs, SR (if any), and the CSI is not
    //     larger than 22 or
    //   - if the total number of bits in the subframe corresponding to spatially bundled HARQ-ACKs, SR (if any),
    //     and the CSI is not larger than 22 then the periodic CSI report is multiplexed with HARQ-ACK on PUCCH
    //     using the determined PUCCH format 3 resource according to [4]
    drop_csi_report &= !(ack_info->simul_cqi_ack_pucch3 && total_uci_bits <= 22);

    // - otherwise, CSI is dropped
    if (drop_csi_report) {
      uci_data->cfg.cqi.data_enable = false;
      uci_data->cfg.cqi.ri_len      = 0;
      csi_report                    = false;
    }
  }

  // For each HARQ ACK/NACK feedback mode
  switch (ack_info->ack_nack_feedback_mode) {
    case SRSRAN_PUCCH_ACK_NACK_FEEDBACK_MODE_NORMAL:
      // Get ACK from PCell only, skipping DRX
      ue_dl_gen_ack_fdd_pcell_skip_drx(ack_info, uci_data, nof_tb);
      break;
    case SRSRAN_PUCCH_ACK_NACK_FEEDBACK_MODE_CS:
      // Configured with more than serving cell and PUCCH Format 1b with channel selection
      if (ack_info->nof_cc == 1) {
        ue_dl_gen_ack_fdd_pcell_skip_drx(ack_info, uci_data, nof_tb);
      } else if (ack_info->is_pusch_available) {
        ue_dl_gen_ack_fdd_all_keep_drx(ack_info, uci_data, nof_tb);
      } else if (uci_data->value.scheduling_request) {
        // For FDD with PUCCH format 1b with channel selection, when both HARQ-ACK and SR are transmitted in the same
        // sub-frame a UE shall transmit the HARQ-ACK on its assigned HARQ-ACK PUCCH resource with channel selection as
        // defined in subclause 10.1.2.2.1 for a negative SR transmission and transmit one HARQ-ACK bit per serving cell
        // on its assigned SR PUCCH resource for a positive SR transmission according to the following:
        // − if only one transport block or a PDCCH indicating downlink SPS release is detected on a serving cell, the
        //   HARQ-ACK bit for the serving cell is the HARQ-ACK bit corresponding to the transport block or the PDCCH
        //   indicating downlink SPS release;
        // − if two transport blocks are received on a serving cell, the HARQ-ACK bit for the serving cell is generated
        //   by spatially bundling the HARQ-ACK bits corresponding to the transport blocks;
        // − if neither PDSCH transmission for which HARQ-ACK response shall be provided nor PDCCH indicating
        //   downlink SPS release is detected for a serving cell, the HARQ-ACK bit for the serving cell is set to NACK;
        ue_dl_gen_ack_fdd_all_spatial_bundling(ack_info, uci_data, nof_tb);
      } else if (csi_report) {
        ue_dl_gen_ack_fdd_pcell_skip_drx(ack_info, uci_data, nof_tb);
      } else {
        ue_dl_gen_ack_fdd_all_keep_drx(ack_info, uci_data, nof_tb);
      }
      break;
    case SRSRAN_PUCCH_ACK_NACK_FEEDBACK_MODE_PUCCH3:
      // According to 3GPP 36.213 Section 10.1.2.2.2 PUCCH format 3 HARQ-ACK procedure
      // For FDD with PUCCH format 3, the UE shall use PUCCH resource n_pucch_3 or n_pucch_1 for transmission of
      // HARQ-ACK in subframe n where
      // - for a PDSCH transmission only on the primary cell indicated by the detection of a corresponding PDCCH in
      //   subframe n − 4 , or for a PDCCH indicating downlink SPS release (defined in subclause 9.2) in subframe n − 4
      //   on the primary cell, the UE shall use PUCCH format 1a/1b and PUCCH resource n_pucch_1.
      // - for a PDSCH transmission only on the primary cell where there is not a corresponding PDCCH detected on
      //   subframe n - 4, the UE shall use PUCCH format 1a/1b and PUCCH resource n_pucch_1 where the value of n_pucch_1
      //   is determined according to higher layer configuration and Table 9.2-2.
      // - for a PDSCH transmission on the secondary cell indicated by the detection of a corresponding PDCCH in
      //   subframe n − 4 , the UE shall use PUCCH format 3 and PUCCH resource n_pucch_3  where the value of n PUCCH
      //   is determined according to higher layer configuration and Table 10.1.2.2.2-1.
      if (tb_count == tb_count_cc0) {
        ue_dl_gen_ack_fdd_pcell_skip_drx(ack_info, uci_data, nof_tb);
      } else {
        ue_dl_gen_ack_fdd_all_keep_drx(ack_info, uci_data, nof_tb);
      }
      break;
    case SRSRAN_PUCCH_ACK_NACK_FEEDBACK_MODE_ERROR:
    default:; // Do nothing
      break;
  }

  // n_cce values are just copied
  for (uint32_t i = 0; i < ack_info->nof_cc; i++) {
    uci_data->cfg.ack[i].ncce[0]       = ack_info->cc[i].m[0].resource.n_cce;
    uci_data->cfg.ack[i].grant_cc_idx  = ack_info->cc[i].m[0].resource.grant_cc_idx;
    uci_data->cfg.ack[i].tpc_for_pucch = ack_info->cc[i].m[0].resource.tpc_for_pucch;
  }
}

// Table 7.3-1
static const uint32_t multiple_acknack[10][2] =
    {{0, 0}, {1, 1}, {1, 0}, {0, 1}, {1, 1}, {1, 0}, {0, 1}, {1, 1}, {1, 0}, {0, 1}};

/* UE downlink procedure for reporting HARQ-ACK bits in TDD, Section 7.3 36.213
 */
static void gen_ack_tdd(bool is_tdd_mode16, const srsran_pdsch_ack_t* ack_info, srsran_uci_data_t* uci_data)
{
  uint32_t V_dai_dl = 0;

  uint32_t nof_tb = 1;
  if (ack_info->transmission_mode > SRSRAN_TM2) {
    nof_tb = SRSRAN_MAX_CODEWORDS;
  }

  if (ack_info->nof_cc > 1) {
    fprintf(stderr, "Error generating HARQ-ACK bits. Only 1 CC is supported in TDD\n");
  }

  // Arrange bits for FDD or TDD Bundling or Multiplexing.
  const srsran_pdsch_ack_cc_t* ack_value = &ack_info->cc[0];
  srsran_uci_cfg_ack_t*        ack_cfg   = &uci_data->cfg.ack[0];

  uint32_t min_k = 10;

  if (ack_value->M > 0) {
    ack_cfg->tdd_ack_M = ack_value->M;

    // ACK/NACK bundling or multiplexing and M=1
    if (!ack_info->tdd_ack_multiplex || ack_value->M == 1) {
      for (uint32_t tb = 0; tb < nof_tb; tb++) {
        bool first_in_bundle = true;
        for (uint32_t k = 0; k < ack_value->M; k++) {
          if (ack_value->m[k].present && ack_value->m[k].value[tb] != 2) {
            // Bundle on time domain
            if (first_in_bundle) {
              uci_data->value.ack.ack_value[tb] = ack_value->m[k].value[tb];
              first_in_bundle                   = false;
            } else {
              uci_data->value.ack.ack_value[tb] =
                  (uint8_t)(((uci_data->value.ack.ack_value[tb] == 1) & (ack_value->m[k].value[tb])) ? 1 : 0);
            }
            // V_dai_dl is for the one with lowest k value
            if (ack_value->m[k].k < min_k) {
              min_k              = ack_value->m[k].k;
              V_dai_dl           = ack_value->m[k].resource.v_dai_dl + 1; // Table 7.3-X
              ack_cfg->ncce[0]   = ack_value->m[k].resource.n_cce;
              ack_cfg->tdd_ack_m = k;
            }
          }
        }
      }
      // ACK/NACK multiplexing and M > 1
    } else {
      for (uint32_t k = 0; k < ack_value->M; k++) {
        // Bundle spatial domain
        bool spatial_ack = true;
        for (uint32_t i = 0; i < nof_tb; i++) {
          if (ack_value->m[k].value[i] != 2) {
            spatial_ack &= (ack_value->m[k].value[i] == 1);
          }
        }
        // In multiplexing for pusch, sort them accordingly
        if (ack_value->m[k].present) {
          uint32_t p = k;
          if (ack_info->is_pusch_available && ack_info->is_grant_available) {
            p = ack_value->m[k].resource.v_dai_dl;
          }
          uci_data->value.ack.ack_value[p] = (uint8_t)(spatial_ack ? 1 : 0);
          ack_cfg->ncce[p]                 = ack_value->m[k].resource.n_cce;
        }
      }
    }
  }

  bool missing_ack = false;

  // Calculate U_dai and count number of ACK for this subframe by spatial bundling across codewords
  uint32_t nof_pos_acks   = 0;
  uint32_t U_dai          = 0;
  uint32_t nof_total_acks = 0;
  for (uint32_t i = 0; i < ack_value->M; i++) {
    bool bundle_spatial = false;
    bool first_bundle   = true;
    for (uint32_t j = 0; j < nof_tb; j++) {
      if (ack_value->m[i].present) {
        if (first_bundle) {
          bundle_spatial = ack_value->m[i].value[j] == 1;
          U_dai++;
          first_bundle = false;
        } else {
          bundle_spatial &= ack_value->m[i].value[j] == 1;
        }
        if (bundle_spatial) {
          nof_pos_acks++;
        }
        if (ack_value->m[i].value[j] != 2) {
          nof_total_acks++;
        }
      }
    }
  }

  // For TDD PUSCH
  if (is_tdd_mode16) {
    uint32_t V_dai_ul = ack_info->V_dai_ul + 1; // Table 7.3-x

    ack_cfg->tdd_is_multiplex = ack_info->tdd_ack_multiplex;

    // Bundling or multiplexing and M=1
    if (!ack_info->tdd_ack_multiplex || ack_info->cc[0].M == 1) {
      // 1 or 2 ACK/NACK bits
      ack_cfg->nof_acks = nof_tb;

      // Determine if there is any missing ACK/NACK in the set and N_bundle value

      // Case not transmitting on PUSCH
      if (!ack_info->is_pusch_available) {
        if ((V_dai_dl != (U_dai - 1) % 4 + 1 && U_dai > 0) || U_dai == 0) {
          // In ul procedure 10.2, skip ACK/NACK in bundling PUCCH
          ack_cfg->nof_acks = 0;
          if (U_dai > 0) {
            missing_ack = true;
          }
        }
        // Transmitting on PUSCH and based on detected PDCCH
      } else if (ack_info->is_grant_available) {
        if (V_dai_ul != (U_dai - 1) % 4 + 1) {
          bzero(uci_data->value.ack.ack_value, nof_tb);
          ack_cfg->N_bundle = V_dai_ul + 2;
        } else {
          ack_cfg->N_bundle = V_dai_ul;
        }
        // do not transmit case
        if (V_dai_ul == 4 && U_dai == 0) {
          ack_cfg->nof_acks = 0;
        }
        // Transmitting on PUSCH not based on grant
      } else {
        if (V_dai_dl != (U_dai - 1) % 4 + 1 && U_dai > 0) {
          bzero(uci_data->value.ack.ack_value, nof_tb);
        }
        ack_cfg->N_bundle = U_dai;
        // do not transmit case
        if (U_dai == 0) {
          ack_cfg->nof_acks = 0;
        }
      }

      // In PUSCH and MIMO, nack 2nd codeword if not received, in PUCCH do not transmit
      if (nof_tb == 2 && uci_data->value.ack.ack_value[1] == 2 && ack_cfg->nof_acks == 2) {
        if (!ack_info->is_pusch_available) {
          ack_cfg->nof_acks = 1;
        } else {
          uci_data->value.ack.ack_value[1] = 0;
        }
      }

      // Multiplexing and M>1
    } else {
      if (ack_info->is_pusch_available) {
        if (ack_info->is_grant_available) {
          // Do not transmit if...
          if (!(V_dai_ul == 4 && U_dai == 0)) {
            ack_cfg->nof_acks = V_dai_ul;
          }
        } else {
          ack_cfg->nof_acks = ack_info->cc[0].M;
        }

        // Set DTX bits to NACKs
        uint32_t count_acks = 0;
        for (uint32_t i = 0; i < ack_cfg->nof_acks; i++) {
          if (uci_data->value.ack.ack_value[i] == 2) {
            uci_data->value.ack.ack_value[i] = 0;
          } else {
            count_acks++;
          }
        }
        if (!count_acks) {
          ack_cfg->nof_acks = 0;
        }
      } else {
        ack_cfg->nof_acks = ack_info->cc[0].M;
      }
    }
  } else {
    ack_cfg->N_bundle = 1;
    ack_cfg->nof_acks = nof_total_acks;
  }

  // Multiple ACK/NACK responses with SR and CQI
  if (ack_cfg->nof_acks && !ack_info->is_pusch_available &&
      (uci_data->value.scheduling_request ||
       ((uci_data->cfg.cqi.data_enable || uci_data->cfg.cqi.ri_len) && ack_info->simul_cqi_ack))) {
    if (missing_ack) {
      uci_data->value.ack.ack_value[0] = 0;
      uci_data->value.ack.ack_value[1] = 0;
    } else {
      nof_pos_acks                     = SRSRAN_MIN(9, nof_pos_acks);
      uci_data->value.ack.ack_value[0] = multiple_acknack[nof_pos_acks][0];
      uci_data->value.ack.ack_value[1] = multiple_acknack[nof_pos_acks][1];
    }
    ack_cfg->nof_acks = 2;
  }
}

/* UE downlink procedure for reporting ACK/NACK, Section 7.3 36.213
 */
void srsran_ue_dl_gen_ack(const srsran_cell_t*      cell,
                          const srsran_dl_sf_cfg_t* sf,
                          const srsran_pdsch_ack_t* ack_info,
                          srsran_uci_data_t*        uci_data)
{
  uci_data->value.ack.valid = true; //< Always true for UE transmitter
  if (cell->frame_type == SRSRAN_FDD) {
    gen_ack_fdd(ack_info, uci_data);
  } else {
    bool is_tdd_mode16 = sf->tdd_config.sf_config >= 1 && sf->tdd_config.sf_config <= 6;
    gen_ack_tdd(is_tdd_mode16, ack_info, uci_data);
  }
}

int srsran_ue_dl_find_and_decode(srsran_ue_dl_t*     q,
                                 srsran_dl_sf_cfg_t* sf,
                                 srsran_ue_dl_cfg_t* cfg,
                                 srsran_pdsch_cfg_t* pdsch_cfg,
                                 uint8_t*            data[SRSRAN_MAX_CODEWORDS],
                                 bool                acks[SRSRAN_MAX_CODEWORDS])
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

    ret = srsran_ue_dl_find_dl_dci(q, sf, cfg, pdsch_cfg->rnti, dci_dl);
  }

  if (ret == 1) {
    // Logging
    if (SRSRAN_DEBUG_ENABLED && get_srsran_verbose_level() >= SRSRAN_VERBOSE_INFO) {
      char str[512];
      srsran_dci_dl_info(&dci_dl[0], str, 512);
      INFO("PDCCH: %s, snr=%.1f dB", str, q->chest_res.snr_db);
    }

    // Force known MBSFN grant
    if (sf->sf_type == SRSRAN_SF_MBSFN) {
      dci_dl[0].rnti                    = SRSRAN_MRNTI;
      dci_dl[0].alloc_type              = SRSRAN_RA_ALLOC_TYPE0;
      dci_dl[0].type0_alloc.rbg_bitmask = 0xffffffff;
      dci_dl[0].tb[0].rv                = 0;
      dci_dl[0].tb[0].mcs_idx           = 2;
      dci_dl[0].format                  = SRSRAN_DCI_FORMAT1;
    }

    // Convert DCI message to DL grant
    if (srsran_ue_dl_dci_to_pdsch_grant(q, sf, cfg, &dci_dl[0], &pdsch_cfg->grant)) {
      ERROR("Error unpacking DCI");
      return SRSRAN_ERROR;
    }else{
        printf("FOUND DCI: tti:%d format:%d nof_prb:%d mcs1:%d\n", sf->tti, dci_dl[0].format, pdsch_cfg->grant.nof_prb, pdsch_cfg->grant.tb[0].mcs_idx);
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
  }
  return ret;
}

/* Yaxiong's decode rnti function */
int srsran_ue_decode_dci_yx(srsran_ue_dl_t*     q,
                                 srsran_dl_sf_cfg_t* sf,
                                 srsran_ue_dl_cfg_t* cfg,
                                 srsran_pdsch_cfg_t* pdsch_cfg,
                                 srsran_dci_search_res_t* dci_res)
{
  int ret = SRSRAN_ERROR;

  //srsran_dci_dl_t    dci_dl[SRSRAN_MAX_DCI_MSG] = {};
  //srsran_dci_ul_t    dci_ul[SRSRAN_MAX_DCI_MSG] = {};
  
  //srsran_pusch_grant_t ul_grant;

  srsran_pmch_cfg_t  pmch_cfg;
  //srsran_pdsch_res_t pdsch_res[SRSRAN_MAX_CODEWORDS];

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

    ret = srsran_ue_dl_find_dl_dci(q, sf, cfg, pdsch_cfg->rnti, dci_res->dci_dl);
  }

  if (ret == 1) {
    //printf("FOUND DL DCI: tti:%d rnti:%d format:%d\n", sf->tti, dci_res->dci_dl[0].rnti, dci_res->dci_dl[0].format);
    if (srsran_ue_dl_dci_to_pdsch_grant_wo_mimo_yx(q, sf, cfg, dci_res->dci_dl, &pdsch_cfg->grant)) {
        ERROR("Error unpacking DCI");
        return SRSRAN_ERROR;
    }else{
        dci_res->nof_dl_dci = 1; 
        printf("FOUND DL DCI: tti:%d\tnof_prb:%d\tnof_tb:%d\ttbs1:%d\ttbs2:%d\tmcs1:%d\tmcs2:%d\n", sf->tti, pdsch_cfg->grant.nof_prb, 
                pdsch_cfg->grant.nof_tb, pdsch_cfg->grant.tb[0].tbs, pdsch_cfg->grant.tb[1].tbs, pdsch_cfg->grant.tb[0].mcs_idx, pdsch_cfg->grant.tb[1].mcs_idx);
    }
  }
    int nof_ul_dci = srsran_ue_dl_find_ul_dci(q, sf, cfg, pdsch_cfg->rnti, dci_res->dci_ul);
    if(nof_ul_dci > 0){
         
        dci_res->nof_ul_dci = 1; 
//        //srsran_ue_ul_dci_to_pusch_grant
//        // Convert DCI to Grant
//
//        // set the ul sf 
//        srsran_ul_sf_cfg_t ul_sf;
//        ZERO_OBJECT(ul_sf);
//        ul_sf.tdd_config = sf->tdd_config;
//        ul_sf.tti        = sf->tti;
//
//        // set the hopping config
//        srsran_pusch_hopping_cfg_t ul_hopping = {.n_sb = 1, .hopping_offset = 0, .hop_mode = 1};
//
//        if (srsran_ra_ul_dci_to_grant(&q->cell, &ul_sf, &ul_hopping, dci_res->dci_ul, &ul_grant)) {
//            return SRSRAN_ERROR; 
//        } 
//
//        printf("FOUND UL DCI: tti:%d hop:%d dmrs:%d\n", sf->tti, dci_res->dci_ul[0].freq_hop_fl, dci_res->dci_ul[0].n_dmrs);
    }

  return ret;
}



void srsran_ue_dl_save_signal(srsran_ue_dl_t* q, srsran_dl_sf_cfg_t* sf, srsran_pdsch_cfg_t* pdsch_cfg)
{
  uint32_t cfi = sf->cfi;
  uint32_t tti = sf->tti % 10;

  srsran_vec_save_file("sf_symbols", q->sf_symbols, SRSRAN_NOF_RE(q->cell) * sizeof(cf_t));
  printf("%d samples\n", SRSRAN_NOF_RE(q->cell));
  srsran_vec_save_file("ce0", q->chest_res.ce[0], SRSRAN_NOF_RE(q->cell) * sizeof(cf_t));
  if (q->cell.nof_ports > 1) {
    srsran_vec_save_file("ce1", q->chest_res.ce[1], SRSRAN_NOF_RE(q->cell) * sizeof(cf_t));
  }
  srsran_vec_save_file("pcfich_ce0", q->pcfich.ce[0], q->pcfich.nof_symbols * sizeof(cf_t));
  srsran_vec_save_file("pcfich_ce1", q->pcfich.ce[1], q->pcfich.nof_symbols * sizeof(cf_t));
  srsran_vec_save_file("pcfich_symbols", q->pcfich.symbols[0], q->pcfich.nof_symbols * sizeof(cf_t));
  srsran_vec_save_file("pcfich_eq_symbols", q->pcfich.d, q->pcfich.nof_symbols * sizeof(cf_t));
  srsran_vec_save_file("pcfich_llr", q->pcfich.data_f, PCFICH_CFI_LEN * sizeof(float));

  srsran_vec_save_file("pdcch_ce0", q->pdcch.ce[0], q->pdcch.nof_cce[cfi - 1] * 36 * sizeof(cf_t));
  srsran_vec_save_file("pdcch_ce1", q->pdcch.ce[1], q->pdcch.nof_cce[cfi - 1] * 36 * sizeof(cf_t));
  srsran_vec_save_file("pdcch_symbols", q->pdcch.symbols[0], q->pdcch.nof_cce[cfi - 1] * 36 * sizeof(cf_t));
  srsran_vec_save_file("pdcch_eq_symbols", q->pdcch.d, q->pdcch.nof_cce[cfi - 1] * 36 * sizeof(cf_t));
  srsran_vec_save_file("pdcch_llr", q->pdcch.llr, q->pdcch.nof_cce[cfi - 1] * 72 * sizeof(float));

  srsran_vec_save_file("pdsch_symbols", q->pdsch.d[0], pdsch_cfg->grant.nof_re * sizeof(cf_t));
  srsran_vec_save_file("llr", q->pdsch.e[0], pdsch_cfg->grant.tb[0].nof_bits * sizeof(cf_t));
  printf("Saved files for tti=%d, sf=%d, cfi=%d, tbs=%d, rv=%d\n",
         tti,
         tti % 10,
         cfi,
         pdsch_cfg->grant.tb[0].tbs,
         pdsch_cfg->grant.tb[0].rv);
}
