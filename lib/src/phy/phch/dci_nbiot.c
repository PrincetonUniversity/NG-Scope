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

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "srsran/phy/common/phy_common.h"
#include "srsran/phy/phch/dci_nbiot.h"
#include "srsran/phy/utils/bit.h"
#include "srsran/phy/utils/debug.h"
#include "srsran/phy/utils/vector.h"

/* Creates the UL NPUSCH resource allocation grant from the random access respone message
 */
int srsran_nbiot_dci_rar_to_ul_grant(srsran_nbiot_dci_rar_grant_t* rar,
                                     srsran_ra_nbiot_ul_grant_t*   grant,
                                     uint32_t                      rx_tti)
{
  // create DCI from rar grant
  srsran_ra_nbiot_ul_dci_t dci;
  bzero(&dci, sizeof(srsran_ra_nbiot_ul_dci_t));
  dci.i_rep      = rar->n_rep;
  dci.i_sc       = rar->i_sc;
  dci.i_mcs      = rar->i_mcs;
  dci.i_delay    = rar->i_delay;
  dci.sc_spacing = (rar->sc_spacing == 1) ? SRSRAN_NPUSCH_SC_SPACING_15000 : SRSRAN_NPUSCH_SC_SPACING_3750;

  // use DCI to fill default UL grant values
  grant->format = SRSRAN_NPUSCH_FORMAT1; // UL-SCH is always format 1
  if (srsran_ra_nbiot_ul_rar_dci_to_grant(&dci, grant, rx_tti)) {
    fprintf(stderr, "Error converting RAR DCI to grant.\n");
    return SRSRAN_ERROR;
  }

  if (SRSRAN_VERBOSE_ISINFO()) {
    srsran_ra_nbiot_ul_grant_fprint(stdout, grant);
  }
  return SRSRAN_SUCCESS;
}

/* Unpack RAR UL grant as defined in Section 16.3.3 of 36.213 */
void srsran_nbiot_dci_rar_grant_unpack(srsran_nbiot_dci_rar_grant_t* rar,
                                       const uint8_t                 grant[SRSRAN_NBIOT_RAR_GRANT_LEN])
{
  uint8_t* grant_ptr = (uint8_t*)grant;
  rar->sc_spacing    = srsran_bit_pack(&grant_ptr, 1);
  rar->i_sc          = srsran_bit_pack(&grant_ptr, 6);
  rar->i_delay       = srsran_bit_pack(&grant_ptr, 2);
  rar->n_rep         = srsran_bit_pack(&grant_ptr, 3);
  rar->i_mcs         = srsran_bit_pack(&grant_ptr, 3);
}

// Creates the UL NPUSCH resource allocation grant from a DCI format N0 message
int srsran_nbiot_dci_msg_to_ul_grant(const srsran_dci_msg_t*          msg,
                                     srsran_ra_nbiot_ul_dci_t*        ul_dci,
                                     srsran_ra_nbiot_ul_grant_t*      grant,
                                     const uint32_t                   rx_tti,
                                     const srsran_npusch_sc_spacing_t spacing)
{
  int ret = SRSRAN_ERROR_INVALID_INPUTS;

  if (msg != NULL && ul_dci != NULL && grant != NULL) {
    ret = SRSRAN_ERROR;

    bzero(ul_dci, sizeof(srsran_ra_nbiot_ul_dci_t));
    bzero(grant, sizeof(srsran_ra_nbiot_ul_grant_t));

    if (srsran_dci_msg_unpack_npusch(msg, ul_dci)) {
      return ret;
    }

    if (srsran_ra_nbiot_ul_dci_to_grant(ul_dci, grant, rx_tti, spacing)) {
      return ret;
    }

    if (SRSRAN_VERBOSE_ISINFO()) {
      srsran_ra_npusch_fprint(stdout, ul_dci);
      srsran_ra_nbiot_ul_grant_fprint(stdout, grant);
    }

    ret = SRSRAN_SUCCESS;
  }
  return ret;
}

/* Unpacks a NB-IoT DCI message and configures the DL grant object
 */
int srsran_nbiot_dci_msg_to_dl_grant(const srsran_dci_msg_t*     msg,
                                     const uint16_t              msg_rnti,
                                     srsran_ra_nbiot_dl_dci_t*   dl_dci,
                                     srsran_ra_nbiot_dl_grant_t* grant,
                                     const uint32_t              sfn,
                                     const uint32_t              sf_idx,
                                     const uint32_t              r_max,
                                     const srsran_nbiot_mode_t   mode)
{
  int ret = SRSRAN_ERROR_INVALID_INPUTS;

  if (msg != NULL && grant != NULL && dl_dci != NULL) {
    ret = SRSRAN_ERROR;

    bzero(dl_dci, sizeof(srsran_ra_nbiot_dl_dci_t));
    bzero(grant, sizeof(srsran_ra_nbiot_dl_grant_t));

    bool crc_is_crnti = false;
    if (msg_rnti >= SRSRAN_CRNTI_START && msg_rnti <= SRSRAN_CRNTI_END) {
      crc_is_crnti = true;
    }
    srsran_dci_format_t tmp = msg->format;
    ret                     = srsran_dci_msg_unpack_npdsch(msg, dl_dci, crc_is_crnti);
    if (ret) {
      fprintf(stderr, "Can't unpack DCI message %s (%d)\n", srsran_dci_format_string(tmp), tmp);
      return ret;
    }
    ret = srsran_ra_nbiot_dl_dci_to_grant(dl_dci, grant, sfn, sf_idx, r_max, false, mode);
    if (ret) {
      fprintf(stderr, "Can't convert DCI %s to grant (%d)\n", srsran_dci_format_string(tmp), tmp);
      return ret;
    }

    if (SRSRAN_VERBOSE_ISINFO()) {
      srsran_nbiot_dl_dci_fprint(stdout, dl_dci);
      srsran_ra_nbiot_dl_grant_fprint(stdout, grant);
    }

    ret = SRSRAN_SUCCESS;
  }
  return ret;
}

// For NB-IoT there are only three possible combinations, i.e.
//     L'  L   nNCCE
// #1  1   0   0
// #2  1   0   1
// #3  2   1   0
bool srsran_nbiot_dci_location_isvalid(const srsran_dci_location_t* c)
{
  if ((c->L == 1 && c->ncce <= 1) || (c->L == 2 && c->ncce == 0)) {
    return true;
  } else {
    return false;
  }
}

uint32_t dci_formatN0_sizeof()
{
  return 23;
}

uint32_t dci_formatN1_sizeof()
{
  // same as for formatN0
  return dci_formatN0_sizeof();
}

uint32_t dci_formatN2_sizeof()
{
  return 15;
}

/* Packs DCI format N0 data to a sequence of bits and store them in msg according
 * to 36.212 13.2.0 clause 6.4.3.1
 *
 * TODO: TPC and cyclic shift for DM RS not implemented
 */
int dci_formatN0_pack(srsran_ra_nbiot_ul_dci_t* data, srsran_dci_msg_t* msg, uint32_t nof_prb)
{
  // pack bits
  uint8_t* y = msg->payload;

  *y++ = 0; // format differentiation

  // TODO: Implement packing

  // Subcarrier indication – 6 bits

  // Resource assignment – 3 bits

  // Scheduling delay – 2 bits

  // Modulation and coding scheme – 4 bits

  // Redundancy version – 1 bit

  // Repetition number – 3 bits

  // New data indicator – 1 bit

  // DCI subframe repetition number – 2 bits

  return SRSRAN_ERROR;
}

/* Packs DCI format N1 data to a sequence of bits and store them in msg according
 * to 36.212 v13.2.0 clause 6.4.3.2
 *
 * TODO: implement packing for NPRACH case
 */
int dci_formatN1_pack(const srsran_ra_nbiot_dl_dci_t* data, srsran_dci_msg_t* msg, bool crc_is_crnti)
{
  int      last_bits_val = 0;
  uint8_t* y             = (uint8_t*)msg->payload;

  *y++ = 1; // format differentiation

  // NPDCCH order indicator – 1 bit
  *y++ = data->alloc.is_ra;
  if (data->alloc.is_ra) {
    // Starting number of NPRACH repetitions – 2 bits

    // Subcarrier indication of NPRACH – 6 bits

    // All the remaining bits in format N1
    last_bits_val = 1;
  } else {
    // default NPDSCH scheduling

    // Scheduling delay – 3 bits
    srsran_bit_unpack(data->alloc.i_delay, &y, 3);

    // Resource assignment – 3 bits
    srsran_bit_unpack(data->alloc.i_sf, &y, 3);

    // Modulation and coding scheme – 4 bits
    srsran_bit_unpack(data->mcs_idx, &y, 4);

    // Repetition number – 4 bits
    srsran_bit_unpack(data->alloc.i_rep, &y, 4);

    // New data indicator – 1 bit
    if (crc_is_crnti) {
      *y++ = 0;
    } else {
      *y++ = data->ndi;
    }

    // HARQ-ACK resource – 4 bits
    if (crc_is_crnti) {
      // reserved
      y += 4;
    } else {
      srsran_bit_unpack(data->alloc.harq_ack, &y, 4);
    }
  }

  // Padding with zeros until reaching final size
  uint32_t n = dci_formatN1_sizeof();
  while (y - msg->payload < n) {
    *y++ = last_bits_val;
  }
  msg->nof_bits = (y - msg->payload);

  return SRSRAN_SUCCESS;
}

// According to Section 6.4.3.1 in TS36.212 v13.2.0
int dci_formatN0_unpack(const srsran_dci_msg_t* msg, srsran_ra_nbiot_ul_dci_t* data)
{
  uint8_t* y = (uint8_t*)msg->payload;

  // make sure it has the expected length
  if (msg->nof_bits != dci_formatN1_sizeof()) {
    fprintf(stderr, "Invalid message length for format N1\n");
    return SRSRAN_ERROR;
  }

  // Format differentiation - 1 bit
  data->format = srsran_bit_pack(&y, 1);

  // Subcarrier indication – 6 bits
  data->i_sc = srsran_bit_pack(&y, 6);

  // Resource assignment – 3 bits
  data->i_ru = srsran_bit_pack(&y, 3);

  // Scheduling delay – 2 bits
  data->i_delay = srsran_bit_pack(&y, 2);

  // Modulation and coding scheme – 4
  data->i_mcs = srsran_bit_pack(&y, 4);

  // Redundancy version – 1 bit
  data->i_rv = srsran_bit_pack(&y, 1);

  // Repetition number – 3 bits
  data->i_rep = srsran_bit_pack(&y, 3);

  // New data indicator – 1 bit
  data->ndi = srsran_bit_pack(&y, 1);

  // DCI subframe repetition number – 2 bits
  data->dci_sf_rep_num = srsran_bit_pack(&y, 2);

  // According to 16.5.1.1, SC spacing is determined by RAR grant
  // TODO: Add support for 3.75kHz
  data->sc_spacing = SRSRAN_NPUSCH_SC_SPACING_15000;

  return SRSRAN_SUCCESS;
}

int dci_formatN1_unpack(const srsran_dci_msg_t* msg, srsran_ra_nbiot_dl_dci_t* data, bool crc_is_crnti)
{
  uint8_t* y = (uint8_t*)msg->payload;

  // make sure it has the expected length
  if (msg->nof_bits != dci_formatN1_sizeof()) {
    fprintf(stderr, "Invalid message length for format N1\n");
    return SRSRAN_ERROR;
  }

  // Format differentiation - 1 bit
  data->format    = srsran_bit_pack(&y, 1);
  data->dci_is_n2 = false;

  // The NPDCCH order indicator (if bit is one, this is for RA procedure)
  data->alloc.is_ra = srsran_bit_pack(&y, 1);

  if (data->alloc.is_ra) {
    // This is a RA precedure, set field according to Section 6.4.3.2 in TS36.212
    data->alloc.nprach_start = srsran_bit_pack(&y, 2);
    data->alloc.nprach_sc    = srsran_bit_pack(&y, 6);
    // set remaining field to 1
    data->alloc.i_delay        = 0xffff;
    data->alloc.i_sf           = 0xffff;
    data->mcs_idx              = 0xffff;
    data->alloc.i_rep          = 0xffff;
    data->ndi                  = true;
    data->alloc.harq_ack       = 0xffff;
    data->alloc.dci_sf_rep_num = 0xffff;
  } else {
    // default NPDSCH scheduling

    // Scheduling delay – 3 bits
    data->alloc.i_delay = srsran_bit_pack(&y, 3);

    // Resource assignment – 3 bits
    data->alloc.i_sf = srsran_bit_pack(&y, 3);

    // Modulation and coding scheme – 4 bits
    data->mcs_idx = srsran_bit_pack(&y, 4);

    // Repetition number – 4 bits
    data->alloc.i_rep = srsran_bit_pack(&y, 4);

    // New data indicator – 1 bit
    if (crc_is_crnti) {
      data->ndi = *y++ ? true : false;
    } else {
      y++; // NDI reserved
    }

    // HARQ-ACK resource – 4 bits
    data->alloc.harq_ack = srsran_bit_pack(&y, 4);

    // DCI subframe repetition number – 2 bits
    data->alloc.dci_sf_rep_num = srsran_bit_pack(&y, 2);
  }
  return SRSRAN_SUCCESS;
}

int dci_formatN2_unpack(const srsran_dci_msg_t* msg, srsran_ra_nbiot_dl_dci_t* data)
{
  uint8_t* y = (uint8_t*)msg->payload;

  // make sure it has the expected length
  if (msg->nof_bits != dci_formatN2_sizeof()) {
    fprintf(stderr, "Invalid message length for format N2\n");
    return SRSRAN_ERROR;
  }

  data->dci_is_n2 = true;

  // Flag for paging/direct indication differentiation – 1 bit, with value 0 for direct indication and value 1 for
  // paging
  data->format = srsran_bit_pack(&y, 1);

  if (data->format == 0) {
    // Direct Indication information – 8 bits provide direct indication of system information update and other fields
    data->dir_indication_info = srsran_bit_pack(&y, 8);
  } else {
    // Paging

    // Resource assignment – 3 bits
    data->alloc.i_sf = srsran_bit_pack(&y, 3);

    // Modulation and coding scheme – 4 bits
    data->mcs_idx = srsran_bit_pack(&y, 4);

    // Repetition number – 4 bits
    data->alloc.i_rep = srsran_bit_pack(&y, 4);

    // DCI subframe repetition number – 3 bits
    data->alloc.dci_sf_rep_num = srsran_bit_pack(&y, 3);
  }
  return SRSRAN_SUCCESS;
}

uint32_t srsran_dci_nbiot_format_sizeof(const srsran_dci_format_t format)
{
  switch (format) {
    case SRSRAN_DCI_FORMATN0:
      return dci_formatN0_sizeof();
    case SRSRAN_DCI_FORMATN1:
      return dci_formatN1_sizeof();
    case SRSRAN_DCI_FORMATN2:
      return dci_formatN2_sizeof();
    default:
      printf("Error computing DCI bits: Unknown format %d\n", format);
      return 0;
  }
}

int srsran_dci_msg_unpack_npdsch(const srsran_dci_msg_t* msg, srsran_ra_nbiot_dl_dci_t* data, const bool crc_is_crnti)
{
  switch (msg->format) {
    case SRSRAN_DCI_FORMATN1:
      return dci_formatN1_unpack(msg, data, crc_is_crnti);
    case SRSRAN_DCI_FORMATN2:
      return dci_formatN2_unpack(msg, data);
    default:
      fprintf(stderr, "DCI unpack npdsch: Invalid DCI format %s\n", srsran_dci_format_string(msg->format));
      return SRSRAN_ERROR;
  }
}

int srsran_dci_msg_unpack_npusch(const srsran_dci_msg_t* msg, srsran_ra_nbiot_ul_dci_t* data)
{
  switch (msg->format) {
    case SRSRAN_DCI_FORMATN0:
      return dci_formatN0_unpack(msg, data);
    default:
      fprintf(stderr, "DCI unpack npusch: Invalid DCI format %s\n", srsran_dci_format_string(msg->format));
      return SRSRAN_ERROR;
  }
}

int srsran_dci_msg_pack_npdsch(const srsran_ra_nbiot_dl_dci_t* data,
                               const srsran_dci_format_t       format,
                               srsran_dci_msg_t*               msg,
                               const bool                      crc_is_crnti)
{
  msg->format = format;
  switch (format) {
    case SRSRAN_DCI_FORMATN1:
      return dci_formatN1_pack(data, msg, crc_is_crnti);
    default:
      fprintf(stderr, "DCI pack npdsch: Invalid DCI format %s in \n", srsran_dci_format_string(format));
      return SRSRAN_ERROR;
  }
}
