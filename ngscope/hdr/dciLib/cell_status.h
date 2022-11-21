#ifndef NGSCOPE_CELL_STATUS
#define NGSCOPE_CELL_STATUS
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
#include "dci_ring_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct{
    uint16_t    targetRNTI;
    int         nof_cell;
    int         remote_sock;
	bool 		remote_enable;
    int         cell_prb[MAX_NOF_RF_DEV];
}cell_status_info_t;

void* cell_status_thread(void* arg);
#endif
