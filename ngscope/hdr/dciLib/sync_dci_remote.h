#ifndef SYNC_DCI_REMOTE_H
#define SYNC_DCI_REMOTE_H

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
#include <sys/socket.h>

#include "srsran/srsran.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef struct{
	uint64_t time_stamp;
	uint16_t tti;
	uint8_t  cell_idx;
	bool 	 ul_reTx;
	uint8_t  proto_v;

	bool 	downlink;
	bool 	uplink;

	ngscope_dci_msg_t ul_dci;
	ngscope_dci_msg_t dl_dci;
}ngscope_dci_sync_t;

void dci_sync_init(ngscope_dci_sync_t* dci_sync);

int ngscope_sync_dci_remote(int sock, ngscope_dci_sync_t dci_sync);
int ngscope_sync_config_remote(int sock, uint16_t* prb_cell, uint8_t nof_cell);
#endif
