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

#define Error(fmt, ...) logger.error(fmt, ##__VA_ARGS__)
#define Warning(fmt, ...) logger.warning(fmt, ##__VA_ARGS__)
#define Info(fmt, ...) logger.info(fmt, ##__VA_ARGS__)
#define Debug(fmt, ...) logger.debug(fmt, ##__VA_ARGS__)

#include "srsue/hdr/stack/mac/dl_harq.h"
#include "srsran/common/mac_pcap.h"
#include "srsran/common/timers.h"

namespace srsue {

dl_harq_entity::dl_harq_entity(uint8_t cc_idx_) :
  proc(SRSRAN_MAX_HARQ_PROC), logger(srslog::fetch_basic_logger("MAC")), cc_idx(cc_idx_)
{}

bool dl_harq_entity::init(ue_rnti* rntis_, demux* demux_unit_)
{
  demux_unit = demux_unit_;
  rntis      = rntis_;

  for (uint32_t i = 0; i < SRSRAN_MAX_HARQ_PROC; i++) {
    if (!proc[i].init(i, this)) {
      return false;
    }
  }
  bcch_proc.init(-1, this);
  return true;
}

/***************** PHY->MAC interface for DL processes **************************/
void dl_harq_entity::new_grant_dl(mac_interface_phy_lte::mac_grant_dl_t  grant,
                                  mac_interface_phy_lte::tb_action_dl_t* action)
{
  bzero(action, sizeof(mac_interface_phy_lte::tb_action_dl_t));

  if (grant.rnti != rntis->get_sps_rnti()) {
    // Set BCCH PID for SI RNTI
    dl_harq_process* proc_ptr = NULL;
    if (grant.rnti == SRSRAN_SIRNTI) {
      proc_ptr = &bcch_proc;
    } else {
      if (grant.pid >= SRSRAN_MAX_HARQ_PROC) {
        Error("Invalid PID: %d", grant.pid);
        return;
      }
      proc_ptr = &proc[grant.pid];
    }
    // Consider the NDI to have been toggled
    if (grant.rnti == rntis->get_temp_rnti() && last_temporal_crnti != rntis->get_temp_rnti()) {
      last_temporal_crnti = rntis->get_temp_rnti();
      proc_ptr->reset_ndi();
      Info("Considering NDI in pid=%d to be toggled for first Temporal C-RNTI", grant.pid);
    }
    proc_ptr->new_grant_dl(grant, action);
  } else {
    Error("SPS not supported");
  }
}

void dl_harq_entity::tb_decoded(mac_interface_phy_lte::mac_grant_dl_t grant, bool ack[SRSRAN_MAX_CODEWORDS])
{
  if (grant.rnti == SRSRAN_SIRNTI) {
    bcch_proc.tb_decoded(grant, ack);
  } else {
    if (grant.pid >= SRSRAN_MAX_HARQ_PROC) {
      Error("Invalid PID: %d", grant.pid);
      return;
    }
    proc[grant.pid].tb_decoded(grant, ack);
  }
}

void dl_harq_entity::reset()
{
  for (uint32_t i = 0; i < SRSRAN_MAX_HARQ_PROC; i++) {
    proc[i].reset();
  }
  bcch_proc.reset();
  dl_sps_assig.clear();
}

void dl_harq_entity::start_pcap(srsran::mac_pcap* pcap_)
{
  pcap = pcap_;
}

void dl_harq_entity::set_si_window_start(int si_window_start_)
{
  si_window_start = si_window_start_;
}

float dl_harq_entity::get_average_retx()
{
  std::unique_lock<std::mutex> lock(retx_cnt_mutex);
  return average_retx;
}
void dl_harq_entity::set_average_retx(uint32_t n_retx)
{
  std::unique_lock<std::mutex> lock(retx_cnt_mutex);
  average_retx = SRSRAN_VEC_CMA((float)n_retx, average_retx, nof_pkts++);
}

dl_harq_entity::dl_harq_process::dl_harq_process() : subproc(SRSRAN_MAX_TB) {}

bool dl_harq_entity::dl_harq_process::init(int pid, dl_harq_entity* parent)
{
  bool ret = true;

  for (uint32_t tb = 0; tb < SRSRAN_MAX_TB; tb++) {
    ret &= subproc[tb].init(pid, parent, tb);
  }
  return ret;
}

void dl_harq_entity::dl_harq_process::reset(void)
{
  for (uint32_t tb = 0; tb < SRSRAN_MAX_TB; tb++) {
    subproc[tb].reset();
  }
}

void dl_harq_entity::dl_harq_process::reset_ndi()
{
  for (uint32_t tb = 0; tb < SRSRAN_MAX_TB; tb++) {
    subproc[tb].reset_ndi();
  }
}

void dl_harq_entity::dl_harq_process::new_grant_dl(mac_interface_phy_lte::mac_grant_dl_t  grant,
                                                   mac_interface_phy_lte::tb_action_dl_t* action)
{
  bzero(action, sizeof(mac_interface_phy_lte::tb_action_dl_t));
  /* For each subprocess... */
  for (uint32_t i = 0; i < SRSRAN_MAX_TB; i++) {
    if (grant.tb[i].tbs) {
      subproc[i].new_grant_dl(grant, action);
    }
  }
}

void dl_harq_entity::dl_harq_process::tb_decoded(mac_interface_phy_lte::mac_grant_dl_t grant,
                                                 bool                                  ack[SRSRAN_MAX_CODEWORDS])
{
  /* For each subprocess... */
  for (uint32_t i = 0; i < SRSRAN_MAX_TB; i++) {
    if (grant.tb[i].tbs) {
      subproc[i].tb_decoded(grant, &ack[i]);
    }
  }
}

bool dl_harq_entity::dl_harq_process::is_sps()
{
  return false;
}

dl_harq_entity::dl_harq_process::dl_tb_process::dl_tb_process() : logger(srslog::fetch_basic_logger("MAC"))
{
  is_initiated = false;
  ack          = false;
  n_retx       = 0;
  bzero(&cur_grant, sizeof(mac_interface_phy_lte::mac_grant_dl_t));
  payload_buffer_ptr = NULL;
}

dl_harq_entity::dl_harq_process::dl_tb_process::~dl_tb_process()
{
  if (is_initiated) {
    srsran_softbuffer_rx_free(&softbuffer);
  }
}

bool dl_harq_entity::dl_harq_process::dl_tb_process::init(int pid, dl_harq_entity* parent, uint32_t tb_idx)
{
  if (srsran_softbuffer_rx_init(&softbuffer, 110)) {
    Error("Error initiating soft buffer");
    return false;
  }

  if (pid < 0) {
    is_bcch   = true;
    this->pid = 0;
  } else {
    this->pid = (uint32_t)pid;
    is_bcch   = false;
  }

  tid          = tb_idx;
  is_first_tb  = true;
  is_initiated = true;
  harq_entity  = parent;
  return true;
}

void dl_harq_entity::dl_harq_process::dl_tb_process::reset()
{
  std::lock_guard<std::mutex> lock(mutex);
  reset_nolock();
}

void dl_harq_entity::dl_harq_process::dl_tb_process::reset_nolock()
{
  bzero(&cur_grant, sizeof(mac_interface_phy_lte::mac_grant_dl_t));
  is_first_tb = true;
  ack         = false;
  n_retx      = 0;

  if (payload_buffer_ptr) {
    if (!is_bcch) {
      harq_entity->demux_unit->deallocate(payload_buffer_ptr);
    }
    payload_buffer_ptr = NULL;
  }
}

void dl_harq_entity::dl_harq_process::dl_tb_process::reset_ndi()
{
  is_first_tb = true;
}

void dl_harq_entity::dl_harq_process::dl_tb_process::new_grant_dl(mac_interface_phy_lte::mac_grant_dl_t  grant,
                                                                  mac_interface_phy_lte::tb_action_dl_t* action)
{
  mutex.lock();

  // Compute RV for BCCH when not specified in PDCCH format
  if (is_bcch && grant.tb[tid].rv == -1) {
    uint32_t k;
    if ((grant.pid / 10) % 2 == 0 && grant.pid % 10 == 5) { // This is SIB1, k is different
      k                = (grant.pid / 20) % 4;
      grant.tb[tid].rv = ((uint32_t)ceilf((float)1.5 * k)) % 4;
    } else {
      k                = (grant.pid - harq_entity->si_window_start) % 4;
      grant.tb[tid].rv = ((uint32_t)ceilf((float)1.5 * k)) % 4;
    }
  }

  calc_is_new_transmission(grant);

  // If this is a new transmission or the size of the TB has changed
  if (is_new_transmission || (cur_grant.tb[tid].tbs != grant.tb[tid].tbs)) {
    if (!is_new_transmission) {
      Debug("DL PID %d: Size of dci changed during a retransmission %d!=%d",
            pid,
            cur_grant.tb[tid].tbs,
            grant.tb[tid].tbs);
    }
    ack    = false;
    n_retx = 0;
    srsran_softbuffer_rx_reset_tbs(&softbuffer, grant.tb[tid].tbs * 8);
  }

  n_retx++;

  // If data has not yet been successfully decoded
  if (!ack) {
    // Save dci
    cur_grant = grant;

    if (payload_buffer_ptr) {
      Warning("DL PID %d: Allocating buffer already allocated. Deallocating.", pid);
      if (!is_bcch) {
        harq_entity->demux_unit->deallocate(payload_buffer_ptr);
      }
    }

    // Instruct the PHY To combine the received data and attempt to decode it
    if (is_bcch) {
      payload_buffer_ptr = harq_entity->demux_unit->request_buffer_bcch(cur_grant.tb[tid].tbs);
    } else {
      payload_buffer_ptr = harq_entity->demux_unit->request_buffer(cur_grant.tb[tid].tbs);
    }

    action->tb[tid].payload = payload_buffer_ptr;
    if (!action->tb[tid].payload) {
      Error("Can't get a buffer for TBS=%d", cur_grant.tb[tid].tbs);
      return;
    }

    action->tb[tid].enabled       = true;
    action->tb[tid].rv            = cur_grant.tb[tid].rv;
    action->tb[tid].softbuffer.rx = &softbuffer;
  } else {
    Info("DL PID %d: Received duplicate TB%d. Discarding and retransmitting ACK (n_retx=%d, reset=%s)",
         pid,
         tid,
         n_retx,
         n_retx > RESET_DUPLICATE_TIMEOUT ? "yes" : "no");
    if (n_retx > RESET_DUPLICATE_TIMEOUT) {
      // reset without trying to acquire the mutex again
      reset_nolock();
    }
  }

  // Do NOT generate ACK if Broadcast Control Channel
  action->generate_ack = not is_bcch;
}

void dl_harq_entity::dl_harq_process::dl_tb_process::tb_decoded(mac_interface_phy_lte::mac_grant_dl_t grant,
                                                                bool*                                 ack_ptr)
{
  if (payload_buffer_ptr) {
    this->ack = *ack_ptr;
    if (ack) {
      if (is_bcch) {
        if (harq_entity->pcap) {
          harq_entity->pcap->write_dl_sirnti(
              payload_buffer_ptr, cur_grant.tb[tid].tbs, ack, cur_grant.tti, harq_entity->cc_idx);
        }
        Debug("Delivering PDU=%d bytes to Dissassemble and Demux unit (BCCH)", cur_grant.tb[tid].tbs);
        harq_entity->demux_unit->push_pdu_bcch(payload_buffer_ptr, cur_grant.tb[tid].tbs);
      } else {
        if (harq_entity->pcap) {
          harq_entity->pcap->write_dl_crnti(
              payload_buffer_ptr, cur_grant.tb[tid].tbs, cur_grant.rnti, ack, cur_grant.tti, harq_entity->cc_idx);
        }
        if (cur_grant.rnti == harq_entity->rntis->get_temp_rnti()) {
          Debug("Delivering PDU=%d bytes to Dissassemble and Demux unit (Temporal C-RNTI)", cur_grant.tb[tid].tbs);
          harq_entity->demux_unit->push_pdu_temp_crnti(payload_buffer_ptr, cur_grant.tb[tid].tbs);

          // If T-CRNTI, update ack value with result from contention resolution
          *ack_ptr = harq_entity->demux_unit->get_uecrid_successful();

        } else {
          Debug("Delivering PDU=%d bytes to Dissassemble and Demux unit", cur_grant.tb[tid].tbs);
          harq_entity->demux_unit->push_pdu(payload_buffer_ptr, cur_grant.tb[tid].tbs, grant.tti);

          // Compute average number of retransmissions per packet
          harq_entity->set_average_retx(n_retx);
        }
      }

    } else if (!is_bcch) {
      harq_entity->demux_unit->deallocate(payload_buffer_ptr);
    }

    payload_buffer_ptr = NULL;

    Info("DL %d (TB %d):  %s tbs=%d, rv=%d, ack=%s, ndi=%d",
         pid,
         tid,
         is_new_transmission ? "newTX" : "reTX ",
         cur_grant.tb[tid].tbs,
         cur_grant.tb[tid].rv,
         ack ? "OK" : "KO",
         cur_grant.tb[tid].ndi);
  }

  if (ack && is_bcch) {
    // reset without trying to acquire the mutex again
    reset_nolock();
  }

  mutex.unlock();
}

// Determine if it's a new transmission 5.3.2.2
bool dl_harq_entity::dl_harq_process::dl_tb_process::calc_is_new_transmission(
    mac_interface_phy_lte::mac_grant_dl_t grant)
{
  if (((grant.tb[tid].ndi_present &&
        grant.tb[tid].ndi != cur_grant.tb[tid].ndi) || // 1st condition (NDI provided and has changed)
       (is_bcch && grant.tb[tid].rv == 0) ||           // 2nd condition (Broadcast and 1st transmission)
       is_first_tb))                                   // 3rd condition (is first tx for this tb)
  {
    is_first_tb         = false;
    is_new_transmission = true;
  } else {
    is_new_transmission = false;
  }

  Debug("Set HARQ for %stransmission", is_new_transmission ? "new " : "re");

  return is_new_transmission;
}

} // namespace srsue
