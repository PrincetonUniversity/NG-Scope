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

#include "srsenb/hdr/stack/rrc/rrc_cell_cfg.h"
#include "srsran/phy/utils/vector.h"

using namespace asn1::rrc;

namespace srsenb {

/********************************
 *    eNB cell common context
 *******************************/

enb_cell_common_list::enb_cell_common_list(const rrc_cfg_t& cfg_) : cfg(cfg_)
{
  cell_list.reserve(cfg.cell_list.size());

  // Store the SIB cfg of each carrier
  for (uint32_t ccidx = 0; ccidx < cfg.cell_list.size(); ++ccidx) {
    cell_list.emplace_back(std::unique_ptr<enb_cell_common>{new enb_cell_common{ccidx, cfg.cell_list[ccidx]}});
    enb_cell_common* new_cell = cell_list.back().get();

    // Set Cell MIB
    asn1::number_to_enum(new_cell->mib.dl_bw, cfg.cell.nof_prb);
    new_cell->mib.phich_cfg.phich_res.value = (phich_cfg_s::phich_res_opts::options)cfg.cell.phich_resources;
    new_cell->mib.phich_cfg.phich_dur.value = (phich_cfg_s::phich_dur_opts::options)cfg.cell.phich_length;

    // Set Cell SIB1
    new_cell->sib1 = cfg.sib1;
    // Update cellId
    sib_type1_s::cell_access_related_info_s_* cell_access = &new_cell->sib1.cell_access_related_info;
    cell_access->cell_id.from_number((cfg.enb_id << 8u) + new_cell->cell_cfg.cell_id);
    cell_access->tac.from_number(new_cell->cell_cfg.tac);
    // Update DL EARFCN
    new_cell->sib1.freq_band_ind = (uint8_t)srsran_band_get_band(new_cell->cell_cfg.dl_earfcn);

    // Set Cell SIB2
    // update PRACH root seq index for this cell
    new_cell->sib2                                      = cfg.sibs[1].sib2();
    new_cell->sib2.rr_cfg_common.prach_cfg.root_seq_idx = new_cell->cell_cfg.root_seq_idx;
    // update carrier freq
    if (new_cell->sib2.freq_info.ul_carrier_freq_present) {
      new_cell->sib2.freq_info.ul_carrier_freq = new_cell->cell_cfg.ul_earfcn;
    }
  }

  // Once all Cells are added to the list, fill the scell list of each cell for convenient access
  for (uint32_t i = 0; i < cell_list.size(); ++i) {
    auto& c = cell_list[i];
    c->scells.resize(cfg.cell_list[i].scell_list.size());
    for (uint32_t j = 0; j < c->scells.size(); ++j) {
      uint32_t cell_id = cfg.cell_list[i].scell_list[j].cell_id;
      auto it = std::find_if(cell_list.begin(), cell_list.end(), [cell_id](const std::unique_ptr<enb_cell_common>& e) {
        return e->cell_cfg.cell_id == cell_id;
      });
      if (it != cell_list.end()) {
        c->scells[j] = it->get();
      }
    }
  }
}

const enb_cell_common* enb_cell_common_list::get_cell_id(uint32_t cell_id) const
{
  auto it = std::find_if(cell_list.begin(), cell_list.end(), [cell_id](const std::unique_ptr<enb_cell_common>& c) {
    return c->cell_cfg.cell_id == cell_id;
  });
  return it == cell_list.end() ? nullptr : it->get();
}

const enb_cell_common* enb_cell_common_list::get_pci(uint32_t pci) const
{
  auto it = std::find_if(cell_list.begin(), cell_list.end(), [pci](const std::unique_ptr<enb_cell_common>& c) {
    return c->cell_cfg.pci == pci;
  });
  return it == cell_list.end() ? nullptr : it->get();
}

std::vector<const enb_cell_common*> get_cfg_intraenb_scells(const enb_cell_common_list& list, uint32_t pcell_enb_cc_idx)
{
  const enb_cell_common*              pcell = list.get_cc_idx(pcell_enb_cc_idx);
  std::vector<const enb_cell_common*> cells(pcell->cell_cfg.scell_list.size());
  for (uint32_t i = 0; i < pcell->cell_cfg.scell_list.size(); ++i) {
    uint32_t cell_id = pcell->cell_cfg.scell_list[i].cell_id;
    cells[i]         = list.get_cell_id(cell_id);
  }
  return cells;
}

std::vector<uint32_t> get_measobj_earfcns(const enb_cell_common& pcell)
{
  // Make a list made of EARFCNs of the PCell and respective SCells (according to conf file)
  std::vector<uint32_t> earfcns{};
  earfcns.reserve(1 + pcell.scells.size());
  earfcns.push_back(pcell.cell_cfg.dl_earfcn);
  for (auto& scell : pcell.scells) {
    earfcns.push_back(scell->cell_cfg.dl_earfcn);
  }
  // sort by earfcn
  std::sort(earfcns.begin(), earfcns.end());
  // remove duplicates
  earfcns.erase(std::unique(earfcns.begin(), earfcns.end()), earfcns.end());
  return earfcns;
}

/*************************
 *  eNB cell resources
 ************************/

freq_res_common_list::freq_res_common_list(const rrc_cfg_t& cfg_) : cfg(cfg_)
{
  for (const auto& c : cfg.cell_list) {
    auto it = pucch_res_list.find(c.dl_earfcn);
    if (it == pucch_res_list.end()) {
      pucch_res_list[c.dl_earfcn] = {};
    }
  }
}

cell_res_common* freq_res_common_list::get_earfcn(uint32_t earfcn)
{
  auto it = pucch_res_list.find(earfcn);
  return (it == pucch_res_list.end()) ? nullptr : &(it->second);
}

/*************************
 *  cell ctxt dedicated
 ************************/

ue_cell_ded_list::ue_cell_ded_list(const rrc_cfg_t&            cfg_,
                                   freq_res_common_list&       cell_res_list_,
                                   const enb_cell_common_list& enb_common_list) :
  logger(srslog::fetch_basic_logger("RRC")), cfg(cfg_), cell_res_list(cell_res_list_), common_list(enb_common_list)
{
  cell_list.reserve(common_list.nof_cells());
}

ue_cell_ded_list::~ue_cell_ded_list()
{
  for (auto& c : cell_list) {
    dealloc_cqi_resources(c.ue_cc_idx);
  }
  dealloc_sr_resources();
  dealloc_pucch_cs_resources();
}

ue_cell_ded* ue_cell_ded_list::get_enb_cc_idx(uint32_t enb_cc_idx)
{
  auto it = std::find_if(cell_list.begin(), cell_list.end(), [enb_cc_idx](const ue_cell_ded& c) {
    return c.cell_common->enb_cc_idx == enb_cc_idx;
  });
  return it == cell_list.end() ? nullptr : &(*it);
}

const ue_cell_ded* ue_cell_ded_list::find_cell(uint32_t earfcn, uint32_t pci) const
{
  auto it = std::find_if(cell_list.begin(), cell_list.end(), [earfcn, pci](const ue_cell_ded& c) {
    return c.get_pci() == pci and c.get_dl_earfcn() == earfcn;
  });
  return it == cell_list.end() ? nullptr : &(*it);
}

ue_cell_ded* ue_cell_ded_list::add_cell(uint32_t enb_cc_idx, bool init_pucch)
{
  const enb_cell_common* cell_common = common_list.get_cc_idx(enb_cc_idx);
  if (cell_common == nullptr) {
    logger.error("cell with enb_cc_idx=%d does not exist.", enb_cc_idx);
    return nullptr;
  }
  ue_cell_ded* ret = get_enb_cc_idx(enb_cc_idx);
  if (ret != nullptr) {
    logger.error("UE already registered cell %d", enb_cc_idx);
    return nullptr;
  }

  uint32_t ue_cc_idx = cell_list.size();

  if (ue_cc_idx == UE_PCELL_CC_IDX) {
    // Fetch PUCCH resources if it's pcell
    pucch_res = cell_res_list.get_earfcn(cell_common->cell_cfg.dl_earfcn);
  }

  cell_list.emplace_back(cell_list.size(), *cell_common);

  // Allocate CQI, SR, and PUCCH CS resources. If failure, do not add new cell
  if (init_pucch) {
    if (not alloc_cell_resources(ue_cc_idx)) {
      rem_last_cell();
      return nullptr;
    }
  }

  return &cell_list.back();
}

bool ue_cell_ded_list::rem_last_cell()
{
  if (cell_list.empty()) {
    return false;
  }
  uint32_t ue_cc_idx = cell_list.size() - 1;
  if (ue_cc_idx == UE_PCELL_CC_IDX) {
    dealloc_sr_resources();
    dealloc_pucch_cs_resources();
  }
  dealloc_cqi_resources(ue_cc_idx);
  cell_list.pop_back();
  return true;
}

bool ue_cell_ded_list::init_pucch_pcell()
{
  if (not alloc_cell_resources(UE_PCELL_CC_IDX)) {
    dealloc_sr_resources();
    dealloc_pucch_cs_resources();
    dealloc_cqi_resources(UE_PCELL_CC_IDX);
    return false;
  }
  return true;
}

bool ue_cell_ded_list::alloc_cell_resources(uint32_t ue_cc_idx)
{
  const uint32_t meas_gap_duration = 6;
  // Allocate CQI, SR, and PUCCH CS resources. If failure, do not add new cell
  if (ue_cc_idx == UE_PCELL_CC_IDX) {
    if (not alloc_sr_resources(cfg.sr_cfg.period)) {
      logger.error("Failed to allocate SR resources for PCell");
      return false;
    }

    ue_cell_ded* cell     = get_ue_cc_idx(UE_PCELL_CC_IDX);
    cell->meas_gap_offset = 0;
    cell->meas_gap_period = cell->cell_common->cell_cfg.meas_cfg.meas_gap_period;
    if (cell->meas_gap_period > 0) {
      if (not cell->cell_common->cell_cfg.meas_cfg.meas_gap_offset_subframe.empty()) {
        // subframes specified
        uint32_t min_users = std::numeric_limits<uint32_t>::max();
        for (uint32_t i = 0; i < cell->cell_common->cell_cfg.meas_cfg.meas_gap_offset_subframe.size(); ++i) {
          uint32_t idx_offset = cell->cell_common->cell_cfg.meas_cfg.meas_gap_offset_subframe[i] / meas_gap_duration;
          if (pucch_res->meas_gap_alloc_map[idx_offset] < min_users) {
            min_users             = pucch_res->meas_gap_alloc_map[idx_offset];
            cell->meas_gap_offset = cell->cell_common->cell_cfg.meas_cfg.meas_gap_offset_subframe[i];
          }
        }
      } else {
        uint32_t min_users = std::numeric_limits<uint32_t>::max();
        for (uint32_t meas_offset = 0; meas_offset < cell->cell_common->cell_cfg.meas_cfg.meas_gap_period;
             meas_offset += meas_gap_duration) {
          if (pucch_res->meas_gap_alloc_map[meas_offset / meas_gap_duration] < min_users) {
            min_users             = pucch_res->meas_gap_alloc_map[meas_offset / meas_gap_duration];
            cell->meas_gap_offset = meas_offset;
          }
        }
      }
      logger.info("Setup measurement gap period=%d offset=%d", cell->meas_gap_period, cell->meas_gap_offset);
    }
  } else {
    if (ue_cc_idx == 1 and not n_pucch_cs_present) {
      // Allocate resources for Format1b CS (will be optional PUCCH3/CS)
      if (not alloc_pucch_cs_resources()) {
        logger.error("Error allocating PUCCH Format1b CS resource for SCell");
        return false;
      }
    } else {
      // if nof CA cells > 1, the Format1b CS is not required
      dealloc_pucch_cs_resources();
    }
  }
  if (not alloc_cqi_resources(ue_cc_idx, cfg.cqi_cfg.period)) {
    logger.error("Failed to allocate CQI resources for cell ue_cc_idx=%d", ue_cc_idx);
    return false;
  }

  return true;
}

/**
 * Set UE Cells. Contrarily to rem_cell/add_cell(), this method avoids unnecessary reallocation of PUCCH resources.
 * @param enb_cc_idxs list of cells supported by the UE
 * @return true if all cells were allocated
 */
bool ue_cell_ded_list::set_cells(const std::vector<uint32_t>& enb_cc_idxs)
{
  // Remove extra previously allocked cells
  while (enb_cc_idxs.size() < cell_list.size()) {
    rem_last_cell();
  }
  if (cell_list.empty()) {
    // There were no previous cells allocated. Just add new ones
    for (auto& cc_idx : enb_cc_idxs) {
      if (not add_cell(cc_idx)) {
        return false;
      }
    }
    return true;
  }

  const enb_cell_common* prev_pcell            = cell_list[UE_PCELL_CC_IDX].cell_common;
  const enb_cell_common* new_pcell             = common_list.get_cc_idx(enb_cc_idxs[0]);
  bool                   pcell_freq_changed    = prev_pcell->cell_cfg.dl_earfcn != new_pcell->cell_cfg.dl_earfcn;
  uint32_t               prev_pcell_enb_cc_idx = prev_pcell->enb_cc_idx;

  if (pcell_freq_changed) {
    // Need to clean all allocated resources if PCell earfcn changes
    while (not cell_list.empty()) {
      rem_last_cell();
    }
    while (cell_list.size() < enb_cc_idxs.size()) {
      if (not add_cell(enb_cc_idxs[cell_list.size()])) {
        return false;
      }
    }
    return true;
  }

  uint32_t ue_cc_idx = 0;
  for (; ue_cc_idx < enb_cc_idxs.size(); ++ue_cc_idx) {
    uint32_t               enb_cc_idx  = enb_cc_idxs[ue_cc_idx];
    const enb_cell_common* cell_common = common_list.get_cc_idx(enb_cc_idx);
    if (cell_common == nullptr) {
      logger.error("cell with enb_cc_idx=%d does not exist.", enb_cc_idx);
      break;
    }
    auto* prev_cell_common = cell_list[ue_cc_idx].cell_common;
    if (enb_cc_idx == prev_cell_common->enb_cc_idx) {
      // Same cell. Do not realloc resources
      continue;
    }

    dealloc_cqi_resources(ue_cc_idx);
    cell_list[ue_cc_idx] = ue_cell_ded{ue_cc_idx, *cell_common};
    if (not alloc_cqi_resources(ue_cc_idx, cfg.cqi_cfg.period)) {
      logger.error("Failed to allocate CQI resources for cell ue_cc_idx=%d", ue_cc_idx);
      break;
    }
  }
  // Remove cells after the last successful insertion
  while (ue_cc_idx < cell_list.size()) {
    rem_last_cell();
  }
  if (cell_list.empty()) {
    // We failed to allocate new PCell. Fallback to old PCell
    add_cell(prev_pcell_enb_cc_idx);
  }
  return ue_cc_idx == enb_cc_idxs.size();
}

bool ue_cell_ded_list::alloc_cqi_resources(uint32_t ue_cc_idx, uint32_t period)
{
  ue_cell_ded* cell = get_ue_cc_idx(ue_cc_idx);
  if (cell == nullptr) {
    logger.error("The user cell ue_cc_idx=%d has not been allocated", ue_cc_idx);
    return false;
  }
  if (cell->cqi_res_present) {
    logger.error("The user cqi resources for cell ue_cc_idx=%d are already allocated", ue_cc_idx);
    return false;
  }

  uint32_t max_users = 12;

  // Allocate all CQI resources for all carriers now
  // Find freq-time resources with least number of users
  int      i_min = -1, j_min = -1;
  uint32_t min_users = std::numeric_limits<uint32_t>::max();
  for (uint32_t i = 0; i < cfg.sibs[1].sib2().rr_cfg_common.pucch_cfg_common.nrb_cqi; i++) {
    for (uint32_t j = 0; j < cfg.cqi_cfg.nof_subframes; j++) {
      if (pucch_res->cqi_sched.nof_users[i][j] < min_users) {
        i_min     = i;
        j_min     = j;
        min_users = pucch_res->cqi_sched.nof_users[i][j];
      }
    }
  }
  if (pucch_res->cqi_sched.nof_users[i_min][j_min] > max_users || i_min == -1 || j_min == -1) {
    logger.error("Not enough PUCCH resources to allocate Scheduling Request");
    return false;
  }

  uint16_t pmi_idx = 0;

  // Compute I_sr
  if (period != 2 && period != 5 && period != 10 && period != 20 && period != 40 && period != 80 && period != 160 &&
      period != 32 && period != 64 && period != 128) {
    logger.error("Invalid CQI Report period %d ms", period);
    return false;
  }
  if (cfg.cqi_cfg.sf_mapping[j_min] < period) {
    if (period != 32 && period != 64 && period != 128) {
      if (period > 2) {
        pmi_idx = period - 3 + cfg.cqi_cfg.sf_mapping[j_min];
      } else {
        pmi_idx = cfg.cqi_cfg.sf_mapping[j_min];
      }
    } else {
      if (period == 32) {
        pmi_idx = 318 + cfg.cqi_cfg.sf_mapping[j_min];
      } else if (period == 64) {
        pmi_idx = 350 + cfg.cqi_cfg.sf_mapping[j_min];
      } else {
        pmi_idx = 414 + cfg.cqi_cfg.sf_mapping[j_min];
      }
    }
  } else {
    logger.error("Allocating CQI: invalid sf_idx=%d for period=%d", cfg.cqi_cfg.sf_mapping[j_min], period);
    return false;
  }

  // Compute n_pucch_2
  uint16_t n_pucch = i_min * max_users + pucch_res->cqi_sched.nof_users[i_min][j_min];

  cell->cqi_res_present   = true;
  cell->cqi_res.pmi_idx   = pmi_idx;
  cell->cqi_res.pucch_res = n_pucch;
  cell->cqi_res.prb_idx   = i_min;
  cell->cqi_res.sf_idx    = j_min;

  pucch_res->cqi_sched.nof_users[i_min][j_min]++;

  logger.info("Allocated CQI resources for ue_cc_idx=%d, time-frequency slot (%d, %d), n_pucch_2=%d, pmi_cfg_idx=%d",
              ue_cc_idx,
              i_min,
              j_min,
              n_pucch,
              pmi_idx);
  return true;
}

bool ue_cell_ded_list::dealloc_cqi_resources(uint32_t ue_cc_idx)
{
  ue_cell_ded* c = get_ue_cc_idx(ue_cc_idx);
  if (c == nullptr or not c->cqi_res_present) {
    return false;
  }

  if (pucch_res->cqi_sched.nof_users[c->cqi_res.prb_idx][c->cqi_res.sf_idx] > 0) {
    pucch_res->cqi_sched.nof_users[c->cqi_res.prb_idx][c->cqi_res.sf_idx]--;
    logger.info("Deallocated CQI resources for time-frequency slot (%d, %d)", c->cqi_res.prb_idx, c->cqi_res.sf_idx);
  } else {
    logger.warning(
        "Removing CQI resources: no users in time-frequency slot (%d, %d)", c->cqi_res.prb_idx, c->cqi_res.sf_idx);
  }
  c->cqi_res_present = false;
  return true;
}

bool ue_cell_ded_list::alloc_sr_resources(uint32_t period)
{
  ue_cell_ded* cell = get_ue_cc_idx(UE_PCELL_CC_IDX);
  if (cell == nullptr) {
    logger.error("The user cell pcell has not been allocated");
    return false;
  }
  if (sr_res_present) {
    logger.error("The user sr resources are already allocated");
    return false;
  }

  uint32_t c                 = SRSRAN_CP_ISNORM(cfg.cell.cp) ? 3 : 2;
  uint32_t delta_pucch_shift = cell->cell_common->sib2.rr_cfg_common.pucch_cfg_common.delta_pucch_shift.to_number();
  delta_pucch_shift          = SRSRAN_MAX(1, delta_pucch_shift); // prevent div by zero
  uint32_t max_users         = 12 * c / delta_pucch_shift;

  // Find freq-time resources with least number of users
  int      i_min = -1, j_min = -1;
  uint32_t min_users = std::numeric_limits<uint32_t>::max();
  for (uint32_t i = 0; i < cfg.sr_cfg.nof_prb; i++) {
    for (uint32_t j = 0; j < cfg.sr_cfg.nof_subframes; j++) {
      if (pucch_res->sr_sched.nof_users[i][j] < min_users) {
        i_min     = i;
        j_min     = j;
        min_users = pucch_res->sr_sched.nof_users[i][j];
      }
    }
  }

  if (pucch_res->sr_sched.nof_users[i_min][j_min] > max_users || i_min == -1 || j_min == -1) {
    logger.error("Not enough PUCCH resources to allocate Scheduling Request");
    return false;
  }

  // Compute I_sr
  if (period != 5 && period != 10 && period != 20 && period != 40 && period != 80) {
    logger.error("Invalid SchedulingRequest period %d ms", period);
    return false;
  }
  if (cfg.sr_cfg.sf_mapping[j_min] < period) {
    sr_res.sr_I = period - 5 + cfg.sr_cfg.sf_mapping[j_min];
  } else {
    logger.error("Allocating SR: invalid sf_idx=%d for period=%d", cfg.sr_cfg.sf_mapping[j_min], period);
    return false;
  }

  // Compute N_pucch_sr
  sr_res.sr_N_pucch = i_min * max_users + pucch_res->sr_sched.nof_users[i_min][j_min];
  if (cell->cell_common->sib2.rr_cfg_common.pucch_cfg_common.ncs_an) {
    sr_res.sr_N_pucch += cell->cell_common->sib2.rr_cfg_common.pucch_cfg_common.ncs_an;
  }

  // Allocate user
  pucch_res->sr_sched.nof_users[i_min][j_min]++;
  sr_res.sr_sched_prb_idx = i_min;
  sr_res.sr_sched_sf_idx  = j_min;
  sr_res_present          = true;

  logger.info("Allocated SR resources in time-freq slot (%d, %d), sf_cfg_idx=%d",
              sr_res.sr_sched_sf_idx,
              sr_res.sr_sched_prb_idx,
              cfg.sr_cfg.sf_mapping[sr_res.sr_sched_sf_idx]);

  return true;
}

bool ue_cell_ded_list::dealloc_sr_resources()
{
  if (sr_res_present) {
    if (pucch_res->sr_sched.nof_users[sr_res.sr_sched_prb_idx][sr_res.sr_sched_sf_idx] > 0) {
      pucch_res->sr_sched.nof_users[sr_res.sr_sched_prb_idx][sr_res.sr_sched_sf_idx]--;
    } else {
      logger.warning("Removing SR resources: no users in time-frequency slot (%d, %d)",
                     sr_res.sr_sched_prb_idx,
                     sr_res.sr_sched_sf_idx);
    }
    logger.info(
        "Deallocated SR resources for time-frequency slot (%d, %d)", sr_res.sr_sched_prb_idx, sr_res.sr_sched_sf_idx);
    logger.debug("Remaining SR allocations for slot (%d, %d): %d",
                 sr_res.sr_sched_prb_idx,
                 sr_res.sr_sched_sf_idx,
                 pucch_res->sr_sched.nof_users[sr_res.sr_sched_prb_idx][sr_res.sr_sched_sf_idx]);
    sr_res_present = false;
    return true;
  }
  return false;
}

bool ue_cell_ded_list::alloc_pucch_cs_resources()
{
  ue_cell_ded* cell = get_ue_cc_idx(UE_PCELL_CC_IDX);
  if (cell == nullptr) {
    logger.error("The user cell pcell has not been allocated");
    return false;
  }
  if (n_pucch_cs_present) {
    logger.error("The user sr resources are already allocated");
    return false;
  }

  const sib_type2_s& sib2      = cell->cell_common->sib2;
  const uint16_t     N_pucch_1 = sib2.rr_cfg_common.pucch_cfg_common.n1_pucch_an;
  // Loop through all available resources
  for (uint32_t i = 0; i < cell_res_common::N_PUCCH_MAX_RES; i++) {
    if (!pucch_res->n_pucch_cs_used[i] && (i <= N_pucch_1 && i != sr_res.sr_N_pucch)) {
      // Allocate resource
      pucch_res->n_pucch_cs_used[i] = true;
      n_pucch_cs_idx                = i;
      n_pucch_cs_present            = true;
      logger.info("Allocated N_pucch_cs=%d", n_pucch_cs_idx);
      return true;
    }
  }
  logger.warning("Could not allocated N_pucch_cs");
  return false;
}

bool ue_cell_ded_list::dealloc_pucch_cs_resources()
{
  if (n_pucch_cs_present) {
    pucch_res->n_pucch_cs_used[n_pucch_cs_idx] = false;
    n_pucch_cs_present                         = false;
    logger.info("Deallocated N_pucch_cs=%d", n_pucch_cs_idx);
    return true;
  }
  return false;
}

} // namespace srsenb
