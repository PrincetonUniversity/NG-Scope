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

#include "srsue/hdr/stack/mac/proc_ra.h"
#include "srsran/common/standard_streams.h"
#include "srsran/interfaces/ue_phy_interfaces.h"
#include "srsran/interfaces/ue_rrc_interfaces.h"
#include "srsue/hdr/stack/mac/mux.h"
#include <inttypes.h> // for printing uint64_t
#include <stdint.h>
#include <stdlib.h>

/* Random access procedure as specified in Section 5.1 of 36.321 */

namespace srsue {

const char* state_str[] = {"RA:    INIT:   ",
                           "RA:    PDCCH:  ",
                           "RA:    Rx:     ",
                           "RA:    Backoff: ",
                           "RA:    ConRes: ",
                           "RA:    WaitComplt: ",
                           "RA:    Complt: "};

#define rError(fmt, ...) logger.error("%s" fmt, state_str[state], ##__VA_ARGS__)
#define rInfo(fmt, ...) logger.info("%s" fmt, state_str[state], ##__VA_ARGS__)
#define rDebug(fmt, ...) logger.debug("%s" fmt, state_str[state], ##__VA_ARGS__)
#define rWarning(fmt, ...) logger.warning("%s" fmt, state_str[state], ##__VA_ARGS__)

// Table 7.2-1. Backoff Parameter values
uint32_t backoff_table[16] = {0, 10, 20, 30, 40, 60, 80, 120, 160, 240, 320, 480, 960, 960, 960, 960};

// Table 7.6-1: DELTA_PREAMBLE values.
int delta_preamble_db_table[5] = {0, 0, -3, -3, 8};

// Initializes memory and pointers to other objects
void ra_proc::init(phy_interface_mac_lte*               phy_h_,
                   rrc_interface_mac*                   rrc_,
                   ue_rnti*                             rntis_,
                   srsran::timer_handler::unique_timer* time_alignment_timer_,
                   mux*                                 mux_unit_,
                   srsran::ext_task_sched_handle*       task_sched_)
{
  phy_h      = phy_h_;
  rntis      = rntis_;
  mux_unit   = mux_unit_;
  rrc        = rrc_;
  task_sched = task_sched_;

  task_queue = task_sched->make_task_queue();

  time_alignment_timer        = time_alignment_timer_;
  contention_resolution_timer = task_sched->get_unique_timer();

  srsran_softbuffer_rx_init(&softbuffer_rar, 10);

  reset();
}

ra_proc::~ra_proc()
{
  srsran_softbuffer_rx_free(&softbuffer_rar);
}

void ra_proc::reset()
{
  state            = IDLE;
  started_by_pdcch = false;
  contention_resolution_timer.stop();
}

void ra_proc::start_pcap(srsran::mac_pcap* pcap_)
{
  pcap = pcap_;
}

// RRC calls to set a new PRACH configuration.
// The configuration is applied by initialization() function.
void ra_proc::set_config(srsran::rach_cfg_t& rach_cfg_)
{
  rach_cfg = rach_cfg_;
}

// RRC might also call this to set additional params during mobility
void ra_proc::set_config_ded(uint32_t preamble_index, uint32_t prach_mask)
{
  next_preamble_idx     = preamble_index;
  next_prach_mask       = prach_mask;
  noncontention_enabled = true;
}

/* Reads the configuration and configures internal variables */
void ra_proc::read_params()
{
  // Read initialization parameters
  if (noncontention_enabled) {
    preambleIndex         = next_preamble_idx;
    maskIndex             = next_prach_mask;
    noncontention_enabled = false;
  } else {
    preambleIndex = 0; // pass when called from higher layers for non-contention based RA
    maskIndex     = 0; // same
  }

  phy_interface_mac_lte::prach_info_t prach_info = phy_h->prach_get_info();
  delta_preamble_db                              = delta_preamble_db_table[prach_info.preamble_format % 5];

  if (rach_cfg.contentionResolutionTimer > 0) {
    contention_resolution_timer.set(rach_cfg.contentionResolutionTimer, [this](uint32_t tid) { timer_expired(tid); });
  }
}

/* Function called by MAC every TTI. Runs a state function until it changes to a different state
 */
void ra_proc::step(uint32_t tti_)
{
  switch (state.load()) {
    case IDLE:
      break;
    case PDCCH_SETUP:
      state_pdcch_setup();
      break;
    case RESPONSE_RECEPTION:
      state_response_reception(tti_);
      break;
    case BACKOFF_WAIT:
      state_backoff_wait(tti_);
      break;
    case CONTENTION_RESOLUTION:
      state_contention_resolution();
      break;
  }
}

/* Waits for PRACH to be transmitted by PHY. Once it's transmitted, configure RA-RNTI and wait for RAR reception
 */
void ra_proc::state_pdcch_setup()
{
  phy_interface_mac_lte::prach_info_t info = phy_h->prach_get_info();
  if (info.is_transmitted) {
    ra_tti  = info.tti_ra;
    ra_rnti = 1 + (ra_tti % 10) + (10 * info.f_id);
    rInfo("seq=%d, ra-rnti=0x%x, ra-tti=%d, f_id=%d", sel_preamble.load(), ra_rnti, info.tti_ra, info.f_id);
    srsran::console(
        "Random Access Transmission: seq=%d, tti=%d, ra-rnti=0x%x\n", sel_preamble.load(), info.tti_ra, ra_rnti);
    rar_window_st = ra_tti + 3;
    rntis->set_rar_rnti(ra_rnti);
    state = RESPONSE_RECEPTION;
  } else {
    rDebug("preamble not yet transmitted");
  }
}

/* Waits for RAR reception. rar_received variable will be set by tb_decoded_ok() function which is called when a DL
 * TB assigned to RA-RNTI is received
 */
void ra_proc::state_response_reception(uint32_t tti)
{
  // do nothing. Processing done in tb_decoded_ok()
  if (!rar_received) {
    uint32_t interval = srsran_tti_interval(tti, ra_tti + 3 + rach_cfg.responseWindowSize - 1);
    if (interval > 0 && interval < 100) {
      logger.warning("RA response not received within the response window");
      response_error();
    }
  }
}

/* Waits for given backoff interval to expire
 */
void ra_proc::state_backoff_wait(uint32_t tti)
{
  if (backoff_interval > 0) {
    // Backoff_interval = 0 is handled before entering here
    // When we arrive to this state, there is already 1 TTI delay
    if (backoff_interval == 1) {
      resource_selection();
    } else {
      // If it's the first time, save TTI
      if (backoff_interval_start == -1) {
        backoff_interval_start = tti;
        backoff_interval--;
      }
      if (srsran_tti_interval(tti, backoff_interval_start) >= backoff_interval) {
        backoff_interval = 0;
        resource_selection();
      }
    }
  }
}

/* Actions during contention resolution state as defined in 5.1.5
 * Resolution of the Contention is made by contention_resolution_id_received() and pdcch_to_crnti()
 */
void ra_proc::state_contention_resolution()
{
  // Once Msg3 is transmitted, start contention resolution timer
  if (mux_unit->msg3_is_transmitted() && !contention_resolution_timer.is_running()) {
    // Start contention resolution timer
    rInfo("Starting ContentionResolutionTimer=%d ms", contention_resolution_timer.duration());
    contention_resolution_timer.run();
  }
}

/* RA procedure initialization as defined in 5.1.1 */
void ra_proc::initialization()
{
  read_params();
  current_task_id++;
  transmitted_contention_id   = 0;
  preambleTransmissionCounter = 1;
  mux_unit->msg3_flush();
  backoff_param_ms  = 0;
  transmitted_crnti = 0;
  resource_selection();
}

/* Resource selection as defined in 5.1.2 */
void ra_proc::resource_selection()
{
  ra_group_t sel_group;
  uint32_t   nof_groupB_preambles = rach_cfg.nof_preambles - rach_cfg.nof_groupA_preambles;

  // If ra-PreambleIndex (Random Access Preamble) and ra-PRACH-MaskIndex (PRACH Mask Index) have been
  // explicitly signalled and ra-PreambleIndex is not 000000:
  if (preambleIndex > 0) {
    // the Random Access Preamble and the PRACH Mask Index are those explicitly signalled.
    sel_maskIndex = maskIndex;
    sel_preamble  = (uint32_t)preambleIndex;
  } else {
    // else the Random Access Preamble shall be selected by the UE as follows:
    if (!mux_unit->msg3_is_transmitted()) {
      // If Msg3 has not yet been transmitted, the UE shall:
      //   if Random Access Preambles group B exists and if the potential message size (data available for transmission
      //   plus MAC header and, where required, MAC control elements) is greater than messageSizeGroupA and if the
      //   pathloss is less than P CMAX – preambleInitialReceivedTargetPower – deltaPreambleMsg3 –
      //   messagePowerOffsetGroupB, then:
      if (nof_groupB_preambles > 0 &&
          new_ra_msg_len > rach_cfg.messageSizeGroupA) { // Check also pathloss (Pcmax,deltaPreamble and powerOffset)
        // select the Random Access Preambles group B;
        sel_group = RA_GROUP_B;
      } else {
        // else:
        //   select the Random Access Preambles group A.
        sel_group = RA_GROUP_A;
      }
      last_msg3_group = sel_group;
    } else {
      // else, if Msg3 is being retransmitted, the UE shall:
      // select the same group of Random Access Preambles as was used for the preamble transmission attempt
      // corresponding to the first transmission of Msg3.
      sel_group = last_msg3_group;
    }

    // randomly select a Random Access Preamble within the selected group. The random function shall be such
    // that each of the allowed selections can be chosen with equal probability;
    if (sel_group == RA_GROUP_A) {
      if (rach_cfg.nof_groupA_preambles > 0) {
        // randomly choose preamble from [0 nof_groupA_preambles)
        sel_preamble = rand() % rach_cfg.nof_groupA_preambles;
      } else {
        rError("Selected group preamble A but nof_groupA_preambles=0");
        state = IDLE;
        return;
      }
    } else {
      if (nof_groupB_preambles) {
        // randomly choose preamble from [nof_groupA_preambles nof_groupB_preambles)
        sel_preamble = rach_cfg.nof_groupA_preambles + rand() % nof_groupB_preambles;
      } else {
        rError("Selected group preamble B but nof_groupA_preambles=0");
        state = IDLE;
        return;
      }
    }

    // set PRACH Mask Index to 0.
    sel_maskIndex = 0;
  }

  rDebug("Selected preambleIndex=%d maskIndex=%d GroupA=%d, GroupB=%d",
         sel_preamble.load(),
         sel_maskIndex,
         rach_cfg.nof_groupA_preambles,
         nof_groupB_preambles);

  // Jump directly to transmission
  preamble_transmission();
}

/* Preamble transmission as defined in 5.1.3 */
void ra_proc::preamble_transmission()
{
  received_target_power_dbm = rach_cfg.iniReceivedTargetPower + delta_preamble_db +
                              (preambleTransmissionCounter - 1) * rach_cfg.powerRampingStep;

  phy_h->prach_send(sel_preamble, sel_maskIndex - 1, received_target_power_dbm);
  rntis->clear_rar_rnti();
  ra_tti                 = 0;
  rar_received           = false;
  backoff_interval_start = -1;

  state = PDCCH_SETUP;
}

// Process Timing Advance Command as defined in Section 5.2
void ra_proc::process_timeadv_cmd(uint32_t tti, uint32_t ta)
{
  if (preambleIndex == 0) {
    // Preamble not selected by UE MAC
    phy_h->set_timeadv_rar(tti, ta);
    // Only if timer is running reset the timer
    if (time_alignment_timer->is_running()) {
      time_alignment_timer->run();
    }
    logger.debug("Applying RAR TA CMD %d", ta);
  } else {
    // Preamble selected by UE MAC
    if (!time_alignment_timer->is_running()) {
      phy_h->set_timeadv_rar(tti, ta);
      time_alignment_timer->run();
      logger.debug("Applying RAR TA CMD %d", ta);
    } else {
      // Ignore TA CMD
      logger.warning("Ignoring RAR TA CMD because timeAlignmentTimer still running");
    }
  }
}

/* Called upon the reception of a DL grant for RA-RNTI
 * Configures the action and softbuffer for the reception of the associated TB
 */
void ra_proc::new_grant_dl(mac_interface_phy_lte::mac_grant_dl_t grant, mac_interface_phy_lte::tb_action_dl_t* action)
{
  bzero(action, sizeof(mac_interface_phy_lte::tb_action_dl_t));

  if (grant.tb[0].tbs < MAX_RAR_PDU_LEN) {
    rDebug("DL dci found RA-RNTI=%d", ra_rnti);
    action->tb[0].enabled       = true;
    action->tb[0].payload       = rar_pdu_buffer;
    action->tb[0].rv            = grant.tb[0].rv;
    action->tb[0].softbuffer.rx = &softbuffer_rar;
    rar_grant_nbytes            = grant.tb[0].tbs;
    if (action->tb[0].rv == 0) {
      srsran_softbuffer_rx_reset(&softbuffer_rar);
    }
  } else {
    rError("Received RAR dci exceeds buffer length (%d>%d)", grant.tb[0].tbs, int(MAX_RAR_PDU_LEN));
  }
}

/* Called from PHY worker upon the successful decoding of a TB addressed to RA-RNTI.
 * We extract the most relevant and time critical details from the RAR PDU,
 * as definied in 5.1.4 and then defer the handling of the RA state machine to be
 * executed on the Stack thread.
 */
void ra_proc::tb_decoded_ok(const uint8_t cc_idx, const uint32_t tti)
{
  if (pcap) {
    pcap->write_dl_ranti(rar_pdu_buffer, rar_grant_nbytes, ra_rnti, true, tti, cc_idx);
  }

  rar_pdu_msg.init_rx(rar_grant_nbytes);
  if (rar_pdu_msg.parse_packet(rar_pdu_buffer) != SRSRAN_SUCCESS) {
    rError("Error decoding RAR PDU");
  }

  rDebug("RAR decoded successfully TBS=%d", rar_grant_nbytes);

  // Set Backoff parameter
  if (rar_pdu_msg.has_backoff()) {
    backoff_param_ms = backoff_table[rar_pdu_msg.get_backoff() % 16];
  } else {
    backoff_param_ms = 0;
  }

  current_ta = 0;

  while (rar_pdu_msg.next()) {
    if (rar_pdu_msg.get()->has_rapid() && rar_pdu_msg.get()->get_rapid() == sel_preamble) {
      rar_received = true;
      process_timeadv_cmd(tti, rar_pdu_msg.get()->get_ta_cmd());

      // TODO: Indicate received target power
      // phy_h->set_target_power_rar(iniReceivedTargetPower, (preambleTransmissionCounter-1)*powerRampingStep);

      uint8_t grant[srsran::rar_subh::RAR_GRANT_LEN] = {};
      rar_pdu_msg.get()->get_sched_grant(grant);

      rntis->clear_rar_rnti();
      phy_h->set_rar_grant(grant, rar_pdu_msg.get()->get_temp_crnti());

      current_ta = rar_pdu_msg.get()->get_ta_cmd();

      rInfo("RAPID=%d, TA=%d, T-CRNTI=0x%x",
            sel_preamble.load(),
            rar_pdu_msg.get()->get_ta_cmd(),
            rar_pdu_msg.get()->get_temp_crnti());

      // Save Temp-CRNTI before generating the reply
      rntis->set_temp_rnti(rar_pdu_msg.get()->get_temp_crnti());

      // Perform actions when preamble was selected by UE MAC
      if (preambleIndex <= 0) {
        mux_unit->msg3_prepare();

        // If this is the first successfully received RAR within this procedure, Msg3 is empty
        if (mux_unit->msg3_is_empty()) {
          // Save transmitted C-RNTI (if any)
          transmitted_crnti = rntis->get_crnti();

          // If we have a C-RNTI, tell Mux unit to append C-RNTI CE if no CCCH SDU transmission
          if (transmitted_crnti) {
            rInfo("Appending C-RNTI MAC CE 0x%x in next transmission", transmitted_crnti.load());
            mux_unit->append_crnti_ce_next_tx(transmitted_crnti);
          }
        }

        // Save transmitted UE contention id, as defined by higher layers
        transmitted_contention_id = rntis->get_contention_id();

        task_queue.push([this]() {
          rDebug("Waiting for Contention Resolution");
          state = CONTENTION_RESOLUTION;
        });
      } else {
        // Preamble selected by Network, defer result handling
        task_queue.push([this]() { complete(); });
      }
    } else {
      if (rar_pdu_msg.get()->has_rapid()) {
        rInfo("Found RAR for preamble %d", rar_pdu_msg.get()->get_rapid());
      }
    }
  }
}

/* Called after RA response window expiration without a valid RAPID or after a reception of an invalid
 * Contention Resolution ID
 */
void ra_proc::response_error()
{
  rntis->clear_temp_rnti();
  preambleTransmissionCounter++;
  contention_resolution_timer.stop();
  if (preambleTransmissionCounter >= rach_cfg.preambleTransMax + 1) {
    rError("Maximum number of transmissions reached (%d)", rach_cfg.preambleTransMax);
    rrc->ra_problem();
    state = IDLE;
  } else {
    backoff_interval_start = -1;
    if (backoff_param_ms) {
      backoff_interval = rand() % backoff_param_ms;
    } else {
      backoff_interval = 0;
    }
    if (backoff_interval) {
      rDebug("Backoff wait interval %d", backoff_interval);
      state = BACKOFF_WAIT;
    } else {
      rInfo("Transmitting new preamble immediately (%d/%d)", preambleTransmissionCounter, rach_cfg.preambleTransMax);
      resource_selection();
    }
  }
}

bool ra_proc::is_contention_resolution()
{
  return state == CONTENTION_RESOLUTION;
}

/* Perform the actions upon completition of the RA procedure as defined in 5.1.6 */
void ra_proc::complete()
{
  // Start looking for PDCCH CRNTI
  if (!transmitted_crnti) {
    rntis->set_crnti_to_temp();
  }
  rntis->clear_temp_rnti();

  mux_unit->msg3_flush();

  rrc->ra_completed();

  srsran::console("Random Access Complete.     c-rnti=0x%x, ta=%d\n", rntis->get_crnti(), current_ta);
  rInfo("Random Access Complete.     c-rnti=0x%x, ta=%d", rntis->get_crnti(), current_ta);

  state = IDLE;
}

void ra_proc::start_mac_order(uint32_t msg_len_bits)
{
  if (state == IDLE) {
    started_by_pdcch = false;
    new_ra_msg_len   = msg_len_bits;
    rInfo("Starting PRACH by MAC order");
    initialization();
  } else {
    logger.warning("Trying to start PRACH by MAC order in invalid state (%s)", state_str[state]);
  }
}

void ra_proc::start_pdcch_order()
{
  if (state == IDLE) {
    started_by_pdcch = true;
    rInfo("Starting PRACH by PDCCH order");
    initialization();
  } else {
    logger.warning("Trying to start PRACH by MAC order in invalid state (%s)", state_str[state]);
  }
}

// Contention Resolution Timer is expired (Section 5.1.5)
void ra_proc::timer_expired(uint32_t timer_id)
{
  rInfo("Contention Resolution Timer expired. Stopping PDCCH Search and going to Response Error");
  response_error();
}

/* Function called by MAC when a Contention Resolution ID CE is received.
 * Since this is called from within a PHY worker thread, we enqueue the handling,
 * check that the contention resolution IDs match and return so the DL TB can be acked.
 *
 * The RA-related actions are scheduled to be executed on the Stack thread,
 * even if we realize later that we have received that in a wrong state.
 */
bool ra_proc::contention_resolution_id_received(uint64_t rx_contention_id)
{
  task_queue.push([this, rx_contention_id]() { contention_resolution_id_received_nolock(rx_contention_id); });
  return (transmitted_contention_id == rx_contention_id);
}

/*
 * Performs the actions defined in 5.1.5 for Temporal C-RNTI Contention Resolution
 */
bool ra_proc::contention_resolution_id_received_nolock(uint64_t rx_contention_id)
{
  bool uecri_successful = false;

  rDebug("MAC PDU Contains Contention Resolution ID CE");

  if (state != CONTENTION_RESOLUTION) {
    rError("Received contention resolution in wrong state. Aborting.");
    response_error();
  }

  // MAC PDU successfully decoded and contains MAC CE contention Id
  contention_resolution_timer.stop();

  if (transmitted_contention_id == rx_contention_id) {
    // UE Contention Resolution ID included in MAC CE matches the CCCH SDU transmitted in Msg3
    uecri_successful = true;
    complete();
  } else {
    rInfo("Transmitted UE Contention Id differs from received Contention ID (0x%" PRIx64 " != 0x%" PRIx64 ")",
          transmitted_contention_id.load(),
          rx_contention_id);

    // Discard MAC PDU
    uecri_successful = false;

    // Contention Resolution not successfully is like RAR not successful
    response_error();
  }

  return uecri_successful;
}

// Called from PHY worker context, defer actions therefore.
void ra_proc::pdcch_to_crnti(bool is_new_uplink_transmission)
{
  task_queue.push([this, is_new_uplink_transmission]() {
    // TS 36.321 Section 5.1.5
    rDebug("PDCCH to C-RNTI received %s new UL transmission", is_new_uplink_transmission ? "with" : "without");
    if ((!started_by_pdcch && is_new_uplink_transmission) || started_by_pdcch) {
      rDebug("PDCCH for C-RNTI received");
      contention_resolution_timer.stop();
      complete();
    }
  });
}

// Called from the Stack thread
void ra_proc::update_rar_window(rnti_window_safe& ra_window)
{
  if (state != RESPONSE_RECEPTION) {
    // reset RAR window params to default values to disable RAR search
    ra_window.reset();
  } else {
    ra_window.set(rach_cfg.responseWindowSize, rar_window_st);
  }
  rDebug("rar_window_start=%d, rar_window_length=%d", ra_window.get_start(), ra_window.get_length());
}

// Restart timer at each Msg3 HARQ retransmission (5.1.5)
void ra_proc::harq_retx()
{
  task_queue.push([this]() {
    if (state != CONTENTION_RESOLUTION) {
      rWarning("Ignore HARQ retx when not in contention resolution.");
      return;
    }
    rInfo("Restarting ContentionResolutionTimer=%d ms", contention_resolution_timer.duration());
    contention_resolution_timer.run();
  });
}

// Called from PHY worker thread
void ra_proc::harq_max_retx()
{
  task_queue.push([this]() {
    if (state != CONTENTION_RESOLUTION) {
      rWarning("Ignore HARQ retx when not in contention resolution.");
      return;
    }
    rWarning("Contention Resolution is considered not successful. Stopping PDCCH Search and going to Response Error");
    response_error();
  });
}

} // namespace srsue
