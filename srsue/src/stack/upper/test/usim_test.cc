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

#include "srsran/support/srsran_test.h"
#include "srsue/hdr/stack/upper/usim.h"

using namespace srsue;

/*
Debug output generated from the OpenAirInterface HSS:

Converted 02f839 to plmn 208.93
Query: SELECT `key`,`sqn`,`rand`,`OPc` FROM `users` WHERE `users`.`imsi`='208930000000001'
Key: 8b.af.47.3f.2f.8f.d0.94.87.cc.cb.d7.09.7c.68.62.
Received SQN 00000000000000006999 converted to 6999
SQN: 00.00.00.00.1b.57.
RAND: 7c.f6.e2.6b.20.0a.ca.27.a1.a0.91.40.f5.cf.9d.62.
OPc: 8e.27.b6.af.0e.69.2e.75.0f.32.66.7a.3b.14.60.5d.
Generated random
RijndaelKeySchedule: K 8BAF473F2F8FD09487CCCBD7097C6862
MAC_A   : 84.ba.37.b0.f6.73.4d.d1.
SQN     : 00.00.00.00.1b.57.
RAND    : 88.38.c3.55.c8.78.aa.57.21.49.fe.69.db.68.6b.5a.
RijndaelKeySchedule: K 8BAF473F2F8FD09487CCCBD7097C6862
AK      : d7.44.51.9b.3e.fd.
CK      : 05.d3.53.3d.fe.7b.e7.2d.42.c7.bb.02.f2.8e.da.7f.
IK      : 26.33.a2.0b.dc.a8.9d.78.58.ba.42.47.8b.e4.d2.4d.
XRES    : e5.5d.88.27.91.8d.ac.c6.
AUTN    : d7.44.51.9b.25.aa.80.00.84.ba.37.b0.f6.73.4d.d1.
0x05 0xd3 0x53 0x3d 0xfe 0x7b 0xe7 0x2d 0x42 0xc7 0xbb 0x02 0xf2 0x8e
0xda 0x7f 0x26 0x33 0xa2 0x0b 0xdc 0xa8 0x9d 0x78 0x58 0xba 0x42 0x47
0x8b 0xe4 0xd2 0x4d 0x10 0x02 0xf8 0x39 0x00 0x03 0xd7 0x44 0x51 0x9b
0x25 0xaa 0x00 0x06
KASME   : a8.27.57.5e.ea.1a.10.17.3a.a1.bf.ce.4b.0c.21.85.e0.51.ef.bd.91.7f.fe.f5.1f.74.29.61.f9.03.7a.35.
*/

static uint8_t rand_enb[] =
    {0x88, 0x38, 0xc3, 0x55, 0xc8, 0x78, 0xaa, 0x57, 0x21, 0x49, 0xfe, 0x69, 0xdb, 0x68, 0x6b, 0x5a};
static uint8_t autn_enb[] =
    {0xd7, 0x44, 0x51, 0x9b, 0x25, 0xaa, 0x80, 0x00, 0x84, 0xba, 0x37, 0xb0, 0xf6, 0x73, 0x4d, 0xd1};

static constexpr uint16_t mcc = 208;
static constexpr uint16_t mnc = 93;

int gen_auth_response_test()
{
  uint8_t res[16];
  int     res_len;
  uint8_t k_asme[32];

  usim_args_t args;
  args.algo     = "milenage";
  args.imei     = "356092040793011";
  args.imsi     = "208930000000001";
  args.k        = "8BAF473F2F8FD09487CCCBD7097C6862";
  args.using_op = true;
  args.op       = "11111111111111111111111111111111";

  auto&       logger = srslog::fetch_basic_logger("USIM", false);
  srsue::usim usim(logger);
  usim.init(&args);

  TESTASSERT(usim.generate_authentication_response(rand_enb, autn_enb, mcc, mnc, res, &res_len, k_asme) == AUTH_OK);
  return SRSRAN_SUCCESS;
}

int mcc_mnc_msin_test()
{
  usim_args_t args;
  args.algo     = "milenage";
  args.imei     = "356092040793011";
  args.imsi     = "208930000000001";
  args.k        = "8BAF473F2F8FD09487CCCBD7097C6862";
  args.using_op = true;
  args.op       = "11111111111111111111111111111111";

  auto&       logger = srslog::fetch_basic_logger("USIM", false);
  srsue::usim usim(logger);
  usim.init(&args);

  uint8_t mcc_correct[]      = {0x2, 0x0, 0x8};
  uint8_t mnc_correct[]      = {0x9, 0x3, 0xf};
  uint8_t msin_correct_bcd[] = {0x00, 0x00, 0x00, 0x00, 0x10};

  uint8_t mcc_test[3];
  usim.get_home_mcc_bytes(mcc_test, 3);

  TESTASSERT(mcc_correct[0] == mcc_test[0]);
  TESTASSERT(mcc_correct[1] == mcc_test[1]);
  TESTASSERT(mcc_correct[2] == mcc_test[2]);

  uint8_t mnc_test[3];
  usim.get_home_mnc_bytes(mnc_test, 3);
  TESTASSERT(mnc_correct[0] == mnc_test[0]);
  TESTASSERT(mnc_correct[1] == mnc_test[1]);
  TESTASSERT(mnc_correct[2] == mnc_test[2]);

  uint8_t msin_test[5];
  usim.get_home_msin_bcd(msin_test, 5);

  TESTASSERT(msin_correct_bcd[0] == msin_test[0]);
  TESTASSERT(msin_correct_bcd[1] == msin_test[1]);
  TESTASSERT(msin_correct_bcd[2] == msin_test[2]);
  TESTASSERT(msin_correct_bcd[3] == msin_test[3]);
  TESTASSERT(msin_correct_bcd[4] == msin_test[4]);
  return SRSRAN_SUCCESS;
}

int main(int argc, char** argv)
{
  // Start the log backend.
  srslog::init();
  TESTASSERT(gen_auth_response_test() == SRSRAN_SUCCESS);
  TESTASSERT(mcc_mnc_msin_test() == SRSRAN_SUCCESS);
}
