#ifndef DCI_DECODER_H
#define DCI_DECODER_H

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
#include "srslte/phy/ue/LTESCOPE_GLOBAL.h"

typedef struct{
    int dci_thd_id;
    int usrp_thd_id;
    int free_order;
}dci_usrp_id_t;

enum receiver_state { DECODE_MIB, DECODE_PDSCH};
void* dci_decoder(void *p);

#endif

