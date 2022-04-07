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

#ifndef SRSRAN_DEMUX_NR_H
#define SRSRAN_DEMUX_NR_H

#include "mac_nr_interfaces.h"
#include "srsran/common/block_queue.h"
#include "srsran/interfaces/ue_rlc_interfaces.h"

namespace srsue {

/**
 * @brief Logical Channel Demultiplexing and MAC CE dissassemble according to TS 38.321
 *
 * Currently only SDU handling for SCH PDU processing is implemented.
 * Downlink CE are parsed but not handled.
 *
 * PDUs can be pushed by multiple HARQ processes in parallel.
 * Handling of the PDUs is done from Stack thread which reads the enqueued PDUs
 * from the thread-safe queue.
 */
class demux_nr : public demux_interface_harq_nr
{
public:
  demux_nr(srslog::basic_logger& logger_);
  ~demux_nr();

  int32_t init(rlc_interface_mac* rlc_);

  void process_pdus(); /// Called by MAC to process received PDUs

  // HARQ interface
  void push_pdu(srsran::unique_byte_buffer_t pdu, uint32_t tti);

private:
  // internal helpers
  void handle_pdu(srsran::unique_byte_buffer_t pdu);

  srslog::basic_logger& logger;
  rlc_interface_mac*    rlc = nullptr;

  ///< currently only DCH PDUs supported (add BCH, PCH, etc)
  srsran::block_queue<srsran::unique_byte_buffer_t> pdu_queue;

  srsran::mac_sch_pdu_nr rx_pdu;
};

} // namespace srsue

#endif // SRSRAN_DEMUX_NR_H
