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

#include "srsenb/hdr/stack/mac/nr/sched_nr_time_rr.h"

namespace srsenb {
namespace sched_nr_impl {

template <typename Predicate>
bool round_robin_apply(slot_ue_map_t& ue_db, uint32_t rr_count, Predicate p)
{
  if (ue_db.empty()) {
    return false;
  }
  auto it = ue_db.begin();
  std::advance(it, (rr_count % ue_db.size()));
  for (uint32_t count = 0; count < ue_db.size(); ++count, ++it) {
    if (it == ue_db.end()) {
      it = ue_db.begin();
    }
    if (p(it->second)) {
      return true;
    }
  }
  return false;
}

void sched_nr_time_rr::sched_dl_users(slot_ue_map_t& ue_db, bwp_slot_allocator& slot_alloc)
{
  // Start with retxs
  if (round_robin_apply(ue_db, slot_alloc.get_pdcch_tti().to_uint(), [&slot_alloc](slot_ue& ue) {
        if (ue.h_dl != nullptr and ue.h_dl->has_pending_retx(slot_alloc.get_tti_rx())) {
          alloc_result res = slot_alloc.alloc_pdsch(ue, ue.h_dl->prbs());
          if (res == alloc_result::success) {
            return true;
          }
        }
        return false;
      })) {
    return;
  }

  // Move on to new txs
  round_robin_apply(ue_db, slot_alloc.get_pdcch_tti().to_uint(), [&slot_alloc](slot_ue& ue) {
    if (ue.h_dl != nullptr and ue.h_dl->empty()) {
      alloc_result res = slot_alloc.alloc_pdsch(ue, prb_interval{0, slot_alloc.cfg.cfg.rb_width});
      if (res == alloc_result::success) {
        return true;
      }
    }
    return false;
  });
}

void sched_nr_time_rr::sched_ul_users(slot_ue_map_t& ue_db, bwp_slot_allocator& slot_alloc)
{
  // Start with retxs
  if (round_robin_apply(ue_db, slot_alloc.get_pdcch_tti().to_uint(), [&slot_alloc](slot_ue& ue) {
        if (ue.h_ul != nullptr and ue.h_ul->has_pending_retx(slot_alloc.get_tti_rx())) {
          alloc_result res = slot_alloc.alloc_pusch(ue, ue.h_ul->prbs());
          if (res == alloc_result::success) {
            return true;
          }
        }
        return false;
      })) {
    return;
  }

  // Move on to new txs
  round_robin_apply(ue_db, slot_alloc.get_pdcch_tti().to_uint(), [&slot_alloc](slot_ue& ue) {
    if (ue.h_ul != nullptr and ue.h_ul->empty()) {
      alloc_result res = slot_alloc.alloc_pusch(ue, prb_interval{0, slot_alloc.cfg.cfg.rb_width});
      if (res == alloc_result::success) {
        return true;
      }
    }
    return false;
  });
}

} // namespace sched_nr_impl
} // namespace srsenb
