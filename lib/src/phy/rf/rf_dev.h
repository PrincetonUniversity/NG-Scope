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

#include "srsran/phy/rf/rf.h"
#include <stdbool.h>

/* RF frontend API */
typedef struct {
  const char* name;
  const char* (*srsran_rf_devname)(void* h);
  int (*srsran_rf_start_rx_stream)(void* h, bool now);
  int (*srsran_rf_stop_rx_stream)(void* h);
  void (*srsran_rf_flush_buffer)(void* h);
  bool (*srsran_rf_has_rssi)(void* h);
  float (*srsran_rf_get_rssi)(void* h);
  void (*srsran_rf_suppress_stdout)(void* h);
  void (*srsran_rf_register_error_handler)(void* h, srsran_rf_error_handler_t error_handler, void* arg);
  int (*srsran_rf_open)(char* args, void** h);
  int (*srsran_rf_open_multi)(char* args, void** h, uint32_t nof_channels);
  int (*srsran_rf_close)(void* h);
  double (*srsran_rf_set_rx_srate)(void* h, double freq);
  int (*srsran_rf_set_rx_gain)(void* h, double gain);
  int (*srsran_rf_set_rx_gain_ch)(void* h, uint32_t ch, double gain);
  int (*srsran_rf_set_tx_gain)(void* h, double gain);
  int (*srsran_rf_set_tx_gain_ch)(void* h, uint32_t ch, double gain);
  double (*srsran_rf_get_rx_gain)(void* h);
  double (*srsran_rf_get_tx_gain)(void* h);
  srsran_rf_info_t* (*srsran_rf_get_info)(void* h);
  double (*srsran_rf_set_rx_freq)(void* h, uint32_t ch, double freq);
  double (*srsran_rf_set_tx_srate)(void* h, double freq);
  double (*srsran_rf_set_tx_freq)(void* h, uint32_t ch, double freq);
  void (*srsran_rf_get_time)(void* h, time_t* secs, double* frac_secs);
  void (*srsran_rf_sync_pps)(void* h);
  int (*srsran_rf_recv_with_time)(void*    h,
                                  void*    data,
                                  uint32_t nsamples,
                                  bool     blocking,
                                  time_t*  secs,
                                  double*  frac_secs);
  int (*srsran_rf_recv_with_time_multi)(void*    h,
                                        void**   data,
                                        uint32_t nsamples,
                                        bool     blocking,
                                        time_t*  secs,
                                        double*  frac_secs);
  int (*srsran_rf_send_timed)(void*  h,
                              void*  data,
                              int    nsamples,
                              time_t secs,
                              double frac_secs,
                              bool   has_time_spec,
                              bool   blocking,
                              bool   is_start_of_burst,
                              bool   is_end_of_burst);
  int (*srsran_rf_send_timed_multi)(void*  h,
                                    void** data,
                                    int    nsamples,
                                    time_t secs,
                                    double frac_secs,
                                    bool   has_time_spec,
                                    bool   blocking,
                                    bool   is_start_of_burst,
                                    bool   is_end_of_burst);
} rf_dev_t;

/* Define implementation for UHD */
#ifdef ENABLE_UHD

#include "rf_uhd_imp.h"

static rf_dev_t dev_uhd = {"UHD",
                           rf_uhd_devname,
                           rf_uhd_start_rx_stream,
                           rf_uhd_stop_rx_stream,
                           rf_uhd_flush_buffer,
                           rf_uhd_has_rssi,
                           rf_uhd_get_rssi,
                           rf_uhd_suppress_stdout,
                           rf_uhd_register_error_handler,
                           rf_uhd_open,
                           .srsran_rf_open_multi = rf_uhd_open_multi,
                           rf_uhd_close,
                           rf_uhd_set_rx_srate,
                           rf_uhd_set_rx_gain,
                           rf_uhd_set_rx_gain_ch,
                           rf_uhd_set_tx_gain,
                           rf_uhd_set_tx_gain_ch,
                           rf_uhd_get_rx_gain,
                           rf_uhd_get_tx_gain,
                           rf_uhd_get_info,
                           rf_uhd_set_rx_freq,
                           rf_uhd_set_tx_srate,
                           rf_uhd_set_tx_freq,
                           rf_uhd_get_time,
                           rf_uhd_sync_pps,
                           rf_uhd_recv_with_time,
                           rf_uhd_recv_with_time_multi,
                           rf_uhd_send_timed,
                           .srsran_rf_send_timed_multi = rf_uhd_send_timed_multi};
#endif

/* Define implementation for bladeRF */
#ifdef ENABLE_BLADERF

#include "rf_blade_imp.h"

static rf_dev_t dev_blade = {"bladeRF",
                             rf_blade_devname,
                             rf_blade_start_rx_stream,
                             rf_blade_stop_rx_stream,
                             rf_blade_flush_buffer,
                             rf_blade_has_rssi,
                             rf_blade_get_rssi,
                             rf_blade_suppress_stdout,
                             rf_blade_register_error_handler,
                             rf_blade_open,
                             .srsran_rf_open_multi = rf_blade_open_multi,
                             rf_blade_close,
                             rf_blade_set_rx_srate,
                             rf_blade_set_rx_gain,
                             rf_blade_set_rx_gain_ch,
                             rf_blade_set_tx_gain,
                             rf_blade_set_tx_gain_ch,
                             rf_blade_get_rx_gain,
                             rf_blade_get_tx_gain,
                             rf_blade_get_info,
                             rf_blade_set_rx_freq,
                             rf_blade_set_tx_srate,
                             rf_blade_set_tx_freq,
                             rf_blade_get_time,
                             NULL,
                             rf_blade_recv_with_time,
                             rf_blade_recv_with_time_multi,
                             rf_blade_send_timed,
                             .srsran_rf_send_timed_multi = rf_blade_send_timed_multi};
#endif

#ifdef ENABLE_SOAPYSDR

#include "rf_soapy_imp.h"

static rf_dev_t dev_soapy = {"soapy",
                             rf_soapy_devname,
                             rf_soapy_start_rx_stream,
                             rf_soapy_stop_rx_stream,
                             rf_soapy_flush_buffer,
                             rf_soapy_has_rssi,
                             rf_soapy_get_rssi,
                             rf_soapy_suppress_stdout,
                             rf_soapy_register_error_handler,
                             rf_soapy_open,
                             rf_soapy_open_multi,
                             rf_soapy_close,
                             rf_soapy_set_rx_srate,
                             rf_soapy_set_rx_gain,
                             rf_soapy_set_rx_gain_ch,
                             rf_soapy_set_tx_gain,
                             rf_soapy_set_tx_gain_ch,
                             rf_soapy_get_rx_gain,
                             rf_soapy_get_tx_gain,
                             rf_soapy_get_info,
                             rf_soapy_set_rx_freq,
                             rf_soapy_set_tx_srate,
                             rf_soapy_set_tx_freq,
                             rf_soapy_get_time,
                             NULL,
                             rf_soapy_recv_with_time,
                             rf_soapy_recv_with_time_multi,
                             rf_soapy_send_timed,
                             .srsran_rf_send_timed_multi = rf_soapy_send_timed_multi};

#endif

/* Define implementation for UHD */
#ifdef ENABLE_ZEROMQ

#include "rf_zmq_imp.h"

static rf_dev_t dev_zmq = {"zmq",
                           rf_zmq_devname,
                           rf_zmq_start_rx_stream,
                           rf_zmq_stop_rx_stream,
                           rf_zmq_flush_buffer,
                           rf_zmq_has_rssi,
                           rf_zmq_get_rssi,
                           rf_zmq_suppress_stdout,
                           rf_zmq_register_error_handler,
                           rf_zmq_open,
                           .srsran_rf_open_multi = rf_zmq_open_multi,
                           rf_zmq_close,
                           rf_zmq_set_rx_srate,
                           rf_zmq_set_rx_gain,
                           rf_zmq_set_rx_gain_ch,
                           rf_zmq_set_tx_gain,
                           rf_zmq_set_tx_gain_ch,
                           rf_zmq_get_rx_gain,
                           rf_zmq_get_tx_gain,
                           rf_zmq_get_info,
                           rf_zmq_set_rx_freq,
                           rf_zmq_set_tx_srate,
                           rf_zmq_set_tx_freq,
                           rf_zmq_get_time,
                           NULL,
                           rf_zmq_recv_with_time,
                           rf_zmq_recv_with_time_multi,
                           rf_zmq_send_timed,
                           .srsran_rf_send_timed_multi = rf_zmq_send_timed_multi};
#endif

/* Define implementation for Sidekiq */
#ifdef ENABLE_SIDEKIQ

#include "rf_skiq_imp.h"

static rf_dev_t dev_skiq = {.name                             = "Sidekiq",
                            .srsran_rf_devname                = rf_skiq_devname,
                            .srsran_rf_start_rx_stream        = rf_skiq_start_rx_stream,
                            .srsran_rf_stop_rx_stream         = rf_skiq_stop_rx_stream,
                            .srsran_rf_flush_buffer           = rf_skiq_flush_buffer,
                            .srsran_rf_has_rssi               = rf_skiq_has_rssi,
                            .srsran_rf_get_rssi               = rf_skiq_get_rssi,
                            .srsran_rf_suppress_stdout        = rf_skiq_suppress_stdout,
                            .srsran_rf_register_error_handler = rf_skiq_register_error_handler,
                            .srsran_rf_open                   = rf_skiq_open,
                            .srsran_rf_open_multi             = rf_skiq_open_multi,
                            .srsran_rf_close                  = rf_skiq_close,
                            .srsran_rf_set_rx_srate           = rf_skiq_set_rx_srate,
                            .srsran_rf_set_tx_srate           = rf_skiq_set_tx_srate,
                            .srsran_rf_set_rx_gain            = rf_skiq_set_rx_gain,
                            .srsran_rf_set_tx_gain            = rf_skiq_set_tx_gain,
                            .srsran_rf_set_tx_gain_ch         = rf_skiq_set_tx_gain_ch,
                            .srsran_rf_set_rx_gain_ch         = rf_skiq_set_rx_gain_ch,
                            .srsran_rf_get_rx_gain            = rf_skiq_get_rx_gain,
                            .srsran_rf_get_tx_gain            = rf_skiq_get_tx_gain,
                            .srsran_rf_get_info               = rf_skiq_get_info,
                            .srsran_rf_set_rx_freq            = rf_skiq_set_rx_freq,
                            .srsran_rf_set_tx_freq            = rf_skiq_set_tx_freq,
                            .srsran_rf_get_time               = rf_skiq_get_time,
                            .srsran_rf_recv_with_time         = rf_skiq_recv_with_time,
                            .srsran_rf_recv_with_time_multi   = rf_skiq_recv_with_time_multi,
                            .srsran_rf_send_timed             = rf_skiq_send_timed,
                            .srsran_rf_send_timed_multi       = rf_skiq_send_timed_multi};
#endif

//#define ENABLE_DUMMY_DEV

#ifdef ENABLE_DUMMY_DEV
int dummy_rcv()
{
  usleep(100000);
  return 1;
}
void dummy_fnc() {}

static rf_dev_t dev_dummy = {"dummy",   dummy_fnc, dummy_fnc, dummy_fnc, dummy_fnc, dummy_fnc, dummy_fnc, dummy_fnc,
                             dummy_fnc, dummy_fnc, dummy_fnc, dummy_fnc, dummy_fnc, dummy_fnc, dummy_fnc, dummy_fnc,
                             dummy_fnc, dummy_fnc, dummy_fnc, dummy_fnc, dummy_fnc, dummy_fnc, dummy_fnc, dummy_rcv,
                             dummy_fnc, dummy_fnc, dummy_fnc, dummy_fnc, dummy_fnc};
#endif

static rf_dev_t* available_devices[] = {

#ifdef ENABLE_UHD
    &dev_uhd,
#endif
#ifdef ENABLE_SOAPYSDR
    &dev_soapy,
#endif
#ifdef ENABLE_BLADERF
    &dev_blade,
#endif
#ifdef ENABLE_ZEROMQ
    &dev_zmq,
#endif
#ifdef ENABLE_SIDEKIQ
    &dev_skiq,
#endif
#ifdef ENABLE_DUMMY_DEV
    &dev_dummy,
#endif
    NULL};
