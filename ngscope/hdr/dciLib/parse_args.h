#ifndef NGSCOPE_PARSE_PROG_H
#define NGSCOPE_PARSE_PROG_H

#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdbool.h>

#include "ngscope_def.h"

typedef struct {
  int      nof_subframes;
  int      cpu_affinity;
  bool     disable_plots;
  bool     disable_plots_except_constellation;
  bool     disable_cfo;
  bool     log_dl;
  bool     log_ul;
  uint32_t time_offset;
  int      force_N_id_2;
  uint16_t rnti;
  char*    input_file_name;
  int      file_offset_time;
  float    file_offset_freq;
  uint32_t file_nof_prb;
  uint32_t file_nof_ports;
  uint32_t file_cell_id;
  bool     enable_cfo_ref;
  char*    estimator_alg;
  char*    rf_dev;
  char*    rf_args;
  int      rf_index;
  uint32_t rf_nof_rx_ant;
  double   rf_freq;

  long long  rf_freq_vec[MAX_NOF_RF_DEV];
  uint32_t rx_nof_rx_ant_vec[MAX_NOF_RF_DEV];

  int      remote_enable;
  float    rf_gain;
  int      net_port;
  char*    net_address;
  int      net_port_signal;
  char*    net_address_signal;
  int      nof_decoder;
  int      nof_rf_dev;
  int      decimate;
  int32_t  mbsfn_area_id;
  uint8_t  non_mbsfn_region;
  uint8_t  mbsfn_sf_mask;
  int      tdd_special_sf;
  int      sf_config;
  int      verbose;
  bool     enable_256qam;
  bool     use_standard_lte_rate;
} prog_args_t;

void args_default(prog_args_t* args);
void usage(prog_args_t* args, char* prog);
#endif
