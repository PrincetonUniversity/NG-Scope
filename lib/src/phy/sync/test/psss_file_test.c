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

#include "srsran/phy/sync/psss.h"
#include "srsran/phy/sync/ssss.h"
#include "srsran/srsran.h"

char*          input_file_name;
float          frequency_offset       = 0.0;
float          snr                    = 100.0;
srsran_cp_t    cp                     = SRSRAN_CP_NORM;
uint32_t       nof_prb                = 6;
bool           use_standard_lte_rates = false;
srsran_sl_tm_t tm                     = SRSRAN_SIDELINK_TM2;
uint32_t       max_subframes          = 10;

srsran_filesource_t fsrc;

void usage(char* prog)
{
  printf("Usage: %s [cdefiopstv]\n", prog);
  printf("\t-i input_file_name\n");
  printf("\t-p nof_prb [Default %d]\n", nof_prb);
  printf("\t-e extended CP [Default normal]\n");
  printf("\t-m max_subframes [Default %d]\n", max_subframes);
  printf("\t-t Sidelink transmission mode {1,2,3,4} [Default %d]\n", (tm + 1));
  printf("\t-d use_standard_lte_rates [Default %i]\n", use_standard_lte_rates);
  printf("\t-v srsran_verbose\n");
}

void parse_args(int argc, char** argv)
{
  int opt;
  while ((opt = getopt(argc, argv, "cdefimpstv")) != -1) {
    switch (opt) {
      case 'd':
        use_standard_lte_rates = true;
        break;
      case 'i':
        input_file_name = argv[optind];
        break;
      case 'm':
        max_subframes = strtoul(argv[optind], NULL, 0);
        break;
      case 'p':
        nof_prb = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 's':
        snr = strtof(argv[optind], NULL);
        break;
      case 't':
        switch (strtol(argv[optind], NULL, 10)) {
          case 1:
            tm = SRSRAN_SIDELINK_TM1;
            break;
          case 2:
            tm = SRSRAN_SIDELINK_TM2;
            break;
          case 3:
            tm = SRSRAN_SIDELINK_TM3;
            break;
          case 4:
            tm = SRSRAN_SIDELINK_TM4;
            break;
          default:
            usage(argv[0]);
            exit(-1);
        }
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
  int ret = SRSRAN_ERROR;
  parse_args(argc, argv);
  srsran_use_standard_symbol_size(use_standard_lte_rates);

  // Init buffers
  cf_t* input_buffer      = NULL;
  cf_t* input_buffer_temp = NULL;
  cf_t* sf_buffer         = NULL;

  // Init PSSS
  srsran_psss_t psss = {};
  if (srsran_psss_init(&psss, nof_prb, cp) < SRSRAN_SUCCESS) {
    ERROR("Error initialising the PSSS");
    goto clean_exit;
  }

  if (!input_file_name || srsran_filesource_init(&fsrc, input_file_name, SRSRAN_COMPLEX_FLOAT_BIN)) {
    printf("Error opening file %s\n", input_file_name);
    goto clean_exit;
  }

  // Allocate memory
  uint32_t sf_n_samples = SRSRAN_SF_LEN_PRB(nof_prb);
  printf("I/Q samples per subframe=%d\n", sf_n_samples);

  uint32_t sf_n_re = SRSRAN_CP_NSYMB(SRSRAN_CP_NORM) * SRSRAN_NRE * 2 * nof_prb;
  sf_buffer        = srsran_vec_cf_malloc(sf_n_re);

  input_buffer      = srsran_vec_cf_malloc(sf_n_samples);
  input_buffer_temp = srsran_vec_cf_malloc(sf_n_samples);

  if (input_buffer == NULL || input_buffer_temp == NULL || sf_buffer == NULL) {
    ERROR("Error allocating buffers");
    goto clean_exit;
  }

  struct timeval t[3];
  gettimeofday(&t[1], NULL);

  bool     sync          = false;
  uint32_t num_subframes = 0;
  int32_t  samples_read  = 0;

  do {
    // Read and normalize samples from file
    samples_read = srsran_filesource_read(&fsrc, input_buffer, sf_n_samples);
    if (samples_read == 0) {
      // read entire file
      break;
    } else if (samples_read != sf_n_samples) {
      printf("Could only read %d of %d requested samples\n", samples_read, sf_n_samples);
      goto clean_exit;
    }

    // Find PSSS signal
    if (srsran_psss_find(&psss, input_buffer, nof_prb, cp) == SRSRAN_SUCCESS) {
      printf("PSSS correlation peak pos: %d value: %f N_id_2: %d\n",
             psss.corr_peak_pos,
             psss.corr_peak_value,
             psss.N_id_2);
      sync = true;
    }
    num_subframes++;
  } while (samples_read == sf_n_samples && num_subframes < max_subframes);

  ret = (sync == SRSRAN_SUCCESS);

clean_exit:
  srsran_filesource_free(&fsrc);
  srsran_psss_free(&psss);

  if (input_buffer != NULL) {
    free(input_buffer);
  }
  if (input_buffer_temp != NULL) {
    free(input_buffer_temp);
  }
  if (sf_buffer != NULL) {
    free(sf_buffer);
  }

  return ret;
}
