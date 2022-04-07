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

#include "srsran/rlc/rlc_um_base.h"
#include "srsran/interfaces/ue_rrc_interfaces.h"
#include <sstream>

namespace srsran {

rlc_um_base::rlc_um_base(srslog::basic_logger&      logger,
                         uint32_t                   lcid_,
                         srsue::pdcp_interface_rlc* pdcp_,
                         srsue::rrc_interface_rlc*  rrc_,
                         srsran::timer_handler*     timers_) :
  logger(logger), lcid(lcid_), pdcp(pdcp_), rrc(rrc_), timers(timers_), pool(byte_buffer_pool::get_instance())
{}

rlc_um_base::~rlc_um_base() {}

void rlc_um_base::stop()
{
  if (tx) {
    tx->stop();
  }

  if (rx) {
    rx->stop();
  }
}

rlc_mode_t rlc_um_base::get_mode()
{
  return rlc_mode_t::um;
}

uint32_t rlc_um_base::get_bearer()
{
  return lcid;
}

bool rlc_um_base::is_mrb()
{
  return cfg.um.is_mrb;
}

void rlc_um_base::reestablish()
{
  tx_enabled = false;

  if (tx) {
    tx->reestablish(); // calls stop and enables tx again
  }

  if (rx) {
    rx->reestablish(); // nothing else needed
  }

  tx_enabled = true;
}

void rlc_um_base::empty_queue()
{
  // Drop all messages in TX SDU queue
  if (tx) {
    tx->empty_queue();
  }
}

/****************************************************************************
 * PDCP interface
 ***************************************************************************/
void rlc_um_base::write_sdu(unique_byte_buffer_t sdu)
{
  if (not tx_enabled || not tx) {
    logger.debug("%s is currently deactivated. Dropping SDU (%d B)", rb_name.c_str(), sdu->N_bytes);
    std::lock_guard<std::mutex> lock(metrics_mutex);
    metrics.num_lost_sdus++;
    return;
  }

  int sdu_bytes = sdu->N_bytes; //< Store SDU length for book-keeping
  if (tx->try_write_sdu(std::move(sdu)) == SRSRAN_SUCCESS) {
    std::lock_guard<std::mutex> lock(metrics_mutex);
    metrics.num_tx_sdus++;
    metrics.num_tx_sdu_bytes += sdu_bytes;
  } else {
    std::lock_guard<std::mutex> lock(metrics_mutex);
    metrics.num_lost_sdus++;
  }
}

void rlc_um_base::discard_sdu(uint32_t discard_sn)
{
  if (not tx_enabled || not tx) {
    logger.debug("%s is currently deactivated. Ignoring SDU discard (SN=%u)", rb_name.c_str(), discard_sn);
    return;
  }
  tx->discard_sdu(discard_sn);
  std::lock_guard<std::mutex> lock(metrics_mutex);
  metrics.num_lost_sdus++;
}

bool rlc_um_base::sdu_queue_is_full()
{
  return tx->sdu_queue_is_full();
}

/****************************************************************************
 * MAC interface
 ***************************************************************************/

bool rlc_um_base::has_data()
{
  if (tx) {
    return tx->has_data();
  }
  return false;
}

uint32_t rlc_um_base::get_buffer_state()
{
  uint32_t newtx_queue   = 0;
  uint32_t prio_tx_queue = 0;
  get_buffer_state(newtx_queue, prio_tx_queue);
  return newtx_queue + prio_tx_queue;
}

void rlc_um_base::get_buffer_state(uint32_t& newtx_queue, uint32_t& prio_tx_queue)
{
  newtx_queue   = 0;
  prio_tx_queue = 0;
  if (tx) {
    newtx_queue = tx->get_buffer_state();
  }
}

uint32_t rlc_um_base::read_pdu(uint8_t* payload, uint32_t nof_bytes)
{
  if (tx && tx_enabled) {
    uint32_t len = tx->build_data_pdu(payload, nof_bytes);
    if (len > 0) {
      std::lock_guard<std::mutex> lock(metrics_mutex);
      metrics.num_tx_pdu_bytes += len;
      metrics.num_tx_pdus++;
    }
    return len;
  }
  return 0;
}

void rlc_um_base::write_pdu(uint8_t* payload, uint32_t nof_bytes)
{
  if (rx && rx_enabled) {
    {
      std::lock_guard<std::mutex> lock(metrics_mutex);
      metrics.num_rx_pdus++;
      metrics.num_rx_pdu_bytes += nof_bytes;
    }
    rx->handle_data_pdu(payload, nof_bytes);
  }
}

rlc_bearer_metrics_t rlc_um_base::get_metrics()
{
  std::lock_guard<std::mutex> lock(metrics_mutex);
  return metrics;
}

void rlc_um_base::reset_metrics()
{
  std::lock_guard<std::mutex> lock(metrics_mutex);
  metrics = {};
}

void rlc_um_base::set_bsr_callback(bsr_callback_t callback)
{
  tx->set_bsr_callback(std::move(callback));
}

/****************************************************************************
 * Helper functions
 ***************************************************************************/

std::string rlc_um_base::get_rb_name(srsue::rrc_interface_rlc* rrc, uint32_t lcid, bool is_mrb)
{
  if (is_mrb) {
    std::stringstream ss;
    ss << "MRB" << lcid;
    return ss.str();
  } else {
    return rrc->get_rb_name(lcid);
  }
}

/****************************************************************************
 * Rx subclass implementation (base)
 ***************************************************************************/

rlc_um_base::rlc_um_base_rx::rlc_um_base_rx(rlc_um_base* parent_) :
  pool(parent_->pool),
  logger(parent_->logger),
  timers(parent_->timers),
  pdcp(parent_->pdcp),
  rrc(parent_->rrc),
  cfg(parent_->cfg),
  metrics(parent_->metrics),
  lcid(parent_->lcid)
{}

rlc_um_base::rlc_um_base_rx::~rlc_um_base_rx() {}

/****************************************************************************
 * Tx subclass implementation (base)
 ***************************************************************************/

rlc_um_base::rlc_um_base_tx::rlc_um_base_tx(rlc_um_base* parent_) :
  logger(parent_->logger), pool(parent_->pool), parent(parent_)
{}

rlc_um_base::rlc_um_base_tx::~rlc_um_base_tx() {}

void rlc_um_base::rlc_um_base_tx::stop()
{
  empty_queue();
  reset();
}

void rlc_um_base::rlc_um_base_tx::reestablish()
{
  stop();
  // bearer is enabled in base class
}

void rlc_um_base::rlc_um_base_tx::empty_queue()
{
  std::lock_guard<std::mutex> lock(mutex);

  // deallocate all SDUs in transmit queue
  while (not tx_sdu_queue.is_empty()) {
    unique_byte_buffer_t buf = tx_sdu_queue.read();
  }

  // deallocate SDU that is currently processed
  tx_sdu.reset();
}

bool rlc_um_base::rlc_um_base_tx::has_data()
{
  return (tx_sdu != nullptr || !tx_sdu_queue.is_empty());
}

void rlc_um_base::rlc_um_base_tx::set_bsr_callback(bsr_callback_t callback)
{
  bsr_callback = callback;
}

void rlc_um_base::rlc_um_base_tx::write_sdu(unique_byte_buffer_t sdu)
{
  if (sdu) {
    logger.info(sdu->msg,
                sdu->N_bytes,
                "%s Tx SDU (%d B, tx_sdu_queue_len=%d)",
                rb_name.c_str(),
                sdu->N_bytes,
                tx_sdu_queue.size());
    tx_sdu_queue.write(std::move(sdu));
  } else {
    logger.warning("NULL SDU pointer in write_sdu()");
  }
}

int rlc_um_base::rlc_um_base_tx::try_write_sdu(unique_byte_buffer_t sdu)
{
  if (sdu) {
    uint8_t*                                 msg_ptr   = sdu->msg;
    uint32_t                                 nof_bytes = sdu->N_bytes;
    srsran::error_type<unique_byte_buffer_t> ret       = tx_sdu_queue.try_write(std::move(sdu));
    if (ret) {
      logger.info(
          msg_ptr, nof_bytes, "%s Tx SDU (%d B, tx_sdu_queue_len=%d)", rb_name.c_str(), nof_bytes, tx_sdu_queue.size());
      return SRSRAN_SUCCESS;
    } else {
      logger.warning(ret.error()->msg,
                     ret.error()->N_bytes,
                     "[Dropped SDU] %s Tx SDU (%d B, tx_sdu_queue_len=%d)",
                     rb_name.c_str(),
                     ret.error()->N_bytes,
                     tx_sdu_queue.size());
    }
  } else {
    logger.warning("NULL SDU pointer in write_sdu()");
  }
  return SRSRAN_ERROR;
}

void rlc_um_base::rlc_um_base_tx::discard_sdu(uint32_t discard_sn)
{
  logger.warning("RLC UM: Discard SDU not implemented yet.");
}

bool rlc_um_base::rlc_um_base_tx::sdu_queue_is_full()
{
  return tx_sdu_queue.is_full();
}

uint32_t rlc_um_base::rlc_um_base_tx::build_data_pdu(uint8_t* payload, uint32_t nof_bytes)
{
  unique_byte_buffer_t pdu;
  {
    std::lock_guard<std::mutex> lock(mutex);
    logger.debug("MAC opportunity - %d bytes", nof_bytes);

    if (tx_sdu == nullptr && tx_sdu_queue.is_empty()) {
      logger.info("No data available to be sent");
      return 0;
    }

    pdu = make_byte_buffer();
    if (!pdu || pdu->N_bytes != 0) {
      logger.error("Failed to allocate PDU buffer");
      return 0;
    }
  }
  return build_data_pdu(std::move(pdu), payload, nof_bytes);
}

} // namespace srsran
