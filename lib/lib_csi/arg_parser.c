#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>

#include "arg_parser.h"

void args_default(prog_args_t *args) {
  args->nof_subframes = -1;
  args->rnti = SRSLTE_SIRNTI;
  args->force_N_id_2 = -1; // Pick the best
  args->nof_thread = 1; // Pick the best
  args->input_file_name = NULL;
  args->disable_cfo = false;
  args->time_offset = 0;
  args->rf_args = "";
  args->rf_dev  = "";
  args->rf_freq = -1.0;
  args->rf_nof_rx_ant = 1;
  args->enable_cfo_ref = false;
  args->average_subframe = false;
#ifdef ENABLE_AGC_DEFAULT
  args->rf_gain = -1.0;
#else
  args->rf_gain = 50.0;
#endif
  args->decimate = 0;
  args->cpu_affinity = -1;
  args->mbsfn_area_id = -1;
  args->non_mbsfn_region = 2;
  args->mbsfn_sf_mask = 32;
}

void usage(prog_args_t *args, char *prog) {
  printf("Usage: %s [agpPoOcildFRDnruMNv] -f rx_frequency (in Hz) | -i input_file\n", prog);
#ifndef DISABLE_RF
  printf("\t-I RF dev [Default %s]\n", args->rf_dev);
  printf("\t-a RF args [Default %s]\n", args->rf_args);
  printf("\t-A Number of RX antennas [Default %d]\n", args->rf_nof_rx_ant);
#ifdef ENABLE_AGC_DEFAULT
  printf("\t-g RF fix RX gain [Default AGC]\n");
#else
  printf("\t-g Set RX gain [Default %.1f dB]\n", args->rf_gain);
#endif
#else
  printf("\t   RF is disabled.\n");
#endif
  printf("\t-i input_file [Default use RF board]\n");
  printf("\t-e number of decoding threads [Default %d]\n", args->nof_thread);
  printf("\t-r RNTI in Hex [Default 0x%x]\n",args->rnti);
  printf("\t-l Force N_id_2 [Default best]\n");
  printf("\t-C Disable CFO correction [Default %s]\n", args->disable_cfo?"Disabled":"Enabled");
  printf("\t-F Enable RS-based CFO correction [Default %s]\n", !args->enable_cfo_ref?"Disabled":"Enabled");
  printf("\t-R Average channel estimates on 1 ms [Default %s]\n", !args->average_subframe?"Disabled":"Enabled");
  printf("\t-t Add time offset [Default %d]\n", args->time_offset);
  printf("\t plots are disabled. Graphics library not available\n");
  printf("\t-y set the cpu affinity mask [Default %d] \n  ",args->cpu_affinity);
  printf("\t-n nof_subframes [Default %d]\n", args->nof_subframes);
  printf("\t-M MBSFN area id [Default %d]\n", args->mbsfn_area_id);
  printf("\t-N Non-MBSFN region [Default %d]\n", args->non_mbsfn_region);
  printf("\t-v [set srslte_verbose to debug, default none]\n");
}


void parse_args(prog_args_t *args, int argc, char **argv) {
  int opt;
  args_default(args);
  while ((opt = getopt(argc, argv, "aeAoglipPcOCtdDFRnvrfuUsSZyWMNB")) != -1) {
    switch (opt) {
    case 'i':
      args->input_file_name = argv[optind];
      break;
    case 'a':
      args->rf_args = argv[optind];
      break;
    case 'A':
      args->rf_nof_rx_ant = atoi(argv[optind]);
      break;
    case 'g':
      args->rf_gain = atof(argv[optind]);
      break;
    case 'C':
      args->disable_cfo = true;
      break;
    case 'F':
      args->enable_cfo_ref = true;
      break;
    case 'I':
      args->rf_dev = argv[optind];
      break;
    case 'R':
      args->average_subframe = true;
      break;
    case 't':
      args->time_offset = atoi(argv[optind]);
      break;
    case 'f':
      args->rf_freq = strtod(argv[optind], NULL);
      break;
    case 'n':
      args->nof_subframes = atoi(argv[optind]);
      break;
    case 'r':
      args->rnti = strtol(argv[optind], NULL, 16);
      break;
    case 'l':
      args->force_N_id_2 = atoi(argv[optind]);
      break;
    case 'e':
      args->nof_thread = atoi(argv[optind]);
      break;
     case 'v':
      srslte_verbose++;
      args->verbose = srslte_verbose;
      break;
    case 'Z':
      args->decimate = atoi(argv[optind]);
      break;
    case 'y':
      args->cpu_affinity = atoi(argv[optind]);
      break;
   case 'M':
      args->mbsfn_area_id = atoi(argv[optind]);
      break;
    case 'N':
      args->non_mbsfn_region = atoi(argv[optind]);
      break;
    case 'B':
      args->mbsfn_sf_mask = atoi(argv[optind]);
      break;
    default:
      usage(args, argv[0]);
      exit(-1);
    }
  }

  if (args->rf_freq < 0 && args->input_file_name == NULL) {
    usage(args, argv[0]);
    exit(-1);
  }
}
