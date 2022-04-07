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

#include "srsepc/hdr/spgw/spgw.h"
#include "srsepc/hdr/mme/mme_gtpc.h"
#include "srsepc/hdr/spgw/gtpc.h"
#include "srsepc/hdr/spgw/gtpu.h"
#include "srsran/upper/gtpu.h"
#include <inttypes.h> // for printing uint64_t

namespace srsepc {

spgw*           spgw::m_instance    = NULL;
pthread_mutex_t spgw_instance_mutex = PTHREAD_MUTEX_INITIALIZER;

spgw::spgw() : m_running(false), thread("SPGW")
{
  m_gtpc = new spgw::gtpc;
  m_gtpu = new spgw::gtpu;
  return;
}

spgw::~spgw()
{
  delete m_gtpc;
  delete m_gtpu;
  return;
}

spgw* spgw::get_instance()
{
  pthread_mutex_lock(&spgw_instance_mutex);
  if (NULL == m_instance) {
    m_instance = new spgw();
  }
  pthread_mutex_unlock(&spgw_instance_mutex);
  return (m_instance);
}

void spgw::cleanup()
{
  pthread_mutex_lock(&spgw_instance_mutex);
  if (NULL != m_instance) {
    delete m_instance;
    m_instance = NULL;
  }
  pthread_mutex_unlock(&spgw_instance_mutex);
}

int spgw::init(spgw_args_t* args, const std::map<std::string, uint64_t>& ip_to_imsi)
{
  int err;

  // Init GTP-U
  if (m_gtpu->init(args, this, m_gtpc) != SRSRAN_SUCCESS) {
    srsran::console("Could not initialize the SPGW's GTP-U.\n");
    return SRSRAN_ERROR_CANT_START;
  }

  // Init GTP-C
  if (m_gtpc->init(args, this, m_gtpu, ip_to_imsi) != SRSRAN_SUCCESS) {
    srsran::console("Could not initialize the S1-U interface.\n");
    return SRSRAN_ERROR_CANT_START;
  }

  m_logger.info("SP-GW Initialized.");
  srsran::console("SP-GW Initialized.\n");
  return SRSRAN_SUCCESS;
}

void spgw::stop()
{
  if (m_running) {
    m_running = false;
    thread_cancel();
    wait_thread_finish();
  }

  m_gtpu->stop();
  m_gtpc->stop();
  return;
}

void spgw::run_thread()
{
  // Mark the thread as running
  m_running = true;
  srsran::unique_byte_buffer_t sgi_msg, s1u_msg, s11_msg;
  s1u_msg = srsran::make_byte_buffer("spgw::run_thread::s1u");
  s11_msg = srsran::make_byte_buffer("spgw::run_thread::s11");

  struct sockaddr_in src_addr_in;
  struct sockaddr_un src_addr_un;
  struct iphdr*      ip_pkt;

  int sgi = m_gtpu->get_sgi();
  int s1u = m_gtpu->get_s1u();
  int s11 = m_gtpc->get_s11();

  size_t buf_len = SRSRAN_MAX_BUFFER_SIZE_BYTES - SRSRAN_BUFFER_HEADER_OFFSET;

  fd_set set;
  int    max_fd = std::max(s1u, sgi);
  max_fd        = std::max(max_fd, s11);
  while (m_running) {
    s1u_msg->clear();
    s11_msg->clear();

    FD_ZERO(&set);
    FD_SET(s1u, &set);
    FD_SET(sgi, &set);
    FD_SET(s11, &set);

    int n = select(max_fd + 1, &set, NULL, NULL, NULL);
    if (n == -1) {
      m_logger.error("Error from select");
    } else if (n) {
      if (FD_ISSET(sgi, &set)) {
        /*
         * SGi messages may need to be queued when waiting for UE Paging procedure.
         * For this reason, buffers for SGi pdus are allocated here and deallocated
         * at the gtpu::send_s1u_pdu() when the PDU is sent, at handle_sgi_pdu() when the PDU is dropped or at
         * gtpc::free_all_queued_packets, which is called when the Downlink Data Notification
         * procedure fails (see handle_downlink_data_notification_acknowledgment and
         * handle_downlink_data_notification_failure)
         */
        m_logger.debug("Message received at SPGW: SGi Message");
        sgi_msg          = srsran::make_byte_buffer("spgw::run_thread::sgi_msg");
        sgi_msg->N_bytes = read(sgi, sgi_msg->msg, buf_len);
        m_gtpu->handle_sgi_pdu(std::move(sgi_msg));
      }
      if (FD_ISSET(s1u, &set)) {
        m_logger.debug("Message received at SPGW: S1-U Message");
        socklen_t addrlen = sizeof(src_addr_in);
        s1u_msg->N_bytes  = recvfrom(s1u, s1u_msg->msg, buf_len, 0, (struct sockaddr*)&src_addr_in, &addrlen);
        m_gtpu->handle_s1u_pdu(s1u_msg.get());
      }
      if (FD_ISSET(s11, &set)) {
        m_logger.debug("Message received at SPGW: S11 Message");
        socklen_t addrlen = sizeof(src_addr_un);
        s11_msg->N_bytes  = recvfrom(s11, s11_msg->msg, buf_len, 0, (struct sockaddr*)&src_addr_un, &addrlen);
        m_gtpc->handle_s11_pdu(s11_msg.get());
      }
    } else {
      m_logger.debug("No data from select.");
    }
  }
  return;
}

} // namespace srsepc
