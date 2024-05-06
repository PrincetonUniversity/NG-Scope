#ifndef DCI_SINK_HH
#define DCI_SINK_HH
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdbool.h>


#define MAX_NOF_CELL 4
#define NOF_LOG_DCI 1000
#define MAX_CLIENT 5
#define MAX_NOF_RNTI_PER_DCI 20


// This structure is used for the DCI exchange between NG-Scope and the app receiver
// We include both downlink and uplink information
// If necessary, we could define other structures that includes more information
// Such as the MCS index. TO do that, we also need to change the DCI sending funcations
typedef struct{
	uint8_t  cell_idx;

    uint64_t time_stamp;
    uint16_t tti;

	uint16_t rnti; 

	// downlink information NOTE: some information are not included (e.g. MCS)
	uint32_t dl_tbs;
    uint8_t  dl_reTx;
	bool 	 dl_rv_flag;

	// uplink information
    uint32_t ul_tbs;
    uint8_t  ul_reTx;
	bool 	 ul_rv_flag;
}ue_dci_t;


// Structure that used for storing the config between the dci sink server/client
// Adding other information inside this cell is possible
typedef struct{
	uint8_t  nof_cell;
	uint16_t cell_prb[MAX_NOF_CELL];
	uint16_t rnti;
}cell_config_t;

typedef struct {
	uint16_t rnti;
	uint32_t dl_tbs;
	uint8_t dl_prb;
	uint8_t dl_reTx;

	uint32_t ul_tbs;
	uint8_t ul_prb;
	uint8_t ul_reTx;
} rnti_dci_t;

// Structure that is used for storing all RNTIs DCI data per cell between the
// dci sink server/client
typedef struct {
	uint8_t  cell_id;
	uint64_t time_stamp;
	uint16_t tti;
	uint64_t total_dl_tbs;
	uint64_t total_ul_tbs;
	uint8_t total_dl_prb;
	uint8_t total_ul_prb;
	uint8_t total_dl_reTx;
	uint8_t total_ul_reTx;
	// TODO: Evaluate MAX_NOF_RNTI
	uint8_t nof_rnti;
	rnti_dci_t rnti_list[MAX_NOF_RNTI_PER_DCI];
} cell_dci_t;



#endif
