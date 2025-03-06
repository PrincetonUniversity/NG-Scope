#ifndef _NGSCOPE_RADIO_H_
#define _NGSCOPE_RADIO_H_

#include "srsran/srsran.h"
#include "srsran/phy/rf/rf.h"
#include "srsran/common/interfaces_common.h"
#include "srsran/radio/radio.h"

#ifdef __cplusplus
extern "C" {
#endif

int srsran_rf_recv_wrapper( void* h,
                            cf_t* data_[SRSRAN_MAX_PORTS], 
                            uint32_t nsamples, 
                            srsran_timestamp_t* t);

class Radio : public srsran::radio {

};

#ifdef __cplusplus
}
#endif

#endif