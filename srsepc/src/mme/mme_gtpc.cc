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

#include "srsepc/hdr/mme/mme_gtpc.h"
#include "srsepc/hdr/mme/s1ap.h"
#include "srsepc/hdr/spgw/spgw.h"
#include "srsran/asn1/gtpc.h"
#include <inttypes.h> // for printing uint64_t

namespace srsepc {

mme_gtpc* mme_gtpc::get_instance()
{
  static std::unique_ptr<mme_gtpc> instance = std::unique_ptr<mme_gtpc>(new mme_gtpc);
  return instance.get();
}

bool mme_gtpc::init()
{
  m_next_ctrl_teid = 1;

  m_s1ap = s1ap::get_instance();

  if (!init_s11()) {
    m_logger.error("Error Initializing MME S11 Interface");
    return false;
  }

  m_logger.info("MME GTP-C Initialized");
  srsran::console("MME GTP-C Initialized\n");
  return true;
}

bool mme_gtpc::init_s11()
{

  socklen_t sock_len;
  char      mme_addr_name[]  = "@mme_s11";
  char      spgw_addr_name[] = "@spgw_s11";

  // Logs
  m_logger.info("Initializing MME S11 interface.");

  // Open Socket
  m_s11 = socket(AF_UNIX, SOCK_DGRAM, 0);
  if (m_s11 < 0) {
    m_logger.error("Error opening UNIX socket. Error %s", strerror(errno));
    return false;
  }

  // Set MME Address
  memset(&m_mme_addr, 0, sizeof(struct sockaddr_un));
  m_mme_addr.sun_family = AF_UNIX;
  snprintf(m_mme_addr.sun_path, sizeof(m_mme_addr.sun_path), "%s", mme_addr_name);
  m_mme_addr.sun_path[0] = '\0';

  // Bind socket to address
  if (bind(m_s11, (const struct sockaddr*)&m_mme_addr, sizeof(m_mme_addr)) == -1) {
    m_logger.error("Error binding UNIX socket. Error %s", strerror(errno));
    return false;
  }

  // Set SPGW Address for later use
  memset(&m_spgw_addr, 0, sizeof(struct sockaddr_un));
  m_spgw_addr.sun_family = AF_UNIX;
  snprintf(m_spgw_addr.sun_path, sizeof(m_spgw_addr.sun_path), "%s", spgw_addr_name);
  m_spgw_addr.sun_path[0] = '\0';

  m_logger.info("MME S11 Initialized");
  srsran::console("MME S11 Initialized\n");
  return true;
}

bool mme_gtpc::send_s11_pdu(const srsran::gtpc_pdu& pdu)
{
  int n;
  m_logger.debug("Sending S-11 GTP-C PDU");

  // TODO Add GTP-C serialization code
  // Send S11 message to SPGW
  n = sendto(m_s11, &pdu, sizeof(pdu), 0, (const sockaddr*)&m_spgw_addr, sizeof(m_spgw_addr));
  if (n < 0) {
    m_logger.error("Error sending to socket. Error %s", strerror(errno));
    srsran::console("Error sending to socket. Error %s\n", strerror(errno));
    return false;
  } else {
    m_logger.debug("MME S11 Sent %d Bytes.", n);
  }
  return true;
}

void mme_gtpc::handle_s11_pdu(srsran::byte_buffer_t* msg)
{
  m_logger.debug("Received S11 message");

  srsran::gtpc_pdu* pdu;
  pdu = (srsran::gtpc_pdu*)msg->msg;
  m_logger.debug("MME Received GTP-C PDU. Message type %s", srsran::gtpc_msg_type_to_str(pdu->header.type));
  switch (pdu->header.type) {
    case srsran::GTPC_MSG_TYPE_CREATE_SESSION_RESPONSE:
      handle_create_session_response(pdu);
      break;
    case srsran::GTPC_MSG_TYPE_MODIFY_BEARER_RESPONSE:
      handle_modify_bearer_response(pdu);
      break;
    case srsran::GTPC_MSG_TYPE_DOWNLINK_DATA_NOTIFICATION:
      handle_downlink_data_notification(pdu);
      break;
    default:
      m_logger.error("Unhandled GTP-C Message type");
  }
  return;
}

bool mme_gtpc::send_create_session_request(uint64_t imsi)
{
  m_logger.info("Sending Create Session Request.");
  srsran::console("Sending Create Session Request.\n");
  struct srsran::gtpc_pdu cs_req_pdu;
  // Initialize GTP-C message to zero
  std::memset(&cs_req_pdu, 0, sizeof(cs_req_pdu));

  struct srsran::gtpc_create_session_request* cs_req = &cs_req_pdu.choice.create_session_request;

  // Setup GTP-C Header. TODO: Length, sequence and other fields need to be added.
  cs_req_pdu.header.piggyback    = false;
  cs_req_pdu.header.teid_present = true;
  cs_req_pdu.header.teid         = 0; // Send create session request to the butler TEID
  cs_req_pdu.header.type         = srsran::GTPC_MSG_TYPE_CREATE_SESSION_REQUEST;

  // Setup GTP-C Create Session Request IEs
  cs_req->imsi = imsi;
  // Control TEID allocated
  cs_req->sender_f_teid.teid = get_new_ctrl_teid();

  m_logger.info("Next MME control TEID: %d", m_next_ctrl_teid);
  m_logger.info("Allocated MME control TEID: %d", cs_req->sender_f_teid.teid);
  srsran::console("Creating Session Response -- IMSI: %" PRIu64 "\n", imsi);
  srsran::console("Creating Session Response -- MME control TEID: %d\n", cs_req->sender_f_teid.teid);

  // APN
  strncpy(cs_req->apn, m_s1ap->m_s1ap_args.mme_apn.c_str(), sizeof(cs_req->apn) - 1);
  cs_req->apn[sizeof(cs_req->apn) - 1] = 0;

  // RAT Type
  // cs_req->rat_type = srsran::GTPC_RAT_TYPE::EUTRAN;

  // Bearer QoS
  cs_req->eps_bearer_context_created.ebi = 5;

  // Check whether this UE is already registed
  std::map<uint64_t, struct gtpc_ctx>::iterator it = m_imsi_to_gtpc_ctx.find(imsi);
  if (it != m_imsi_to_gtpc_ctx.end()) {
    m_logger.warning("Create Session Request being called for an UE with an active GTP-C connection.");
    m_logger.warning("Deleting previous GTP-C connection.");
    std::map<uint32_t, uint64_t>::iterator jt = m_mme_ctr_teid_to_imsi.find(it->second.mme_ctr_fteid.teid);
    if (jt == m_mme_ctr_teid_to_imsi.end()) {
      m_logger.error("Could not find IMSI from MME Ctrl TEID. MME Ctr TEID: %d", it->second.mme_ctr_fteid.teid);
    } else {
      m_mme_ctr_teid_to_imsi.erase(jt);
    }
    m_imsi_to_gtpc_ctx.erase(it);
    // No need to send delete session request to the SPGW.
    // The create session request will be interpreted as a new request and SPGW will delete locally in existing context.
  }

  // Save RX Control TEID
  m_mme_ctr_teid_to_imsi.insert(std::pair<uint32_t, uint64_t>(cs_req->sender_f_teid.teid, imsi));

  // Save GTP-C context
  gtpc_ctx_t gtpc_ctx;
  std::memset(&gtpc_ctx, 0, sizeof(gtpc_ctx_t));
  gtpc_ctx.mme_ctr_fteid = cs_req->sender_f_teid;
  m_imsi_to_gtpc_ctx.insert(std::pair<uint64_t, gtpc_ctx_t>(imsi, gtpc_ctx));

  // Send msg to SPGW
  send_s11_pdu(cs_req_pdu);
  return true;
}

bool mme_gtpc::handle_create_session_response(srsran::gtpc_pdu* cs_resp_pdu)
{
  struct srsran::gtpc_create_session_response* cs_resp = &cs_resp_pdu->choice.create_session_response;
  m_logger.info("Received Create Session Response");
  srsran::console("Received Create Session Response\n");
  if (cs_resp_pdu->header.type != srsran::GTPC_MSG_TYPE_CREATE_SESSION_RESPONSE) {
    m_logger.warning("Could not create GTPC session. Not a create session response");
    // TODO Handle error
    return false;
  }
  if (cs_resp->cause.cause_value != srsran::GTPC_CAUSE_VALUE_REQUEST_ACCEPTED) {
    m_logger.warning("Could not create GTPC session. Create Session Request not accepted");
    // TODO Handle error
    return false;
  }

  // Get IMSI from the control TEID
  std::map<uint32_t, uint64_t>::iterator id_it = m_mme_ctr_teid_to_imsi.find(cs_resp_pdu->header.teid);
  if (id_it == m_mme_ctr_teid_to_imsi.end()) {
    m_logger.warning("Could not find IMSI from Ctrl TEID.");
    return false;
  }
  uint64_t imsi = id_it->second;

  m_logger.info("MME GTPC Ctrl TEID %" PRIu64 ", IMSI %" PRIu64 "", cs_resp_pdu->header.teid, imsi);

  // Get S-GW Control F-TEID
  srsran::gtp_fteid_t sgw_ctr_fteid = {};
  sgw_ctr_fteid.teid                = cs_resp_pdu->header.teid;
  sgw_ctr_fteid.ipv4 = 0; // TODO This is not used for now. In the future it will be obtained from the socket addr_info

  // Get S-GW S1-u F-TEID
  if (cs_resp->eps_bearer_context_created.s1_u_sgw_f_teid_present == false) {
    m_logger.error("Did not receive SGW S1-U F-TEID in create session response");
    return false;
  }
  srsran::console("Create Session Response -- SPGW control TEID %d\n", sgw_ctr_fteid.teid);
  m_logger.info("Create Session Response -- SPGW control TEID %d", sgw_ctr_fteid.teid);
  in_addr s1u_addr;
  s1u_addr.s_addr = cs_resp->eps_bearer_context_created.s1_u_sgw_f_teid.ipv4;
  srsran::console("Create Session Response -- SPGW S1-U Address: %s\n", inet_ntoa(s1u_addr));
  m_logger.info("Create Session Response -- SPGW S1-U Address: %s", inet_ntoa(s1u_addr));

  // Check UE Ipv4 address was allocated
  if (cs_resp->paa_present != true) {
    m_logger.error("PDN Adress Allocation not present");
    return false;
  }
  if (cs_resp->paa.pdn_type != srsran::GTPC_PDN_TYPE_IPV4) {
    m_logger.error("IPv6 not supported yet");
    return false;
  }

  // Save create session response info to E-RAB context
  nas* nas_ctx = m_s1ap->find_nas_ctx_from_imsi(imsi);
  if (nas_ctx == NULL) {
    m_logger.error("Could not find UE context. IMSI %015" PRIu64 "", imsi);
    return false;
  }
  emm_ctx_t* emm_ctx = &nas_ctx->m_emm_ctx;
  ecm_ctx_t* ecm_ctx = &nas_ctx->m_ecm_ctx;

  // Save UE IP to nas ctxt
  emm_ctx->ue_ip.s_addr = cs_resp->paa.ipv4;
  srsran::console("SPGW Allocated IP %s to IMSI %015" PRIu64 "\n", inet_ntoa(emm_ctx->ue_ip), emm_ctx->imsi);

  // Save SGW ctrl F-TEID in GTP-C context
  std::map<uint64_t, struct gtpc_ctx>::iterator it_g = m_imsi_to_gtpc_ctx.find(imsi);
  if (it_g == m_imsi_to_gtpc_ctx.end()) {
    // Could not find GTP-C Context
    m_logger.error("Could not find GTP-C context");
    return false;
  }
  gtpc_ctx_t* gtpc_ctx    = &it_g->second;
  gtpc_ctx->sgw_ctr_fteid = sgw_ctr_fteid;

  // Set EPS bearer context
  // TODO default EPS bearer is hard-coded
  int        default_bearer = 5;
  esm_ctx_t* esm_ctx        = &nas_ctx->m_esm_ctx[default_bearer];
  esm_ctx->pdn_addr_alloc   = cs_resp->paa;
  esm_ctx->sgw_s1u_fteid    = cs_resp->eps_bearer_context_created.s1_u_sgw_f_teid;
  m_s1ap->m_s1ap_ctx_mngmt_proc->send_initial_context_setup_request(nas_ctx, default_bearer);
  return true;
}

bool mme_gtpc::send_modify_bearer_request(uint64_t imsi, uint16_t erab_to_modify, srsran::gtp_fteid_t* enb_fteid)
{
  m_logger.info("Sending GTP-C Modify bearer request");
  srsran::gtpc_pdu mb_req_pdu;
  std::memset(&mb_req_pdu, 0, sizeof(mb_req_pdu));

  std::map<uint64_t, gtpc_ctx_t>::iterator it = m_imsi_to_gtpc_ctx.find(imsi);
  if (it == m_imsi_to_gtpc_ctx.end()) {
    m_logger.error("Modify bearer request for UE without GTP-C connection");
    return false;
  }
  srsran::gtp_fteid_t sgw_ctr_fteid = it->second.sgw_ctr_fteid;

  srsran::gtpc_header* header = &mb_req_pdu.header;
  header->teid_present        = true;
  header->teid                = sgw_ctr_fteid.teid;
  header->type                = srsran::GTPC_MSG_TYPE_MODIFY_BEARER_REQUEST;

  srsran::gtpc_modify_bearer_request* mb_req                = &mb_req_pdu.choice.modify_bearer_request;
  mb_req->eps_bearer_context_to_modify.ebi                  = erab_to_modify;
  mb_req->eps_bearer_context_to_modify.s1_u_enb_f_teid.ipv4 = enb_fteid->ipv4;
  mb_req->eps_bearer_context_to_modify.s1_u_enb_f_teid.teid = enb_fteid->teid;

  m_logger.info("GTP-C Modify bearer request -- S-GW Control TEID %d", sgw_ctr_fteid.teid);
  struct in_addr addr;
  addr.s_addr = enb_fteid->ipv4;
  m_logger.info("GTP-C Modify bearer request -- S1-U TEID 0x%x, IP %s", enb_fteid->teid, inet_ntoa(addr));

  // Send msg to SPGW
  send_s11_pdu(mb_req_pdu);
  return true;
}

void mme_gtpc::handle_modify_bearer_response(srsran::gtpc_pdu* mb_resp_pdu)
{
  uint32_t                               mme_ctrl_teid = mb_resp_pdu->header.teid;
  std::map<uint32_t, uint64_t>::iterator imsi_it       = m_mme_ctr_teid_to_imsi.find(mme_ctrl_teid);
  if (imsi_it == m_mme_ctr_teid_to_imsi.end()) {
    m_logger.error("Could not find IMSI from control TEID");
    return;
  }

  uint8_t ebi = mb_resp_pdu->choice.modify_bearer_response.eps_bearer_context_modified.ebi;
  m_logger.debug("Activating EPS bearer with id %d", ebi);
  m_s1ap->activate_eps_bearer(imsi_it->second, ebi);

  return;
}

bool mme_gtpc::send_delete_session_request(uint64_t imsi)
{
  m_logger.info("Sending GTP-C Delete Session Request request. IMSI %" PRIu64 "", imsi);
  srsran::gtpc_pdu del_req_pdu;
  std::memset(&del_req_pdu, 0, sizeof(del_req_pdu));
  srsran::gtp_fteid_t sgw_ctr_fteid;
  srsran::gtp_fteid_t mme_ctr_fteid;

  // Get S-GW Ctr TEID
  std::map<uint64_t, gtpc_ctx_t>::iterator it_ctx = m_imsi_to_gtpc_ctx.find(imsi);
  if (it_ctx == m_imsi_to_gtpc_ctx.end()) {
    m_logger.error("Could not find GTP-C context to remove");
    return false;
  }

  sgw_ctr_fteid               = it_ctx->second.sgw_ctr_fteid;
  mme_ctr_fteid               = it_ctx->second.mme_ctr_fteid;
  srsran::gtpc_header* header = &del_req_pdu.header;
  header->teid_present        = true;
  header->teid                = sgw_ctr_fteid.teid;
  header->type                = srsran::GTPC_MSG_TYPE_DELETE_SESSION_REQUEST;

  srsran::gtpc_delete_session_request* del_req = &del_req_pdu.choice.delete_session_request;
  del_req->cause.cause_value                   = srsran::GTPC_CAUSE_VALUE_ISR_DEACTIVATION;
  m_logger.info("GTP-C Delete Session Request -- S-GW Control TEID %d", sgw_ctr_fteid.teid);

  // Send msg to SPGW
  send_s11_pdu(del_req_pdu);

  // Delete GTP-C context
  std::map<uint32_t, uint64_t>::iterator it_imsi = m_mme_ctr_teid_to_imsi.find(mme_ctr_fteid.teid);
  if (it_imsi == m_mme_ctr_teid_to_imsi.end()) {
    m_logger.error("Could not find IMSI from MME ctr TEID");
  } else {
    m_mme_ctr_teid_to_imsi.erase(it_imsi);
  }
  m_imsi_to_gtpc_ctx.erase(it_ctx);
  return true;
}

void mme_gtpc::send_release_access_bearers_request(uint64_t imsi)
{
  // The GTP-C connection will not be torn down, just the user plane bearers.
  m_logger.info("Sending GTP-C Release Access Bearers Request");
  srsran::gtpc_pdu rel_req_pdu;
  std::memset(&rel_req_pdu, 0, sizeof(rel_req_pdu));
  srsran::gtp_fteid_t sgw_ctr_fteid;

  // Get S-GW Ctr TEID
  std::map<uint64_t, gtpc_ctx_t>::iterator it_ctx = m_imsi_to_gtpc_ctx.find(imsi);
  if (it_ctx == m_imsi_to_gtpc_ctx.end()) {
    m_logger.error("Could not find GTP-C context to remove");
    return;
  }
  sgw_ctr_fteid = it_ctx->second.sgw_ctr_fteid;

  // Set GTP-C header
  srsran::gtpc_header* header = &rel_req_pdu.header;
  header->teid_present        = true;
  header->teid                = sgw_ctr_fteid.teid;
  header->type                = srsran::GTPC_MSG_TYPE_RELEASE_ACCESS_BEARERS_REQUEST;

  srsran::gtpc_release_access_bearers_request* rel_req = &rel_req_pdu.choice.release_access_bearers_request;
  m_logger.info("GTP-C Release Access Berarers Request -- S-GW Control TEID %d", sgw_ctr_fteid.teid);

  // Send msg to SPGW
  send_s11_pdu(rel_req_pdu);

  return;
}

bool mme_gtpc::handle_downlink_data_notification(srsran::gtpc_pdu* dl_not_pdu)
{
  uint32_t                                 mme_ctrl_teid = dl_not_pdu->header.teid;
  srsran::gtpc_downlink_data_notification* dl_not        = &dl_not_pdu->choice.downlink_data_notification;
  std::map<uint32_t, uint64_t>::iterator   imsi_it       = m_mme_ctr_teid_to_imsi.find(mme_ctrl_teid);
  if (imsi_it == m_mme_ctr_teid_to_imsi.end()) {
    m_logger.error("Could not find IMSI from control TEID");
    return false;
  }

  if (!dl_not->eps_bearer_id_present) {
    m_logger.error("No EPS bearer Id in downlink data notification");
    return false;
  }
  uint8_t ebi = dl_not->eps_bearer_id;
  m_logger.debug("Downlink Data Notification -- IMSI: %015" PRIu64 ", EBI %d", imsi_it->second, ebi);

  m_s1ap->send_paging(imsi_it->second, ebi);
  return true;
}

void mme_gtpc::send_downlink_data_notification_acknowledge(uint64_t imsi, enum srsran::gtpc_cause_value cause)
{
  m_logger.debug("Sending GTP-C Data Notification Acknowledge. Cause %d", cause);
  srsran::gtpc_pdu    not_ack_pdu;
  srsran::gtp_fteid_t sgw_ctr_fteid;
  std::memset(&not_ack_pdu, 0, sizeof(not_ack_pdu));

  // get s-gw ctr teid
  std::map<uint64_t, gtpc_ctx_t>::iterator it_ctx = m_imsi_to_gtpc_ctx.find(imsi);
  if (it_ctx == m_imsi_to_gtpc_ctx.end()) {
    m_logger.error("could not find gtp-c context to remove");
    return;
  }
  sgw_ctr_fteid = it_ctx->second.sgw_ctr_fteid;

  // set gtp-c header
  srsran::gtpc_header* header = &not_ack_pdu.header;
  header->teid_present        = true;
  header->teid                = sgw_ctr_fteid.teid;
  header->type                = srsran::GTPC_MSG_TYPE_DOWNLINK_DATA_NOTIFICATION_ACKNOWLEDGE;

  srsran::gtpc_downlink_data_notification_acknowledge* not_ack =
      &not_ack_pdu.choice.downlink_data_notification_acknowledge;
  m_logger.info("gtp-c downlink data notification acknowledge -- s-gw control teid %d", sgw_ctr_fteid.teid);

  // send msg to spgw
  send_s11_pdu(not_ack_pdu);
  return;
}

bool mme_gtpc::send_downlink_data_notification_failure_indication(uint64_t imsi, enum srsran::gtpc_cause_value cause)
{
  m_logger.debug("Sending GTP-C Data Notification Failure Indication. Cause %d", cause);
  srsran::gtpc_pdu    not_fail_pdu;
  srsran::gtp_fteid_t sgw_ctr_fteid;
  std::memset(&not_fail_pdu, 0, sizeof(not_fail_pdu));

  // get s-gw ctr teid
  std::map<uint64_t, gtpc_ctx_t>::iterator it_ctx = m_imsi_to_gtpc_ctx.find(imsi);
  if (it_ctx == m_imsi_to_gtpc_ctx.end()) {
    m_logger.error("could not find gtp-c context to send paging failure");
    return false;
  }
  sgw_ctr_fteid = it_ctx->second.sgw_ctr_fteid;

  // set gtp-c header
  srsran::gtpc_header* header = &not_fail_pdu.header;
  header->teid_present        = true;
  header->teid                = sgw_ctr_fteid.teid;
  header->type                = srsran::GTPC_MSG_TYPE_DOWNLINK_DATA_NOTIFICATION_FAILURE_INDICATION;

  srsran::gtpc_downlink_data_notification_failure_indication* not_fail =
      &not_fail_pdu.choice.downlink_data_notification_failure_indication;
  not_fail->cause.cause_value = cause;
  m_logger.info("Downlink Data Notification Failure Indication -- SP-GW control teid %d", sgw_ctr_fteid.teid);

  // send msg to spgw
  send_s11_pdu(not_fail_pdu);
  return true;
}

} // namespace srsepc
