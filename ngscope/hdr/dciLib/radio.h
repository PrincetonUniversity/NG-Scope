#ifndef NGSCOPE_RADIO_H
#define NGSCOPE_RADIO_H

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

#ifdef __cplusplus
extern "C" {
#endif

#include "srsran/phy/rf/rf.h"
#include "srsran/phy/rf/rf_utils.h"

#include "parse_args.h"
//#include "rf_utils.h"
int radio_init_and_start(srsran_rf_t* rf, 
                    srsran_cell_t* cell, 
                    prog_args_t prog_args, 
                    cell_search_cfg_t* cell_detect_config,
                    float* search_cell_cfo);

int radio_stop(srsran_rf_t* rf);
#ifdef __cplusplus
}
#endif

#endif
