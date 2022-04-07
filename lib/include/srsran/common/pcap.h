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

#ifndef SRSRAN_PCAP_H
#define SRSRAN_PCAP_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#define PCAP_CONTEXT_HEADER_MAX 256

#define MAC_LTE_DLT 147
#define NAS_LTE_DLT 148
#define UDP_DLT 149 // UDP needs to be selected as protocol
#define S1AP_LTE_DLT 150
#define NAS_5G_DLT 151

/* This structure gets written to the start of the file */
typedef struct pcap_hdr_s {
  unsigned int   magic_number;  /* magic number */
  unsigned short version_major; /* major version number */
  unsigned short version_minor; /* minor version number */
  unsigned int   thiszone;      /* GMT to local correction */
  unsigned int   sigfigs;       /* accuracy of timestamps */
  unsigned int   snaplen;       /* max length of captured packets, in octets */
  unsigned int   network;       /* data link type */
} pcap_hdr_t;

/* This structure precedes each packet */
typedef struct pcaprec_hdr_s {
  unsigned int ts_sec;   /* timestamp seconds */
  unsigned int ts_usec;  /* timestamp microseconds */
  unsigned int incl_len; /* number of octets of packet saved in file */
  unsigned int orig_len; /* actual length of packet */
} pcaprec_hdr_t;

/* radioType */
#define FDD_RADIO 1
#define TDD_RADIO 2

/* Direction */
#define DIRECTION_UPLINK 0
#define DIRECTION_DOWNLINK 1

/* rntiType */
#define NO_RNTI 0 /* Used for BCH-BCH */
#define P_RNTI 1
#define RA_RNTI 2
#define C_RNTI 3
#define SI_RNTI 4
#define SPS_RNTI 5
#define M_RNTI 6
#define SL_BCH_RNTI 7
#define SL_RNTI 8
#define SC_RNTI 9
#define G_RNTI 10

#define MAC_LTE_START_STRING "mac-lte"
#define MAC_LTE_PAYLOAD_TAG 0x01
#define MAC_LTE_RNTI_TAG 0x02
#define MAC_LTE_UEID_TAG 0x03
#define MAC_LTE_FRAME_SUBFRAME_TAG 0x04
#define MAC_LTE_PREDFINED_DATA_TAG 0x05
#define MAC_LTE_RETX_TAG 0x06
#define MAC_LTE_CRC_STATUS_TAG 0x07
#define MAC_LTE_CARRIER_ID_TAG 0x0A
#define MAC_LTE_NB_MODE_TAG 0x0F

/* Context information for every MAC PDU that will be logged */
typedef struct MAC_Context_Info_t {
  unsigned short radioType;
  unsigned char  direction;
  unsigned char  rntiType;
  unsigned short rnti;
  unsigned short ueid;
  unsigned char  isRetx;
  unsigned char  crcStatusOK;
  unsigned char  cc_idx;

  unsigned short sysFrameNumber;
  unsigned short subFrameNumber;

  unsigned char nbiotMode;
} MAC_Context_Info_t;

/* Context information for every NAS PDU that will be logged */
typedef struct NAS_Context_Info_s {
  // No Context yet
  unsigned char dummy;
} NAS_Context_Info_t;

#define MAC_NR_START_STRING "mac-nr"
#define MAC_NR_PHR_TYPE2_OTHERCELL_TAG 0x05
#define MAC_NR_HARQID 0x06

/* Context information for every MAC NR PDU that will be logged */
typedef struct {
  uint8_t  radioType;
  uint8_t  direction;
  uint8_t  rntiType;
  uint16_t rnti;
  uint16_t ueid;
  uint8_t  harqid;

  uint8_t phr_type2_othercell;

  uint16_t system_frame_number;
  uint8_t  sub_frame_number;
  uint16_t length;
} mac_nr_context_info_t;

/* RLC-LTE disector */

/* rlcMode */
#define RLC_TM_MODE 1
#define RLC_UM_MODE 2
#define RLC_AM_MODE 4
#define RLC_PREDEF 8

/* priority ? */

/* channelType */
#define CHANNEL_TYPE_CCCH 1
#define CHANNEL_TYPE_BCCH_BCH 2
#define CHANNEL_TYPE_PCCH 3
#define CHANNEL_TYPE_SRB 4
#define CHANNEL_TYPE_DRB 5
#define CHANNEL_TYPE_BCCH_DL_SCH 6
#define CHANNEL_TYPE_MCCH 7
#define CHANNEL_TYPE_MTCH 8

/* sequenceNumberLength */
#define UM_SN_LENGTH_5_BITS 5
#define UM_SN_LENGTH_10_BITS 10
#define AM_SN_LENGTH_10_BITS 10
#define AM_SN_LENGTH_16_BITS 16

/* Narrow band mode */
typedef enum { rlc_no_nb_mode = 0, rlc_nb_mode = 1 } rlc_lte_nb_mode;

/* Context information for every RLC PDU that will be logged */
typedef struct {
  unsigned char   rlcMode;
  unsigned char   direction;
  unsigned char   priority;
  unsigned char   sequenceNumberLength;
  unsigned short  ueid;
  unsigned short  channelType;
  unsigned short  channelId; /* for SRB: 1=SRB1, 2=SRB2, 3=SRB1bis; for DRB: DRB ID */
  unsigned short  pduLength;
  bool            extendedLiField;
  rlc_lte_nb_mode nbMode;
} RLC_Context_Info_t;

// See Wireshark's packet-rlc-lte.h for details
#define RLC_LTE_START_STRING "rlc-lte"
#define RLC_LTE_PAYLOAD_TAG 0x01
#define RLC_LTE_SN_LENGTH_TAG 0x02
#define RLC_LTE_DIRECTION_TAG 0x03
#define RLC_LTE_PRIORITY_TAG 0x04
#define RLC_LTE_UEID_TAG 0x05
#define RLC_LTE_CHANNEL_TYPE_TAG 0x06
#define RLC_LTE_CHANNEL_ID_TAG 0x07
#define RLC_LTE_EXT_LI_FIELD_TAG 0x08
#define RLC_LTE_NB_MODE_TAG 0x09

/* Context information for every S1AP PDU that will be logged */
typedef struct S1AP_Context_Info_s {
  // No Context yet
  unsigned char dummy;
} S1AP_Context_Info_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Open the file and write file header */
FILE* DLT_PCAP_Open(uint32_t DLT, const char* fileName);

/* Close the PCAP file */
void DLT_PCAP_Close(FILE* fd);

/* Write an individual MAC PDU (PCAP packet header + mac-context + mac-pdu) */
int LTE_PCAP_MAC_WritePDU(FILE* fd, MAC_Context_Info_t* context, const unsigned char* PDU, unsigned int length);
int LTE_PCAP_MAC_UDP_WritePDU(FILE* fd, MAC_Context_Info_t* context, const unsigned char* PDU, unsigned int length);
int LTE_PCAP_PACK_MAC_CONTEXT_TO_BUFFER(MAC_Context_Info_t* context, uint8_t* PDU, unsigned int length);

/* Write an individual NAS PDU (PCAP packet header + nas-context + nas-pdu) */
int LTE_PCAP_NAS_WritePDU(FILE* fd, NAS_Context_Info_t* context, const unsigned char* PDU, unsigned int length);

/* Write an individual RLC PDU (PCAP packet header + UDP header + rlc-context + rlc-pdu) */
int LTE_PCAP_RLC_WritePDU(FILE* fd, RLC_Context_Info_t* context, const unsigned char* PDU, unsigned int length);

/* Write an individual S1AP PDU (PCAP packet header + s1ap-context + s1ap-pdu) */
int LTE_PCAP_S1AP_WritePDU(FILE* fd, S1AP_Context_Info_t* context, const unsigned char* PDU, unsigned int length);

/* Write an individual NR MAC PDU (PCAP packet header + UDP header + nr-mac-context + mac-pdu) */
int NR_PCAP_MAC_UDP_WritePDU(FILE* fd, mac_nr_context_info_t* context, const unsigned char* PDU, unsigned int length);
int NR_PCAP_PACK_MAC_CONTEXT_TO_BUFFER(mac_nr_context_info_t* context, uint8_t* buffer, unsigned int length);

#ifdef __cplusplus
}
#endif

#endif // SRSRAN_PCAP_H
