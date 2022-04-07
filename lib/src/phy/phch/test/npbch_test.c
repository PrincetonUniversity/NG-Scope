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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include "srsran/phy/phch/npbch.h"
#include "srsran/phy/utils/debug.h"
#include "srsran/phy/utils/vector.h"

#define HAVE_OFDM 0
#define HAVE_MIB_NB 1
srsran_nbiot_cell_t cell = {};

void usage(char* prog)
{
  printf("Usage: %s [cpv]\n", prog);
  printf("\t-c cell id [Default %d]\n", cell.base.id);
  printf("\t-p cell.nof_ports [Default %d]\n", cell.base.nof_ports);
  printf("\t-n cell.nof_prb [Default %d]\n", cell.base.nof_prb);
  printf("\t-v [set srsran_verbose to debug, default none]\n");
}

void parse_args(int argc, char** argv)
{
  int opt;
  while ((opt = getopt(argc, argv, "cpnv")) != -1) {
    switch (opt) {
      case 'p':
        cell.base.nof_ports = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'n':
        cell.base.nof_prb = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'c':
        cell.base.id = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'v':
        increase_srsran_verbose_level();
        break;
      default:
        usage(argv[0]);
        exit(-1);
    }
  }
}

int re_extract_test()
{
  int ret = SRSRAN_ERROR;

  srsran_nbiot_cell_t cell2 = {};
  cell2.base.id             = 257;
  cell2.base.nof_ports      = 1;
  cell2.n_id_ncell          = 257;
  cell2.nof_ports           = 1;
  cell2.base.nof_prb        = 1;

  if (!srsran_nbiot_cell_isvalid(&cell2)) {
    printf("cell is not properly configured\n");
    return ret;
  }

  cf_t sf_syms[168];
  for (int i = 0; i < 168; i++) {
    sf_syms[i] = i + 1;
  }

  cf_t npbch_syms[100];
  int  nof_ext_syms = srsran_npbch_cp(sf_syms, npbch_syms, cell2, false);

  if (SRSRAN_VERBOSE_ISINFO()) {
    for (int i = 0; i < nof_ext_syms; i++) {
      printf("re%d:", i);
      srsran_vec_fprint_c(stdout, &npbch_syms[i], 1);
    }
  }

  if (nof_ext_syms == 100) {
    ret = SRSRAN_SUCCESS;
    printf("RE extraction ok!\n");
  }
  return ret;
}

int main(int argc, char** argv)
{
  int            ret = SRSRAN_ERROR;
  srsran_npbch_t npbch;
  uint8_t        bch_payload_tx[SRSRAN_MIB_NB_LEN], bch_payload_rx[SRSRAN_MIB_NB_LEN];
  cf_t*          ce[SRSRAN_MAX_PORTS];
  int            nof_re;
  cf_t*          sf_symbols[SRSRAN_MAX_PORTS];
  uint32_t       nof_rx_ports;

  parse_args(argc, argv);

  if (re_extract_test() != SRSRAN_SUCCESS) {
    return ret;
  }

  // set essential cell params
  cell.base.nof_prb = 1;
  cell.nof_ports    = 1;

  nof_re = SRSRAN_SF_LEN_RE(cell.base.nof_prb, SRSRAN_CP_NORM);

  /* init memory */
  for (int i = 0; i < cell.nof_ports; i++) {
    ce[i] = srsran_vec_cf_malloc(nof_re);
    if (!ce[i]) {
      perror("malloc");
      exit(-1);
    }
    for (int j = 0; j < nof_re; j++) {
      ce[i][j] = 1;
    }
    sf_symbols[i] = srsran_vec_cf_malloc(nof_re);
    if (!sf_symbols[i]) {
      perror("malloc");
      exit(-1);
    }
    memset(sf_symbols[i], 0, sizeof(cf_t) * nof_re);
  }

  if (srsran_npbch_init(&npbch)) {
    fprintf(stderr, "Error creating NPBCH object\n");
    exit(-1);
  }
  if (srsran_npbch_set_cell(&npbch, cell)) {
    fprintf(stderr, "Error setting cell in NPBCH object\n");
    exit(-1);
  }

#if HAVE_MIB_NB
  srsran_mib_nb_t mib_nb = {};
  mib_nb.sched_info_sib1 = 3; // according to Table 16.4.1.3-3 in 36.213 that means 4 NPDSCH repetitions
  mib_nb.sys_info_tag    = 1;
  mib_nb.ac_barring      = false;
  mib_nb.mode            = SRSRAN_NBIOT_MODE_STANDALONE;
  uint32_t hfn           = 82;
  uint32_t sfn           = 0;
  srsran_npbch_mib_pack(hfn, sfn, mib_nb, bch_payload_tx);
#else
  // srand(0);
  srand(time(NULL));
  for (i = 0; i < SRSRAN_MIB_NB_LEN; i++) {
    bch_payload_tx[i] = rand() % 2;
  }
#endif

  if (srsran_npbch_put_subframe(&npbch, bch_payload_tx, sf_symbols, 0)) {
    fprintf(stderr, "Error encoding NPBCH\n");
    exit(-1);
  }
#if 0
  /* combine outputs */
  for (i=1;i<cell.base.nof_ports;i++) {
    for (j=0;j<nof_re;j++) {
      sf_symbols[0][j] += sf_symbols[i][j];
    }
  }
#endif

#if HAVE_OFDM
  cf_t          td_signal[nof_re * 2];
  srsran_ofdm_t ofdm_tx;
  srsran_ofdm_t ofdm_rx;

  if (srsran_ofdm_tx_init(&ofdm_tx, SRSRAN_CP_NORM, cell.base.nof_prb)) {
    fprintf(stderr, "Error creating iFFT object\n");
    exit(-1);
  }
  // srsran_ofdm_set_normalize(&ofdm_tx, true);

  if (srsran_ofdm_rx_init(&ofdm_rx, SRSRAN_CP_NORM, cell.base.nof_prb)) {
    fprintf(stderr, "Error initializing FFT\n");
    return -1;
  }

  // transfer into time doamin and back
  srsran_ofdm_tx_sf(&ofdm_tx, sf_symbols[0], td_signal);
  srsran_ofdm_rx_sf(&ofdm_rx, td_signal, sf_symbols[0]);

  srsran_ofdm_tx_free(&ofdm_tx);
  srsran_ofdm_rx_free(&ofdm_rx);
#endif

  srsran_npbch_decode_reset(&npbch);
  if (srsran_npbch_decode(&npbch, sf_symbols[0], ce, 0, bch_payload_rx, &nof_rx_ports, NULL)) {
    printf("Error decoding\n");
    exit(-1);
  }

  srsran_npbch_free(&npbch);

  for (int i = 0; i < cell.nof_ports; i++) {
    free(ce[i]);
    free(sf_symbols[i]);
  }
  printf("Tx ports: %d - Rx ports: %d\n", cell.nof_ports, nof_rx_ports);
  printf("Tx payload: ");
  srsran_vec_fprint_hex(stdout, bch_payload_tx, SRSRAN_MIB_NB_LEN);
  printf("Rx payload: ");
  srsran_vec_fprint_hex(stdout, bch_payload_rx, SRSRAN_MIB_NB_LEN);

#if HAVE_MIB_NB
  srsran_mib_nb_t mib_nb_rx;
  srsran_npbch_mib_unpack(bch_payload_rx, &mib_nb_rx);
  srsran_mib_nb_printf(stdout, cell, &mib_nb_rx);
#endif

  if (nof_rx_ports == cell.nof_ports && !memcmp(bch_payload_rx, bch_payload_tx, sizeof(uint8_t) * SRSRAN_MIB_NB_LEN)) {
    printf("OK\n");
    exit(0);
  } else {
    printf("Error\n");
    exit(-1);
  }
}
