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

#include "rlc_test_common.h"
#include "srsran/common/test_common.h"
#include "srsran/config.h"
#include "srsran/interfaces/ue_pdcp_interfaces.h"
#include "srsran/rlc/rlc.h"
#include "srsran/rlc/rlc_um_nr.h"

#include <array>
#include <iostream>
#include <vector>

#define PCAP 0
#define PCAP_CRNTI (0x1001)
#define PCAP_TTI (666)

using namespace srsran;

#if PCAP
#include "srsran/common/mac_pcap.h"
#include "srsran/mac/mac_sch_pdu_nr.h"
static std::unique_ptr<srsran::mac_pcap> pcap_handle = nullptr;
#endif

int write_pdu_to_pcap(const uint32_t lcid, const uint8_t* payload, const uint32_t len)
{
#if PCAP
  if (pcap_handle) {
    byte_buffer_t          tx_buffer;
    srsran::mac_sch_pdu_nr tx_pdu;
    tx_pdu.init_tx(&tx_buffer, len + 10);
    tx_pdu.add_sdu(lcid, payload, len);
    tx_pdu.pack();
    pcap_handle->write_dl_crnti_nr(tx_buffer.msg, tx_buffer.N_bytes, PCAP_CRNTI, true, PCAP_TTI);
    return SRSRAN_SUCCESS;
  }
#endif
  return SRSRAN_ERROR;
}

template <std::size_t N>
srsran::byte_buffer_t make_pdu_and_log(const std::array<uint8_t, N>& tv)
{
  srsran::byte_buffer_t pdu;
  memcpy(pdu.msg, tv.data(), tv.size());
  pdu.N_bytes = tv.size();
  write_pdu_to_pcap(4, tv.data(), tv.size());
  return pdu;
}

// Helper class to create two pre-configured RLC instances
class rlc_um_nr_test_context1
{
public:
  rlc_um_nr_test_context1() :
    logger1(srslog::fetch_basic_logger("RLC_UM_1", false)),
    logger2(srslog::fetch_basic_logger("RLC_UM_2", false)),
    timers(16),
    rlc1(logger1, 3, &tester, &tester, &timers),
    rlc2(logger2, 3, &tester, &tester, &timers)
  {
    // setup logging
    logger1.set_level(srslog::basic_levels::debug);
    logger1.set_hex_dump_max_size(-1);
    logger2.set_level(srslog::basic_levels::debug);
    logger2.set_hex_dump_max_size(-1);

    // configure RLC entities
    rlc_config_t cnfg = rlc_config_t::default_rlc_um_nr_config(12);
    if (rlc1.configure(cnfg) != true) {
      fprintf(stderr, "Couldn't configure RLC1 object\n");
    }
    if (rlc2.configure(cnfg) != true) {
      fprintf(stderr, "Couldn't configure RLC2 object\n");
    }

    tester.set_expected_sdu_len(1);
  }

  srslog::basic_logger& logger1;
  srslog::basic_logger& logger2;
  srsran::timer_handler timers;
  rlc_um_tester         tester;
  rlc_um_nr             rlc1, rlc2;
};

// Basic test to write UM PDU with 6 bit SN (full SDUs are transmitted in each PDU)
int rlc_um_nr_test1()
{
  rlc_um_nr_test_context1 ctxt;

  const uint32_t num_sdus = 5, num_pdus = 5;

  // Push 5 SDUs into RLC1
  byte_buffer_pool*    pool = byte_buffer_pool::get_instance();
  unique_byte_buffer_t sdu_bufs[num_sdus];
  for (uint32_t i = 0; i < num_sdus; i++) {
    sdu_bufs[i]          = srsran::make_byte_buffer();
    *sdu_bufs[i]->msg    = i; // Write the index into the buffer
    sdu_bufs[i]->N_bytes = 1; // Give each buffer a size of 1 byte
    ctxt.rlc1.write_sdu(std::move(sdu_bufs[i]));
  }

  TESTASSERT(14 == ctxt.rlc1.get_buffer_state());

  // Read 5 PDUs from RLC1 (1 byte each)
  unique_byte_buffer_t pdu_bufs[num_pdus];
  for (uint32_t i = 0; i < num_pdus; i++) {
    pdu_bufs[i]          = srsran::make_byte_buffer();
    int len              = ctxt.rlc1.read_pdu(pdu_bufs[i]->msg, 4); // 3 bytes for header + payload
    pdu_bufs[i]->N_bytes = len;

    // write PCAP
    write_pdu_to_pcap(4, pdu_bufs[i]->msg, pdu_bufs[i]->N_bytes);
  }

  TESTASSERT(0 == ctxt.rlc1.get_buffer_state());

  // Write 5 PDUs into RLC2
  for (uint32_t i = 0; i < num_pdus; i++) {
    ctxt.rlc2.write_pdu(pdu_bufs[i]->msg, pdu_bufs[i]->N_bytes);
  }

  TESTASSERT(0 == ctxt.rlc2.get_buffer_state());

  TESTASSERT(num_sdus == ctxt.tester.get_num_sdus());
  for (uint32_t i = 0; i < ctxt.tester.sdus.size(); i++) {
    TESTASSERT(ctxt.tester.sdus.at(i)->N_bytes == 1);
    TESTASSERT(*(ctxt.tester.sdus[i]->msg) == i);
  }

  return SRSRAN_SUCCESS;
}

// Basic test for SDU segmentation
int rlc_um_nr_test2(bool reverse_rx = false)
{
  rlc_um_nr_test_context1 ctxt;

  const uint32_t num_sdus = 1;
  const uint32_t sdu_size = 100;

  ctxt.tester.set_expected_sdu_len(sdu_size);

  // Push SDUs into RLC1
  byte_buffer_pool*    pool = byte_buffer_pool::get_instance();
  unique_byte_buffer_t sdu_bufs[num_sdus];
  for (uint32_t i = 0; i < num_sdus; i++) {
    sdu_bufs[i] = srsran::make_byte_buffer();
    // Write the index into the buffer
    for (uint32_t k = 0; k < sdu_size; ++k) {
      sdu_bufs[i]->msg[k] = i;
    }
    sdu_bufs[i]->N_bytes = sdu_size;
    ctxt.rlc1.write_sdu(std::move(sdu_bufs[i]));
  }

  // TODO: check buffer state calculation
  TESTASSERT(103 == ctxt.rlc1.get_buffer_state());

  // Read PDUs from RLC1 with grant of 25 Bytes each
  const uint32_t       max_num_pdus = 10;
  uint32_t             num_pdus     = 0;
  unique_byte_buffer_t pdu_bufs[max_num_pdus];

  while (ctxt.rlc1.get_buffer_state() != 0 && num_pdus < max_num_pdus) {
    pdu_bufs[num_pdus]          = srsran::make_byte_buffer();
    int len                     = ctxt.rlc1.read_pdu(pdu_bufs[num_pdus]->msg, 25); // 3 bytes for header + payload
    pdu_bufs[num_pdus]->N_bytes = len;

    // write PCAP
    write_pdu_to_pcap(4, pdu_bufs[num_pdus]->msg, pdu_bufs[num_pdus]->N_bytes);

    num_pdus++;
  }

  TESTASSERT(0 == ctxt.rlc1.get_buffer_state());

  // Write PDUs into RLC2
  if (reverse_rx) {
    // receive PDUs in reverse order
    for (uint32_t i = num_pdus; i > 0; i--) {
      ctxt.rlc2.write_pdu(pdu_bufs[i - 1]->msg, pdu_bufs[i - 1]->N_bytes);
    }
  } else {
    // receive PDUs in order
    for (uint32_t i = 0; i < num_pdus; i++) {
      ctxt.rlc2.write_pdu(pdu_bufs[i]->msg, pdu_bufs[i]->N_bytes);
    }
  }

  TESTASSERT(0 == ctxt.rlc2.get_buffer_state());

  TESTASSERT(num_sdus == ctxt.tester.get_num_sdus());
  for (uint32_t i = 0; i < ctxt.tester.sdus.size(); i++) {
    TESTASSERT(ctxt.tester.sdus.at(i)->N_bytes == sdu_size);
    TESTASSERT(*(ctxt.tester.sdus[i]->msg) == i);
  }

  return SRSRAN_SUCCESS;
}

// Test reception of segmented RLC PDUs (two different SDUs with same PDU segmentation)
int rlc_um_nr_test4()
{
  rlc_um_nr_test_context1 ctxt;

  const uint32_t num_sdus = 2;
  const uint32_t sdu_size = 100;

  ctxt.tester.set_expected_sdu_len(sdu_size);

  // Push SDUs into RLC1
  byte_buffer_pool*    pool = byte_buffer_pool::get_instance();
  unique_byte_buffer_t sdu_bufs[num_sdus];
  for (uint32_t i = 0; i < num_sdus; i++) {
    sdu_bufs[i] = srsran::make_byte_buffer();
    // Write the index into the buffer
    for (uint32_t k = 0; k < sdu_size; ++k) {
      sdu_bufs[i]->msg[k] = i;
    }
    sdu_bufs[i]->N_bytes = sdu_size;
    ctxt.rlc1.write_sdu(std::move(sdu_bufs[i]));
  }

  // TODO: check buffer state calculation
  int bsr = ctxt.rlc1.get_buffer_state();
  TESTASSERT(bsr == 205);

  // Read PDUs from RLC1 with grant of 25 Bytes each
  const uint32_t       max_num_pdus = 20;
  uint32_t             num_pdus     = 0;
  unique_byte_buffer_t pdu_bufs[max_num_pdus];

  while (ctxt.rlc1.get_buffer_state() != 0 && num_pdus < max_num_pdus) {
    pdu_bufs[num_pdus]          = srsran::make_byte_buffer();
    int len                     = ctxt.rlc1.read_pdu(pdu_bufs[num_pdus]->msg, 25); // 3 bytes for header + payload
    pdu_bufs[num_pdus]->N_bytes = len;
    num_pdus++;
  }

  TESTASSERT(0 == ctxt.rlc1.get_buffer_state());

  // Write PDUs into RLC2 (except 1 and 6
  for (uint32_t i = 0; i < num_pdus; i++) {
    if (i != 1 && i != 6) {
      ctxt.rlc2.write_pdu(pdu_bufs[i]->msg, pdu_bufs[i]->N_bytes);
      write_pdu_to_pcap(4, pdu_bufs[i]->msg, pdu_bufs[i]->N_bytes);
    }
  }

  // write remaining two PDUs in reverse-order (so SN=1 is received first)
  ctxt.rlc2.write_pdu(pdu_bufs[6]->msg, pdu_bufs[6]->N_bytes);
  write_pdu_to_pcap(4, pdu_bufs[6]->msg, pdu_bufs[6]->N_bytes);

  ctxt.rlc2.write_pdu(pdu_bufs[1]->msg, pdu_bufs[1]->N_bytes);
  write_pdu_to_pcap(4, pdu_bufs[1]->msg, pdu_bufs[1]->N_bytes);

  TESTASSERT(0 == ctxt.rlc2.get_buffer_state());

  TESTASSERT(num_sdus == ctxt.tester.get_num_sdus());
  for (uint32_t i = 0; i < ctxt.tester.sdus.size(); i++) {
    TESTASSERT(ctxt.tester.sdus.at(i)->N_bytes == sdu_size);

    // common tester makes sure the all bytes within the SDU are the same, but it doesn't verify the SDUs are the ones
    // transmitted, so check this here
    if (i == 0) {
      // first SDU is SN=1
      TESTASSERT(*(ctxt.tester.sdus.at(i)->msg) == 0x01);
    } else {
      // second SDU is SN=0
      TESTASSERT(*(ctxt.tester.sdus.at(i)->msg) == 0x00);
    }
  }

  return SRSRAN_SUCCESS;
}

// Handling of re-transmitted segments (e.g. after PHY retransmission)
int rlc_um_nr_test5(const uint32_t last_sn)
{
  rlc_um_nr_test_context1 ctxt;

  const uint32_t num_sdus = 1;
  const uint32_t sdu_size = 100;

  ctxt.tester.set_expected_sdu_len(sdu_size);

  // Push SDUs into RLC1
  byte_buffer_pool*    pool = byte_buffer_pool::get_instance();
  unique_byte_buffer_t sdu_bufs[num_sdus];
  for (uint32_t i = 0; i < num_sdus; i++) {
    sdu_bufs[i] = srsran::make_byte_buffer();
    // Write the index into the buffer
    for (uint32_t k = 0; k < sdu_size; ++k) {
      sdu_bufs[i]->msg[k] = i;
    }
    sdu_bufs[i]->N_bytes = sdu_size;
    ctxt.rlc1.write_sdu(std::move(sdu_bufs[i]));
  }

  // TODO: check buffer state calculation
  TESTASSERT(103 == ctxt.rlc1.get_buffer_state());

  // Read PDUs from RLC1 with grant of 25 Bytes each
  const uint32_t       max_num_pdus = 10;
  uint32_t             num_pdus     = 0;
  unique_byte_buffer_t pdu_bufs[max_num_pdus];

  while (ctxt.rlc1.get_buffer_state() != 0 && num_pdus < max_num_pdus) {
    pdu_bufs[num_pdus]          = srsran::make_byte_buffer();
    int len                     = ctxt.rlc1.read_pdu(pdu_bufs[num_pdus]->msg, 25); // 3 bytes for header + payload
    pdu_bufs[num_pdus]->N_bytes = len;

    // write PCAP
    write_pdu_to_pcap(4, pdu_bufs[num_pdus]->msg, pdu_bufs[num_pdus]->N_bytes);

    num_pdus++;
  }

  TESTASSERT(0 == ctxt.rlc1.get_buffer_state());

  // Write alls PDUs twice into RLC2 (except third)
  for (uint32_t k = 0; k < 2; k++) {
    for (uint32_t i = 0; i < num_pdus; i++) {
      if (i != last_sn) {
        ctxt.rlc2.write_pdu(pdu_bufs[i]->msg, pdu_bufs[i]->N_bytes);
      }
    }
  }

  // Write third PDU
  ctxt.rlc2.write_pdu(pdu_bufs[last_sn]->msg, pdu_bufs[last_sn]->N_bytes);

  TESTASSERT(0 == ctxt.rlc2.get_buffer_state());

  TESTASSERT(num_sdus == ctxt.tester.get_num_sdus());
  for (uint32_t i = 0; i < ctxt.tester.sdus.size(); i++) {
    TESTASSERT(ctxt.tester.sdus.at(i)->N_bytes == sdu_size);
    TESTASSERT(*(ctxt.tester.sdus[i]->msg) == i);
  }

  return SRSRAN_SUCCESS;
}

// Test of wrap-around of reassembly window
int rlc_um_nr_test6()
{
  rlc_um_nr_test_context1 ctxt;

  const uint32_t num_sdus = 64;
  const uint32_t sdu_size = 10;

  ctxt.tester.set_expected_sdu_len(sdu_size);

  // Push SDUs into RLC1
  byte_buffer_pool*    pool = byte_buffer_pool::get_instance();
  unique_byte_buffer_t sdu_bufs[num_sdus];
  for (uint32_t i = 0; i < num_sdus; i++) {
    sdu_bufs[i] = srsran::make_byte_buffer();
    // Write the index into the buffer
    for (uint32_t k = 0; k < sdu_size; ++k) {
      sdu_bufs[i]->msg[k] = i;
    }
    sdu_bufs[i]->N_bytes = sdu_size;
    ctxt.rlc1.write_sdu(std::move(sdu_bufs[i]));
  }

  // TODO: check buffer state calculation
  // TESTASSERT(103 == ctxt.rlc1.get_buffer_state());

  // Read PDUs from RLC1 with grant of 8 Bytes each
  const uint32_t       max_num_pdus = num_sdus * 2; // we need 2 PDUs for each SDU
  uint32_t             num_pdus     = 0;
  unique_byte_buffer_t pdu_bufs[max_num_pdus];

  while (ctxt.rlc1.get_buffer_state() != 0 && num_pdus < max_num_pdus) {
    pdu_bufs[num_pdus]          = srsran::make_byte_buffer();
    int len                     = ctxt.rlc1.read_pdu(pdu_bufs[num_pdus]->msg, 8); // 3 bytes for header + payload
    pdu_bufs[num_pdus]->N_bytes = len;

    // write PCAP
    write_pdu_to_pcap(4, pdu_bufs[num_pdus]->msg, pdu_bufs[num_pdus]->N_bytes);

    num_pdus++;
  }

  TESTASSERT(0 == ctxt.rlc1.get_buffer_state());

  // Write PDUs into RLC2
  for (uint32_t i = 0; i < num_pdus; i++) {
    ctxt.rlc2.write_pdu(pdu_bufs[i]->msg, pdu_bufs[i]->N_bytes);
  }

  TESTASSERT(0 == ctxt.rlc2.get_buffer_state());

  TESTASSERT(num_sdus == ctxt.tester.get_num_sdus());
  for (uint32_t i = 0; i < ctxt.tester.sdus.size(); i++) {
    TESTASSERT(ctxt.tester.sdus.at(i)->N_bytes == sdu_size);
    TESTASSERT(*(ctxt.tester.sdus[i]->msg) == i);
  }

  return SRSRAN_SUCCESS;
}

// Segment loss received too many new PDUs (lost PDU outside of reassembly window)
int rlc_um_nr_test7()
{
  rlc_um_nr_test_context1 ctxt;

  const uint32_t num_sdus = 64;
  const uint32_t sdu_size = 10;

  ctxt.tester.set_expected_sdu_len(sdu_size);

  // Push SDUs into RLC1
  byte_buffer_pool*    pool = byte_buffer_pool::get_instance();
  unique_byte_buffer_t sdu_bufs[num_sdus];
  for (uint32_t i = 0; i < num_sdus; i++) {
    sdu_bufs[i] = srsran::make_byte_buffer();
    // Write the index into the buffer
    for (uint32_t k = 0; k < sdu_size; ++k) {
      sdu_bufs[i]->msg[k] = i;
    }
    sdu_bufs[i]->N_bytes = sdu_size;
    ctxt.rlc1.write_sdu(std::move(sdu_bufs[i]));
  }

  // TODO: check buffer state calculation
  // TESTASSERT(103 == ctxt.rlc1.get_buffer_state());

  // Read PDUs from RLC1 with grant of 8 Bytes each
  const uint32_t       max_num_pdus = num_sdus * 2; // we need 2 PDUs for each SDU
  uint32_t             num_pdus     = 0;
  unique_byte_buffer_t pdu_bufs[max_num_pdus];

  while (ctxt.rlc1.get_buffer_state() != 0 && num_pdus < max_num_pdus) {
    pdu_bufs[num_pdus]          = srsran::make_byte_buffer();
    int len                     = ctxt.rlc1.read_pdu(pdu_bufs[num_pdus]->msg, 8); // 3 bytes for header + payload
    pdu_bufs[num_pdus]->N_bytes = len;

    // write PCAP
    write_pdu_to_pcap(4, pdu_bufs[num_pdus]->msg, pdu_bufs[num_pdus]->N_bytes);

    num_pdus++;
  }

  TESTASSERT(0 == ctxt.rlc1.get_buffer_state());

  // Write PDUs into RLC2 (except 11th)
  for (uint32_t i = 0; i < num_pdus; i++) {
    if (i != 10) {
      ctxt.rlc2.write_pdu(pdu_bufs[i]->msg, pdu_bufs[i]->N_bytes);
    }
  }

  TESTASSERT(0 == ctxt.rlc2.get_buffer_state());

  TESTASSERT(num_sdus - 1 == ctxt.tester.get_num_sdus());
  for (uint32_t i = 0; i < ctxt.tester.sdus.size(); i++) {
    TESTASSERT(ctxt.tester.sdus.at(i)->N_bytes == sdu_size);
  }

  // let t-reassembly expire
  while (ctxt.timers.nof_running_timers() != 0) {
    ctxt.timers.step_all();
  }

  rlc_bearer_metrics_t rlc2_metrics = ctxt.rlc2.get_metrics();
  TESTASSERT(rlc2_metrics.num_lost_pdus == 1);

  return SRSRAN_SUCCESS;
}

// Segment loss and expiry of reassembly timer
int rlc_um_nr_test8()
{
  rlc_um_nr_test_context1 ctxt;

  const uint32_t num_sdus = 10;
  const uint32_t sdu_size = 10;

  ctxt.tester.set_expected_sdu_len(sdu_size);

  // Push SDUs into RLC1
  byte_buffer_pool*    pool = byte_buffer_pool::get_instance();
  unique_byte_buffer_t sdu_bufs[num_sdus];
  for (uint32_t i = 0; i < num_sdus; i++) {
    sdu_bufs[i] = srsran::make_byte_buffer();
    // Write the index into the buffer
    for (uint32_t k = 0; k < sdu_size; ++k) {
      sdu_bufs[i]->msg[k] = i;
    }
    sdu_bufs[i]->N_bytes = sdu_size;
    ctxt.rlc1.write_sdu(std::move(sdu_bufs[i]));
  }

  // TODO: check buffer state calculation
  // TESTASSERT(103 == ctxt.rlc1.get_buffer_state());

  // Read PDUs from RLC1 with grant of 8 Bytes each
  const uint32_t       max_num_pdus = 20 * 2; // we need 2 PDUs for each SDU
  uint32_t             num_pdus     = 0;
  unique_byte_buffer_t pdu_bufs[max_num_pdus];

  while (ctxt.rlc1.get_buffer_state() != 0 && num_pdus < max_num_pdus) {
    pdu_bufs[num_pdus]          = srsran::make_byte_buffer();
    int len                     = ctxt.rlc1.read_pdu(pdu_bufs[num_pdus]->msg, 8); // 3 bytes for header + payload
    pdu_bufs[num_pdus]->N_bytes = len;

    // write PCAP
    write_pdu_to_pcap(4, pdu_bufs[num_pdus]->msg, pdu_bufs[num_pdus]->N_bytes);

    num_pdus++;
  }

  TESTASSERT(0 == ctxt.rlc1.get_buffer_state());

  // Write PDUs into RLC2 (except 2nd)
  for (uint32_t i = 0; i < num_pdus; i++) {
    if (i != 2) {
      ctxt.rlc2.write_pdu(pdu_bufs[i]->msg, pdu_bufs[i]->N_bytes);
    }
  }

  TESTASSERT(0 == ctxt.rlc2.get_buffer_state());

  // let t-reassembly expire
  while (ctxt.timers.nof_running_timers() != 0) {
    ctxt.timers.step_all();
  }

  TESTASSERT(num_sdus - 1 == ctxt.tester.get_num_sdus());
  for (uint32_t i = 0; i < ctxt.tester.sdus.size(); i++) {
    TESTASSERT(ctxt.tester.sdus.at(i)->N_bytes == sdu_size);
  }

  rlc_bearer_metrics_t rlc2_metrics = ctxt.rlc2.get_metrics();
  TESTASSERT(rlc2_metrics.num_lost_pdus == 1);

  return SRSRAN_SUCCESS;
}

// Similar to rlc_um_nr_test9() but out-of-order PDUs have SNs (from multiple SDUs)
int rlc_um_nr_test9()
{
  rlc_um_nr_test_context1 ctxt;

  const uint32_t num_sdus = 2;
  const uint32_t sdu_size = 20;

  ctxt.tester.set_expected_sdu_len(sdu_size);

  // Push SDUs into RLC1
  byte_buffer_pool*    pool = byte_buffer_pool::get_instance();
  unique_byte_buffer_t sdu_bufs[num_sdus];
  for (uint32_t i = 0; i < num_sdus; i++) {
    sdu_bufs[i] = srsran::make_byte_buffer();
    // Write the index into the buffer
    for (uint32_t k = 0; k < sdu_size; ++k) {
      sdu_bufs[i]->msg[k] = i;
    }
    sdu_bufs[i]->N_bytes = sdu_size;
    ctxt.rlc1.write_sdu(std::move(sdu_bufs[i]));
  }

  // Read PDUs from RLC1 with grant smaller than SDU size
  const uint32_t       max_num_pdus = 10;
  uint32_t             num_pdus     = 0;
  unique_byte_buffer_t pdu_bufs[max_num_pdus];

  while (ctxt.rlc1.get_buffer_state() != 0 && num_pdus < max_num_pdus) {
    pdu_bufs[num_pdus]          = srsran::make_byte_buffer();
    int len                     = ctxt.rlc1.read_pdu(pdu_bufs[num_pdus]->msg, 10); // 3 bytes for header + payload
    pdu_bufs[num_pdus]->N_bytes = len;

    // write PCAP
    write_pdu_to_pcap(4, pdu_bufs[num_pdus]->msg, pdu_bufs[num_pdus]->N_bytes);

    num_pdus++;
  }

  TESTASSERT(num_pdus == 6);

  // Write all PDUs such that the middle section of SN=0 is received after the start section of SN=1
  ctxt.rlc2.write_pdu(pdu_bufs[0]->msg, pdu_bufs[0]->N_bytes);
  // skip 2nd PDU which is the middle section of SN=0
  ctxt.rlc2.write_pdu(pdu_bufs[2]->msg, pdu_bufs[2]->N_bytes);
  ctxt.rlc2.write_pdu(pdu_bufs[3]->msg, pdu_bufs[3]->N_bytes);

  // now feed the middle part of SN=0
  ctxt.rlc2.write_pdu(pdu_bufs[1]->msg, pdu_bufs[1]->N_bytes);

  // and the rest of SN=1
  ctxt.rlc2.write_pdu(pdu_bufs[4]->msg, pdu_bufs[4]->N_bytes);
  ctxt.rlc2.write_pdu(pdu_bufs[5]->msg, pdu_bufs[5]->N_bytes);

  TESTASSERT(num_sdus == ctxt.tester.get_num_sdus());
  for (uint32_t i = 0; i < ctxt.tester.sdus.size(); i++) {
    TESTASSERT(ctxt.tester.sdus.at(i)->N_bytes == sdu_size);
    TESTASSERT(*(ctxt.tester.sdus[i]->msg) == i);
  }

  return SRSRAN_SUCCESS;
}

int main(int argc, char** argv)
{
#if PCAP
  pcap_handle = std::unique_ptr<srsran::mac_pcap>(new srsran::mac_pcap());
  pcap_handle->open("rlc_um_nr_test.pcap");
#endif

  srslog::init();

  if (rlc_um_nr_test1()) {
    fprintf(stderr, "rlc_um_nr_test1() failed.\n");
    return SRSRAN_ERROR;
  }

  if (rlc_um_nr_test2()) {
    fprintf(stderr, "rlc_um_nr_test2() failed.\n");
    return SRSRAN_ERROR;
  }

  // same like above but PDUs delivered in reverse order
  if (rlc_um_nr_test2(true)) {
    fprintf(stderr, "rlc_um_nr_test2(true) failed.\n");
    return SRSRAN_ERROR;
  }

  if (rlc_um_nr_test4()) {
    fprintf(stderr, "rlc_um_nr_test4() failed.\n");
    return SRSRAN_ERROR;
  }

  for (uint32_t i = 0; i < 5; ++i) {
    if (rlc_um_nr_test5(i)) {
      fprintf(stderr, "rlc_um_nr_test5() for i=%d failed.\n", i);
      return SRSRAN_ERROR;
    }
  }

  if (rlc_um_nr_test6()) {
    fprintf(stderr, "rlc_um_nr_test6() failed.\n");
    return SRSRAN_ERROR;
  }

  if (rlc_um_nr_test7()) {
    fprintf(stderr, "rlc_um_nr_test7() failed.\n");
    return SRSRAN_ERROR;
  }

  if (rlc_um_nr_test8()) {
    fprintf(stderr, "rlc_um_nr_test8() failed.\n");
    return SRSRAN_ERROR;
  }

// temporarily disabling
#if 0
  if (rlc_um_nr_test9()) {
    fprintf(stderr, "rlc_um_nr_test9() failed.\n");
    return SRSRAN_ERROR;
  }
#endif

#if PCAP
  pcap_handle->close();
#endif

  return SRSRAN_SUCCESS;
}
