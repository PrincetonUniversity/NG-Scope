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

#include "srsran/srsran.h"
#include "srsran/support/srsran_test.h"
#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

static srsran_cell_t cell = {6,              // nof_prb
                             1,              // nof_ports
                             1,              // cell_id
                             SRSRAN_CP_NORM, // cyclic prefix
                             SRSRAN_PHICH_NORM,
                             SRSRAN_PHICH_R_1, // PHICH length
                             SRSRAN_FDD};

static srsran_refsignal_dmrs_pusch_cfg_t dmrs_pusch_cfg = {};
static srsran_refsignal_srs_cfg_t        srs_cfg        = {};

static uint32_t test_counter = 0;

static float                 snr_db  = 20.0f;
static float                 n0_dbm  = 30.0f - 20.0f;
static srsran_channel_awgn_t channel = {};

#define CHEST_TEST_SRS_SNR_DB_TOLERANCE 10.0f
#define CHEST_TEST_SRS_TA_US_TOLERANCE 0.5f

// SRS index and sub-frame configuration possible values limited to SF=0
#define I_SRS_COUNT 9
static const uint32_t i_srs_values[I_SRS_COUNT] = {0, 2, 7, 17, 37, 77, 157, 317, 637};

#define SF_CONFIG_COUNT 9
static const uint32_t sf_config_values[SF_CONFIG_COUNT] = {0, 1, 3, 7, 9, 13, 14};

void usage(char* prog)
{
  printf("Usage: %s [recov]\n", prog);

  printf("\t-r nof_prb [Default %d]\n", cell.nof_prb);
  printf("\t-e extended cyclic prefix [Default normal]\n");

  printf("\t-c cell_id [Default %d]\n", cell.id);

  printf("\t-v increase verbosity\n");
}

typedef struct {
  uint32_t                      sf_size;
  srsran_refsignal_ul_t         refsignal_ul;
  srsran_refsignal_srs_pregen_t srs_pregen;
  srsran_chest_ul_t             chest_ul;
  cf_t*                         sf_symbols;
  srsran_chest_ul_res_t         chest_ul_res;
} srs_test_context_t;

int srs_test_context_init(srs_test_context_t* q)
{
  q->sf_size = SRSRAN_SF_LEN_RE(SRSRAN_MAX_PRB, cell.cp);

  // Set cell
  if (srsran_refsignal_ul_set_cell(&q->refsignal_ul, cell) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }

  // Pregenerate signals
  if (srsran_refsignal_srs_pregen(&q->refsignal_ul, &q->srs_pregen, &srs_cfg, &dmrs_pusch_cfg) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }

  // Allocate buffer
  q->sf_symbols = srsran_vec_cf_malloc(q->sf_size);
  if (q->sf_symbols == NULL) {
    return SRSRAN_ERROR;
  }

  // Create UL channel estimator
  if (srsran_chest_ul_init(&q->chest_ul, SRSRAN_MAX_PRB) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }

  // Set cell in UL channel estimator
  if (srsran_chest_ul_set_cell(&q->chest_ul, cell) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }

  // Initialise UL channel estimator result
  if (srsran_chest_ul_res_init(&q->chest_ul_res, SRSRAN_MAX_PRB) != SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }

  return SRSRAN_SUCCESS;
}

void srs_test_context_free(srs_test_context_t* q)
{
  srsran_refsignal_srs_pregen_free(&q->refsignal_ul, &q->srs_pregen);
  srsran_chest_ul_free(&q->chest_ul);
  srsran_chest_ul_res_free(&q->chest_ul_res);
  if (q->sf_symbols) {
    free(q->sf_symbols);
  }
}
int srs_test_context_run(srs_test_context_t* q)
{
  srsran_ul_sf_cfg_t ul_sf_cfg = {};

  INFO("   TEST: bw_cfg=%d; sf_cfg=%d; B=%d; b_hops=%d; n_srs=%d; I_srs=%d;",
       srs_cfg.bw_cfg,
       srs_cfg.subframe_config,
       srs_cfg.B,
       srs_cfg.b_hop,
       srs_cfg.n_srs,
       srs_cfg.I_srs);

  // Assert SRS transmission SF
  TESTASSERT(srsran_refsignal_srs_send_cs(srs_cfg.subframe_config, ul_sf_cfg.tti) == 1);
  TESTASSERT(srsran_refsignal_srs_send_cs(srs_cfg.subframe_config, ul_sf_cfg.tti) == 1);

  // Set resource grid to zero
  srsran_vec_cf_zero(q->sf_symbols, q->sf_size);

  // Put sounding reference signals
  TESTASSERT(srsran_refsignal_srs_put(
                 &q->refsignal_ul, &srs_cfg, ul_sf_cfg.tti, q->srs_pregen.r[ul_sf_cfg.tti], q->sf_symbols) ==
             SRSRAN_SUCCESS);

  // Apply AWGN channel
  if (!isnan(snr_db) && !isinf(snr_db)) {
    srsran_channel_awgn_run_c(&channel, q->sf_symbols, q->sf_symbols, q->sf_size);
  }

  // Estimate
  TESTASSERT(srsran_chest_ul_estimate_srs(
                 &q->chest_ul, &ul_sf_cfg, &srs_cfg, &dmrs_pusch_cfg, q->sf_symbols, &q->chest_ul_res) ==
             SRSRAN_SUCCESS);

  INFO("RESULTS: tti=%d; snr_db=%+.1f; noise_estimate_dbm=%+.1f; ta_us=%+.1f;",
       ul_sf_cfg.tti,
       q->chest_ul_res.snr_db,
       q->chest_ul_res.noise_estimate_dbm,
       q->chest_ul_res.ta_us);

  // Assert SRS measurements
  TESTASSERT(fabsf(q->chest_ul_res.snr_db - snr_db) < CHEST_TEST_SRS_SNR_DB_TOLERANCE);
  TESTASSERT(fabsf(q->chest_ul_res.noise_estimate_dbm - n0_dbm) < CHEST_TEST_SRS_SNR_DB_TOLERANCE);
  TESTASSERT(fabsf(q->chest_ul_res.ta_us) < CHEST_TEST_SRS_TA_US_TOLERANCE);

  return SRSRAN_SUCCESS;
}

void parse_args(int argc, char** argv)
{
  int opt;
  while ((opt = getopt(argc, argv, "recov")) != -1) {
    switch (opt) {
      case 'r':
        cell.nof_prb = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'e':
        cell.cp = SRSRAN_CP_EXT;
        break;
      case 'c':
        cell.id = (uint32_t)strtol(argv[optind], NULL, 10);
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
  srs_test_context_t context = {};
  int                ret     = SRSRAN_SUCCESS;

  parse_args(argc, argv);

  if (srsran_channel_awgn_init(&channel, 123456789) != SRSRAN_SUCCESS) {
    ret = SRSRAN_ERROR;
  }

  if (!isnan(snr_db) && !isinf(snr_db)) {
    n0_dbm = 30.0f - snr_db;
    srsran_channel_awgn_set_n0(&channel, n0_dbm - 33.0f);
  }

  for (srs_cfg.bw_cfg = 0; srs_cfg.bw_cfg < 8 && !ret; srs_cfg.bw_cfg++) {
    for (srs_cfg.B = 0; srs_cfg.B < 4 && !ret; srs_cfg.B++) {
      for (srs_cfg.n_srs = 0; srs_cfg.n_srs < 8 && !ret; srs_cfg.n_srs++) {
        // Initialise context
        ret = srs_test_context_init(&context);
        if (ret) {
          printf("Failed setting context: bw_cfg=%d; B=%d; n_srs=%d;\n", srs_cfg.bw_cfg, srs_cfg.B, srs_cfg.n_srs);
        }

        for (uint32_t sf_config = 0; sf_config < SF_CONFIG_COUNT && !ret; sf_config++) {
          srs_cfg.subframe_config = sf_config_values[sf_config];

          for (srs_cfg.b_hop = 0; srs_cfg.b_hop < 4 && !ret; srs_cfg.b_hop++) {
            for (uint32_t i_srs = 0; i_srs < I_SRS_COUNT && !ret; i_srs++) {
              srs_cfg.I_srs = i_srs_values[i_srs];

              // Run actual test
              ret = srs_test_context_run(&context);
              if (!ret) {
                test_counter++;
              }
            }
          }
        }

        // Free context
        srs_test_context_free(&context);
      }
    }
  }

  srsran_channel_awgn_free(&channel);

  if (!ret) {
    printf("OK, %d test passed successfully.\n", test_counter);
  } else {
    printf("Failed at test %d.\n", test_counter);
  }

  return ret;
}
