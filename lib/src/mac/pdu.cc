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

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "srsran/common/standard_streams.h"
#include "srsran/mac/pdu.h"

extern "C" {
#include "srsran/phy/utils/bit.h"
#include "srsran/phy/utils/vector.h"
}

// Table 6.1.3.1-1 Buffer size levels for BSR
static uint32_t btable[64] = {
    0,     1,     10,    12,    14,    17,    19,    22,    26,    31,    36,    42,    49,    57,     67,     78,
    91,    107,   125,   146,   171,   200,   234,   274,   321,   376,   440,   515,   603,   706,    826,    967,
    1132,  1326,  1552,  1817,  2127,  2490,  2915,  3413,  3995,  4667,  5476,  6411,  7505,  8787,   10287,  12043,
    14099, 16507, 19325, 22624, 26487, 31009, 36304, 42502, 49759, 58255, 68201, 79846, 93479, 109439, 128125, 150000};

namespace srsran {

/*************************
 *       DL-SCH LCID
 *************************/

const char* to_string_short(dl_sch_lcid v)
{
  switch (v) {
    case dl_sch_lcid::CCCH:
      return "CCCH";
    case dl_sch_lcid::SCELL_ACTIVATION_4_OCTET:
      return "SCellAct4";
    case dl_sch_lcid::SCELL_ACTIVATION:
      return "SCellAct";
    case dl_sch_lcid::CON_RES_ID:
      return "ConResId";
    case dl_sch_lcid::TA_CMD:
      return "TA_CMD";
    case dl_sch_lcid::DRX_CMD:
      return "DRX_CMD";
    case dl_sch_lcid::PADDING:
      return "PAD";
    default:
      return "Unrecognized LCID";
  }
}

const char* to_string(dl_sch_lcid v)
{
  switch (v) {
    case dl_sch_lcid::CCCH:
      return "CCCH";
    case dl_sch_lcid::SCELL_ACTIVATION_4_OCTET:
      return "Activation/Deactivation (4 octets)";
    case dl_sch_lcid::SCELL_ACTIVATION:
      return "Activation/Deactivation (1 octet)";
    case dl_sch_lcid::CON_RES_ID:
      return "UE Contention Resolution Identity";
    case dl_sch_lcid::TA_CMD:
      return "Timing Advance Command";
    case dl_sch_lcid::DRX_CMD:
      return "DRX Command";
    case dl_sch_lcid::PADDING:
      return "Padding";
    default:
      return "Unrecognized LCID";
  }
}

uint32_t ce_size(dl_sch_lcid v)
{
  switch (v) {
    case dl_sch_lcid::SCELL_ACTIVATION_4_OCTET:
      return 4;
    case dl_sch_lcid::SCELL_ACTIVATION:
      return 1;
    case dl_sch_lcid::CON_RES_ID:
      return 6;
    case dl_sch_lcid::TA_CMD:
      return 1;
    case dl_sch_lcid::DRX_CMD:
      return 0;
    default:
      return 0;
  }
}

uint32_t ce_subheader_size(dl_sch_lcid v)
{
  return CE_SUBHEADER_LEN;
}

uint32_t ce_total_size(dl_sch_lcid v)
{
  return ce_subheader_size(v) + ce_size(v);
}

/*************************
 *       UL-SCH LCID
 *************************/

const char* to_string(ul_sch_lcid v)
{
  switch (v) {
    case ul_sch_lcid::CCCH:
      return "CCCH";
    case ul_sch_lcid::PHR_REPORT_EXT:
      return "Extended Power Headroom Report";
    case ul_sch_lcid::PHR_REPORT:
      return "Power Headroom Report";
    case ul_sch_lcid::CRNTI:
      return "C-RNTI";
    case ul_sch_lcid::TRUNC_BSR:
      return "Truncated BSR";
    case ul_sch_lcid::SHORT_BSR:
      return "Short BSR";
    case ul_sch_lcid::LONG_BSR:
      return "Long BSR";
    case ul_sch_lcid::PADDING:
      return "Padding";
    default:
      return "Unrecognized LCID";
  }
}

uint32_t ce_size(ul_sch_lcid v)
{
  switch (v) {
    case ul_sch_lcid::PHR_REPORT:
      return 1;
    case ul_sch_lcid::CRNTI:
      return 2;
    case ul_sch_lcid::TRUNC_BSR:
      return 1;
    case ul_sch_lcid::SHORT_BSR:
      return 1;
    case ul_sch_lcid::LONG_BSR:
      return 3;
    default:
      return 0;
  }
}

const char* to_string(mch_lcid v)
{
  switch (v) {
    case mch_lcid::MCCH:
      return "MCCH";
    case mch_lcid::MCH_SCHED_INFO:
      return "MCH Scheduling Information";
    case mch_lcid::PADDING:
      return "Padding";
    default:
      return "Unrecognized MCH LCID";
  }
}

const char* lcid_t::to_string() const
{
  switch (type) {
    case ch_type::dl_sch:
      return srsran::to_string(dl_sch);
    case ch_type::ul_sch:
      return srsran::to_string(ul_sch);
    case ch_type::mch:
      return srsran::to_string(mch);
    default:
      return "unrecognized lcid type\n";
  }
}

bool lcid_t::is_sdu() const
{
  switch (type) {
    case ch_type::dl_sch:
      return srsran::is_sdu(dl_sch);
    case ch_type::ul_sch:
      return srsran::is_sdu(ul_sch);
    case ch_type::mch:
      return srsran::is_sdu(mch);
    default:
      return false;
  }
}

/*************************
 *       SCH PDU
 *************************/

void sch_pdu::to_string(fmt::memory_buffer& buffer)
{
  fmt::format_to(buffer, "{}", is_ul() ? "UL" : "DL");
  pdu::to_string(buffer);
}

void sch_pdu::parse_packet(uint8_t* ptr)
{
  pdu::parse_packet(ptr);

  // Correct size for last SDU
  if (nof_subheaders > 0) {
    uint32_t read_len = 0;
    for (int i = 0; i < nof_subheaders - 1; i++) {
      read_len += subheaders[i].size_plus_header();
    }

    int n_sub = pdu_len - read_len - 1;

    if (n_sub >= 0) {
      subheaders[nof_subheaders - 1].set_payload_size(n_sub);
    } else {
      logger.warning(ptr, pdu_len, "Corrupted MAC PDU (read_len=%d, pdu_len=%d)", read_len, pdu_len);

      // reset PDU
      init_(buffer_tx, pdu_len, pdu_is_ul);
    }
  }
}

uint8_t* sch_pdu::write_packet()
{
  return write_packet(srslog::fetch_basic_logger("MAC"));
}

/* Writes the MAC PDU in the packet, including the MAC headers and CE payload. Section 6.1.2 */
uint8_t* sch_pdu::write_packet(srslog::basic_logger& log)
{
  // set padding to remaining length in PDU
  uint32_t num_padding = rem_len;

  // Determine if we are transmitting CEs only
  bool ce_only = last_sdu_idx < 0 ? true : false;

  // Determine if we will need multi-byte padding or 1/2 bytes padding
  bool     multibyte_padding = false;
  uint32_t onetwo_padding    = 0;
  if (num_padding > 2) {
    multibyte_padding = true;
    // Add 1 header for padding
    num_padding--;
    // Add the header for the last SDU
    if (!ce_only) {
      num_padding -= (subheaders[last_sdu_idx].get_header_size(false) - 1); // Because we were assuming it was the one
    }
  } else if (num_padding > 0) {
    onetwo_padding = num_padding;
    num_padding    = 0;
  }

  // Determine the header size and CE payload size
  uint32_t header_sz     = 0;
  uint32_t ce_payload_sz = 0;
  for (int i = 0; i < nof_subheaders; i++) {
    header_sz += subheaders[i].get_header_size(!multibyte_padding && i == last_sdu_idx);
    if (!subheaders[i].is_sdu()) {
      ce_payload_sz += subheaders[i].get_payload_size();
    }
  }
  if (multibyte_padding) {
    header_sz += 1;
  } else if (onetwo_padding) {
    header_sz += onetwo_padding;
  }

  uint32_t total_header_size = header_sz + ce_payload_sz;

  // make sure there is enough room for header
  if (buffer_tx->get_headroom() < total_header_size) {
    log.error("Not enough headroom for MAC header (%d < %d).", buffer_tx->get_headroom(), total_header_size);
    return nullptr;
  }

  // Rewind PDU pointer and leave space for entire header
  buffer_tx->msg -= total_header_size;
  buffer_tx->N_bytes += total_header_size;

  // Start writing header and CE payload before the start of the SDU payload
  uint8_t* ptr = buffer_tx->msg;

  // Add single/two byte padding first
  for (uint32_t i = 0; i < onetwo_padding; i++) {
    sch_subh padding;
    padding.set_padding();
    padding.write_subheader(&ptr, pdu_len > onetwo_padding ? false : true);
  }

  // Then write subheaders for MAC CE
  for (int i = 0; i < nof_subheaders; i++) {
    if (!subheaders[i].is_sdu()) {
      subheaders[i].write_subheader(&ptr, ce_only && !multibyte_padding && i == (nof_subheaders - 1));
    }
  }

  // Then for SDUs
  if (!ce_only) {
    for (int i = 0; i < nof_subheaders; i++) {
      if (subheaders[i].is_sdu()) {
        subheaders[i].write_subheader(&ptr, !multibyte_padding && i == last_sdu_idx);
      }
    }
  }
  // and finally add multi-byte padding
  if (multibyte_padding) {
    sch_subh padding_multi;
    padding_multi.set_padding(num_padding);
    padding_multi.write_subheader(&ptr, true);
  }

  // Write CE payloads (SDU payloads already in the buffer)
  for (int i = 0; i < nof_subheaders; i++) {
    if (!subheaders[i].is_sdu()) {
      subheaders[i].write_payload(&ptr);
    }
  }

  if (buffer_tx->get_tailroom() < num_padding) {
    log.error("Not enough tailroom for MAC padding (%d < %d).", buffer_tx->get_tailroom(), num_padding);
    return nullptr;
  }

  // Set padding to zeros (if any)
  if (num_padding > 0) {
    bzero(&buffer_tx->msg[buffer_tx->N_bytes], num_padding * sizeof(uint8_t));
    buffer_tx->N_bytes += num_padding;
  }

  // Print warning if we have padding only
  if (nof_subheaders <= 0 && nof_subheaders < (int)max_subheaders) {
    log.debug("Writing MAC PDU with padding only (%d B)", pdu_len);
  }

  log.debug("Wrote PDU: pdu_len=%d, header_and_ce=%d (%d+%d), nof_subh=%d, last_sdu=%d, onepad=%d, multi=%d",
            pdu_len,
            header_sz + ce_payload_sz,
            header_sz,
            ce_payload_sz,
            nof_subheaders,
            last_sdu_idx,
            onetwo_padding,
            num_padding);

  if (buffer_tx->N_bytes != pdu_len) {
    srsran::console("------------------------------\n");
    srsran::console("Wrote PDU: pdu_len=%d, expected_pdu_len=%d, header_and_ce=%d (%d+%d), nof_subh=%d, last_sdu=%d, "
                    "onepad=%d, multi=%d\n",
                    buffer_tx->N_bytes,
                    pdu_len,
                    header_sz + ce_payload_sz,
                    header_sz,
                    ce_payload_sz,
                    nof_subheaders,
                    last_sdu_idx,
                    onetwo_padding,
                    num_padding);
    srsran::console("------------------------------\n");

    log.error(
        "Wrote PDU: pdu_len=%d, expected_pdu_len=%d, header_and_ce=%d (%d+%d), nof_subh=%d, last_sdu=%d, onepad=%d, "
        "multi=%d",
        buffer_tx->N_bytes,
        pdu_len,
        header_sz + ce_payload_sz,
        header_sz,
        ce_payload_sz,
        nof_subheaders,
        last_sdu_idx,
        onetwo_padding,
        num_padding);

    for (int i = 0; i < nof_subheaders; i++) {
      log.error("SUBH %d is_sdu=%d, head_size=%d, payload=%d",
                i,
                subheaders[i].is_sdu(),
                subheaders[i].get_header_size(i == (nof_subheaders - 1)),
                subheaders[i].get_payload_size());
    }

    return nullptr;
  }

  return buffer_tx->msg;
}

int sch_pdu::rem_size()
{
  return rem_len;
}

int sch_pdu::get_pdu_len()
{
  return pdu_len;
}

uint32_t sch_pdu::size_header_sdu(uint32_t nbytes)
{
  if (nbytes == 0) {
    return 0;
  } else if (nbytes < 128) {
    return 2;
  } else {
    return 3;
  }
}

bool sch_pdu::has_space_ce(uint32_t nbytes, bool var_len)
{
  uint32_t head_len = var_len ? size_header_sdu(nbytes) : 1;
  if (rem_len >= nbytes + head_len) {
    return true;
  } else {
    return false;
  }
}

bool sch_pdu::update_space_ce(uint32_t nbytes, bool var_len)
{
  uint32_t head_len = var_len ? size_header_sdu(nbytes) : 1;
  if (has_space_ce(nbytes)) {
    rem_len -= nbytes + head_len;
    return true;
  } else {
    return false;
  }
}

bool sch_pdu::has_space_sdu(uint32_t nbytes)
{
  int s = get_sdu_space();

  if (s < 0) {
    return false;
  } else {
    return (uint32_t)s >= nbytes;
  }
}

bool sch_pdu::update_space_sdu(uint32_t nbytes)
{
  if (has_space_sdu(nbytes)) {
    if (last_sdu_idx < 0) {
      rem_len -= (nbytes + 1);
    } else {
      rem_len -= (nbytes + 1 + (size_header_sdu(subheaders[last_sdu_idx].get_payload_size()) - 1));
    }
    last_sdu_idx = cur_idx;
    return true;
  } else {
    return false;
  }
}

int sch_pdu::get_sdu_space()
{
  int32_t ret = 0;
  if (last_sdu_idx < 0) {
    ret = rem_len - 1;
  } else {
    ret = rem_len - (size_header_sdu(subheaders[last_sdu_idx].get_payload_size()) - 1) - 1;
  }
  ret = std::min(ret >= 0 ? ret : 0u, buffer_tx->get_tailroom());
  return ret;
}

void sch_subh::init()
{
  srsran_vec_u8_zero(w_payload_ce, 64);
  payload          = w_payload_ce;
  lcid             = 0;
  nof_bytes        = 0;
  nof_mch_sched_ce = 0;
  cur_mch_sched_ce = 0;
}

ul_sch_lcid sch_subh::ul_sch_ce_type()
{
  auto ret = static_cast<ul_sch_lcid>(lcid);
  return is_mac_ce(ret) ? ret : static_cast<ul_sch_lcid>(SDU);
}

dl_sch_lcid sch_subh::dl_sch_ce_type()
{
  auto ret = static_cast<dl_sch_lcid>(lcid);
  return is_mac_ce(ret) ? ret : static_cast<dl_sch_lcid>(SDU);
}

mch_lcid sch_subh::mch_ce_type()
{
  auto ret = static_cast<mch_lcid>(lcid);
  return is_mac_ce(ret) ? ret : static_cast<mch_lcid>(SDU);
}

void sch_subh::set_payload_size(uint32_t size)
{
  nof_bytes = size;
}

uint32_t sch_subh::size_plus_header()
{
  if (is_sdu() || is_var_len_ce()) {
    return sch_pdu::size_header_sdu(nof_bytes) + nof_bytes;
  }
  // All others are 1-byte headers
  return 1 + nof_bytes;
}

uint32_t sch_subh::sizeof_ce(uint32_t lcid, bool is_ul)
{
  if (type == SCH_SUBH_TYPE) {
    if (is_ul) {
      return ce_size(static_cast<ul_sch_lcid>(lcid));
    } else {
      return ce_size(static_cast<dl_sch_lcid>(lcid));
    }
  }
  if (type == MCH_SUBH_TYPE) {
    switch (static_cast<mch_lcid>(lcid)) {
      case mch_lcid::MCH_SCHED_INFO:
        return nof_mch_sched_ce * 2;
      case mch_lcid::PADDING:
        return 0;
      default:
        return 0;
    }
  }
  return 0;
}

bool sch_subh::is_sdu()
{
  return lcid_t{type == MCH_SUBH_TYPE ? lcid_t::ch_type::mch : lcid_t::ch_type::ul_sch, lcid}.is_sdu();
}

bool sch_subh::is_var_len_ce()
{
  return (mch_lcid::MCH_SCHED_INFO == mch_ce_type()) && (MCH_SUBH_TYPE == type);
}

uint16_t sch_subh::get_c_rnti()
{
  return (uint16_t)payload[0] << 8 | payload[1];
}

uint64_t sch_subh::get_con_res_id()
{
  return le64toh(((uint64_t)payload[5]) | (((uint64_t)payload[4]) << 8) | (((uint64_t)payload[3]) << 16) |
                 (((uint64_t)payload[2]) << 24) | (((uint64_t)payload[1]) << 32) | (((uint64_t)payload[0]) << 40));
}

float sch_subh::get_phr()
{
  return (float)(payload[0] & 0x3f) - 23;
}

uint32_t sch_subh::get_bsr(uint32_t buff_size_idx[4], uint32_t buff_size_bytes[4])
{
  uint32_t nonzero_lcg = 0;
  if (ul_sch_ce_type() == ul_sch_lcid::LONG_BSR) {
    buff_size_idx[0] = (payload[0] & 0xFC) >> 2;
    buff_size_idx[1] = (payload[0] & 0x03) << 4 | (payload[1] & 0xF0) >> 4;
    buff_size_idx[2] = (payload[1] & 0x0F) << 2 | (payload[2] & 0xC0) >> 6;
    buff_size_idx[3] = (payload[2] & 0x3F);
  } else {
    nonzero_lcg                    = (payload[0] & 0xc0) >> 6;
    buff_size_idx[nonzero_lcg % 4] = payload[0] & 0x3f;
  }
  for (int i = 0; i < 4; i++) {
    if (buff_size_idx[i] > 0) {
      if (buff_size_idx[i] < 63) {
        buff_size_bytes[i] = btable[1 + buff_size_idx[i]];
      } else {
        buff_size_bytes[i] = btable[63];
      }
    }
  }
  return nonzero_lcg;
}

bool sch_subh::get_next_mch_sched_info(uint8_t* lcid_, uint16_t* mtch_stop)
{
  uint16_t mtch_stop_ce;
  nof_mch_sched_ce = nof_bytes / 2;
  if (cur_mch_sched_ce < nof_mch_sched_ce) {
    *lcid_       = (payload[cur_mch_sched_ce * 2] & 0xF8) >> 3;
    mtch_stop_ce = ((uint16_t)(payload[cur_mch_sched_ce * 2] & 0x07)) << 8;
    mtch_stop_ce += payload[cur_mch_sched_ce * 2 + 1];
    cur_mch_sched_ce++;
    *mtch_stop = (mtch_stop_ce == sch_subh::MTCH_STOP_EMPTY) ? (0) : (mtch_stop_ce);
    return true;
  }
  return false;
}

uint8_t sch_subh::get_ta_cmd()
{
  return (uint8_t)payload[0] & 0x3f;
}

uint8_t sch_subh::get_activation_deactivation_cmd()
{
  /* 3GPP 36.321 section 6.1.3.8 Activation/Deactivation MAC Control Element */
  return payload[0];
}

uint32_t sch_subh::get_sdu_lcid()
{
  return lcid;
}

uint32_t sch_subh::get_payload_size()
{
  return nof_bytes;
}

uint32_t sch_subh::get_header_size(bool is_last)
{
  if (!is_last) {
    if (is_sdu() || (lcid == (uint32_t)mch_lcid::MCH_SCHED_INFO && type == MCH_SUBH_TYPE)) {
      return sch_pdu::size_header_sdu(nof_bytes);
    }
    return 1; // All others are 1-byte
  } else {
    return 1; // Last subheader (CE or SDU) has always 1 byte header
  }
}

uint8_t* sch_subh::get_sdu_ptr()
{
  return payload;
}

void sch_subh::set_padding(uint32_t padding_len)
{
  lcid      = (uint32_t)dl_sch_lcid::PADDING;
  nof_bytes = padding_len;
}

void sch_subh::set_type(subh_type type_)
{
  type = type_;
};

void sch_subh::set_padding()
{
  set_padding(0);
}

// Update the BSR content without changing the type
void sch_subh::update_bsr(uint32_t buff_size[4], ul_sch_lcid format)
{
  uint32_t nonzero_lcg = 0;
  for (int i = 0; i < 4; i++) {
    if (buff_size[i]) {
      nonzero_lcg = i;
    }
  }

  if (format == ul_sch_lcid::LONG_BSR) {
    w_payload_ce[0] = ((buff_size_table(buff_size[0]) & 0x3f) << 2) | ((buff_size_table(buff_size[1]) & 0x30) >> 4);
    w_payload_ce[1] = ((buff_size_table(buff_size[1]) & 0xf) << 4) | ((buff_size_table(buff_size[2]) & 0x3c) >> 2);
    w_payload_ce[2] = ((buff_size_table(buff_size[2]) & 0x3) << 6) | ((buff_size_table(buff_size[3]) & 0x3f));
  } else {
    w_payload_ce[0] = (nonzero_lcg & 0x3) << 6 | (buff_size_table(buff_size[nonzero_lcg]) & 0x3f);
  }
}

// Makes a MAC PDU sub header a BSR subheader
// It checks that enough space is left in the MAC PDU and
// updates the remaning length of the parent MAC PDU
// @return true if given BSR format could be set, false otherwise
bool sch_subh::set_bsr(uint32_t buff_size[4], ul_sch_lcid format)
{
  uint32_t ce_size = format == ul_sch_lcid::LONG_BSR ? 3 : 1;
  if (((sch_pdu*)parent)->has_space_ce(ce_size)) {
    update_bsr(buff_size, format);
    lcid = (uint32_t)format;
    ((sch_pdu*)parent)->update_space_ce(ce_size);
    nof_bytes = ce_size;
    return true;
  } else {
    return false;
  }
}

bool sch_subh::set_c_rnti(uint16_t crnti)
{
  crnti = htole32(crnti);
  if (((sch_pdu*)parent)->has_space_ce(2)) {
    w_payload_ce[0] = (uint8_t)((crnti & 0xff00) >> 8);
    w_payload_ce[1] = (uint8_t)((crnti & 0x00ff));
    lcid            = (uint32_t)ul_sch_lcid::CRNTI;
    ((sch_pdu*)parent)->update_space_ce(2);
    nof_bytes = 2;
    return true;
  } else {
    return false;
  }
}
bool sch_subh::set_con_res_id(uint64_t con_res_id)
{
  con_res_id = htole64(con_res_id);
  if (((sch_pdu*)parent)->has_space_ce(6)) {
    w_payload_ce[0] = (uint8_t)((con_res_id & 0xff0000000000) >> 40);
    w_payload_ce[1] = (uint8_t)((con_res_id & 0x00ff00000000) >> 32);
    w_payload_ce[2] = (uint8_t)((con_res_id & 0x0000ff000000) >> 24);
    w_payload_ce[3] = (uint8_t)((con_res_id & 0x000000ff0000) >> 16);
    w_payload_ce[4] = (uint8_t)((con_res_id & 0x00000000ff00) >> 8);
    w_payload_ce[5] = (uint8_t)((con_res_id & 0x0000000000ff));
    lcid            = (uint32_t)dl_sch_lcid::CON_RES_ID;
    ((sch_pdu*)parent)->update_space_ce(6);
    nof_bytes = 6;
    return true;
  } else {
    return false;
  }
}
bool sch_subh::set_phr(float phr)
{
  if (((sch_pdu*)parent)->has_space_ce(1)) {
    w_payload_ce[0] = phr_report_table(phr) & 0x3f;
    lcid            = (uint32_t)ul_sch_lcid::PHR_REPORT;
    ((sch_pdu*)parent)->update_space_ce(1);
    nof_bytes = 1;
    return true;
  } else {
    return false;
  }
}

bool sch_subh::set_ta_cmd(uint8_t ta_cmd)
{
  if (((sch_pdu*)parent)->has_space_ce(1)) {
    w_payload_ce[0] = ta_cmd & 0x3f;
    lcid            = (uint32_t)dl_sch_lcid::TA_CMD;
    ((sch_pdu*)parent)->update_space_ce(1);
    nof_bytes = 1;
    return true;
  } else {
    return false;
  }
}

bool sch_subh::set_scell_activation_cmd(const std::array<bool, SRSRAN_MAX_CARRIERS>& active_scell_idxs)
{
  const uint32_t nof_octets = 1;
  if (not((sch_pdu*)parent)->has_space_ce(nof_octets)) {
    return false;
  }
  // first bit is reserved
  w_payload_ce[0] = 0;
  for (uint8_t i = 1; i < SRSRAN_MAX_CARRIERS; ++i) {
    w_payload_ce[0] |= (static_cast<uint8_t>(active_scell_idxs[i]) << i);
  }
  lcid = (uint32_t)dl_sch_lcid::SCELL_ACTIVATION;
  ((sch_pdu*)parent)->update_space_ce(nof_octets);
  nof_bytes = nof_octets;
  return true;
}

bool sch_subh::set_next_mch_sched_info(uint8_t lcid_, uint16_t mtch_stop)
{
  if (((sch_pdu*)parent)->has_space_ce(2, true)) {
    uint16_t mtch_stop_ce                  = (mtch_stop) ? (mtch_stop) : (sch_subh::MTCH_STOP_EMPTY);
    w_payload_ce[nof_mch_sched_ce * 2]     = (lcid_ & 0x1F) << 3 | (uint8_t)((mtch_stop_ce & 0x0700) >> 8);
    w_payload_ce[nof_mch_sched_ce * 2 + 1] = (uint8_t)(mtch_stop_ce & 0xff);
    nof_mch_sched_ce++;
    lcid = (uint32_t)mch_lcid::MCH_SCHED_INFO;
    ((sch_pdu*)parent)->update_space_ce(2, true);
    nof_bytes += 2;
    return true;
  }
  return false;
}

int sch_subh::set_sdu(uint32_t lcid_, uint32_t requested_bytes_, read_pdu_interface* sdu_itf_)
{
  if (((sch_pdu*)parent)->has_space_sdu(requested_bytes_)) {
    lcid    = lcid_;
    payload = ((sch_pdu*)parent)->get_current_sdu_ptr();

    // Copy data and get final number of bytes written to the MAC PDU
    int sdu_sz = sdu_itf_->read_pdu(lcid, payload, requested_bytes_);

    if (sdu_sz < 0) {
      return SRSRAN_ERROR;
    }
    if (sdu_sz == 0) {
      return 0;
    } else {
      // Save final number of written bytes
      nof_bytes = sdu_sz;

      if (nof_bytes > (int32_t)requested_bytes_) {
        return SRSRAN_ERROR;
      }
    }

    ((sch_pdu*)parent)->update_space_sdu(nof_bytes);
    ((sch_pdu*)parent)->add_sdu(nof_bytes);

    return nof_bytes;
  } else {
    return SRSRAN_ERROR;
  }
}

int sch_subh::set_sdu(uint32_t lcid_, uint32_t nof_bytes_, uint8_t* payload)
{
  if (((sch_pdu*)parent)->has_space_sdu(nof_bytes_)) {
    lcid = lcid_;

    memcpy(((sch_pdu*)parent)->get_current_sdu_ptr(), payload, nof_bytes_);

    ((sch_pdu*)parent)->add_sdu(nof_bytes_);
    ((sch_pdu*)parent)->update_space_sdu(nof_bytes_);
    nof_bytes = nof_bytes_;

    return (int)nof_bytes;
  } else {
    return -1;
  }
}

// Section 6.2.1
void sch_subh::write_subheader(uint8_t** ptr, bool is_last)
{
  *(*ptr) = (uint8_t)(is_last ? 0 : (1 << 5)) | ((uint8_t)lcid & 0x1f);
  *ptr += 1;
  if (is_sdu() || is_var_len_ce()) {
    // MAC SDU: R/R/E/LCID/F/L subheader
    // 2nd and 3rd octet
    if (!is_last) {
      if (nof_bytes >= 128) {
        *(*ptr) = (uint8_t)1 << 7 | ((nof_bytes & 0x7f00) >> 8);
        *ptr += 1;
        *(*ptr) = (uint8_t)(nof_bytes & 0xff);
        *ptr += 1;
      } else {
        *(*ptr) = (uint8_t)(nof_bytes & 0x7f);
        *ptr += 1;
      }
    }
  }
}

void sch_subh::write_payload(uint8_t** ptr)
{
  if (is_sdu()) {
    // SDU is written directly during subheader creation
  } else {
    nof_bytes = sizeof_ce(lcid, parent->is_ul());
    memcpy(*ptr, w_payload_ce, nof_bytes * sizeof(uint8_t));
  }
  *ptr += nof_bytes;
}

bool sch_subh::read_subheader(uint8_t** ptr)
{
  // Skip R
  bool e_bit = (bool)(*(*ptr) & 0x20) ? true : false;
  lcid       = (uint8_t) * (*ptr) & 0x1f;
  *ptr += 1;
  if (is_sdu() || is_var_len_ce()) {
    if (e_bit) {
      F_bit     = (bool)(*(*ptr) & 0x80) ? true : false;
      nof_bytes = (uint32_t) * (*ptr) & 0x7f;
      *ptr += 1;
      if (F_bit) {
        nof_bytes = nof_bytes << 8 | ((uint32_t) * (*ptr) & 0xff);
        *ptr += 1;
      }
    } else {
      nof_bytes = 0;
      F_bit     = 0;
    }
  } else {
    nof_bytes = sizeof_ce(lcid, parent->is_ul());
  }
  return e_bit;
}

void sch_subh::read_payload(uint8_t** ptr)
{
  payload = *ptr;
  *ptr += nof_bytes;
}

void sch_subh::to_string(fmt::memory_buffer& buffer)
{
  if (is_sdu()) {
    fmt::format_to(buffer, "LCID={} len={}", lcid, nof_bytes);
  } else if (type == SCH_SUBH_TYPE) {
    if (parent->is_ul()) {
      switch ((ul_sch_lcid)lcid) {
        case ul_sch_lcid::CRNTI:
          fmt::format_to(buffer, "CRNTI: rnti=0x{:x}", get_c_rnti());
          break;
        case ul_sch_lcid::PHR_REPORT:
          fmt::format_to(buffer, "PHR: ph={}", get_phr());
          break;
        case ul_sch_lcid::TRUNC_BSR:
        case ul_sch_lcid::SHORT_BSR:
        case ul_sch_lcid::LONG_BSR: {
          uint32_t buff_size_idx[4]   = {};
          uint32_t buff_size_bytes[4] = {};
          uint32_t lcg                = get_bsr(buff_size_idx, buff_size_bytes);
          if (ul_sch_ce_type() == ul_sch_lcid::LONG_BSR) {
            fmt::format_to(buffer, "LBSR: b=");
            for (uint32_t i = 0; i < 4; i++) {
              fmt::format_to(buffer, "{} ", buff_size_idx[i]);
            }
          } else if (ul_sch_ce_type() == ul_sch_lcid::SHORT_BSR) {
            fmt::format_to(buffer, "SBSR: lcg={} b={}", lcg, buff_size_idx[lcg]);
          } else {
            fmt::format_to(buffer, "TBSR: lcg={} b={}", lcg, buff_size_idx[lcg]);
          }
        } break;
        case ul_sch_lcid::PADDING:
          fmt::format_to(buffer, "PAD: len={}", get_payload_size());
          break;
        default:
          // do nothing
          break;
      }
    } else {
      switch ((dl_sch_lcid)lcid) {
        case dl_sch_lcid::CON_RES_ID:
          fmt::format_to(buffer, "CON_RES: id=0x{:x}", get_con_res_id());
          break;
        case dl_sch_lcid::TA_CMD:
          fmt::format_to(buffer, "TA: ta={}", get_ta_cmd());
          break;
        case dl_sch_lcid::SCELL_ACTIVATION_4_OCTET:
        case dl_sch_lcid::SCELL_ACTIVATION:
          fmt::format_to(buffer, "SCELL_ACT");
          break;
        case dl_sch_lcid::DRX_CMD:
          fmt::format_to(buffer, "DRX");
          break;
        case dl_sch_lcid::PADDING:
          fmt::format_to(buffer, "PAD: len={}", get_payload_size());
          break;
        default:
          break;
      }
    }
  } else if (type == MCH_SUBH_TYPE) {
    switch ((mch_lcid)lcid) {
      case mch_lcid::MCH_SCHED_INFO:
        fmt::format_to(buffer, "MCH_SCHED_INFO");
        break;
      case mch_lcid::PADDING:
        fmt::format_to(buffer, "PAD: len={}", get_payload_size());
        break;
      default:
        break;
    }
  }
}

uint8_t sch_subh::buff_size_table(uint32_t buffer_size)
{
  if (buffer_size == 0) {
    return 0;
  } else if (buffer_size > 150000) {
    return 63;
  } else {
    for (int i = 1; i < 62; i++) {
      if (buffer_size <= btable[i + 1]) {
        return i;
      }
    }
    return 62;
  }
}

// Implements Table 9.1.8.4-1 Power headroom report mapping (36.133)
uint8_t sch_subh::phr_report_table(float phr_value)
{
  if (phr_value < -23) {
    phr_value = -23;
  }
  if (phr_value > 40) {
    phr_value = 40;
  }
  return (uint8_t)floor(phr_value + 23);
}

void rar_pdu::to_string(fmt::memory_buffer& buffer)
{
  fmt::format_to(buffer, "MAC PDU for RAR: ");
  pdu::to_string(buffer);
}

rar_pdu::rar_pdu(uint32_t max_rars_, srslog::basic_logger& logger) : pdu(max_rars_, logger)
{
  backoff_indicator     = 0;
  has_backoff_indicator = false;
}

uint8_t rar_pdu::get_backoff()
{
  return backoff_indicator;
}

bool rar_pdu::has_backoff()
{
  return has_backoff_indicator;
}

void rar_pdu::set_backoff(uint8_t bi)
{
  has_backoff_indicator = true;
  backoff_indicator     = bi;
}

// Section 6.1.5
bool rar_pdu::write_packet(uint8_t* ptr)
{
  uint8_t* init_ptr = ptr;

  // Write Backoff Indicator, if any
  if (has_backoff_indicator) {
    *(ptr) = backoff_indicator & 0xf;
    if (nof_subheaders > 0) {
      *(ptr) |= 1 << 7;
    }
    ptr++;
  }

  // Write RAR subheaders
  for (int i = 0; i < nof_subheaders; i++) {
    subheaders[i].write_subheader(&ptr, i == nof_subheaders - 1);
  }
  // Write payload
  for (int i = 0; i < nof_subheaders; i++) {
    subheaders[i].write_payload(&ptr);
  }

  // Set padding to zeros (if any)
  int32_t payload_len = ptr - init_ptr;
  int32_t pad_len     = rem_len - payload_len;
  if (pad_len < 0) {
    logger.warning("Error packing RAR PDU (payload_len=%d, rem_len=%d)", payload_len, rem_len);
    return false;
  } else {
    bzero(ptr, pad_len * sizeof(uint8_t));
  }

  return true;
}

void rar_subh::to_string(fmt::memory_buffer& buffer)
{
  if (type == RAPID) {
    fmt::format_to(buffer, "RAPID: {}, Temp C-RNTI: {}, TA: {}, UL Grant: ", preamble, temp_rnti, ta);
  } else {
    fmt::format_to(buffer, "Backoff Indicator: {} ", int32_t(((rar_pdu*)parent)->get_backoff()));
  }

  char tmp[16];
  srsran_vec_sprint_hex(tmp, sizeof(tmp), grant, RAR_GRANT_LEN);
  fmt::format_to(buffer, "{}", tmp);
}

void rar_subh::init()
{
  bzero(grant, sizeof(uint8_t) * RAR_GRANT_LEN);
  ta        = 0;
  temp_rnti = 0;
}

uint32_t rar_subh::get_rapid()
{
  return preamble;
}

bool rar_subh::has_rapid()
{
  return (type == RAPID);
}

void rar_subh::get_sched_grant(uint8_t grant_[RAR_GRANT_LEN])
{
  memcpy(grant_, grant, sizeof(uint8_t) * RAR_GRANT_LEN);
}

uint32_t rar_subh::get_ta_cmd()
{
  return ta;
}

uint16_t rar_subh::get_temp_crnti()
{
  return temp_rnti;
}

void rar_subh::set_rapid(uint32_t rapid)
{
  preamble = rapid;
}

void rar_subh::set_sched_grant(uint8_t grant_[RAR_GRANT_LEN])
{
  memcpy(grant, grant_, sizeof(uint8_t) * RAR_GRANT_LEN);
}

void rar_subh::set_ta_cmd(uint32_t ta_)
{
  ta = ta_;
}

void rar_subh::set_temp_crnti(uint16_t temp_rnti_)
{
  temp_rnti = temp_rnti_;
}

// Section 6.2.2
void rar_subh::write_subheader(uint8_t** ptr, bool is_last)
{
  *(*ptr) = (uint8_t)(!is_last << 7 | 1 << 6 | (preamble & 0x3f));
  *ptr += 1;
}
// Section 6.2.3
void rar_subh::write_payload(uint8_t** ptr)
{
  *(*ptr + 0) = (uint8_t)((ta & 0x7f0) >> 4);
  *(*ptr + 1) = (uint8_t)((ta & 0xf) << 4) | (grant[0] << 3) | (grant[1] << 2) | (grant[2] << 1) | grant[3];
  uint8_t* x  = &grant[4];
  *(*ptr + 2) = (uint8_t)srsran_bit_pack(&x, 8);
  *(*ptr + 3) = (uint8_t)srsran_bit_pack(&x, 8);
  *(*ptr + 4) = (uint8_t)((temp_rnti & 0xff00) >> 8);
  *(*ptr + 5) = (uint8_t)(temp_rnti & 0x00ff);
  *ptr += 6;
}

void rar_subh::read_payload(uint8_t** ptr)
{
  if (type == RAPID) {
    ta         = ((uint32_t) * (*ptr + 0) & 0x7f) << 4 | (*(*ptr + 1) & 0xf0) >> 4;
    grant[0]   = *(*ptr + 1) & 0x8 ? 1 : 0;
    grant[1]   = *(*ptr + 1) & 0x4 ? 1 : 0;
    grant[2]   = *(*ptr + 1) & 0x2 ? 1 : 0;
    grant[3]   = *(*ptr + 1) & 0x1 ? 1 : 0;
    uint8_t* x = &grant[4];
    srsran_bit_unpack(*(*ptr + 2), &x, 8);
    srsran_bit_unpack(*(*ptr + 3), &x, 8);
    temp_rnti = ((uint16_t) * (*ptr + 4)) << 8 | *(*ptr + 5);
    *ptr += 6;
  }
}

bool rar_subh::read_subheader(uint8_t** ptr)
{
  bool e_bit = *(*ptr) & 0x80 ? true : false;
  type       = *(*ptr) & 0x40 ? RAPID : BACKOFF;
  if (type == RAPID) {
    preamble = *(*ptr) & 0x3f;
  } else {
    ((rar_pdu*)parent)->set_backoff(*(*ptr) & 0xf);
  }
  *ptr += 1;
  return e_bit;
}

} // namespace srsran
