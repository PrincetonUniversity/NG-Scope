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

#include "srsepc/hdr/spgw/gtpc.h"
#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <inttypes.h> // for printing uint64_t
#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/ip.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>

namespace srsepc {

/**********************************************
 *
 * GTP-C class that handles the GTP-C protocol
 * comminication with the MME
 *
 **********************************************/
spgw::gtpc::gtpc() : m_h_next_ue_ip(0), m_next_ctrl_teid(1), m_next_user_teid(1), m_max_paging_queue(0)
{
  return;
}

spgw::gtpc::~gtpc()
{
  return;
}

int spgw::gtpc::init(spgw_args_t*                           args,
                     spgw*                                  spgw,
                     gtpu_interface_gtpc*                   gtpu,
                     const std::map<std::string, uint64_t>& ip_to_imsi)
{
  int err;

  // Init interfaces
  m_spgw = spgw;
  m_gtpu = gtpu;

  // Init S11 interface
  err = init_s11(args);
  if (err != SRSRAN_SUCCESS) {
    srsran::console("Could not initialize the S11 interface.\n");
    return err;
  }

  // Init IP pool
  err = init_ue_ip(args, ip_to_imsi);
  if (err != SRSRAN_SUCCESS) {
    srsran::console("Could not initialize the IP pool.\n");
    return err;
  }

  // Limit paging queue
  m_max_paging_queue = args->max_paging_queue;

  m_logger.info("SPGW S11 Initialized.");
  srsran::console("SPGW S11 Initialized.\n");
  return 0;
}

void spgw::gtpc::stop()
{
  std::map<uint32_t, spgw_tunnel_ctx*>::iterator it = m_teid_to_tunnel_ctx.begin();
  while (it != m_teid_to_tunnel_ctx.end()) {
    m_logger.info("Deleting SP-GW GTP-C Tunnel. IMSI: %015" PRIu64 "", it->second->imsi);
    srsran::console("Deleting SP-GW GTP-C Tunnel. IMSI: %015" PRIu64 "\n", it->second->imsi);
    delete it->second;
    m_teid_to_tunnel_ctx.erase(it++);
  }
  return;
}

int spgw::gtpc::init_s11(spgw_args_t* args)
{
  socklen_t sock_len;
  char      spgw_addr_name[] = "@spgw_s11";
  char      mme_addr_name[]  = "@mme_s11";

  // Logs
  m_logger.info("Initializing SPGW S11 interface.");

  // Open Socket
  m_s11 = socket(AF_UNIX, SOCK_DGRAM, 0);
  if (m_s11 < 0) {
    m_logger.error("Error opening UNIX socket. Error %s", strerror(errno));
    return SRSRAN_ERROR_CANT_START;
  }

  // Set MME Address
  memset(&m_mme_addr, 0, sizeof(struct sockaddr_un));
  m_mme_addr.sun_family = AF_UNIX;
  snprintf(m_mme_addr.sun_path, sizeof(m_mme_addr.sun_path), "%s", mme_addr_name);
  m_mme_addr.sun_path[0] = '\0';

  // Set SPGW Address
  memset(&m_spgw_addr, 0, sizeof(struct sockaddr_un));
  m_spgw_addr.sun_family = AF_UNIX;
  snprintf(m_spgw_addr.sun_path, sizeof(m_spgw_addr.sun_path), "%s", spgw_addr_name);
  m_spgw_addr.sun_path[0] = '\0';

  // Bind socket to address
  if (bind(m_s11, (const struct sockaddr*)&m_spgw_addr, sizeof(m_spgw_addr)) == -1) {
    m_logger.error("Error binding UNIX socket. Error %s", strerror(errno));
    return SRSRAN_ERROR_CANT_START;
  }
  return SRSRAN_SUCCESS;
}

bool spgw::gtpc::send_s11_pdu(const srsran::gtpc_pdu& pdu)
{
  m_logger.debug("SPGW Sending S11 PDU! N_Bytes: %zd", sizeof(pdu));

  // TODO add serialization code here
  // Send S11 message to MME
  int n = sendto(m_s11, &pdu, sizeof(pdu), 0, (const sockaddr*)&m_mme_addr, sizeof(m_mme_addr));
  if (n < 0) {
    m_logger.error("Error sending to socket. Error %s", strerror(errno));
    return false;
  } else {
    m_logger.debug("SPGW S11 Sent %d Bytes.", n);
  }
  return true;
}

void spgw::gtpc::handle_s11_pdu(srsran::byte_buffer_t* msg)
{
  // TODO add deserialization code here
  srsran::gtpc_pdu* pdu = (srsran::gtpc_pdu*)msg->msg;
  srsran::console("Received GTP-C PDU. Message type: %s\n", srsran::gtpc_msg_type_to_str(pdu->header.type));
  m_logger.debug("Received GTP-C PDU. Message type: %s", srsran::gtpc_msg_type_to_str(pdu->header.type));
  switch (pdu->header.type) {
    case srsran::GTPC_MSG_TYPE_CREATE_SESSION_REQUEST:
      handle_create_session_request(pdu->choice.create_session_request);
      break;
    case srsran::GTPC_MSG_TYPE_MODIFY_BEARER_REQUEST:
      handle_modify_bearer_request(pdu->header, pdu->choice.modify_bearer_request);
      break;
    case srsran::GTPC_MSG_TYPE_DELETE_SESSION_REQUEST:
      handle_delete_session_request(pdu->header, pdu->choice.delete_session_request);
      break;
    case srsran::GTPC_MSG_TYPE_RELEASE_ACCESS_BEARERS_REQUEST:
      handle_release_access_bearers_request(pdu->header, pdu->choice.release_access_bearers_request);
      break;
    case srsran::GTPC_MSG_TYPE_DOWNLINK_DATA_NOTIFICATION_ACKNOWLEDGE:
      handle_downlink_data_notification_acknowledge(pdu->header, pdu->choice.downlink_data_notification_acknowledge);
      break;
    case srsran::GTPC_MSG_TYPE_DOWNLINK_DATA_NOTIFICATION_FAILURE_INDICATION:
      handle_downlink_data_notification_failure_indication(pdu->header,
                                                           pdu->choice.downlink_data_notification_failure_indication);
      break;
    default:
      m_logger.error("Unhandled GTP-C message type");
  }
  return;
}

void spgw::gtpc::handle_create_session_request(const struct srsran::gtpc_create_session_request& cs_req)
{
  m_logger.info("SPGW Received Create Session Request");
  spgw_tunnel_ctx_t* tunnel_ctx;
  int                default_bearer_id = 5;
  // Check if IMSI has active GTP-C and/or GTP-U
  bool gtpc_present = m_imsi_to_ctr_teid.count(cs_req.imsi);
  if (gtpc_present) {
    srsran::console("SPGW: GTP-C context for IMSI %015" PRIu64 " already exists.\n", cs_req.imsi);
    delete_gtpc_ctx(m_imsi_to_ctr_teid[cs_req.imsi]);
    srsran::console("SPGW: Deleted previous context.\n");
  }

  m_logger.info("Creating new GTP-C context");
  tunnel_ctx = create_gtpc_ctx(cs_req);

  // Create session response message
  srsran::gtpc_pdu cs_resp_pdu;
  std::memset(&cs_resp_pdu, 0, sizeof(cs_resp_pdu));
  srsran::gtpc_header*                  header  = &cs_resp_pdu.header;
  srsran::gtpc_create_session_response* cs_resp = &cs_resp_pdu.choice.create_session_response;

  // Setup GTP-C header
  header->piggyback    = false;
  header->teid_present = true;
  header->teid         = tunnel_ctx->dw_ctrl_fteid.teid; // Send create session response to the UE's MME Ctrl TEID
  header->type         = srsran::GTPC_MSG_TYPE_CREATE_SESSION_RESPONSE;

  // Initialize to zero
  std::memset(cs_resp, 0, sizeof(struct srsran::gtpc_create_session_response));
  // Setup Cause
  cs_resp->cause.cause_value = srsran::GTPC_CAUSE_VALUE_REQUEST_ACCEPTED;
  // Setup sender F-TEID (ctrl)
  cs_resp->sender_f_teid.ipv4_present = true;
  cs_resp->sender_f_teid              = tunnel_ctx->up_ctrl_fteid;

  // Bearer context created
  cs_resp->eps_bearer_context_created.ebi                     = default_bearer_id;
  cs_resp->eps_bearer_context_created.cause.cause_value       = srsran::GTPC_CAUSE_VALUE_REQUEST_ACCEPTED;
  cs_resp->eps_bearer_context_created.s1_u_sgw_f_teid_present = true;
  cs_resp->eps_bearer_context_created.s1_u_sgw_f_teid         = tunnel_ctx->up_user_fteid;

  // Fill in the PAA
  cs_resp->paa_present      = true;
  cs_resp->paa.pdn_type     = srsran::GTPC_PDN_TYPE_IPV4;
  cs_resp->paa.ipv4_present = true;
  cs_resp->paa.ipv4         = tunnel_ctx->ue_ipv4;
  m_logger.info("Sending Create Session Response");

  // Send Create session response to MME
  send_s11_pdu(cs_resp_pdu);
  return;
}

void spgw::gtpc::handle_modify_bearer_request(const struct srsran::gtpc_header&                mb_req_hdr,
                                              const struct srsran::gtpc_modify_bearer_request& mb_req)
{
  m_logger.info("Received Modified Bearer Request");

  // Get control tunnel info from mb_req PDU
  uint32_t                                         ctrl_teid = mb_req_hdr.teid;
  std::map<uint32_t, spgw_tunnel_ctx_t*>::iterator tunnel_it = m_teid_to_tunnel_ctx.find(ctrl_teid);
  if (tunnel_it == m_teid_to_tunnel_ctx.end()) {
    m_logger.warning("Could not find TEID %d to modify", ctrl_teid);
    return;
  }
  spgw_tunnel_ctx_t* tunnel_ctx = tunnel_it->second;

  // Store user DW link TEID
  tunnel_ctx->dw_user_fteid.teid = mb_req.eps_bearer_context_to_modify.s1_u_enb_f_teid.teid;
  tunnel_ctx->dw_user_fteid.ipv4 = mb_req.eps_bearer_context_to_modify.s1_u_enb_f_teid.ipv4;
  // Set up actual tunnel
  m_logger.info("Setting Up GTP-U tunnel. Tunnel info: ");
  struct in_addr addr;
  addr.s_addr = tunnel_ctx->ue_ipv4;
  m_logger.info("IMSI: %015" PRIu64 ", UE IP: %s ", tunnel_ctx->imsi, inet_ntoa(addr));
  m_logger.info(
      "S-GW Rx Ctrl TEID 0x%x, MME Rx Ctrl TEID 0x%x", tunnel_ctx->up_ctrl_fteid.teid, tunnel_ctx->dw_ctrl_fteid.teid);
  m_logger.info("S-GW Rx Ctrl IP (NA), MME Rx Ctrl IP (NA)");

  struct in_addr addr2;
  addr2.s_addr = tunnel_ctx->up_user_fteid.ipv4;
  m_logger.info("S-GW Rx User TEID 0x%x, S-GW Rx User IP %s", tunnel_ctx->up_user_fteid.teid, inet_ntoa(addr2));

  struct in_addr addr3;
  addr3.s_addr = tunnel_ctx->dw_user_fteid.ipv4;
  m_logger.info("eNB Rx User TEID 0x%x, eNB Rx User IP %s", tunnel_ctx->dw_user_fteid.teid, inet_ntoa(addr3));

  // Setup IP to F-TEID map
  m_gtpu->modify_gtpu_tunnel(tunnel_ctx->ue_ipv4, tunnel_ctx->dw_user_fteid, tunnel_ctx->up_ctrl_fteid.teid);

  // Mark paging as done & send queued packets
  if (tunnel_ctx->paging_pending == true) {
    tunnel_ctx->paging_pending = false;
    m_logger.debug("Modify Bearer Request received after Downling Data Notification was sent");
    srsran::console("Modify Bearer Request received after Downling Data Notification was sent\n");
    m_gtpu->send_all_queued_packets(tunnel_ctx->dw_user_fteid, tunnel_ctx->paging_queue);
  }

  // Setting up Modify bearer response PDU
  // Header
  srsran::gtpc_pdu mb_resp_pdu;
  std::memset(&mb_resp_pdu, 0, sizeof(mb_resp_pdu));
  srsran::gtpc_header* header = &mb_resp_pdu.header;
  header->piggyback           = false;
  header->teid_present        = true;
  header->teid                = tunnel_ctx->dw_ctrl_fteid.teid;
  header->type                = srsran::GTPC_MSG_TYPE_MODIFY_BEARER_RESPONSE;

  // PDU
  srsran::gtpc_modify_bearer_response* mb_resp           = &mb_resp_pdu.choice.modify_bearer_response;
  mb_resp->cause.cause_value                             = srsran::GTPC_CAUSE_VALUE_REQUEST_ACCEPTED;
  mb_resp->eps_bearer_context_modified.ebi               = tunnel_ctx->ebi;
  mb_resp->eps_bearer_context_modified.cause.cause_value = srsran::GTPC_CAUSE_VALUE_REQUEST_ACCEPTED;

  // Send Modify Bearer Response PDU
  send_s11_pdu(mb_resp_pdu);
  return;
}

void spgw::gtpc::handle_delete_session_request(const srsran::gtpc_header&                 header,
                                               const srsran::gtpc_delete_session_request& del_req_pdu)
{
  uint32_t                                         ctrl_teid = header.teid;
  std::map<uint32_t, spgw_tunnel_ctx_t*>::iterator tunnel_it = m_teid_to_tunnel_ctx.find(ctrl_teid);
  if (tunnel_it == m_teid_to_tunnel_ctx.end()) {
    m_logger.warning("Could not find TEID 0x%x to delete session", ctrl_teid);
    return;
  }
  spgw_tunnel_ctx_t* tunnel_ctx = tunnel_it->second;
  in_addr_t          ue_ipv4    = tunnel_ctx->ue_ipv4;
  m_gtpu->delete_gtpu_tunnel(ue_ipv4);
  delete_gtpc_ctx(ctrl_teid);
  return;
}

void spgw::gtpc::handle_release_access_bearers_request(const srsran::gtpc_header&                         header,
                                                       const srsran::gtpc_release_access_bearers_request& rel_req)
{
  // Find tunel ctxt
  uint32_t                                         ctrl_teid = header.teid;
  std::map<uint32_t, spgw_tunnel_ctx_t*>::iterator tunnel_it = m_teid_to_tunnel_ctx.find(ctrl_teid);
  if (tunnel_it == m_teid_to_tunnel_ctx.end()) {
    m_logger.warning("Could not find TEID 0x%x to release bearers", ctrl_teid);
    return;
  }
  spgw_tunnel_ctx_t* tunnel_ctx = tunnel_it->second;
  in_addr_t          ue_ipv4    = tunnel_ctx->ue_ipv4;

  // Delete data tunnel & do NOT delete control tunnel
  m_gtpu->delete_gtpu_tunnel(ue_ipv4);
  return;
}

bool spgw::gtpc::send_downlink_data_notification(uint32_t spgw_ctr_teid)
{
  m_logger.debug("Sending Downlink Notification Request");

  struct srsran::gtpc_pdu dl_not_pdu;
  std::memset(&dl_not_pdu, 0, sizeof(dl_not_pdu));
  struct srsran::gtpc_header*                     header = &dl_not_pdu.header;
  struct srsran::gtpc_downlink_data_notification* dl_not = &dl_not_pdu.choice.downlink_data_notification;

  // Find MME Ctrl TEID
  std::map<uint32_t, spgw_tunnel_ctx_t*>::iterator tunnel_it = m_teid_to_tunnel_ctx.find(spgw_ctr_teid);
  if (tunnel_it == m_teid_to_tunnel_ctx.end()) {
    m_logger.warning("Could not find TEID 0x%x to send downlink notification.", spgw_ctr_teid);
    return false;
  }
  spgw_tunnel_ctx_t* tunnel_ctx = tunnel_it->second;

  // Check if there is no Paging already pending.
  if (tunnel_ctx->paging_pending == true) {
    m_logger.debug("UE Downlink Data Notification still pending.");
    return false;
  }

  tunnel_ctx->paging_pending = true;
  srsran::console("Found UE for Downlink Notification \n");
  srsran::console("MME Ctr TEID 0x%x, IMSI: %015" PRIu64 "\n", tunnel_ctx->dw_ctrl_fteid.teid, tunnel_ctx->imsi);

  // Setup GTP-C header
  header->piggyback    = false;
  header->teid_present = true;
  header->teid         = tunnel_ctx->dw_ctrl_fteid.teid; // Send downlink data notification to the UE's MME Ctrl TEID
  header->type         = srsran::GTPC_MSG_TYPE_DOWNLINK_DATA_NOTIFICATION;

  dl_not->eps_bearer_id_present = true;
  dl_not->eps_bearer_id         = 5; // Only default bearer supported.

  // Send Downlink Data Notification PDU
  send_s11_pdu(dl_not_pdu);
  return true;
}

void spgw::gtpc::handle_downlink_data_notification_acknowledge(
    const srsran::gtpc_header&                                 header,
    const srsran::gtpc_downlink_data_notification_acknowledge& not_ack)
{
  m_logger.debug("Handling downlink data notification acknowledge");

  // Find tunel ctxt
  uint32_t                                         ctrl_teid = header.teid;
  std::map<uint32_t, spgw_tunnel_ctx_t*>::iterator tunnel_it = m_teid_to_tunnel_ctx.find(ctrl_teid);
  if (tunnel_it == m_teid_to_tunnel_ctx.end()) {
    m_logger.warning("Could not find TEID 0x%x to handle notification acknowldge", ctrl_teid);
    return;
  }
  spgw_tunnel_ctx_t* tunnel_ctx = tunnel_it->second;
  if (not_ack.cause.cause_value == srsran::GTPC_CAUSE_VALUE_CONTEXT_NOT_FOUND ||
      not_ack.cause.cause_value == srsran::GTPC_CAUSE_VALUE_UE_ALREADY_RE_ATTACHED ||
      not_ack.cause.cause_value == srsran::GTPC_CAUSE_VALUE_UNABLE_TO_PAGE_UE ||
      not_ack.cause.cause_value == srsran::GTPC_CAUSE_VALUE_UNABLE_TO_PAGE_UE_DUE_TO_SUSPENSION) {
    m_logger.warning("Downlink Data Notification Acknowledge indicates failure.");
    free_all_queued_packets(tunnel_ctx);
    tunnel_ctx->paging_pending = false;
  } else if (not_ack.cause.cause_value != srsran::GTPC_CAUSE_VALUE_REQUEST_ACCEPTED) {
    m_logger.warning("Invalid cause in Downlink Data Notification Acknowledge.");
    free_all_queued_packets(tunnel_ctx);
    tunnel_ctx->paging_pending = false;
  }
  return;
}

void spgw::gtpc::handle_downlink_data_notification_failure_indication(
    const srsran::gtpc_header&                                        header,
    const srsran::gtpc_downlink_data_notification_failure_indication& not_fail)
{
  m_logger.debug("Handling downlink data notification failure indication");
  // Find tunel ctxt
  uint32_t                                         ctrl_teid = header.teid;
  std::map<uint32_t, spgw_tunnel_ctx_t*>::iterator tunnel_it = m_teid_to_tunnel_ctx.find(ctrl_teid);
  if (tunnel_it == m_teid_to_tunnel_ctx.end()) {
    m_logger.warning("Could not find TEID 0x%x to handle notification failure indication", ctrl_teid);
    return;
  }

  spgw_tunnel_ctx_t* tunnel_ctx = tunnel_it->second;
  if (not_fail.cause.cause_value == srsran::GTPC_CAUSE_VALUE_UE_NOT_RESPONDING ||
      not_fail.cause.cause_value == srsran::GTPC_CAUSE_VALUE_SERVICE_DENIED ||
      not_fail.cause.cause_value == srsran::GTPC_CAUSE_VALUE_UE_ALREADY_RE_ATTACHED) {
    m_logger.debug("Downlink Data Notification failure indication cause: %d.", not_fail.cause.cause_value);
  } else {
    m_logger.warning("Invalid cause in Downlink Data Notification Failure Indication %d", not_fail.cause.cause_value);
  }
  free_all_queued_packets(tunnel_ctx);
  tunnel_ctx->paging_pending = false;
  return;
}

/*
 * Context management functions
 */
spgw_tunnel_ctx_t* spgw::gtpc::create_gtpc_ctx(const struct srsran::gtpc_create_session_request& cs_req)
{
  // Setup uplink control TEID
  uint64_t spgw_uplink_ctrl_teid = get_new_ctrl_teid();
  // Setup uplink user TEID
  uint64_t spgw_uplink_user_teid = get_new_user_teid();
  // Allocate UE IP
  in_addr_t ue_ip = get_new_ue_ipv4(cs_req.imsi);

  uint8_t default_bearer_id = 5;

  srsran::console("SPGW: Allocated Ctrl TEID %" PRIu64 "\n", spgw_uplink_ctrl_teid);
  srsran::console("SPGW: Allocated User TEID %" PRIu64 "\n", spgw_uplink_user_teid);
  struct in_addr ue_ip_;
  ue_ip_.s_addr = ue_ip;
  srsran::console("SPGW: Allocate UE IP %s\n", inet_ntoa(ue_ip_));

  // Save the UE IP to User TEID map
  spgw_tunnel_ctx_t* tunnel_ctx = new spgw_tunnel_ctx_t{};

  tunnel_ctx->imsi = cs_req.imsi;
  tunnel_ctx->ebi  = default_bearer_id;

  tunnel_ctx->up_ctrl_fteid.teid = spgw_uplink_ctrl_teid;
  tunnel_ctx->ue_ipv4            = ue_ip;
  tunnel_ctx->up_user_fteid.teid = spgw_uplink_user_teid;
  tunnel_ctx->up_user_fteid.ipv4 = m_gtpu->get_s1u_addr();
  tunnel_ctx->dw_ctrl_fteid.teid = cs_req.sender_f_teid.teid;
  tunnel_ctx->dw_ctrl_fteid.ipv4 = cs_req.sender_f_teid.ipv4;
  std::memset(&tunnel_ctx->dw_user_fteid, 0, sizeof(srsran::gtp_fteid_t));

  m_teid_to_tunnel_ctx.insert(std::pair<uint32_t, spgw_tunnel_ctx_t*>(spgw_uplink_ctrl_teid, tunnel_ctx));
  m_imsi_to_ctr_teid.insert(std::pair<uint64_t, uint32_t>(cs_req.imsi, spgw_uplink_ctrl_teid));
  return tunnel_ctx;
}

bool spgw::gtpc::delete_gtpc_ctx(uint32_t ctrl_teid)
{
  spgw_tunnel_ctx_t* tunnel_ctx;
  if (!m_teid_to_tunnel_ctx.count(ctrl_teid)) {
    m_logger.error("Could not find GTP context to delete.");
    return false;
  }
  tunnel_ctx = m_teid_to_tunnel_ctx[ctrl_teid];

  // Remove Ctrl TEID from GTP-U Mapping
  m_gtpu->delete_gtpc_tunnel(tunnel_ctx->ue_ipv4);

  // Remove Ctrl TEID from IMSI to control TEID map
  m_imsi_to_ctr_teid.erase(tunnel_ctx->imsi);

  // Remove GTP context from control TEID mapping
  m_teid_to_tunnel_ctx.erase(ctrl_teid);
  delete tunnel_ctx;
  return true;
}

/*
 * Queueing functions
 */
bool spgw::gtpc::queue_downlink_packet(uint32_t ctrl_teid, srsran::unique_byte_buffer_t msg)
{
  spgw_tunnel_ctx_t* tunnel_ctx;
  if (!m_teid_to_tunnel_ctx.count(ctrl_teid)) {
    m_logger.error("Could not find GTP context to queue.");
    goto pkt_discard;
  }
  tunnel_ctx = m_teid_to_tunnel_ctx[ctrl_teid];
  if (!tunnel_ctx->paging_pending) {
    m_logger.error("Paging not pending. Not queueing packet");
    goto pkt_discard;
  }

  if (tunnel_ctx->paging_queue.size() < m_max_paging_queue) {
    tunnel_ctx->paging_queue.push(std::move(msg));
    m_logger.debug(
        "Queued packet. IMSI %" PRIu64 ", Packets in Queue %zd", tunnel_ctx->imsi, tunnel_ctx->paging_queue.size());
  } else {
    m_logger.debug(
        "Paging queue full. IMSI %" PRIu64 ", Packets in Queue %zd", tunnel_ctx->imsi, tunnel_ctx->paging_queue.size());
    goto pkt_discard;
  }
  return true;

pkt_discard:
  return false;
}

bool spgw::gtpc::free_all_queued_packets(spgw_tunnel_ctx_t* tunnel_ctx)
{
  if (!tunnel_ctx->paging_pending) {
    m_logger.info("Trying to free queued packets, but paging is not pending.");
  }

  while (!tunnel_ctx->paging_queue.empty()) {
    srsran::unique_byte_buffer_t pkt = std::move(tunnel_ctx->paging_queue.front());
    m_logger.debug("Dropping packet. Bytes %d", pkt->N_bytes);
    tunnel_ctx->paging_queue.pop();
  }
  return true;
}

int spgw::gtpc::init_ue_ip(spgw_args_t* args, const std::map<std::string, uint64_t>& ip_to_imsi)
{
  std::map<std::string, uint64_t>::const_iterator iter = ip_to_imsi.find(args->sgi_if_addr);

  // check for collision w/our ip address
  if (iter != ip_to_imsi.end()) {
    m_logger.error("SPGW: static ip addr %s for imsi %015" PRIu64 ", is reserved for the epc tun interface",
                   iter->first.c_str(),
                   iter->second);
    return SRSRAN_ERROR_OUT_OF_BOUNDS;
  }

  // load our imsi to ip lookup table
  for (std::map<std::string, uint64_t>::const_iterator iter = ip_to_imsi.begin(); iter != ip_to_imsi.end(); ++iter) {
    struct in_addr in_addr;
    if (inet_pton(AF_INET, iter->first.c_str(), &in_addr.s_addr) != 1) {
      perror("inet_pton");
      return SRSRAN_ERROR;
    }
    if (!m_imsi_to_ip.insert(std::make_pair(iter->second, in_addr)).second) {
      m_logger.error(
          "SPGW: duplicate imsi %015" PRIu64 " for static ip address %s.", iter->second, iter->first.c_str());
      return SRSRAN_ERROR_OUT_OF_BOUNDS;
    }
  }

  // XXX TODO add an upper bound to ip addr range via config, use 254 for now
  // first address is allocated to the epc tun interface, start w/next addr
  for (uint32_t n = 1; n < 254; ++n) {
    struct in_addr ue_addr;
    if (inet_pton(AF_INET, args->sgi_if_addr.c_str(), &ue_addr.s_addr) != 1) {
      m_logger.error("Invalid sgi_if_addr: %s", args->sgi_if_addr.c_str());
      srsran::console("Invalid sgi_if_addr: %s\n", args->sgi_if_addr.c_str());
      perror("inet_pton");
      return SRSRAN_ERROR;
    }
    ue_addr.s_addr = ue_addr.s_addr + htonl(n);

    std::map<std::string, uint64_t>::const_iterator iter = ip_to_imsi.find(inet_ntoa(ue_addr));
    if (iter != ip_to_imsi.end()) {
      m_logger.debug("SPGW: init_ue_ip ue ip addr %s is reserved for imsi %015" PRIu64 ", not adding to pool",
                     iter->first.c_str(),
                     iter->second);
    } else {
      m_ue_ip_addr_pool.insert(ue_addr.s_addr);
      m_logger.debug("SPGW: init_ue_ip ue ip addr %s is added to pool", inet_ntoa(ue_addr));
    }
  }
  return SRSRAN_SUCCESS;
}

in_addr_t spgw::gtpc::get_new_ue_ipv4(uint64_t imsi)
{
  struct in_addr ue_addr;

  std::map<uint64_t, struct in_addr>::const_iterator iter = m_imsi_to_ip.find(imsi);
  if (iter != m_imsi_to_ip.end()) {
    ue_addr = iter->second;
    m_logger.info("SPGW: get_new_ue_ipv4 static ip addr %s", inet_ntoa(ue_addr));
  } else {
    if (m_ue_ip_addr_pool.empty()) {
      m_logger.error("SPGW: ue address pool is empty");
      ue_addr.s_addr = 0;
    } else {
      ue_addr.s_addr = *m_ue_ip_addr_pool.begin();
      m_ue_ip_addr_pool.erase(ue_addr.s_addr);
      m_logger.info("SPGW: get_new_ue_ipv4 pool ip addr %s", inet_ntoa(ue_addr));
    }
  }
  return ue_addr.s_addr;
}

} // namespace srsepc
