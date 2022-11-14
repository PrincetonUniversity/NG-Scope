#ifndef NGSCOPE_DCI_LOG_H
#define NGSCOPE_DCI_LOG_H

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

#include "srsran/srsran.h"
#include "ngscope_def.h"
#include "status_tracker.h"
#include "parse_args.h"
#include "load_config.h"


#ifdef __cplusplus
extern "C" {
#endif

void fill_file_descriptor(FILE* fd_dl[MAX_NOF_RF_DEV],
							FILE* 	fd_ul[MAX_NOF_RF_DEV],
							ngscope_config_t* config);
//
//							bool 	log_dl, 
//							bool 	log_ul, 
//							int  	nof_rf_dev,
//							long long* rf_freq);

void* dci_log_thread(void* p);
#endif
