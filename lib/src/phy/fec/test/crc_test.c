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

#include "crc_test.h"
#include "srsran/srsran.h"

int      num_bits = 5001, crc_length = 24;
uint32_t crc_poly = 0x1864CFB;
uint32_t seed     = 1;

void usage(char* prog)
{
  printf("Usage: %s [nlps]\n", prog);
  printf("\t-n num_bits [Default %d]\n", num_bits);
  printf("\t-l crc_length [Default %d]\n", crc_length);
  printf("\t-p crc_poly (Hex) [Default 0x%x]\n", crc_poly);
  printf("\t-s seed [Default 0=time]\n");
  printf("\t-v [set srsran_verbose to debug, default none]\n");
}

void parse_args(int argc, char** argv)
{
  int opt;
  while ((opt = getopt(argc, argv, "nlpsv")) != -1) {
    switch (opt) {
      case 'n':
        num_bits = (int)strtol(argv[optind], NULL, 10);
        break;
      case 'l':
        crc_length = (int)strtol(argv[optind], NULL, 10);
        break;
      case 'p':
        crc_poly = (uint32_t)strtoul(argv[optind], NULL, 16);
        break;
      case 's':
        seed = (uint32_t)strtoul(argv[optind], NULL, 0);
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
  int          i;
  uint8_t*     data;
  uint32_t     crc_word, expected_word;
  srsran_crc_t crc_p;

  parse_args(argc, argv);

  data = srsran_vec_u8_malloc(num_bits + crc_length * 2);
  if (!data) {
    perror("malloc");
    exit(-1);
  }

  if (!seed) {
    seed = time(NULL);
  }
  srand(seed);

  // Generate data
  for (i = 0; i < num_bits; i++) {
    data[i] = rand() % 2;
  }

  if (SRSRAN_DEBUG_ENABLED && get_srsran_verbose_level() >= SRSRAN_VERBOSE_INFO && !is_handler_registered()) {
    INFO("data=");
    srsran_vec_fprint_b(stdout, data, num_bits);
  }

  // Initialize CRC params and tables
  if (srsran_crc_init(&crc_p, crc_poly, crc_length)) {
    exit(-1);
  }

  // generate CRC word
  crc_word = srsran_crc_checksum(&crc_p, data, num_bits);

  INFO("checksum=%x", crc_word);

  free(data);

  // check if generated word is as expected
  if (get_expected_word(num_bits, crc_length, crc_poly, seed, &expected_word)) {
    ERROR("Test parameters not defined in test_results.h");
    exit(-1);
  }
  exit(expected_word != crc_word);
}
