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
#include <unistd.h>

#include "srsran/phy/utils/random.h"
#include "srsran/srsran.h"

static int         nof_prb               = -1;
static srsran_cp_t cp                    = SRSRAN_CP_NORM;
static int         nof_repetitions       = 1;
static float       rx_window_offset      = 0.5f;
static float       freq_shift_f          = 0.0f;
static double      phase_compensation_hz = 0.0;
static uint32_t    force_symbol_sz       = 0;
static double      elapsed_us(struct timeval* ts_start, struct timeval* ts_end)
{
  if (ts_end->tv_usec > ts_start->tv_usec) {
    return ((double)ts_end->tv_sec - (double)ts_start->tv_sec) * 1000000 + (double)ts_end->tv_usec -
           (double)ts_start->tv_usec;
  } else {
    return ((double)ts_end->tv_sec - (double)ts_start->tv_sec - 1) * 1000000 + ((double)ts_end->tv_usec + 1000000) -
           (double)ts_start->tv_usec;
  }
}

static void usage(char* prog)
{
  printf("Usage: %s\n", prog);
  printf("\t-N Force symbol size, 0 for auto [Default %d]\n", force_symbol_sz);
  printf("\t-n Force number of Resource blocks [Default All]\n");
  printf("\t-e extended cyclic prefix [Default Normal]\n");
  printf("\t-r nof_repetitions [Default %d]\n", nof_repetitions);
  printf("\t-o rx window offset (portion of CP length) [Default %.1f]\n", rx_window_offset);
  printf("\t-s frequency shift (normalised with sampling rate) [Default %.1f]\n", freq_shift_f);
  printf("\t-p Phase compensation carrier frequency in Hz [Default %.1f]\n", phase_compensation_hz);
}

static void parse_args(int argc, char** argv)
{
  int opt;
  while ((opt = getopt(argc, argv, "Nnerosp")) != -1) {
    switch (opt) {
      case 'n':
        nof_prb = (int)strtol(argv[optind], NULL, 10);
        break;
      case 'N':
        force_symbol_sz = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'e':
        cp = SRSRAN_CP_EXT;
        break;
      case 'r':
        nof_repetitions = (int)strtol(argv[optind], NULL, 10);
        break;
      case 'o':
        rx_window_offset = SRSRAN_MIN(1.0f, SRSRAN_MAX(0.0f, strtof(argv[optind], NULL)));
        break;
      case 's':
        freq_shift_f = SRSRAN_MIN(1.0f, SRSRAN_MAX(0.0f, strtof(argv[optind], NULL)));
        break;
      case 'p':
        phase_compensation_hz = strtod(argv[optind], NULL);
        break;
      default:
        usage(argv[0]);
        exit(-1);
    }
  }
}

int main(int argc, char** argv)
{
  srsran_random_t random_gen = srsran_random_init(0);
  struct timeval  start, end;
  srsran_ofdm_t   fft = {}, ifft = {};
  cf_t *          input, *outfft, *outifft;
  float           mse;
  uint32_t        n_prb, max_prb;

  parse_args(argc, argv);

  if (nof_prb == -1) {
    n_prb   = 6;
    max_prb = SRSRAN_MAX_PRB;
  } else {
    n_prb   = (uint32_t)nof_prb;
    max_prb = (uint32_t)nof_prb;
  }
  while (n_prb <= max_prb) {
    uint32_t symbol_sz = (force_symbol_sz) ? force_symbol_sz : (uint32_t)srsran_symbol_sz(n_prb);
    uint32_t n_re      = SRSRAN_CP_NSYMB(cp) * n_prb * SRSRAN_NRE * SRSRAN_NOF_SLOTS_PER_SF;
    uint32_t sf_len    = SRSRAN_SF_LEN(symbol_sz);

    printf("Running test for %d PRB, %d RE... ", n_prb, n_re);
    fflush(stdout);

    input   = srsran_vec_cf_malloc(n_re);
    outfft  = srsran_vec_cf_malloc(n_re);
    outifft = srsran_vec_cf_malloc(sf_len);
    if (!input || !outfft || !outifft) {
      perror("malloc");
      exit(-1);
    }
    srsran_vec_cf_zero(outifft, sf_len);

    srsran_ofdm_cfg_t ofdm_cfg     = {};
    ofdm_cfg.cp                    = cp;
    ofdm_cfg.in_buffer             = input;
    ofdm_cfg.out_buffer            = outifft;
    ofdm_cfg.nof_prb               = n_prb;
    ofdm_cfg.symbol_sz             = symbol_sz;
    ofdm_cfg.freq_shift_f          = freq_shift_f;
    ofdm_cfg.normalize             = true;
    ofdm_cfg.phase_compensation_hz = phase_compensation_hz;
    if (srsran_ofdm_tx_init_cfg(&ifft, &ofdm_cfg)) {
      ERROR("Error initializing iFFT");
      exit(-1);
    }

    ofdm_cfg.in_buffer        = outifft;
    ofdm_cfg.out_buffer       = outfft;
    ofdm_cfg.rx_window_offset = rx_window_offset;
    ofdm_cfg.freq_shift_f     = -freq_shift_f;
    if (srsran_ofdm_rx_init_cfg(&fft, &ofdm_cfg)) {
      ERROR("Error initializing FFT");
      exit(-1);
    }

    if (isnormal(freq_shift_f)) {
      nof_repetitions = 1;
    }

    // Generate Random data
    srsran_random_uniform_complex_dist_vector(random_gen, input, n_re, -1.0f, +1.0f);

    // Execute Tx
    gettimeofday(&start, NULL);
    for (uint32_t i = 0; i < nof_repetitions; i++) {
      srsran_ofdm_tx_sf(&ifft);
    }
    gettimeofday(&end, NULL);
    printf(" Tx@%.1fMsps", (float)(sf_len * nof_repetitions) / elapsed_us(&start, &end));

    // Execute Rx
    gettimeofday(&start, NULL);
    for (uint32_t i = 0; i < nof_repetitions; i++) {
      srsran_ofdm_rx_sf(&fft);
    }
    gettimeofday(&end, NULL);
    printf(" Rx@%.1fMsps", (double)(sf_len * nof_repetitions) / elapsed_us(&start, &end));

    // compute Mean Square Error
    srsran_vec_sub_ccc(input, outfft, outfft, n_re);
    mse = sqrtf(srsran_vec_avg_power_cf(outfft, n_re));

    printf(" MSE=%.6f\n", mse);

    if (mse >= 0.0001) {
      printf("MSE too large\n");
      exit(-1);
    }

    srsran_ofdm_rx_free(&fft);
    srsran_ofdm_tx_free(&ifft);

    free(input);
    free(outfft);
    free(outifft);

    n_prb++;
  }

  srsran_random_free(random_gen);

  exit(0);
}
