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

#include <stdio.h>
#include <stdlib.h>

#include "srsran/common/liblte_security.h"
#include "srsran/common/security.h"
#include "srsran/common/test_common.h"
/*
 * Prototypes
 */

int32 arrcmp(uint8_t const* const a, uint8_t const* const b, uint32 len)
{
  uint32 i = 0;

  for (i = 0; i < len; i++) {
    if (a[i] != b[i]) {
      return a[i] - b[i];
    }
  }
  return 0;
}

void arrprint(uint8_t const* const a, uint32 len)
{
  uint32 i = 0;

  for (i = 0; i < len; i++) {
    printf("0x%02x ", a[i]);
    if ((i % 16 == 0) && i)
      printf("\n");
  }
  printf("\n");
  return;
}

/*
 * Tests
 *
 * Document Reference: 35.208 e00
 */

/*
 * Functions
 */

int test_set_2()
{
  LIBLTE_ERROR_ENUM err_lte = LIBLTE_ERROR_INVALID_INPUTS;
  int32             err_cmp = 0;

  uint8_t k[]    = {0x46, 0x5b, 0x5c, 0xe8, 0xb1, 0x99, 0xb4, 0x9f, 0xaa, 0x5f, 0x0a, 0x2e, 0xe2, 0x38, 0xa6, 0xbc};
  uint8_t rand[] = {0x23, 0x55, 0x3c, 0xbe, 0x96, 0x37, 0xa8, 0x9d, 0x21, 0x8a, 0xe6, 0x4d, 0xae, 0x47, 0xbf, 0x35};
  uint8_t sqn[]  = {0xff, 0x9b, 0xb4, 0xd0, 0xb6, 0x07};
  uint8_t amf[]  = {0xb9, 0xb9};
  uint8_t op[]   = {0xcd, 0xc2, 0x02, 0xd5, 0x12, 0x3e, 0x20, 0xf6, 0x2b, 0x6d, 0x67, 0x6a, 0xc7, 0x2c, 0xb3, 0x18};
  // f1

  uint8_t opc_o[16];
  err_lte = liblte_compute_opc(k, op, opc_o);
  TESTASSERT(err_lte == LIBLTE_SUCCESS);

  arrprint(opc_o, sizeof(opc_o));

  uint8_t opc_a[] = {0xcd, 0x63, 0xcb, 0x71, 0x95, 0x4a, 0x9f, 0x4e, 0x48, 0xa5, 0x99, 0x4e, 0x37, 0xa0, 0x2b, 0xaf};
  err_cmp         = arrcmp(opc_o, opc_a, sizeof(opc_o));
  TESTASSERT(err_cmp == 0);

  uint8_t mac_o[8];
  err_lte = liblte_security_milenage_f1(k, opc_o, rand, sqn, amf, mac_o);
  TESTASSERT(err_lte == LIBLTE_SUCCESS);

  arrprint(mac_o, sizeof(mac_o));

  uint8_t mac_a[] = {0x4a, 0x9f, 0xfa, 0xc3, 0x54, 0xdf, 0xaf, 0xb3};

  // compare mac a
  err_cmp = arrcmp(mac_o, mac_a, sizeof(mac_a));
  TESTASSERT(err_cmp == 0);

  // f1 star

  uint8_t mac_so[8];
  err_lte = liblte_security_milenage_f1_star(k, opc_o, rand, sqn, amf, mac_so);

  TESTASSERT(err_lte == LIBLTE_SUCCESS);

  uint8_t mac_s[] = {0x01, 0xcf, 0xaf, 0x9e, 0xc4, 0xe8, 0x71, 0xe9};

  arrprint(mac_so, sizeof(mac_so));

  err_cmp = arrcmp(mac_so, mac_s, sizeof(mac_s));
  TESTASSERT(err_cmp == 0);

  // f2345
  uint8_t res_o[8];
  uint8_t ck_o[16];
  uint8_t ik_o[16];
  uint8_t ak_o[6];

  err_lte = liblte_security_milenage_f2345(k, opc_o, rand, res_o, ck_o, ik_o, ak_o);

  TESTASSERT(err_lte == LIBLTE_SUCCESS);

  uint8_t res[] = {0xa5, 0x42, 0x11, 0xd5, 0xe3, 0xba, 0x50, 0xbf};
  uint8_t ck[]  = {0xb4, 0x0b, 0xa9, 0xa3, 0xc5, 0x8b, 0x2a, 0x05, 0xbb, 0xf0, 0xd9, 0x87, 0xb2, 0x1b, 0xf8, 0xcb};
  uint8_t ik[]  = {0xf7, 0x69, 0xbc, 0xd7, 0x51, 0x04, 0x46, 0x04, 0x12, 0x76, 0x72, 0x71, 0x1c, 0x6d, 0x34, 0x41};
  uint8_t ak[]  = {0xaa, 0x68, 0x9c, 0x64, 0x83, 0x70};

  // RESPONSE
  arrprint(res_o, sizeof(res_o));

  err_cmp = arrcmp(res_o, res, sizeof(res));
  TESTASSERT(err_cmp == 0);

  // CK
  arrprint(ck_o, sizeof(ck_o));

  err_cmp = arrcmp(ck_o, ck, sizeof(ck));
  TESTASSERT(err_cmp == 0);

  // IK
  arrprint(ik_o, sizeof(ik_o));
  err_cmp = arrcmp(ik_o, ik, sizeof(ik));
  TESTASSERT(err_cmp == 0);

  // AK
  arrprint(ak_o, sizeof(ak_o));
  err_cmp = arrcmp(ak_o, ak, sizeof(ak));
  TESTASSERT(err_cmp == 0);

  // f star
  uint8_t ak_star_o[6];

  err_lte = liblte_security_milenage_f5_star(k, opc_o, rand, ak_star_o);
  TESTASSERT(err_lte == LIBLTE_SUCCESS);

  arrprint(ak_star_o, sizeof(ak_star_o));
  uint8_t ak_star[] = {0x45, 0x1e, 0x8b, 0xec, 0xa4, 0x3b};
  err_cmp           = arrcmp(ak_star_o, ak_star, sizeof(ak_star));
  TESTASSERT(err_cmp == 0);
  return SRSRAN_SUCCESS;
}

/*
  Own test sets
*/

int test_set_xor_own_set_1()
{
  auto& logger = srslog::fetch_basic_logger("LOG", false);

  uint8_t k[]    = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
  uint8_t rand[] = {0xf9, 0x6a, 0xe3, 0x6e, 0x2d, 0x65, 0xfa, 0x84, 0x64, 0xc4, 0x98, 0xff, 0xc8, 0x30, 0x38, 0x0f};

  uint8_t res_o[16];
  uint8_t ck_o[16];
  uint8_t ik_o[16];
  uint8_t ak_o[6];

  TESTASSERT(srsran::security_xor_f2345(k, rand, res_o, ck_o, ik_o, ak_o) == SRSRAN_SUCCESS);

  uint8_t res[] = {0xf9, 0x7b, 0xc1, 0x5d, 0x69, 0x30, 0x9c, 0xf3};
  uint8_t ck[]  = {0x7b, 0xc1, 0x5d, 0x69, 0x30, 0x9c, 0xf3, 0xec, 0x5d, 0x32, 0x44, 0x04, 0xed, 0xd6, 0xf0, 0xf9};
  uint8_t ik[]  = {0xc1, 0x5d, 0x69, 0x30, 0x9c, 0xf3, 0xec, 0x5d, 0x32, 0x44, 0x04, 0xed, 0xd6, 0xf0, 0xf9, 0x7b};
  uint8_t ak[]  = {0x5d, 0x69, 0x30, 0x9c, 0xf3, 0xec};

  logger.info(res_o, sizeof(res_o), "RES: ");
  TESTASSERT(arrcmp(res_o, res, sizeof(res)) == 0);

  // CK
  logger.info(ck_o, sizeof(ck_o), "CK: ");
  TESTASSERT(arrcmp(ck_o, ck, sizeof(ck)) == 0);

  // IK
  logger.info(ik_o, sizeof(ik_o), "IK: ");
  TESTASSERT(arrcmp(ik_o, ik, sizeof(ik)) == 0);

  // AK
  logger.info(ak_o, sizeof(ak_o), "AK: ");
  TESTASSERT(arrcmp(ak_o, ak, sizeof(ak)) == 0);

  uint8_t sqn[] = {0x00, 0x00, 0x00, 0x00, 0x12, 0xd9};
  uint8_t amf[] = {0x90, 0x01};
  uint8_t mac_o[8];
  TESTASSERT(srsran::security_xor_f1(k, rand, sqn, amf, mac_o) == SRSRAN_SUCCESS);

  uint8_t mac[] = {0xf9, 0x7b, 0xc1, 0x5d, 0x7b, 0xe9, 0x0c, 0xf2};

  // MAC
  logger.info(mac_o, sizeof(mac_o), "MAC: ");
  TESTASSERT(arrcmp(mac_o, mac, sizeof(mac)) == 0);

  return SRSRAN_SUCCESS;
}

int main(int argc, char* argv[])
{
  auto& logger = srslog::fetch_basic_logger("LOG", false);
  logger.set_level(srslog::basic_levels::debug);
  logger.set_hex_dump_max_size(128);

  srslog::init();

  TESTASSERT(test_set_2() == SRSRAN_SUCCESS);
  TESTASSERT(test_set_xor_own_set_1() == SRSRAN_SUCCESS);
  return SRSRAN_SUCCESS;
}
