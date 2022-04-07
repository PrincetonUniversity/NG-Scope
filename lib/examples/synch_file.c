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
#include <strings.h>
#include <sys/time.h>
#include <unistd.h>

#include "srsran/srsran.h"

char* input_file_name;
char* output_file_name = "abs_corr.txt";
int   nof_frames = 100, frame_length = 9600, symbol_sz = 128;
float corr_peak_threshold = 25.0;
int   out_N_id_2 = 0, force_N_id_2 = -1;

#define CFO_AUTO -9999.0
float force_cfo = CFO_AUTO;

void usage(char* prog)
{
  printf("Usage: %s [olntsNfcv] -i input_file\n", prog);
  printf("\t-o output_file [Default %s]\n", output_file_name);
  printf("\t-l frame_length [Default %d]\n", frame_length);
  printf("\t-n number of frames [Default %d]\n", nof_frames);
  printf("\t-t correlation threshold [Default %g]\n", corr_peak_threshold);
  printf("\t-s symbol_sz [Default %d]\n", symbol_sz);
  printf("\t-N out_N_id_2 [Default %d]\n", out_N_id_2);
  printf("\t-f force_N_id_2 [Default %d]\n", force_N_id_2);
  printf("\t-c force_cfo [Default disabled]\n");
  printf("\t-v srsran_verbose\n");
}

void parse_args(int argc, char** argv)
{
  int opt;
  while ((opt = getopt(argc, argv, "ionltsNfcv")) != -1) {
    switch (opt) {
      case 'i':
        input_file_name = argv[optind];
        break;
      case 'o':
        output_file_name = argv[optind];
        break;
      case 'n':
        nof_frames = (int)strtol(argv[optind], NULL, 10);
        break;
      case 'l':
        frame_length = (int)strtol(argv[optind], NULL, 10);
        break;
      case 't':
        corr_peak_threshold = strtof(argv[optind], NULL);
        break;
      case 's':
        symbol_sz = (int)strtol(argv[optind], NULL, 10);
        break;
      case 'N':
        out_N_id_2 = (int)strtol(argv[optind], NULL, 10);
        break;
      case 'f':
        force_N_id_2 = (int)strtol(argv[optind], NULL, 10);
        break;
      case 'c':
        force_cfo = strtof(argv[optind], NULL);
        break;
      case 'v':
        increase_srsran_verbose_level();
        break;
      default:
        usage(argv[0]);
        exit(-1);
    }
  }
  if (!input_file_name) {
    usage(argv[0]);
    exit(-1);
  }
}

int main(int argc, char** argv)
{
  srsran_filesource_t fsrc;
  srsran_filesink_t   fsink;
  srsran_pss_t        pss[3]; // One for each N_id_2
  srsran_sss_t        sss[3]; // One for each N_id_2
  srsran_cfo_t        cfocorr;
  int                 peak_pos[3];
  float*              cfo;
  float               peak_value[3];
  int                 frame_cnt;
  cf_t*               input;
  uint32_t            m0, m1;
  float               m0_value, m1_value;
  uint32_t            N_id_2;
  int                 sss_idx;
  struct timeval      tdata[3];
  int*                exec_time;

  if (argc < 3) {
    usage(argv[0]);
    exit(-1);
  }

  parse_args(argc, argv);

  gettimeofday(&tdata[1], NULL);
  printf("Initializing...");
  fflush(stdout);

  if (srsran_filesource_init(&fsrc, input_file_name, SRSRAN_COMPLEX_FLOAT_BIN)) {
    ERROR("Error opening file %s", input_file_name);
    exit(-1);
  }
  if (srsran_filesink_init(&fsink, output_file_name, SRSRAN_COMPLEX_FLOAT_BIN)) {
    ERROR("Error opening file %s", output_file_name);
    exit(-1);
  }

  input = srsran_vec_cf_malloc(frame_length);
  if (!input) {
    perror("malloc");
    exit(-1);
  }
  cfo = srsran_vec_f_malloc(nof_frames);
  if (!cfo) {
    perror("malloc");
    exit(-1);
  }
  exec_time = srsran_vec_i32_malloc(nof_frames);
  if (!exec_time) {
    perror("malloc");
    exit(-1);
  }

  if (srsran_cfo_init(&cfocorr, frame_length)) {
    ERROR("Error initiating CFO");
    return -1;
  }

  /* We have 2 options here:
   * a) We create 3 pss objects, each initialized with a different N_id_2
   * b) We create 1 pss object which scans for each N_id_2 one after another.
   * a) requries more memory but has less latency and is paralellizable.
   */
  for (N_id_2 = 0; N_id_2 < 3; N_id_2++) {
    if (srsran_pss_init_fft(&pss[N_id_2], frame_length, symbol_sz)) {
      ERROR("Error initializing PSS object");
      exit(-1);
    }
    if (srsran_pss_set_N_id_2(&pss[N_id_2], N_id_2)) {
      ERROR("Error initializing N_id_2");
      exit(-1);
    }
    if (srsran_sss_init(&sss[N_id_2], symbol_sz)) {
      ERROR("Error initializing SSS object");
      exit(-1);
    }
    if (srsran_sss_set_N_id_2(&sss[N_id_2], N_id_2)) {
      ERROR("Error initializing N_id_2");
      exit(-1);
    }
  }
  gettimeofday(&tdata[2], NULL);
  get_time_interval(tdata);
  printf("done in %ld s %ld ms\n", tdata[0].tv_sec, tdata[0].tv_usec / 1000);

  printf("\n\tFr.Cnt\tN_id_2\tN_id_1\tSubf\tPSS Peak/Avg\tIdx\tm0\tm1\tCFO\n");
  printf("\t===============================================================================\n");

  /* read all file or nof_frames */
  frame_cnt = 0;
  while (frame_length == srsran_filesource_read(&fsrc, input, frame_length) && frame_cnt < nof_frames) {
    gettimeofday(&tdata[1], NULL);
    if (force_cfo != CFO_AUTO) {
      srsran_cfo_correct(&cfocorr, input, input, force_cfo / 128);
    }

    if (force_N_id_2 != -1) {
      N_id_2           = force_N_id_2;
      peak_pos[N_id_2] = srsran_pss_find_pss(&pss[N_id_2], input, &peak_value[N_id_2]);
    } else {
      for (N_id_2 = 0; N_id_2 < 3; N_id_2++) {
        peak_pos[N_id_2] = srsran_pss_find_pss(&pss[N_id_2], input, &peak_value[N_id_2]);
      }
      float max_value = -99999;
      N_id_2          = -1;
      int i;
      for (i = 0; i < 3; i++) {
        if (peak_value[i] > max_value) {
          max_value = peak_value[i];
          N_id_2    = i;
        }
      }
    }

    /* If peak detected */
    if (peak_value[N_id_2] > corr_peak_threshold) {
      sss_idx = peak_pos[N_id_2] - 2 * (symbol_sz + SRSRAN_CP_LEN(symbol_sz, SRSRAN_CP_NORM_LEN));
      if (sss_idx >= 0) {
        srsran_sss_m0m1_diff(&sss[N_id_2], &input[sss_idx], &m0, &m0_value, &m1, &m1_value);

        cfo[frame_cnt] = srsran_pss_cfo_compute(&pss[N_id_2], &input[peak_pos[N_id_2] - 128]);
        printf("\t%d\t%d\t%d\t%d\t%.3f\t\t%3d\t%d\t%d\t%.3f\n",
               frame_cnt,
               N_id_2,
               srsran_sss_N_id_1(&sss[N_id_2], m0, m1, m1_value + m0_value),
               srsran_sss_subframe(m0, m1),
               peak_value[N_id_2],
               peak_pos[N_id_2],
               m0,
               m1,
               cfo[frame_cnt]);
      }
    }
    gettimeofday(&tdata[2], NULL);
    get_time_interval(tdata);
    exec_time[frame_cnt] = tdata[0].tv_usec;
    frame_cnt++;
  }

  int   i;
  float avg_time = 0;
  for (i = 0; i < frame_cnt; i++) {
    avg_time += (float)exec_time[i];
  }
  avg_time /= frame_cnt;
  printf("\n");
  printf("Average exec time: %.3f ms / frame. %.3f Msamp/s (%.3f\%% CPU)\n",
         avg_time / 1000,
         frame_length / avg_time,
         100 * avg_time / 5000 * (9600 / (float)frame_length));

  float cfo_mean = 0;
  for (i = 0; i < frame_cnt; i++) {
    cfo_mean += cfo[i] / frame_cnt * (9600 / frame_length);
  }
  printf("Average CFO: %.3f\n", cfo_mean);

  for (N_id_2 = 0; N_id_2 < 3; N_id_2++) {
    srsran_pss_free(&pss[N_id_2]);
    srsran_sss_free(&sss[N_id_2]);
  }

  srsran_filesource_free(&fsrc);
  srsran_filesink_free(&fsink);

  free(input);
  free(cfo);

  printf("Done\n");
  exit(0);
}
