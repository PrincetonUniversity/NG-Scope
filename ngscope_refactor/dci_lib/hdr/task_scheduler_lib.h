#ifndef NGSCOPE_TASK_SCHE_LIB_H
#define NGSCOPE_TASK_SCHE_LIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ngscope_def.h"
#include "parse_args.h"
#include "radio.h"
#include "srsran/srsran.h"

// Init the task scheduler
int task_scheduler_init(prog_args_t       prog_args,
                        srsran_rf_t*      rf,
                        srsran_cell_t*    cell,
                        srsran_ue_sync_t* ue_sync,
                        srsran_ue_mib_t*  ue_mib,
                        srsran_ue_dl_t*   ue_dl,
                        uint32_t          rf_nof_rx_ant,
                        cf_t*             sync_buffer[SRSRAN_MAX_PORTS]);

int task_init_decode_config(srsran_dl_sf_cfg_t*     dl_sf,
                            srsran_ue_dl_cfg_t*     ue_dl_cfg,
                            srsran_pdsch_cfg_t*     pdsch_cfg,
                            srsran_cell_t*          cell,
                            prog_args_t*            prog_args,
                            srsran_softbuffer_rx_t* rx_softbuffers);

int task_ue_mib_decode_sfn(srsran_ue_mib_t* ue_mib, srsran_cell_t* cell, uint32_t* sfn, bool decode_pdcch);

#ifdef __cplusplus
}
#endif

#endif
