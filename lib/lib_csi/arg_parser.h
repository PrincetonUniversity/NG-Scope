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
#include <stdint.h> 
#include <stdbool.h>
#include "srslte/srslte.h"

/**********************************************************************
 *  Program arguments processing
 ***********************************************************************/
typedef struct {
  int nof_subframes;
  int cpu_affinity;
  bool disable_cfo;
  uint32_t time_offset;
  int force_N_id_2;
  int nof_thread;
  uint16_t rnti;
  char *input_file_name;
  bool enable_cfo_ref;
  bool average_subframe;
  char *rf_dev;
  char *rf_args;
  uint32_t rf_nof_rx_ant;
  double rf_freq;
  float rf_gain;
  int decimate;
  int32_t mbsfn_area_id;
  uint8_t  non_mbsfn_region;
  uint8_t  mbsfn_sf_mask;
  int verbose;
}prog_args_t;

void args_default(prog_args_t *args);
void parse_args(prog_args_t *args, int argc, char **argv);
