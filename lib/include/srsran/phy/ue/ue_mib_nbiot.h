/**
 * Copyright 2013-2021 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 *
 * srsRAN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsRAN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#ifndef SRSRAN_UE_MIB_NBIOT_H
#define SRSRAN_UE_MIB_NBIOT_H

#include "srsran/config.h"
#include "srsran/phy/ch_estimation/chest_dl_nbiot.h"
#include "srsran/phy/dft/ofdm.h"
#include "srsran/phy/phch/npbch.h"
#include "srsran/phy/sync/cfo.h"
#include "srsran/phy/ue/ue_sync_nbiot.h"

#define SRSRAN_UE_MIB_NBIOT_NOF_PRB 6
#define SRSRAN_UE_MIB_NBIOT_FOUND 1
#define SRSRAN_UE_MIB_NBIOT_NOTFOUND 0

/**
 * \brief This object decodes the MIB-NB from the NPBCH of an NB-IoT LTE signal.
 *
 * The function srsran_ue_mib_nbiot_decode() shall be called multiple
 * times, each passing a number of samples multiple of 19200,
 * sampled at 1.92 MHz (that is, 10 ms of samples).
 *
 * The function uses the sync_t object to find the NPSS sequence and
 * decode the NPBCH to obtain the MIB.
 *
 * The function returns 0 until the MIB is decoded.
 */

typedef struct SRSRAN_API {
  srsran_sync_nbiot_t sfind;

  cf_t* sf_symbols;
  cf_t* ce[SRSRAN_MAX_PORTS];

  srsran_ofdm_t           fft;
  srsran_chest_dl_nbiot_t chest;
  srsran_npbch_t          npbch;

  uint8_t  last_bch_payload[SRSRAN_MIB_NB_LEN];
  uint32_t nof_tx_ports;
  uint32_t nof_rx_antennas;
  uint32_t sfn_offset;

  uint32_t frame_cnt;
} srsran_ue_mib_nbiot_t;

SRSRAN_API int srsran_ue_mib_nbiot_init(srsran_ue_mib_nbiot_t* q, cf_t** in_buffer, uint32_t max_prb);

SRSRAN_API int srsran_ue_mib_nbiot_set_cell(srsran_ue_mib_nbiot_t* q, srsran_nbiot_cell_t cell);

SRSRAN_API void srsran_ue_mib_nbiot_free(srsran_ue_mib_nbiot_t* q);

SRSRAN_API void srsran_ue_mib_nbiot_reset(srsran_ue_mib_nbiot_t* q);

SRSRAN_API int srsran_ue_mib_nbiot_decode(srsran_ue_mib_nbiot_t* q,
                                          cf_t*                  input,
                                          uint8_t*               bch_payload,
                                          uint32_t*              nof_tx_ports,
                                          int*                   sfn_offset);

/* This interface uses ue_mib and ue_sync to first get synchronized subframes
 * and then decode MIB
 *
 * This object calls the pbch object with nof_ports=0 for blind nof_ports determination
 */
typedef struct {
  srsran_ue_mib_nbiot_t  ue_mib;
  srsran_nbiot_ue_sync_t ue_sync;
  cf_t*                  sf_buffer[SRSRAN_MAX_PORTS];
  uint32_t               nof_rx_antennas;
} srsran_ue_mib_sync_nbiot_t;

SRSRAN_API int
srsran_ue_mib_sync_nbiot_init_multi(srsran_ue_mib_sync_nbiot_t* q,
                                    int(recv_callback)(void*, cf_t* [SRSRAN_MAX_PORTS], uint32_t, srsran_timestamp_t*),
                                    uint32_t nof_rx_antennas,
                                    void*    stream_handler);

SRSRAN_API void srsran_ue_mib_sync_nbiot_free(srsran_ue_mib_sync_nbiot_t* q);

SRSRAN_API int srsran_ue_mib_sync_nbiot_set_cell(srsran_ue_mib_sync_nbiot_t* q, srsran_nbiot_cell_t cell);

SRSRAN_API void srsran_ue_mib_sync_nbiot_reset(srsran_ue_mib_sync_nbiot_t* q);

SRSRAN_API int srsran_ue_mib_sync_nbiot_decode(srsran_ue_mib_sync_nbiot_t* q,
                                               uint32_t                    max_frames_timeout,
                                               uint8_t*                    bch_payload,
                                               uint32_t*                   nof_tx_ports,
                                               int*                        sfn_offset);

#endif // SRSRAN_UE_MIB_NBIOT_H
