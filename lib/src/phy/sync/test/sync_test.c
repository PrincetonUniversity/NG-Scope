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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include <stdbool.h>

#include "srsran/srsran.h"

int         cell_id = -1, offset = 0;
srsran_cp_t cp      = SRSRAN_CP_NORM;
uint32_t    nof_prb = 6;

#define FLEN SRSRAN_SF_LEN(fft_size)

void usage(char* prog)
{
  printf("Usage: %s [cpoev]\n", prog);
  printf("\t-c cell_id [Default check for all]\n");
  printf("\t-p nof_prb [Default %d]\n", nof_prb);
  printf("\t-o offset [Default %d]\n", offset);
  printf("\t-e extended CP [Default normal]\n");
  printf("\t-v srsran_verbose\n");
}

void parse_args(int argc, char** argv)
{
  int opt;
  while ((opt = getopt(argc, argv, "cpoev")) != -1) {
    switch (opt) {
      case 'c':
        cell_id = (int)strtol(argv[optind], NULL, 10);
        break;
      case 'p':
        nof_prb = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'o':
        offset = (int)strtol(argv[optind], NULL, 10);
        break;
      case 'e':
        cp = SRSRAN_CP_EXT;
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

int main(int argc, char** argv)
{
  int           N_id_2, sf_idx, find_sf;
  cf_t *        buffer, *fft_buffer;
  cf_t          pss_signal[SRSRAN_PSS_LEN];
  float         sss_signal0[SRSRAN_SSS_LEN]; // for subframe 0
  float         sss_signal5[SRSRAN_SSS_LEN]; // for subframe 5
  int           cid, max_cid;
  uint32_t      find_idx;
  srsran_sync_t syncobj;
  srsran_ofdm_t ifft;
  int           fft_size;

  parse_args(argc, argv);

  fft_size = srsran_symbol_sz(nof_prb);
  if (fft_size < 0) {
    ERROR("Invalid nof_prb=%d", nof_prb);
    exit(-1);
  }

  buffer = srsran_vec_cf_malloc(FLEN);
  if (!buffer) {
    perror("malloc");
    exit(-1);
  }

  fft_buffer = srsran_vec_cf_malloc(FLEN * 2);
  if (!fft_buffer) {
    perror("malloc");
    exit(-1);
  }

  if (srsran_ofdm_tx_init(&ifft, cp, buffer, fft_buffer, nof_prb)) {
    ERROR("Error creating iFFT object");
    exit(-1);
  }

  if (srsran_sync_init(&syncobj, FLEN, FLEN, fft_size)) {
    ERROR("Error initiating PSS/SSS");
    return -1;
  }

  srsran_sync_set_cp(&syncobj, cp);

  /* Set a very high threshold to make sure the correlation is ok */
  srsran_sync_set_threshold(&syncobj, 5.0);
  srsran_sync_set_sss_algorithm(&syncobj, SSS_PARTIAL_3);

  if (cell_id == -1) {
    cid     = 0;
    max_cid = 49;
  } else {
    cid     = cell_id;
    max_cid = cell_id;
  }
  while (cid <= max_cid) {
    N_id_2 = cid % 3;

    /* Generate PSS/SSS signals */
    srsran_pss_generate(pss_signal, N_id_2);
    srsran_sss_generate(sss_signal0, sss_signal5, cid);

    srsran_sync_set_N_id_2(&syncobj, N_id_2);

    // SF1 is SF5
    for (sf_idx = 0; sf_idx < 2; sf_idx++) {
      memset(buffer, 0, sizeof(cf_t) * FLEN);
      srsran_pss_put_slot(pss_signal, buffer, nof_prb, cp);
      srsran_sss_put_slot(sf_idx ? sss_signal5 : sss_signal0, buffer, nof_prb, cp);

      /* Transform to OFDM symbols */
      memset(fft_buffer, 0, sizeof(cf_t) * FLEN);
      srsran_ofdm_tx_sf(&ifft);

      /* Apply sample offset */
      for (int i = 0; i < FLEN; i++) {
        fft_buffer[FLEN - i - 1 + offset] = fft_buffer[FLEN - i - 1];
      }
      srsran_vec_cf_zero(fft_buffer, offset);

      if (srsran_sync_find(&syncobj, fft_buffer, 0, &find_idx) < 0) {
        ERROR("Error running srsran_sync_find");
        exit(-1);
      }
      find_sf = srsran_sync_get_sf_idx(&syncobj);
      printf("cell_id: %d find: %d, offset: %d, ns=%d find_ns=%d\n", cid, find_idx, offset, sf_idx, find_sf);
      if (find_idx != offset + FLEN / 2) {
        printf("offset != find_offset: %d != %d\n", find_idx, offset + FLEN / 2);
        exit(-1);
      }
      if (sf_idx * 5 != find_sf) {
        printf("ns != find_ns\n");
        exit(-1);
      }
      if (srsran_sync_get_cp(&syncobj) != cp) {
        printf("Detected CP should be %s\n", SRSRAN_CP_ISNORM(cp) ? "Normal" : "Extended");
        exit(-1);
      }
    }
    cid++;
  }

  free(fft_buffer);
  free(buffer);

  srsran_sync_free(&syncobj);
  srsran_ofdm_tx_free(&ifft);

  printf("Ok\n");
  exit(0);
}
