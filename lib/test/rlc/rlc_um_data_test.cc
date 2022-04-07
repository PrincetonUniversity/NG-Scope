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

#include "srsran/common/test_common.h"
#include "srsran/rlc/rlc_um_lte.h"
#include <iostream>

// Fixed header only
uint8_t  pdu1[]   = {0x18, 0xE2};
uint32_t PDU1_LEN = 2;

// Fixed + 1 LI field (value 104)
uint8_t  pdu2[]   = {0x1C, 0xE1, 0x06, 0x80};
uint32_t PDU2_LEN = 4;

int main(int argc, char** argv)
{
  srslog::init();

  srsran::rlc_umd_pdu_header_t h;
  srsran::byte_buffer_t        b1, b2;

  memcpy(b1.msg, &pdu1[0], PDU1_LEN);
  b1.N_bytes = PDU1_LEN;
  rlc_um_read_data_pdu_header(&b1, srsran::rlc_umd_sn_size_t::size10bits, &h);
  TESTASSERT(0x03 == h.fi);
  TESTASSERT(0 == h.N_li);
  TESTASSERT(226 == h.sn);
  rlc_um_write_data_pdu_header(&h, &b2);
  TESTASSERT(b2.N_bytes == PDU1_LEN);
  for (uint32_t i = 0; i < b2.N_bytes; i++)
    TESTASSERT(b2.msg[i] == b1.msg[i]);

  b1.clear();
  b2.clear();
  memset(&h, 0, sizeof(srsran::rlc_umd_pdu_header_t));

  memcpy(b1.msg, &pdu2[0], PDU2_LEN);
  b1.N_bytes = PDU2_LEN;
  rlc_um_read_data_pdu_header(&b1, srsran::rlc_umd_sn_size_t::size10bits, &h);
  TESTASSERT(0x03 == h.fi);
  TESTASSERT(225 == h.sn);
  TESTASSERT(1 == h.N_li);
  TESTASSERT(104 == h.li[0]);
  rlc_um_write_data_pdu_header(&h, &b2);
  TESTASSERT(b2.N_bytes == PDU2_LEN);
  for (uint32_t i = 0; i < b2.N_bytes; i++)
    TESTASSERT(b2.msg[i] == b1.msg[i]);
}
