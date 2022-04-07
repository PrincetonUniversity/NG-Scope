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

#include <complex.h>
#include <math.h>
#include <srsran/phy/ch_estimation/refsignal_dl.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "srsran/phy/ch_estimation/refsignal_dl_nbiot.h"
#include "srsran/phy/common/phy_common.h"
#include "srsran/phy/common/sequence.h"
#include "srsran/phy/utils/debug.h"
#include "srsran/phy/utils/vector.h"

#define EXTRA_DEBUG 0

uint32_t srsran_nbiot_refsignal_dl_v(uint32_t port_id, uint32_t ref_symbol_idx)
{
  uint32_t v = 0;
  switch (port_id) {
    case 0:
      if (!(ref_symbol_idx % 2)) {
        v = 0;
      } else {
        v = 3;
      }
      break;
    case 1:
      if (!(ref_symbol_idx % 2)) {
        v = 3;
      } else {
        v = 0;
      }
      break;
  }
  return v;
}

uint32_t srsran_refsignal_dl_nbiot_nof_symbols(uint32_t port_id)
{
  if (port_id < 2) {
    return 4;
  } else {
    return 2;
  }
}

uint32_t srsran_refsignal_dl_nbiot_fidx(srsran_nbiot_cell_t cell, uint32_t l, uint32_t port_id, uint32_t m)
{
  return (6 * m + ((srsran_nbiot_refsignal_dl_v(port_id, l) + (cell.n_id_ncell % 6)) % 6));
}

inline uint32_t srsran_nbiot_refsignal_dl_nsymbol(uint32_t l, srsran_cp_t cp, uint32_t port_id)
{
  if (port_id < 2) {
    if (l % 2) {
      return (l / 2 + 1) * SRSRAN_CP_NSYMB(cp) - 3;
    } else {
      return (l / 2) * SRSRAN_CP_NSYMB(cp);
    }
  } else {
    return 1 + l * SRSRAN_CP_NSYMB(cp);
  }
}

inline uint32_t srsran_refsignal_nbiot_cs_nof_re(srsran_nbiot_cell_t* cell, uint32_t port_id)
{
  return srsran_refsignal_cs_nof_symbols(NULL, NULL, port_id) * cell->base.nof_prb * 2; // 2 RE per PRB
}

/** Allocates and precomputes the Narrowband Reference Signal (NRS) signal for
 * the 20 slots in a subframe
 */
int srsran_refsignal_dl_nbiot_init(srsran_refsignal_dl_nbiot_t* q)
{
  int ret = SRSRAN_ERROR_INVALID_INPUTS;

  if (q != NULL) {
    ret = SRSRAN_ERROR;

    bzero(q, sizeof(srsran_refsignal_dl_nbiot_t));

    for (uint32_t p = 0; p < 2; p++) {
      for (uint32_t i = 0; i < SRSRAN_NOF_SF_X_FRAME; i++) {
        q->pilots[p][i] = srsran_vec_cf_malloc(SRSRAN_NBIOT_REFSIGNAL_NUM_SF(SRSRAN_NBIOT_MAX_PRB, p));
        if (!q->pilots[p][i]) {
          perror("malloc");
          goto free_and_exit;
        }
      }
    }

    ret = SRSRAN_SUCCESS;
  }
free_and_exit:
  if (ret == SRSRAN_ERROR) {
    srsran_refsignal_dl_nbiot_free(q);
  }
  return ret;
}

int srsran_refsignal_dl_nbiot_set_cell(srsran_refsignal_dl_nbiot_t* q, srsran_nbiot_cell_t cell)
{
  int ret = SRSRAN_ERROR_INVALID_INPUTS;
  INFO("Generating NRS for n_id_ncell=%d", cell.n_id_ncell);

  if (q != NULL && srsran_nbiot_cell_isvalid(&cell)) {
    ret = SRSRAN_ERROR;

    q->cell = cell;

    srsran_sequence_t seq;
    bzero(&seq, sizeof(srsran_sequence_t));
    if (srsran_sequence_init(&seq, SRSRAN_NOF_SLOTS_PER_SF * SRSRAN_NBIOT_MAX_PORTS * SRSRAN_MAX_PRB)) {
      goto free_and_exit;
    }

    for (uint32_t ns = 0; ns < SRSRAN_NSLOTS_X_FRAME; ns++) {
      for (uint32_t p = 0; p < 2; p++) {
        uint32_t nsymbols = srsran_refsignal_dl_nbiot_nof_symbols(p) / 2;
        for (uint32_t l = 0; l < nsymbols; l++) {
          /* Compute sequence init value */
          uint32_t lp     = srsran_refsignal_nrs_nsymbol(l);
          uint32_t N_cp   = 1;
          uint32_t c_init = 1024 * (7 * (ns + 1) + lp + 1) * (2 * cell.n_id_ncell + 1) + 2 * cell.n_id_ncell + N_cp;

          /* generate sequence for this symbol and slot */
          srsran_sequence_set_LTE_pr(&seq, SRSRAN_NOF_SLOTS_PER_SF + SRSRAN_NBIOT_MAX_PORTS * SRSRAN_MAX_PRB, c_init);

          /* Compute signal */
          for (uint32_t i = 0; i < 2; i++) {
            uint32_t mp = i + SRSRAN_MAX_PRB - 1;
            /* save signal */
            int idx =
                SRSRAN_NBIOT_REFSIGNAL_PILOT_IDX(i, (ns % 2) * nsymbols + l, SRSRAN_NBIOT_DEFAULT_NUM_PRB_BASECELL);
            DEBUG("Ref port=%d, lp=%d, ns=%d, ns/2=%d, idx=%d, i=%d, nsymbols=%d, l=%d",
                  p,
                  lp,
                  ns,
                  ns / 2,
                  idx,
                  i,
                  nsymbols,
                  l);
            __real__ q->pilots[p][ns / 2][idx] = (1 - 2 * (float)seq.c[2 * mp + 0]) * M_SQRT1_2;
            __imag__ q->pilots[p][ns / 2][idx] = (1 - 2 * (float)seq.c[2 * mp + 1]) * M_SQRT1_2;
          }
        }
      }
    }
    srsran_sequence_free(&seq);
    ret = SRSRAN_SUCCESS;
  }
free_and_exit:
  if (ret == SRSRAN_ERROR) {
    srsran_refsignal_dl_nbiot_free(q);
  }
  return ret;
}

// Deallocates a srsran_refsignal_cs_t object allocated with srsran_refsignal_cs_init
void srsran_refsignal_dl_nbiot_free(srsran_refsignal_dl_nbiot_t* q)
{
  for (int p = 0; p < 2; p++) {
    for (int i = 0; i < SRSRAN_NOF_SF_X_FRAME; i++) {
      if (q->pilots[p][i]) {
        free(q->pilots[p][i]);
      }
    }
  }
  bzero(q, sizeof(srsran_refsignal_dl_nbiot_t));
}

/** Regardless of the number of antenna ports, NRS are always transmitted
 * in the last two symbols of each slot
 */
inline uint32_t srsran_refsignal_nrs_nsymbol(uint32_t l)
{
  if (l % 2 == 0) {
    return ((l / 2) * SRSRAN_CP_NORM_NSYMB + 5);
  } else {
    return ((l / 2) * SRSRAN_CP_NORM_NSYMB + 6);
  }
}

/** Maps the NRS reference signal initialized with srsran_refsignal_cs_init() into an array of subframe symbols
    Note that the NRS signal is identical to the CRS, but is initalized with NCellID
 */
int srsran_refsignal_nrs_put_sf(srsran_nbiot_cell_t cell, uint32_t port_id, cf_t* pilots, cf_t* sf_symbols)
{
  if (srsran_nbiot_cell_isvalid(&cell) && srsran_portid_isvalid(port_id) && pilots != NULL && sf_symbols != NULL) {
    for (int l = 0; l < srsran_refsignal_dl_nbiot_nof_symbols(port_id); l++) {
      uint32_t nsymbol = srsran_refsignal_nrs_nsymbol(l);
      // Two reference symbols per OFDM symbol
      for (int m = 0; m < SRSRAN_NBIOT_NUM_NRS_X_SYM_X_PORT; m++) {
        uint32_t fidx = srsran_refsignal_dl_nbiot_fidx(cell, l, port_id, m) + cell.nbiot_prb * SRSRAN_NRE;
        sf_symbols[SRSRAN_RE_IDX(cell.base.nof_prb, nsymbol, fidx)] = pilots[SRSRAN_NBIOT_REFSIGNAL_PILOT_IDX(m, l, 1)];
#if EXTRA_DEBUG
        DEBUG("port: %d, re_idx: %d, pilot_idx: %d, %+2.2f%+2.2fi",
              port_id,
              SRSRAN_RE_IDX(cell.base.nof_prb, nsymbol, fidx),
              SRSRAN_NBIOT_REFSIGNAL_PILOT_IDX(i, l, 1),
              __real__ sf_symbols[SRSRAN_RE_IDX(cell.base.nof_prb, nsymbol, fidx)],
              __imag__ sf_symbols[SRSRAN_RE_IDX(cell.base.nof_prb, nsymbol, fidx)]);
#endif
      }
    }

#if EXTRA_DEBUG
    if (SRSRAN_VERBOSE_ISDEBUG()) {
      DEBUG("SAVED FILE chest_nbiot_tx_after_mapping.bin: NRS after mapping", 0);
      srsran_vec_save_file(
          "chest_nbiot_tx_after_mapping.bin", pilots, SRSRAN_REFSIGNAL_NUM_SF(1, port_id) * sizeof(cf_t));
    }
#endif
    return SRSRAN_SUCCESS;
  } else {
    return SRSRAN_ERROR_INVALID_INPUTS;
  }
}

/** Copies the RE containing references from an array of subframe symbols to the pilots array. */
int srsran_refsignal_nrs_get_sf(srsran_nbiot_cell_t cell, uint32_t port_id, cf_t* sf_symbols, cf_t* pilots)
{
  if (srsran_nbiot_cell_isvalid(&cell) && srsran_portid_isvalid(port_id) && pilots != NULL && sf_symbols != NULL) {
    for (int l = 0; l < srsran_refsignal_dl_nbiot_nof_symbols(port_id); l++) {
      uint32_t nsymbol = srsran_refsignal_nrs_nsymbol(l);
      // read both pilots of this symbol
      for (int m = 0; m < SRSRAN_NBIOT_NUM_NRS_X_SYM_X_PORT; m++) {
        uint32_t fidx = srsran_refsignal_dl_nbiot_fidx(cell, l, port_id, m) + cell.nbiot_prb * SRSRAN_NRE;
        pilots[SRSRAN_NBIOT_REFSIGNAL_PILOT_IDX(m, l, 1)] = sf_symbols[SRSRAN_RE_IDX(cell.base.nof_prb, nsymbol, fidx)];
#if EXTRA_DEBUG
        DEBUG("port: %d, pilot_idx: %d, re_idx: %d, %+2.2f%+2.2fi",
              port_id,
              SRSRAN_NBIOT_REFSIGNAL_PILOT_IDX(m, l, 1),
              SRSRAN_RE_IDX(cell.base.nof_prb, nsymbol, fidx),
              __real__ pilots[SRSRAN_NBIOT_REFSIGNAL_PILOT_IDX(m, l, 1)],
              __imag__ pilots[SRSRAN_NBIOT_REFSIGNAL_PILOT_IDX(m, l, 1)]);
#endif
      }
    }

#if EXTRA_DEBUG
    if (SRSRAN_VERBOSE_ISDEBUG()) {
      DEBUG("SAVED FILE chest_nbiot_rx_after_demapping.bin: NRS after demapping", 0);
      srsran_vec_save_file(
          "chest_nbiot_rx_after_demapping.bin", pilots, SRSRAN_REFSIGNAL_NUM_SF(1, port_id) * sizeof(cf_t));
    }
#endif
    return SRSRAN_SUCCESS;
  } else {
    return SRSRAN_ERROR_INVALID_INPUTS;
  }
}
