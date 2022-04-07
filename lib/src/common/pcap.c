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

#include "srsran/common/pcap.h"
#include <arpa/inet.h>
#include <linux/udp.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

/* Open the file and write file header */
FILE* DLT_PCAP_Open(uint32_t DLT, const char* fileName)
{
  pcap_hdr_t file_header = {
      0xa1b2c3d4, /* magic number */
      2,
      4,     /* version number is 2.4 */
      0,     /* timezone */
      0,     /* sigfigs - apparently all tools do this */
      65535, /* snaplen - this should be long enough */
      DLT    /* Data Link Type (DLT).  Set as unused value 147 for now */
  };

  FILE* fd = fopen(fileName, "w");
  if (fd == NULL) {
    printf("Failed to open file \"%s\" for writing\n", fileName);
    return NULL;
  }

  /* Write the file header */
  fwrite(&file_header, sizeof(pcap_hdr_t), 1, fd);

  return fd;
}

/* Close the PCAP file */
void DLT_PCAP_Close(FILE* fd)
{
  if (fd) {
    fclose(fd);
  }
}

/* Packs MAC Contect to a buffer */
inline int LTE_PCAP_PACK_MAC_CONTEXT_TO_BUFFER(MAC_Context_Info_t* context, uint8_t* buffer, unsigned int length)
{
  int      offset = 0;
  uint16_t tmp16;

  if (buffer == NULL || length < PCAP_CONTEXT_HEADER_MAX) {
    printf("Error: Can't write to empty file handle\n");
    return -1;
  }

  /*****************************************************************/
  /* Context information (same as written by UDP heuristic clients */
  buffer[offset++] = context->radioType;
  buffer[offset++] = context->direction;
  buffer[offset++] = context->rntiType;

  /* RNTI */
  buffer[offset++] = MAC_LTE_RNTI_TAG;
  tmp16            = htons(context->rnti);
  memcpy(buffer + offset, &tmp16, 2);
  offset += 2;

  /* UEId */
  buffer[offset++] = MAC_LTE_UEID_TAG;
  tmp16            = htons(context->ueid);
  memcpy(buffer + offset, &tmp16, 2);
  offset += 2;

  /* Subframe Number and System Frame Number */
  /* SFN is stored in 12 MSB and SF in 4 LSB */
  buffer[offset++] = MAC_LTE_FRAME_SUBFRAME_TAG;
  tmp16            = (context->sysFrameNumber << 4) | context->subFrameNumber;
  tmp16            = htons(tmp16);
  memcpy(buffer + offset, &tmp16, 2);
  offset += 2;

  /* CRC Status */
  buffer[offset++] = MAC_LTE_CRC_STATUS_TAG;
  buffer[offset++] = context->crcStatusOK;

  /* CARRIER ID */
  buffer[offset++] = MAC_LTE_CARRIER_ID_TAG;
  buffer[offset++] = context->cc_idx;

  /* NB-IoT mode tag */
  buffer[offset++] = MAC_LTE_NB_MODE_TAG;
  buffer[offset++] = context->nbiotMode;

  /* Data tag immediately preceding PDU */
  buffer[offset++] = MAC_LTE_PAYLOAD_TAG;
  return offset;
}

/* Write an individual PDU (PCAP packet header + mac-context + mac-pdu) */
inline int LTE_PCAP_MAC_WritePDU(FILE* fd, MAC_Context_Info_t* context, const unsigned char* PDU, unsigned int length)
{
  pcaprec_hdr_t packet_header;
  uint8_t       context_header[PCAP_CONTEXT_HEADER_MAX];
  int           offset = 0;

  /* Can't write if file wasn't successfully opened */
  if (fd == NULL) {
    printf("Error: Can't write to empty file handle\n");
    return 0;
  }

  offset += LTE_PCAP_PACK_MAC_CONTEXT_TO_BUFFER(context, &context_header[offset], PCAP_CONTEXT_HEADER_MAX);

  /****************************************************************/
  /* PCAP Header                                                  */
  struct timeval t;
  gettimeofday(&t, NULL);
  packet_header.ts_sec   = t.tv_sec;
  packet_header.ts_usec  = t.tv_usec;
  packet_header.incl_len = offset + length;
  packet_header.orig_len = offset + length;

  /***************************************************************/
  /* Now write everything to the file                            */
  fwrite(&packet_header, sizeof(pcaprec_hdr_t), 1, fd);
  fwrite(context_header, 1, offset, fd);
  fwrite(PDU, 1, length, fd);

  return 1;
}

/* Write an individual PDU (PCAP packet header + mac-context + mac-pdu) */
inline int
LTE_PCAP_MAC_UDP_WritePDU(FILE* fd, MAC_Context_Info_t* context, const unsigned char* PDU, unsigned int length)
{
  pcaprec_hdr_t  packet_header;
  uint8_t        context_header[PCAP_CONTEXT_HEADER_MAX] = {};
  int            offset                                  = 0;
  struct udphdr* udp_header;
  // uint16_t       tmp16;

  /* Can't write if file wasn't successfully opened */
  if (fd == NULL) {
    printf("Error: Can't write to empty file handle\n");
    return 0;
  }
  // Add dummy UDP header, start with src and dest port
  udp_header       = (struct udphdr*)context_header;
  udp_header->dest = htons(0xdead);
  offset += 2;
  udp_header->source = htons(0xbeef);
  offset += 2;
  // length to be filled later
  udp_header->len = 0x0000;
  offset += 2;
  // dummy CRC
  udp_header->check = 0x0000;
  offset += 2;

  // Start magic string
  memcpy(&context_header[offset], MAC_LTE_START_STRING, strlen(MAC_LTE_START_STRING));
  offset += strlen(MAC_LTE_START_STRING);

  offset += LTE_PCAP_PACK_MAC_CONTEXT_TO_BUFFER(context, &context_header[offset], PCAP_CONTEXT_HEADER_MAX);
  udp_header->len = htons(length + offset);

  /****************************************************************/
  /* PCAP Header                                                  */
  struct timeval t;
  gettimeofday(&t, NULL);
  packet_header.ts_sec   = t.tv_sec;
  packet_header.ts_usec  = t.tv_usec;
  packet_header.incl_len = offset + length;
  packet_header.orig_len = offset + length;

  /***************************************************************/
  /* Now write everything to the file                            */
  fwrite(&packet_header, sizeof(pcaprec_hdr_t), 1, fd);
  fwrite(context_header, 1, offset, fd);
  fwrite(PDU, 1, length, fd);

  return 1;
}

/* Write an individual PDU (PCAP packet header + nas-context + nas-pdu) */
int LTE_PCAP_NAS_WritePDU(FILE* fd, NAS_Context_Info_t* context, const unsigned char* PDU, unsigned int length)
{
  pcaprec_hdr_t packet_header;

  /* Can't write if file wasn't successfully opened */
  if (fd == NULL) {
    printf("Error: Can't write to empty file handle\n");
    return 0;
  }

  /****************************************************************/
  /* PCAP Header                                                  */
  struct timeval t;
  gettimeofday(&t, NULL);
  packet_header.ts_sec   = t.tv_sec;
  packet_header.ts_usec  = t.tv_usec;
  packet_header.incl_len = length;
  packet_header.orig_len = length;

  /***************************************************************/
  /* Now write everything to the file                            */
  fwrite(&packet_header, sizeof(pcaprec_hdr_t), 1, fd);
  fwrite(PDU, 1, length, fd);

  return 1;
}

/**************************************************************************
 * API functions for writing RLC-LTE PCAP files                           *
 **************************************************************************/

/* Write an individual RLC PDU (PCAP packet header + UDP header + rlc-context + rlc-pdu) */
int LTE_PCAP_RLC_WritePDU(FILE* fd, RLC_Context_Info_t* context, const unsigned char* PDU, unsigned int length)
{
  pcaprec_hdr_t packet_header;
  char          context_header[PCAP_CONTEXT_HEADER_MAX] = {};
  int           offset                                  = 0;
  uint16_t      tmp16;

  /* Can't write if file wasn't successfully opened */
  if (fd == NULL) {
    printf("Error: Can't write to empty file handle\n");
    return 0;
  }

  /*****************************************************************/

  // Add dummy UDP header, start with src and dest port
  context_header[offset++] = 0xde;
  context_header[offset++] = 0xad;
  context_header[offset++] = 0xbe;
  context_header[offset++] = 0xef;
  // length
  tmp16 = length + 30;
  if (context->rlcMode == RLC_UM_MODE) {
    tmp16 += 2; // RLC UM requires two bytes more for SN length (see below
  }
  context_header[offset++] = (tmp16 & 0xff00) >> 8;
  context_header[offset++] = (tmp16 & 0xff);
  // dummy CRC
  context_header[offset++] = 0xde;
  context_header[offset++] = 0xad;

  // Start magic string
  memcpy(&context_header[offset], RLC_LTE_START_STRING, strlen(RLC_LTE_START_STRING));
  offset += strlen(RLC_LTE_START_STRING);

  // Fixed field RLC mode
  context_header[offset++] = context->rlcMode;

  // Conditional fields
  if (context->rlcMode == RLC_UM_MODE) {
    context_header[offset++] = RLC_LTE_SN_LENGTH_TAG;
    context_header[offset++] = context->sequenceNumberLength;
  }

  // Optional fields
  context_header[offset++] = RLC_LTE_DIRECTION_TAG;
  context_header[offset++] = context->direction;

  context_header[offset++] = RLC_LTE_PRIORITY_TAG;
  context_header[offset++] = context->priority;

  context_header[offset++] = RLC_LTE_UEID_TAG;
  tmp16                    = htons(context->ueid);
  memcpy(context_header + offset, &tmp16, 2);
  offset += 2;

  context_header[offset++] = RLC_LTE_CHANNEL_TYPE_TAG;
  tmp16                    = htons(context->channelType);
  memcpy(context_header + offset, &tmp16, 2);
  offset += 2;

  context_header[offset++] = RLC_LTE_CHANNEL_ID_TAG;
  tmp16                    = htons(context->channelId);
  memcpy(context_header + offset, &tmp16, 2);
  offset += 2;

  // Now the actual PDU
  context_header[offset++] = RLC_LTE_PAYLOAD_TAG;

  // PCAP header
  struct timeval t;
  gettimeofday(&t, NULL);
  packet_header.ts_sec   = t.tv_sec;
  packet_header.ts_usec  = t.tv_usec;
  packet_header.incl_len = offset + length;
  packet_header.orig_len = offset + length;

  // Write everything to file
  fwrite(&packet_header, sizeof(pcaprec_hdr_t), 1, fd);
  fwrite(context_header, 1, offset, fd);
  fwrite(PDU, 1, length, fd);

  return 1;
}

/* Write an individual PDU (PCAP packet header + s1ap-context + s1ap-pdu) */
int LTE_PCAP_S1AP_WritePDU(FILE* fd, S1AP_Context_Info_t* context, const unsigned char* PDU, unsigned int length)
{
  pcaprec_hdr_t packet_header;

  /* Can't write if file wasn't successfully opened */
  if (fd == NULL) {
    printf("Error: Can't write to empty file handle\n");
    return 0;
  }

  /****************************************************************/
  /* PCAP Header                                                  */
  struct timeval t;
  gettimeofday(&t, NULL);
  packet_header.ts_sec   = t.tv_sec;
  packet_header.ts_usec  = t.tv_usec;
  packet_header.incl_len = length;
  packet_header.orig_len = length;

  /***************************************************************/
  /* Now write everything to the file                            */
  fwrite(&packet_header, sizeof(pcaprec_hdr_t), 1, fd);
  fwrite(PDU, 1, length, fd);

  return 1;
}

/**************************************************************************
 * API functions for writing MAC-NR PCAP files                           *
 **************************************************************************/
inline int NR_PCAP_PACK_MAC_CONTEXT_TO_BUFFER(mac_nr_context_info_t* context, uint8_t* buffer, unsigned int length)
{
  int      offset = 0;
  uint16_t tmp16;

  if (buffer == NULL || length < PCAP_CONTEXT_HEADER_MAX) {
    printf("Error: Writing buffer null or length to small \n");
    return -1;
  }

  /*****************************************************************/
  /* Context information (same as written by UDP heuristic clients */
  buffer[offset++] = context->radioType;
  buffer[offset++] = context->direction;
  buffer[offset++] = context->rntiType;

  /* RNTI */
  buffer[offset++] = MAC_LTE_RNTI_TAG;
  tmp16            = htons(context->rnti);
  memcpy(buffer + offset, &tmp16, 2);
  offset += 2;

  /* UEId */
  buffer[offset++] = MAC_LTE_UEID_TAG;
  tmp16            = htons(context->ueid);
  memcpy(buffer + offset, &tmp16, 2);
  offset += 2;

  /* HARQID */
  buffer[offset++] = MAC_NR_HARQID;
  buffer[offset++] = context->harqid;

  /* PHR Type2 other cell */
  buffer[offset++] = MAC_NR_PHR_TYPE2_OTHERCELL_TAG;
  buffer[offset++] = context->phr_type2_othercell;

  /* Subframe Number and System Frame Number */
  /* SFN is stored in 12 MSB and SF in 4 LSB */
  buffer[offset++] = MAC_LTE_FRAME_SUBFRAME_TAG;
  tmp16            = (context->system_frame_number << 4) | context->sub_frame_number;
  tmp16            = htons(tmp16);
  memcpy(buffer + offset, &tmp16, 2);
  offset += 2;

  /* Data tag immediately preceding PDU */
  buffer[offset++] = MAC_LTE_PAYLOAD_TAG;
  return offset;
}

/* Write an individual NR MAC PDU (PCAP packet header + UDP header + nr-mac-context + mac-pdu) */
int NR_PCAP_MAC_UDP_WritePDU(FILE* fd, mac_nr_context_info_t* context, const unsigned char* PDU, unsigned int length)
{
  uint8_t        context_header[PCAP_CONTEXT_HEADER_MAX] = {};
  struct udphdr* udp_header;
  int            offset = 0;

  /* Can't write if file wasn't successfully opened */
  if (fd == NULL) {
    printf("Error: Can't write to empty file handle\n");
    return -1;
  }

  // Add dummy UDP header, start with src and dest port
  udp_header       = (struct udphdr*)context_header;
  udp_header->dest = htons(0xdead);
  offset += 2;
  udp_header->source = htons(0xbeef);
  offset += 2;
  // length to be filled later
  udp_header->len = 0x0000;
  offset += 2;
  // dummy CRC
  udp_header->check = 0x0000;
  offset += 2;

  // Start magic string
  memcpy(&context_header[offset], MAC_NR_START_STRING, strlen(MAC_NR_START_STRING));
  offset += strlen(MAC_NR_START_STRING);

  offset += NR_PCAP_PACK_MAC_CONTEXT_TO_BUFFER(context, &context_header[offset], PCAP_CONTEXT_HEADER_MAX);

  udp_header->len = htons(offset + length);

  if (offset != 31) {
    printf("ERROR Does not match offset %d != 31\n", offset);
  }

  /****************************************************************/
  /* PCAP Header                                                  */
  struct timeval t;
  gettimeofday(&t, NULL);
  pcaprec_hdr_t packet_header;
  packet_header.ts_sec   = t.tv_sec;
  packet_header.ts_usec  = t.tv_usec;
  packet_header.incl_len = offset + length;
  packet_header.orig_len = offset + length;

  /***************************************************************/
  /* Now write everything to the file                            */
  fwrite(&packet_header, sizeof(pcaprec_hdr_t), 1, fd);
  fwrite(context_header, 1, offset, fd);
  fwrite(PDU, 1, length, fd);

  return 1;
}