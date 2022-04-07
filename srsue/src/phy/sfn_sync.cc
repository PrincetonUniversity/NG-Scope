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

#include "srsue/hdr/phy/sfn_sync.h"
#include "srsran/interfaces/ue_phy_interfaces.h"

#define Error(fmt, ...)                                                                                                \
  if (SRSRAN_DEBUG_ENABLED)                                                                                            \
  logger.error(fmt, ##__VA_ARGS__)
#define Warning(fmt, ...)                                                                                              \
  if (SRSRAN_DEBUG_ENABLED)                                                                                            \
  logger.warning(fmt, ##__VA_ARGS__)
#define Info(fmt, ...)                                                                                                 \
  if (SRSRAN_DEBUG_ENABLED)                                                                                            \
  logger.info(fmt, ##__VA_ARGS__)
#define Debug(fmt, ...)                                                                                                \
  if (SRSRAN_DEBUG_ENABLED)                                                                                            \
  logger.debug(fmt, ##__VA_ARGS__)

namespace srsue {

sfn_sync::~sfn_sync()
{
  srsran_ue_mib_free(&ue_mib);
}

void sfn_sync::init(srsran_ue_sync_t*    ue_sync_,
                    const phy_args_t*    phy_args_,
                    srsran::rf_buffer_t& buffer,
                    uint32_t             buffer_max_samples_,
                    uint32_t             nof_subframes)
{
  ue_sync  = ue_sync_;
  phy_args = phy_args_;
  timeout  = nof_subframes;

  mib_buffer         = buffer;
  buffer_max_samples = buffer_max_samples_;

  // MIB decoder uses a single receiver antenna in logical channel 0
  if (srsran_ue_mib_init(&ue_mib, buffer.get(0), SRSRAN_MAX_PRB)) {
    Error("SYNC:  Initiating UE MIB decoder");
  }
}

bool sfn_sync::set_cell(srsran_cell_t cell_)
{
  if (srsran_ue_mib_set_cell(&ue_mib, cell_)) {
    Error("SYNC:  Setting cell: initiating ue_mib");
    return false;
  }
  reset();
  return true;
}

void sfn_sync::reset()
{
  cnt = 0;
  srsran_ue_mib_reset(&ue_mib);
}

sfn_sync::ret_code sfn_sync::run_subframe(srsran_cell_t*                               cell_,
                                          uint32_t*                                    tti_cnt,
                                          std::array<uint8_t, SRSRAN_BCH_PAYLOAD_LEN>& bch_payload,
                                          bool                                         sfidx_only)
{
  int ret = srsran_ue_sync_zerocopy(ue_sync, mib_buffer.to_cf_t(), buffer_max_samples);
  if (ret < 0) {
    Error("SYNC:  Error calling ue_sync_get_buffer.");
    return ERROR;
  }

  if (ret == 1) {
    sfn_sync::ret_code ret2 = decode_mib(cell_, tti_cnt, nullptr, bch_payload, sfidx_only);
    if (ret2 != SFN_NOFOUND) {
      return ret2;
    }
  } else {
    Info("SYNC:  Waiting for PSS while trying to decode MIB (%d/%d)", cnt, timeout);
  }

  cnt++;
  if (cnt >= timeout) {
    cnt = 0;
    return SFN_NOFOUND;
  }

  return IDLE;
}

sfn_sync::ret_code sfn_sync::decode_mib(srsran_cell_t*                               cell_,
                                        uint32_t*                                    tti_cnt,
                                        srsran::rf_buffer_t*                         ext_buffer,
                                        std::array<uint8_t, SRSRAN_BCH_PAYLOAD_LEN>& bch_payload,
                                        bool                                         sfidx_only)
{
  // If external buffer provided not equal to internal buffer, copy samples from channel/port 0
  if (ext_buffer != nullptr) {
    memcpy(mib_buffer.get(0), ext_buffer->get(0), sizeof(cf_t) * ue_sync->sf_len);
  }

  if (srsran_ue_sync_get_sfidx(ue_sync) == 0) {
    // Skip MIB decoding if we are only interested in subframe 0
    if (sfidx_only) {
      if (tti_cnt) {
        *tti_cnt = 0;
      }
      return SFX0_FOUND;
    }

    int sfn_offset = 0;
    int n          = srsran_ue_mib_decode(&ue_mib, bch_payload.data(), NULL, &sfn_offset);
    switch (n) {
      default:
        Error("SYNC:  Error decoding MIB while synchronising SFN");
        return ERROR;
      case SRSRAN_UE_MIB_FOUND:
        uint32_t sfn;
        srsran_pbch_mib_unpack(bch_payload.data(), cell_, &sfn);

        sfn = (sfn + sfn_offset) % 1024;
        if (tti_cnt) {
          *tti_cnt = 10 * sfn;

          // Check if SNR is below the minimum threshold
          if (ue_mib.chest_res.snr_db < phy_args->in_sync_snr_db_th) {
            Info("SYNC:  MIB decoded, SNR is too low (%+.1f < %+.1f)",
                 ue_mib.chest_res.snr_db,
                 phy_args->in_sync_snr_db_th);
            return SFN_NOFOUND;
          }

          Info("SYNC:  DONE, SNR=%.1f dB, TTI=%d, sfn_offset=%d", ue_mib.chest_res.snr_db, *tti_cnt, sfn_offset);
        }

        reset();
        return SFN_FOUND;
      case SRSRAN_UE_MIB_NOTFOUND:
        Info("SYNC:  Found PSS but could not decode MIB. SNR=%.1f dB (%d/%d)", ue_mib.chest_res.snr_db, cnt, timeout);
        return SFN_NOFOUND;
    }
  }

  return IDLE;
}

}; // namespace srsue