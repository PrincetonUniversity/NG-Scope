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

#ifdef __cplusplus
extern "C" {
#endif

void auto_dci_logging(ngscope_CA_status_t* q,
                        FILE* fd_dl[MAX_NOF_RF_DEV], FILE* fd_ul[MAX_NOF_RF_DEV],
                        bool cell_ready[MAX_NOF_RF_DEV],
                        int* curr_header, int* last_header,
                        int  nof_dev);

void fill_file_descirptor(FILE* fd_dl[MAX_NOF_RF_DEV],
                            FILE* fd_ul[MAX_NOF_RF_DEV],
                            bool log_dl, bool log_ul, int nof_rf_dev,
                            long long* rf_freq);

#endif
