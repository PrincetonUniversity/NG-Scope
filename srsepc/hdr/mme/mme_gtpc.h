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
#ifndef SRSEPC_MME_GTPC_H
#define SRSEPC_MME_GTPC_H

#include "nas.h"
#include "srsran/asn1/gtpc.h"
#include "srsran/common/buffer_pool.h"
#include <sys/socket.h>
#include <sys/un.h>

namespace srsepc {

class spgw;
class s1ap;

class mme_gtpc : public gtpc_interface_nas
{
public:
  typedef struct gtpc_ctx {
    srsran::gtp_fteid_t mme_ctr_fteid;
    srsran::gtp_fteid_t sgw_ctr_fteid;
  } gtpc_ctx_t;

  virtual ~mme_gtpc() = default;

  static mme_gtpc* get_instance();

  bool init();
  bool send_s11_pdu(const srsran::gtpc_pdu& pdu);
  void handle_s11_pdu(srsran::byte_buffer_t* msg);

  virtual bool send_create_session_request(uint64_t imsi);
  bool         handle_create_session_response(srsran::gtpc_pdu* cs_resp_pdu);
  virtual bool send_modify_bearer_request(uint64_t imsi, uint16_t erab_to_modify, srsran::gtp_fteid_t* enb_fteid);
  void         handle_modify_bearer_response(srsran::gtpc_pdu* mb_resp_pdu);
  void         send_release_access_bearers_request(uint64_t imsi);
  virtual bool send_delete_session_request(uint64_t imsi);
  bool         handle_downlink_data_notification(srsran::gtpc_pdu* dl_not_pdu);
  void         send_downlink_data_notification_acknowledge(uint64_t imsi, enum srsran::gtpc_cause_value cause);
  virtual bool send_downlink_data_notification_failure_indication(uint64_t imsi, enum srsran::gtpc_cause_value cause);

  int get_s11();

private:
  mme_gtpc() = default;

  srslog::basic_logger& m_logger = srslog::fetch_basic_logger("MME GTPC");
  s1ap*                 m_s1ap;

  uint32_t                            m_next_ctrl_teid;
  std::map<uint32_t, uint64_t>        m_mme_ctr_teid_to_imsi;
  std::map<uint64_t, struct gtpc_ctx> m_imsi_to_gtpc_ctx;

  int                m_s11;
  struct sockaddr_un m_mme_addr, m_spgw_addr;

  bool     init_s11();
  uint32_t get_new_ctrl_teid();
};

inline uint32_t mme_gtpc::get_new_ctrl_teid()
{
  return m_next_ctrl_teid++;
}

inline int mme_gtpc::get_s11()
{
  return m_s11;
}

} // namespace srsepc
#endif // SRSEPC_MME_GTPC_H
