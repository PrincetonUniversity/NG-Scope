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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include "srsran/srsran.h"

static uint32_t nof_tx_bits = 0;
static uint32_t nof_rx_bits = 0;

void usage(char* prog)
{
  printf("Usage: %s -t nof_tx_bits -r nof_rx_bits\n", prog);
}

void parse_args(int argc, char** argv)
{
  int opt;
  while ((opt = getopt(argc, argv, "tr")) != -1) {
    switch (opt) {
      case 't':
        nof_tx_bits = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'r':
        nof_rx_bits = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      default:
        usage(argv[0]);
        exit(-1);
    }
  }
  if (nof_tx_bits == 0) {
    usage(argv[0]);
    exit(-1);
  }
  if (nof_rx_bits == 0) {
    usage(argv[0]);
    exit(-1);
  }
}

int main(int argc, char** argv)
{
  int      i;
  uint8_t *bits, *rm_bits;
  float *  rm_symbols, *unrm_symbols;
  int      nof_errors;

  parse_args(argc, argv);

  bits = srsran_vec_u8_malloc(nof_tx_bits);
  if (!bits) {
    perror("malloc");
    exit(-1);
  }
  rm_bits = srsran_vec_u8_malloc(nof_rx_bits);
  if (!rm_bits) {
    perror("malloc");
    exit(-1);
  }
  rm_symbols = srsran_vec_f_malloc(nof_rx_bits);
  if (!rm_symbols) {
    perror("malloc");
    exit(-1);
  }
  unrm_symbols = srsran_vec_f_malloc(nof_tx_bits);
  if (!unrm_symbols) {
    perror("malloc");
    exit(-1);
  }

  for (i = 0; i < nof_tx_bits; i++) {
    bits[i] = rand() % 2;
  }

  if (srsran_rm_conv_tx(bits, nof_tx_bits, rm_bits, nof_rx_bits)) {
    exit(-1);
  }

  for (i = 0; i < nof_rx_bits; i++) {
    rm_symbols[i] = rm_bits[i] ? 1 : -1;
  }

  if (srsran_rm_conv_rx(rm_symbols, nof_rx_bits, unrm_symbols, nof_tx_bits)) {
    exit(-1);
  }

  nof_errors = 0;
  for (i = 0; i < nof_tx_bits; i++) {
    if ((unrm_symbols[i] > 0) != bits[i]) {
      nof_errors++;
    }
  }
  if (nof_rx_bits > nof_tx_bits) {
    if (nof_errors) {
      printf("nof_errors=%d\n", nof_errors);
      exit(-1);
    }
  }

  free(bits);
  free(rm_bits);
  free(rm_symbols);
  free(unrm_symbols);

  printf("Ok\n");
  exit(0);
}
