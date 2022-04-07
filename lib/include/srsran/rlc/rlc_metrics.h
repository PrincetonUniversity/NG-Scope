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

#ifndef SRSRAN_RLC_METRICS_H
#define SRSRAN_RLC_METRICS_H

#include "srsran/common/common.h"
#include <iostream>

namespace srsran {

typedef struct {
  // SDU metrics
  uint32_t num_tx_sdus;
  uint32_t num_rx_sdus;
  uint64_t num_tx_sdu_bytes;
  uint64_t num_rx_sdu_bytes;
  uint32_t num_lost_sdus; //< Count dropped SDUs at Tx due to bearer inactivity or empty buffer
  uint64_t rx_latency_ms; //< Average time in ms from first RLC segment to full SDU

  // PDU metrics
  uint32_t num_tx_pdus;
  uint32_t num_rx_pdus;
  uint64_t num_tx_pdu_bytes;
  uint64_t num_rx_pdu_bytes;
  uint32_t num_lost_pdus; //< Lost PDUs registered at Rx

  // misc metrics
  uint32_t rx_buffered_bytes; //< sum of payload of PDUs buffered in rx_window
} rlc_bearer_metrics_t;

typedef struct {
  rlc_bearer_metrics_t bearer[SRSRAN_N_RADIO_BEARERS];
  rlc_bearer_metrics_t mrb_bearer[SRSRAN_N_MCH_LCIDS];
} rlc_metrics_t;

} // namespace srsran

#endif // SRSRAN_RLC_METRICS_H
