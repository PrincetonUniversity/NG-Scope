#ifndef _SINGLE_THREAD_H_
#define _SINGLE_THREAD_H_

#include <iostream>

#include "srsran/srsran.h"
#include "srsran/phy/common/phy_common.h"
#include "srsran/asn1/rrc/si.h"
#include "srsran/asn1/rrc.h"
#include "srsran/asn1/rrc_utils.h"

#include "common_type_define.h"
#include "runtimeConfig.h"

#ifdef __cplusplus
extern "C" {
#endif

void singleThreadProc(std::shared_ptr<ngscope_config_t> ngscope_config, srsran_ue_sync_t* ue_sync, srsran_cell_t cell, cf_t** cur_buffer);

#ifdef __cplusplus
}
#endif

#endif