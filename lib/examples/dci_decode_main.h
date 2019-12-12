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
#include "srslte/common/gen_mch_tables.h"
#include "srslte/common/crash_handler.h"
#include <srslte/phy/common/phy_common.h>
#include "srslte/phy/io/filesink.h"
#include "srslte/srslte.h"
#include "srslte/phy/ue/lte_scope.h"
#include "srslte/phy/ue/ue_list.h"

/**********************************************************************
 *  Program arguments processing
 ***********************************************************************/
typedef struct {
  int nof_subframes;
  int cpu_affinity;
  bool disable_plots;
  bool disable_plots_except_constellation;
  bool disable_cfo;
  uint32_t time_offset;
  int force_N_id_2;
  int nof_thread;
  uint16_t rnti;
  char *input_file_name;
  int file_offset_time;
  float file_offset_freq;
  uint32_t file_nof_prb;
  uint32_t file_nof_ports;
  uint32_t file_cell_id;
  bool enable_cfo_ref;
  bool average_subframe;
  char *rf_args;
  uint32_t rf_nof_rx_ant;
  double rf_freq;
  float rf_gain;
  int net_port;
  char *net_address;
  int net_port_signal;
  char *net_address_signal;
  int decimate;
  int32_t mbsfn_area_id;
  uint8_t  non_mbsfn_region;
  uint8_t  mbsfn_sf_mask;
  int verbose;
}prog_args_t;
enum receiver_state { DECODE_MIB, DECODE_PDSCH};

void args_default(prog_args_t *args);
void usage(prog_args_t *args, char *prog);
void parse_args(prog_args_t *args, int argc, char **argv);



void dci_decode_main();
void* dci_decode_multi(void *p);
