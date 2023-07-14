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
#include <stdint.h>
#include "ngscope/hdr/dciLib/parse_args.h"

void args_default(prog_args_t* args)
{
  args->disable_plots                      = false;
  args->disable_plots_except_constellation = false;
  args->nof_subframes                      = -1;
  //args->rnti                               = 0xFFFF; //SRSRAN_SIRNTI;
  args->rnti                               = 0x1315; //SRSRAN_SIRNTI;
  args->force_N_id_2                       = -1; // Pick the best
  args->tdd_special_sf                     = -1;
  args->sf_config                          = 2;
  args->input_file_name                    = NULL;
  args->disable_cfo                        = false;
  args->log_dl                             = false;
  args->log_ul                             = false;
  args->time_offset                        = 0;
  args->file_nof_prb                       = 25;
  args->file_nof_ports                     = 1;
  args->file_cell_id                       = 0;
  args->file_offset_time                   = 0;
  args->file_offset_freq                   = 0;
  args->rf_dev                             = (char*)"";
  args->rf_args                            = (char*)"";
  args->rf_index                           = 0; 
  args->rf_freq                            = 2.355e9;
  args->rf_nof_rx_ant                      = 1;
  args->remote_enable                      = false;
  args->enable_cfo_ref                     = false;
  args->estimator_alg                      = (char*)"interpolate";
  args->enable_256qam                      = true;
  args->rf_gain                            = -1.0;
  args->net_port                           = -1;
  args->net_address                        = (char*)"127.0.0.1";
  args->net_port_signal                    = -1;
  args->net_address_signal                 = (char*)"127.0.0.1";
  args->nof_decoder                        = 2;
  args->nof_rf_dev                         = 1;
  args->decimate                           = 0;
  args->cpu_affinity                       = -1;
  args->mbsfn_area_id                      = -1;
  args->non_mbsfn_region                   = 2;
  args->mbsfn_sf_mask                      = 32;
}


void usage(prog_args_t* args, char* prog)
{
  printf("Usage: %s [adgpPoOcildFRDnruMNvTG] -f rx_frequency (in Hz) | -i input_file\n", prog);
#ifndef DISABLE_RF
  printf("\t-I RF dev [Default %s]\n", args->rf_dev);
  printf("\t-a RF args [Default %s]\n", args->rf_args);
  printf("\t-a RF index [Default %d]\n", args->rf_index);
  printf("\t-A Number of RX antennas [Default %d]\n", args->rf_nof_rx_ant);
  printf("\t-A Number of DCI decoders [Default %d]\n", args->nof_decoder);
#ifdef ENABLE_AGC_DEFAULT
  printf("\t-g RF fix RX gain [Default AGC]\n");
#else
  printf("\t-g Set RX gain [Default %.1f dB]\n", args->rf_gain);
#endif
#else
  printf("\t   RF is disabled.\n");
#endif
  printf("\t-i input_file [Default use RF board]\n");
  printf("\t-o offset frequency correction (in Hz) for input file [Default %.1f Hz]\n", args->file_offset_freq);
  printf("\t-O offset samples for input file [Default %d]\n", args->file_offset_time);
  printf("\t-p nof_prb for input file [Default %d]\n", args->file_nof_prb);
  printf("\t-P nof_ports for input file [Default %d]\n", args->file_nof_ports);
  printf("\t-c cell_id for input file [Default %d]\n", args->file_cell_id);
  printf("\t-r RNTI in Hex [Default 0x%x]\n", args->rnti);
  printf("\t-l Force N_id_2 [Default best]\n");
  printf("\t-C Disable CFO correction [Default %s]\n", args->disable_cfo ? "Disabled" : "Enabled"); 
  printf("\t-F Enable RS-based CFO correction [Default %s]\n", !args->enable_cfo_ref ? "Disabled" : "Enabled");
  printf("\t-R Channel estimates algorithm (average, interpolate, wiener) [Default %s]\n", args->estimator_alg);
  printf("\t-t Add time offset [Default %d]\n", args->time_offset);
  printf("\t-T Set TDD special subframe configuration [Default %d]\n", args->tdd_special_sf);
  printf("\t-G Set TDD uplink/downlink configuration [Default %d]\n", args->sf_config);
#ifdef ENABLE_GUI
  printf("\t-d disable plots [Default enabled]\n");
  printf("\t-D disable all but constellation plots [Default enabled]\n");
#else  /* ENABLE_GUI */
  printf("\t plots are disabled. Graphics library not available\n");
#endif /* ENABLE_GUI */
  printf("\t-y set the cpu affinity mask [Default %d] \n  ", args->cpu_affinity);
  printf("\t-n nof_subframes [Default %d]\n", args->nof_subframes);
  printf("\t-s remote UDP port to send input signal (-1 does nothing with it) [Default %d]\n", args->net_port_signal);
  printf("\t-S remote UDP address to send input signal [Default %s]\n", args->net_address_signal);
  printf("\t-u remote TCP port to send data (-1 does nothing with it) [Default %d]\n", args->net_port);
  printf("\t-U remote TCP address to send data [Default %s]\n", args->net_address);
  printf("\t-M MBSFN area id [Default %d]\n", args->mbsfn_area_id);
  printf("\t-N Non-MBSFN region [Default %d]\n", args->non_mbsfn_region);
  printf("\t-q Enable/Disable 256QAM modulation (default %s)\n", args->enable_256qam ? "enabled" : "disabled");
  printf("\t-Q Use standard LTE sample rates (default %s)\n", args->use_standard_lte_rate ? "enabled" : "disabled");
  printf("\t-v [set srsran_verbose to debug, default none]\n");
}


