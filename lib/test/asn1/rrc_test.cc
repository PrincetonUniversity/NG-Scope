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

#include "srsran/asn1/rrc.h"
#include "srsran/common/test_common.h"
#include <cstdio>

using namespace asn1;
using namespace asn1::rrc;

// TESTS

int test_generic()
{
  pusch_enhance_cfg_r14_c choice_type1;

  // make suce a choice is always started in null mode
  TESTASSERT(choice_type1.type() == pusch_enhance_cfg_r14_c::types::nulltype);

  // Test deep copy of choice types
  sib_type14_r11_s::eab_param_r11_c_ choice2;
  choice2.set_eab_per_plmn_list_r11();
  choice2.eab_per_plmn_list_r11().resize(5);
  for (uint32_t i = 0; i < 5; ++i) {
    choice2.eab_per_plmn_list_r11()[i].eab_cfg_r11_present = true;
  }
  {
    sib_type14_r11_s::eab_param_r11_c_ choice2_cpy(choice2);
    TESTASSERT(choice2_cpy.type() == sib_type14_r11_s::eab_param_r11_c_::types::eab_per_plmn_list_r11);
    TESTASSERT(choice2_cpy.eab_per_plmn_list_r11()[3].eab_cfg_r11_present);
    TESTASSERT(choice2_cpy.eab_per_plmn_list_r11().data() != choice2.eab_per_plmn_list_r11().data());

    choice2_cpy = choice2;
    TESTASSERT(choice2_cpy.type() == sib_type14_r11_s::eab_param_r11_c_::types::eab_per_plmn_list_r11);
    TESTASSERT(choice2_cpy.eab_per_plmn_list_r11()[3].eab_cfg_r11_present);
    TESTASSERT(choice2_cpy.eab_per_plmn_list_r11().data() != choice2.eab_per_plmn_list_r11().data());
  }

  asn1::rrc::setup_e e;
  e.value = setup_opts::setup;
  TESTASSERT(e.value == setup_e::setup);

  //
  // srb_to_add_mod_s::rlc_cfg_c_ choice_type2;
  // choice_type2.set(srb_to_add_mod_s::rlc_cfg_c_::types::explicit_value);
  // printf("%s\n", choice_type2.explicit_value().type().to_string().c_str());

  return 0;
}

int test_json_printer()
{
  as_cfg_s cfg;
  //  cfg.source_other_cfg_r9 = asn1::make_copy_ptr(other_cfg_r9_s());
  // To avoid errors with unitialized enums
  cfg.source_mib.phich_cfg.phich_res.value = phich_cfg_s::phich_res_e_::half;
  cfg.source_mib.phich_cfg.phich_dur.value = phich_cfg_s::phich_dur_e_::normal;

  json_writer jw;
  //  cfg.to_json(jw);
  //  printf("%s\n", jw.to_string().c_str());
  return 0;
}

int test_compare_eq()
{
  // TEST1: Compare pdcp configurations
  pdcp_cfg_s pdcp1, pdcp2;
  pdcp1.discard_timer_present     = true;
  pdcp1.discard_timer             = pdcp_cfg_s::discard_timer_opts::ms1500;
  pdcp1.rlc_um_present            = true;
  pdcp1.rlc_um.pdcp_sn_size.value = pdcp_cfg_s::rlc_um_s_::pdcp_sn_size_opts::len7bits;
  TESTASSERT(pdcp1 != pdcp2);
  pdcp2 = pdcp1;
  TESTASSERT(pdcp1 == pdcp2);
  pdcp2.rlc_um.pdcp_sn_size.value = pdcp_cfg_s::rlc_um_s_::pdcp_sn_size_opts::len12bits;
  TESTASSERT(pdcp1 != pdcp2);
  pdcp2                                                = pdcp1;
  pdcp1.hdr_compress.set_rohc().profiles.profile0x0002 = true;
  TESTASSERT(pdcp1 != pdcp2);
  pdcp2.hdr_compress.set_rohc().profiles.profile0x0003 = true;
  TESTASSERT(pdcp1 != pdcp2);
  pdcp2.hdr_compress = pdcp1.hdr_compress;
  TESTASSERT(pdcp1 == pdcp2);

  // TEST1: Compare drb configurations
  drb_to_add_mod_s drb1, drb2;
  drb1.drb_id                = 1;
  drb1.eps_bearer_id_present = true;
  drb1.eps_bearer_id         = 5;
  TESTASSERT(drb1 != drb2);
  drb2 = drb1;
  TESTASSERT(drb1 == drb2);
  drb1.pdcp_cfg = pdcp1;
  TESTASSERT(drb1 == drb2); // pdcp_cfg not yet flagged as present
  drb1.pdcp_cfg_present = true;
  TESTASSERT(drb1 != drb2);
  drb2 = drb1;

  drb1.rlc_cfg_present                               = true;
  drb1.rlc_cfg.set_am().dl_am_rlc.t_reordering.value = t_reordering_opts::ms15;
  drb1.lc_ch_id_present                              = true;
  drb1.lc_ch_id                                      = 3;
  TESTASSERT(drb1 != drb2);
  drb1 = drb2;
  TESTASSERT(drb1 == drb2);

  return SRSRAN_SUCCESS;
}

int test_mib_msg()
{
  uint8_t  rrc_msg[]   = {0x94, 0x64, 0xC0};
  uint32_t rrc_msg_len = sizeof(rrc_msg);
  cbit_ref bref(&rrc_msg[0], sizeof(rrc_msg));
  cbit_ref bref0(&rrc_msg[0], sizeof(rrc_msg));

  bcch_bch_msg_s bcch_bch_msg;
  bcch_bch_msg.unpack(bref);

  bcch_bch_msg_type_s& bcch_msg = bcch_bch_msg.msg;
  TESTASSERT(bcch_msg.dl_bw == mib_s::dl_bw_e_::n75);
  TESTASSERT(bcch_msg.phich_cfg.phich_dur == phich_cfg_s::phich_dur_e_::extended);
  TESTASSERT(bcch_msg.phich_cfg.phich_res == phich_cfg_s::phich_res_e_::half);
  TESTASSERT(bcch_msg.sys_frame_num == "00011001");
  TESTASSERT(bcch_msg.sched_info_sib1_br_r13 == 6);
  TESTASSERT(not bcch_msg.sys_info_unchanged_br_r15);
  TESTASSERT(bcch_msg.spare == "0000");

  TESTASSERT((uint32_t)bref.distance(bref0) / 8 == rrc_msg_len);

  uint8_t rrc_msg2[rrc_msg_len];
  bit_ref bref2(&rrc_msg2[0], sizeof(rrc_msg2));
  bcch_bch_msg.pack(bref2);
  TESTASSERT(bref.distance(bref0) == bref2.distance(bit_ref(&rrc_msg2[0], sizeof(rrc_msg2))));
  TESTASSERT(memcmp(rrc_msg2, rrc_msg, rrc_msg_len) == 0);

  //  // test print
  //  json_writer j;
  //  bcch_bch_msg.to_json(j);
  //  std::cout << j.to_string() << std::endl;

  TESTASSERT(test_pack_unpack_consistency(bcch_bch_msg) == SRSASN_SUCCESS);

  return 0;
}

int test_bcch_dl_sch_msg()
{
  // 000149001250400800094000A03F01000A7FC9800104286C000C
  uint8_t  rrc_msg[]   = {0x00, 0x01, 0x49, 0x00, 0x12, 0x50, 0x40, 0x08, 0x00, 0x09, 0x40, 0x00, 0xA0,
                       0x3F, 0x01, 0x00, 0x0A, 0x7F, 0xC9, 0x80, 0x01, 0x04, 0x28, 0x6C, 0x00, 0x0C};
  uint32_t rrc_msg_len = sizeof(rrc_msg);
  cbit_ref bref(&rrc_msg[0], sizeof(rrc_msg));
  cbit_ref bref0(&rrc_msg[0], sizeof(rrc_msg));

  bcch_dl_sch_msg_s bcch_msg;
  bcch_msg.unpack(bref);

  TESTASSERT(bcch_msg.msg.type() == bcch_dl_sch_msg_type_c::types::c1);
  TESTASSERT(bcch_msg.msg.c1().type() == bcch_dl_sch_msg_type_c::c1_c_::types::sys_info);
  sys_info_s::crit_exts_c_* sinfo = &bcch_msg.msg.c1().sys_info().crit_exts;
  TESTASSERT(sinfo->type() == sys_info_s::crit_exts_c_::types::sys_info_r8);
  sys_info_r8_ies_s& sys_r8 = sinfo->sys_info_r8();
  TESTASSERT(not sys_r8.non_crit_ext_present);
  TESTASSERT(sys_r8.sib_type_and_info.size() == 1);
  TESTASSERT(sys_r8.sib_type_and_info[0].type() == sib_info_item_c::types::sib2);
  sib_type2_s* sib2 = &(sys_r8.sib_type_and_info[0].sib2());
  TESTASSERT(sib2->ac_barr_info_present);
  TESTASSERT(not sib2->mbsfn_sf_cfg_list_present);
  TESTASSERT(sib2->ac_barr_info.ac_barr_for_mo_sig_present);
  TESTASSERT(not sib2->ac_barr_info.ac_barr_for_mo_data_present);
  TESTASSERT(not sib2->ac_barr_info.ac_barr_for_emergency);
  TESTASSERT(sib2->ac_barr_info.ac_barr_for_mo_sig.ac_barr_factor == ac_barr_cfg_s::ac_barr_factor_e_::p60);
  TESTASSERT(sib2->ac_barr_info.ac_barr_for_mo_sig.ac_barr_time == ac_barr_cfg_s::ac_barr_time_e_::s4);
  TESTASSERT(sib2->ac_barr_info.ac_barr_for_mo_sig.ac_barr_for_special_ac == "00000");
  rr_cfg_common_sib_s* res_sib = &sib2->rr_cfg_common;
  TESTASSERT(not res_sib->rach_cfg_common.preamb_info.preambs_group_a_cfg_present);
  TESTASSERT(res_sib->rach_cfg_common.preamb_info.nof_ra_preambs ==
             rach_cfg_common_s::preamb_info_s_::nof_ra_preambs_e_::n40);
  TESTASSERT(res_sib->rach_cfg_common.pwr_ramp_params.pwr_ramp_step == pwr_ramp_params_s::pwr_ramp_step_e_::db0);
  TESTASSERT(res_sib->rach_cfg_common.pwr_ramp_params.preamb_init_rx_target_pwr ==
             pwr_ramp_params_s::preamb_init_rx_target_pwr_e_::dbm_minus100);
  TESTASSERT(res_sib->rach_cfg_common.ra_supervision_info.preamb_trans_max == preamb_trans_max_e::n3);
  TESTASSERT(res_sib->rach_cfg_common.ra_supervision_info.ra_resp_win_size ==
             rach_cfg_common_s::ra_supervision_info_s_::ra_resp_win_size_e_::sf6);
  TESTASSERT(res_sib->rach_cfg_common.ra_supervision_info.mac_contention_resolution_timer ==
             rach_cfg_common_s::ra_supervision_info_s_::mac_contention_resolution_timer_e_::sf8);
  TESTASSERT(res_sib->rach_cfg_common.max_harq_msg3_tx == 1);

  TESTASSERT(res_sib->bcch_cfg.mod_period_coeff == bcch_cfg_s::mod_period_coeff_e_::n2);
  TESTASSERT(res_sib->pcch_cfg.default_paging_cycle == pcch_cfg_s::default_paging_cycle_e_::rf128);
  TESTASSERT(res_sib->pcch_cfg.nb == pcch_cfg_s::nb_e_::four_t);
  TESTASSERT(res_sib->prach_cfg.root_seq_idx == 0);
  TESTASSERT(res_sib->prach_cfg.prach_cfg_info.prach_cfg_idx == 18);
  TESTASSERT(res_sib->prach_cfg.prach_cfg_info.high_speed_flag);
  TESTASSERT(res_sib->prach_cfg.prach_cfg_info.zero_correlation_zone_cfg == 0);
  TESTASSERT(res_sib->prach_cfg.prach_cfg_info.prach_freq_offset == 0);
  TESTASSERT(res_sib->pdsch_cfg_common.ref_sig_pwr == -50);
  TESTASSERT(res_sib->pdsch_cfg_common.p_b == 0);
  //...
  TESTASSERT(res_sib->pucch_cfg_common.n1_pucch_an == 511);
  TESTASSERT(res_sib->srs_ul_cfg_common.type() == srs_ul_cfg_common_c::types::release);
  TESTASSERT(res_sib->ul_pwr_ctrl_common.p0_nominal_pusch == -50);
  //...
  TESTASSERT(sib2->freq_info.add_spec_emission == 1);
  TESTASSERT(sib2->time_align_timer_common == time_align_timer_e::sf1920);

  TESTASSERT(ceil(bref.distance(bref0) / 8.0) == rrc_msg_len);

  uint8_t rrc_msg2[rrc_msg_len];
  bit_ref bref2(&rrc_msg2[0], sizeof(rrc_msg2));
  bcch_msg.pack(bref2);
  TESTASSERT(bref.distance(bref0) == bref2.distance(bit_ref(&rrc_msg2[0], sizeof(rrc_msg2))));
  TESTASSERT(memcmp(rrc_msg2, rrc_msg, rrc_msg_len) == 0);

  //  // test print
  //  json_writer j;
  //  bcch_msg.to_json(j);
  //  std::cout << j.to_string() << std::endl;

  TESTASSERT(test_pack_unpack_consistency(bcch_msg) == SRSASN_SUCCESS);

  return 0;
}

int test_bcch_dl_sch_msg2()
{
  // 406404e100070019b018c06010A940
  uint8_t  rrc_msg[]   = {0x40, 0x64, 0x04, 0xe1, 0x00, 0x07, 0x00, 0x19, 0xb0, 0x18, 0xc0, 0x60, 0x10, 0xA9, 0x40};
  uint32_t rrc_msg_len = sizeof(rrc_msg);
  cbit_ref bref(&rrc_msg[0], sizeof(rrc_msg));
  cbit_ref bref0(&rrc_msg[0], sizeof(rrc_msg));

  bcch_dl_sch_msg_s bcch_msg;
  bcch_msg.unpack(bref);

  TESTASSERT(bcch_msg.msg.type() == bcch_dl_sch_msg_type_c::types::c1);
  TESTASSERT(bcch_msg.msg.c1().type() == bcch_dl_sch_msg_type_c::c1_c_::types::sib_type1);
  sib_type1_s& sib1 = bcch_msg.msg.c1().sib_type1();
  TESTASSERT(not sib1.p_max_present);
  TESTASSERT(sib1.sched_info_list.size() == 1);
  TESTASSERT(sib1.sched_info_list[0].si_periodicity.value == si_periodicity_r12_e::rf16);
  TESTASSERT(sib1.sched_info_list[0].sib_map_info.size() == 1);
  TESTASSERT(sib1.sched_info_list[0].sib_map_info[0] == sib_type_e::sib_type13_v920);

  TESTASSERT(ceil(bref.distance(bref0) / 8.0) == rrc_msg_len);

  uint8_t rrc_msg2[rrc_msg_len];
  bit_ref bref2(&rrc_msg2[0], sizeof(rrc_msg2));
  bcch_msg.pack(bref2);
  TESTASSERT(bref.distance(bref0) == bref2.distance(bit_ref(&rrc_msg2[0], sizeof(rrc_msg2))));
  TESTASSERT(memcmp(rrc_msg2, rrc_msg, rrc_msg_len) == 0);

  TESTASSERT(test_pack_unpack_consistency(bcch_msg) == SRSASN_SUCCESS);

  return 0;
}

int test_bcch_dl_sch_msg3()
{
  // 00830992B7EC9300A3424B000C000500205D6AAAF04200C01DDC801C4880030010A713228500
  uint8_t  rrc_msg[]   = {0x00, 0x83, 0x09, 0x92, 0xB7, 0xEC, 0x93, 0x00, 0xA3, 0x42, 0x4B, 0x00, 0x0C,
                       0x00, 0x05, 0x00, 0x20, 0x5D, 0x6A, 0xAA, 0xF0, 0x42, 0x00, 0xC0, 0x1D, 0xDC,
                       0x80, 0x1C, 0x48, 0x80, 0x03, 0x00, 0x10, 0xA7, 0x13, 0x22, 0x85, 0x00};
  uint32_t rrc_msg_len = sizeof(rrc_msg);
  cbit_ref bref(&rrc_msg[0], sizeof(rrc_msg));

  bcch_dl_sch_msg_s bcch_msg;
  bcch_msg.unpack(bref);

  TESTASSERT(bcch_msg.msg.type() == bcch_dl_sch_msg_type_c::types::c1);

  TESTASSERT(ceil(bref.distance(rrc_msg) / 8.0) == rrc_msg_len);

  uint8_t rrc_msg2[rrc_msg_len];
  bit_ref bref2(&rrc_msg2[0], sizeof(rrc_msg2));
  bcch_msg.pack(bref2);

  bref = cbit_ref(&rrc_msg2[0], sizeof(rrc_msg2));
  bcch_msg.unpack(bref);
  TESTASSERT(bref.distance(rrc_msg2) == bref2.distance(rrc_msg2));

  bit_ref bref3(&rrc_msg[0], sizeof(rrc_msg));
  bcch_msg.pack(bref3);

  TESTASSERT(bref3.distance(rrc_msg) == bref2.distance(rrc_msg2));
  TESTASSERT(memcmp(rrc_msg2, rrc_msg, bref3.distance_bytes(rrc_msg)) == 0);

  TESTASSERT(test_pack_unpack_consistency(bcch_msg) == SRSASN_SUCCESS);

  return 0;
}

int test_dl_dcch_msg()
{
  // 20021008000C406000
  uint8_t  rrc_msg[]   = {0x20, 0x02, 0x10, 0x08, 0x00, 0x0C, 0x40, 0x60, 0x00};
  uint32_t rrc_msg_len = sizeof(rrc_msg);
  cbit_ref bref(&rrc_msg[0], sizeof(rrc_msg));
  cbit_ref bref0(&rrc_msg[0], sizeof(rrc_msg));

  dl_dcch_msg_s dl_dcch_msg;
  dl_dcch_msg.unpack(bref);

  TESTASSERT(ceil(bref.distance(bref0) / 8.0) == rrc_msg_len);

  TESTASSERT(dl_dcch_msg.msg.type() == dl_dcch_msg_type_c::types::c1);
  TESTASSERT(dl_dcch_msg.msg.c1().type() == dl_dcch_msg_type_c::c1_c_::types::rrc_conn_recfg);
  rrc_conn_recfg_s* rrc_recfg = &dl_dcch_msg.msg.c1().rrc_conn_recfg();
  TESTASSERT(rrc_recfg->rrc_transaction_id == 0);
  TESTASSERT(rrc_recfg->crit_exts.type() == rrc_conn_recfg_s::crit_exts_c_::types::c1);
  TESTASSERT(rrc_recfg->crit_exts.c1().type() == rrc_conn_recfg_s::crit_exts_c_::c1_c_::types::rrc_conn_recfg_r8);
  rrc_conn_recfg_r8_ies_s* rrc_recfg_r8 = &rrc_recfg->crit_exts.c1().rrc_conn_recfg_r8();
  TESTASSERT(not rrc_recfg_r8->meas_cfg_present);
  TESTASSERT(not rrc_recfg_r8->mob_ctrl_info_present);
  TESTASSERT(not rrc_recfg_r8->ded_info_nas_list_present);
  TESTASSERT(rrc_recfg_r8->rr_cfg_ded_present);
  TESTASSERT(not rrc_recfg_r8->security_cfg_ho_present);
  TESTASSERT(not rrc_recfg_r8->non_crit_ext_present);

  //...
  TESTASSERT(rrc_recfg_r8->rr_cfg_ded.drb_to_add_mod_list_present);
  TESTASSERT(rrc_recfg_r8->rr_cfg_ded.drb_to_add_mod_list.size() == 1);
  drb_to_add_mod_s* drb = &rrc_recfg_r8->rr_cfg_ded.drb_to_add_mod_list[0];
  TESTASSERT(drb->drb_id == 1);
  //...
  TESTASSERT(drb->rlc_cfg_v1510.is_present());

  //  // test print
  //  json_writer j;
  //  dl_dcch_msg.to_json(j);
  //  std::cout << j.to_string() << std::endl;

  TESTASSERT(test_pack_unpack_consistency(dl_dcch_msg) == SRSASN_SUCCESS);

  return 0;
}

int ue_rrc_conn_recfg_r15_v10_test()
{
  // 201695a8000005143a0002900878b0000046625a03593800000000083a100a48aa1a2780280002a782800002a783000002a78400000001c2900e080848e0434b73a32b93732ba0336b73198181b0336b1b19a1a980233b8393982808c8005332f037f7f7d7d7f7f2f83027a12027a122805fb2a7830400000f38900f78b962ca4f5380dfb9c0327002ea03a03b1793400f40010800d9809016cda8141a0020c8287000b001efb00024a082120205024a04e3f0d00000
  uint8_t rrc_msg[] = {
      0x20, 0x16, 0x95, 0xa8, 0x00, 0x00, 0x05, 0x14, 0x3a, 0x00, 0x02, 0x90, 0x08, 0x78, 0xb0, 0x00, 0x00, 0x46, 0x62,
      0x5a, 0x03, 0x59, 0x38, 0x00, 0x00, 0x00, 0x00, 0x08, 0x3a, 0x10, 0x0a, 0x48, 0xaa, 0x1a, 0x27, 0x80, 0x28, 0x00,
      0x02, 0xa7, 0x82, 0x80, 0x00, 0x02, 0xa7, 0x83, 0x00, 0x00, 0x02, 0xa7, 0x84, 0x00, 0x00, 0x00, 0x01, 0xc2, 0x90,
      0x0e, 0x08, 0x08, 0x48, 0xe0, 0x43, 0x4b, 0x73, 0xa3, 0x2b, 0x93, 0x73, 0x2b, 0xa0, 0x33, 0x6b, 0x73, 0x19, 0x81,
      0x81, 0xb0, 0x33, 0x6b, 0x1b, 0x19, 0xa1, 0xa9, 0x80, 0x23, 0x3b, 0x83, 0x93, 0x98, 0x28, 0x08, 0xc8, 0x00, 0x53,
      0x32, 0xf0, 0x37, 0xf7, 0xf7, 0xd7, 0xd7, 0xf7, 0xf2, 0xf8, 0x30, 0x27, 0xa1, 0x20, 0x27, 0xa1, 0x22, 0x80, 0x5f,
      0xb2, 0xa7, 0x83, 0x04, 0x00, 0x00, 0x0f, 0x38, 0x90, 0x0f, 0x78, 0xb9, 0x62, 0xca, 0x4f, 0x53, 0x80, 0xdf, 0xb9,
      0xc0, 0x32, 0x70, 0x02, 0xea, 0x03, 0xa0, 0x3b, 0x17, 0x93, 0x40, 0x0f, 0x40, 0x01, 0x08, 0x00, 0xd9, 0x80, 0x90,
      0x16, 0xcd, 0xa8, 0x14, 0x1a, 0x00, 0x20, 0xc8, 0x28, 0x70, 0x00, 0xb0, 0x01, 0xef, 0xb0, 0x00, 0x24, 0xa0, 0x82,
      0x12, 0x02, 0x05, 0x02, 0x4a, 0x04, 0xe3, 0xf0, 0xd0, 0x00, 0x00};
  uint32_t rrc_msg_len = sizeof(rrc_msg);
  cbit_ref bref(&rrc_msg[0], sizeof(rrc_msg));
  cbit_ref bref0(&rrc_msg[0], sizeof(rrc_msg));

  dl_dcch_msg_s dl_dcch_msg;
  dl_dcch_msg.unpack(bref);

  TESTASSERT(bref.distance_bytes() == (int)rrc_msg_len);

  TESTASSERT(dl_dcch_msg.msg.type() == dl_dcch_msg_type_c::types::c1);
  TESTASSERT(dl_dcch_msg.msg.c1().type() == dl_dcch_msg_type_c::c1_c_::types::rrc_conn_recfg);
  rrc_conn_recfg_s* rrc_recfg = &dl_dcch_msg.msg.c1().rrc_conn_recfg();
  TESTASSERT(rrc_recfg->rrc_transaction_id == 0);
  TESTASSERT(rrc_recfg->crit_exts.type() == rrc_conn_recfg_s::crit_exts_c_::types::c1);
  TESTASSERT(rrc_recfg->crit_exts.c1().type() == rrc_conn_recfg_s::crit_exts_c_::c1_c_::types::rrc_conn_recfg_r8);
  rrc_conn_recfg_r8_ies_s* rrc_recfg_r8 = &rrc_recfg->crit_exts.c1().rrc_conn_recfg_r8();
  TESTASSERT(rrc_recfg_r8->meas_cfg_present);
  TESTASSERT(not rrc_recfg_r8->mob_ctrl_info_present);
  TESTASSERT(rrc_recfg_r8->ded_info_nas_list_present);
  TESTASSERT(rrc_recfg_r8->rr_cfg_ded_present);
  TESTASSERT(not rrc_recfg_r8->security_cfg_ho_present);
  TESTASSERT(rrc_recfg_r8->non_crit_ext_present);

  // MeasConfig
  TESTASSERT(not rrc_recfg_r8->meas_cfg.meas_obj_to_rem_list_present);
  TESTASSERT(rrc_recfg_r8->meas_cfg.meas_obj_to_add_mod_list_present);
  TESTASSERT(not rrc_recfg_r8->meas_cfg.report_cfg_to_rem_list_present);
  TESTASSERT(rrc_recfg_r8->meas_cfg.report_cfg_to_add_mod_list_present);
  TESTASSERT(not rrc_recfg_r8->meas_cfg.meas_id_to_rem_list_present);
  TESTASSERT(rrc_recfg_r8->meas_cfg.meas_id_to_add_mod_list_present);
  TESTASSERT(rrc_recfg_r8->meas_cfg.quant_cfg_present);
  TESTASSERT(rrc_recfg_r8->meas_cfg.s_measure_present);
  TESTASSERT(rrc_recfg_r8->meas_cfg.speed_state_pars_present);

  TESTASSERT(rrc_recfg_r8->meas_cfg.quant_cfg.quant_cfg_eutra_present);
  TESTASSERT(not rrc_recfg_r8->meas_cfg.quant_cfg.quant_cfg_utra_present);
  TESTASSERT(not rrc_recfg_r8->meas_cfg.quant_cfg.quant_cfg_geran_present);
  TESTASSERT(not rrc_recfg_r8->meas_cfg.quant_cfg.quant_cfg_cdma2000_present);
  TESTASSERT(rrc_recfg_r8->meas_cfg.quant_cfg.quant_cfg_eutra.filt_coef_rsrp_present);
  TESTASSERT(rrc_recfg_r8->meas_cfg.quant_cfg.quant_cfg_eutra.filt_coef_rsrp == filt_coef_e::fc6);
  TESTASSERT(rrc_recfg_r8->meas_cfg.quant_cfg.quant_cfg_eutra.filt_coef_rsrq_present);
  TESTASSERT(rrc_recfg_r8->meas_cfg.quant_cfg.quant_cfg_eutra.filt_coef_rsrq == filt_coef_e::fc4);
  TESTASSERT(rrc_recfg_r8->meas_cfg.s_measure == 90);
  TESTASSERT(rrc_recfg_r8->meas_cfg.speed_state_pars.type() == meas_cfg_s::speed_state_pars_c_::types::release);

  TESTASSERT(rrc_recfg_r8->ded_info_nas_list.size() == 1);
  TESTASSERT(rrc_recfg_r8->ded_info_nas_list[0].size() == 107);

  // RR Config
  TESTASSERT(rrc_recfg_r8->rr_cfg_ded.srb_to_add_mod_list_present);
  TESTASSERT(rrc_recfg_r8->rr_cfg_ded.drb_to_add_mod_list_present);

  TESTASSERT(rrc_recfg_r8->rr_cfg_ded.srb_to_add_mod_list.size() == 1);
  srb_to_add_mod_s* srb = &rrc_recfg_r8->rr_cfg_ded.srb_to_add_mod_list[0];
  TESTASSERT(srb->rlc_cfg_present);
  TESTASSERT(srb->lc_ch_cfg_present);
  TESTASSERT(srb->srb_id == 2);
  TESTASSERT(srb->rlc_cfg.type() == srb_to_add_mod_s::rlc_cfg_c_::types::explicit_value);
  rlc_cfg_c& explicit_value = srb->rlc_cfg.explicit_value();
  TESTASSERT(explicit_value.type() == rlc_cfg_c::types::am);
  rlc_cfg_c::am_s_& am = explicit_value.am();
  TESTASSERT(am.ul_am_rlc.t_poll_retx == t_poll_retx_e::ms35);
  TESTASSERT(am.ul_am_rlc.poll_pdu == poll_pdu_e::pinfinity);
  TESTASSERT(am.ul_am_rlc.poll_byte == poll_byte_e::kbinfinity);
  TESTASSERT(am.ul_am_rlc.max_retx_thres == ul_am_rlc_s::max_retx_thres_e_::t32);
  TESTASSERT(am.dl_am_rlc.t_reordering == t_reordering_e::ms35);
  TESTASSERT(am.dl_am_rlc.t_status_prohibit == t_status_prohibit_e::ms0);
  TESTASSERT(srb->lc_ch_cfg.type() == srb_to_add_mod_s::lc_ch_cfg_c_::types::explicit_value);
  TESTASSERT(srb->lc_ch_cfg.explicit_value().ul_specific_params_present);
  lc_ch_cfg_s::ul_specific_params_s_& ulparams = srb->lc_ch_cfg.explicit_value().ul_specific_params;
  TESTASSERT(ulparams.lc_ch_group_present);
  TESTASSERT(ulparams.prio == 3);
  TESTASSERT(ulparams.prioritised_bit_rate == lc_ch_cfg_s::ul_specific_params_s_::prioritised_bit_rate_e_::infinity);
  TESTASSERT(ulparams.bucket_size_dur == lc_ch_cfg_s::ul_specific_params_s_::bucket_size_dur_e_::ms50);
  TESTASSERT(ulparams.lc_ch_group == 0);

  TESTASSERT(rrc_recfg_r8->rr_cfg_ded.drb_to_add_mod_list.size() == 1);
  drb_to_add_mod_s* drb = &rrc_recfg_r8->rr_cfg_ded.drb_to_add_mod_list[0];
  TESTASSERT(drb->eps_bearer_id_present);
  TESTASSERT(not drb->pdcp_cfg_present);
  TESTASSERT(drb->rlc_cfg_present);
  TESTASSERT(drb->lc_ch_id_present);
  TESTASSERT(drb->lc_ch_cfg_present);
  TESTASSERT(drb->eps_bearer_id == 5);
  TESTASSERT(drb->drb_id == 1);
  TESTASSERT(drb->rlc_cfg.type() == rlc_cfg_c::types::um_bi_dir);
  TESTASSERT(drb->rlc_cfg.um_bi_dir().ul_um_rlc.sn_field_len == sn_field_len_e::size10);
  TESTASSERT(drb->rlc_cfg.um_bi_dir().dl_um_rlc.sn_field_len == sn_field_len_e::size10);
  TESTASSERT(drb->rlc_cfg.um_bi_dir().dl_um_rlc.t_reordering == t_reordering_e::ms40);
  TESTASSERT(drb->lc_ch_id == 3);
  TESTASSERT(drb->lc_ch_cfg.ul_specific_params_present);
  TESTASSERT(drb->lc_ch_cfg.ul_specific_params.lc_ch_group_present);
  TESTASSERT(drb->lc_ch_cfg.ul_specific_params.prio == 12);
  TESTASSERT(drb->lc_ch_cfg.ul_specific_params.prioritised_bit_rate ==
             lc_ch_cfg_s::ul_specific_params_s_::prioritised_bit_rate_e_::kbps8);
  TESTASSERT(drb->lc_ch_cfg.ul_specific_params.bucket_size_dur ==
             lc_ch_cfg_s::ul_specific_params_s_::bucket_size_dur_e_::ms300);
  TESTASSERT(drb->lc_ch_cfg.ul_specific_params.lc_ch_group == 3);
  // ...
  TESTASSERT(not rrc_recfg_r8->rr_cfg_ded.phys_cfg_ded.pdsch_cfg_ded_present);
  TESTASSERT(rrc_recfg_r8->rr_cfg_ded.phys_cfg_ded.pucch_cfg_ded_present);
  TESTASSERT(not rrc_recfg_r8->rr_cfg_ded.phys_cfg_ded.pucch_cfg_ded.tdd_ack_nack_feedback_mode_present);
  TESTASSERT(rrc_recfg_r8->rr_cfg_ded.phys_cfg_ded.pucch_cfg_ded.ack_nack_repeat.type() ==
             pucch_cfg_ded_s::ack_nack_repeat_c_::types::release);
  TESTASSERT(rrc_recfg_r8->rr_cfg_ded.phys_cfg_ded.ant_info_r10.is_present());
  TESTASSERT(not rrc_recfg_r8->rr_cfg_ded.phys_cfg_ded.ant_info_ul_r10.is_present());
  TESTASSERT(not rrc_recfg_r8->rr_cfg_ded.phys_cfg_ded.cif_presence_r10_present);
  TESTASSERT(rrc_recfg_r8->rr_cfg_ded.phys_cfg_ded.cqi_report_cfg_r10.is_present());
  TESTASSERT(rrc_recfg_r8->rr_cfg_ded.phys_cfg_ded.csi_rs_cfg_r10.is_present());
  // ...
  TESTASSERT(rrc_recfg_r8->rr_cfg_ded.phys_cfg_ded.ant_info_r10->type() ==
             phys_cfg_ded_s::ant_info_r10_c_::types::explicit_value_r10);
  ant_info_ded_r10_s& ant_r10 = rrc_recfg_r8->rr_cfg_ded.phys_cfg_ded.ant_info_r10->explicit_value_r10();
  TESTASSERT(ant_r10.codebook_subset_restrict_r10_present);
  TESTASSERT(ant_r10.tx_mode_r10 == ant_info_ded_r10_s::tx_mode_r10_e_::tm3);
  TESTASSERT(ant_r10.codebook_subset_restrict_r10.length() == 2);
  TESTASSERT(ant_r10.codebook_subset_restrict_r10 == "11");
  TESTASSERT(ant_r10.ue_tx_ant_sel.type() == ant_info_ded_r10_s::ue_tx_ant_sel_c_::types::release);
  // ...
  TESTASSERT(not rrc_recfg_r8->non_crit_ext.late_non_crit_ext_present);
  TESTASSERT(rrc_recfg_r8->non_crit_ext.non_crit_ext_present);
  TESTASSERT(not rrc_recfg_r8->non_crit_ext.non_crit_ext.other_cfg_r9_present);
  TESTASSERT(not rrc_recfg_r8->non_crit_ext.non_crit_ext.full_cfg_r9_present);
  TESTASSERT(not rrc_recfg_r8->non_crit_ext.non_crit_ext.non_crit_ext.scell_to_release_list_r10_present);
  TESTASSERT(not rrc_recfg_r8->non_crit_ext.non_crit_ext.non_crit_ext.scell_to_add_mod_list_r10_present);
  TESTASSERT(rrc_recfg_r8->non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext_present);
  TESTASSERT(not rrc_recfg_r8->non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.sib_type1_ded_r11_present);
  TESTASSERT(rrc_recfg_r8->non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext_present);
  TESTASSERT(
      not rrc_recfg_r8->non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.wlan_offload_info_r12_present);
  TESTASSERT(not rrc_recfg_r8->non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.scg_cfg_r12_present);
  TESTASSERT(
      not rrc_recfg_r8->non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.sl_sync_tx_ctrl_r12_present);
  TESTASSERT(
      not rrc_recfg_r8->non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.sl_disc_cfg_r12_present);
  TESTASSERT(
      not rrc_recfg_r8->non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.sl_comm_cfg_r12_present);
  TESTASSERT(rrc_recfg_r8->non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext_present);
  TESTASSERT(not rrc_recfg_r8->non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext
                     .scell_to_release_list_ext_r13_present);
  TESTASSERT(not rrc_recfg_r8->non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext
                     .scell_to_add_mod_list_ext_r13_present);
  TESTASSERT(not rrc_recfg_r8->non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext
                     .lwa_cfg_r13_present);
  TESTASSERT(not rrc_recfg_r8->non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext
                     .lwip_cfg_r13_present);
  TESTASSERT(not rrc_recfg_r8->non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext
                     .rclwi_cfg_r13_present);
  TESTASSERT(
      rrc_recfg_r8->non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext_present);
  TESTASSERT(not rrc_recfg_r8->non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext
                     .non_crit_ext.sl_v2x_cfg_ded_r14_present);
  TESTASSERT(not rrc_recfg_r8->non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext
                     .non_crit_ext.scell_to_add_mod_list_ext_v1430_present);
  TESTASSERT(not rrc_recfg_r8->non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext
                     .non_crit_ext.per_cc_gap_ind_request_r14_present);
  TESTASSERT(not rrc_recfg_r8->non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext
                     .non_crit_ext.sib_type2_ded_r14_present);
  TESTASSERT(rrc_recfg_r8->non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext
                 .non_crit_ext_present);
  rrc_conn_recfg_v1510_ies_s* v1510 = &rrc_recfg_r8->non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext.non_crit_ext
                                           .non_crit_ext.non_crit_ext.non_crit_ext;
  TESTASSERT(not v1510->nr_cfg_r15_present);
  TESTASSERT(not v1510->sk_counter_r15_present);
  TESTASSERT(v1510->nr_radio_bearer_cfg1_r15_present);
  TESTASSERT(not v1510->nr_radio_bearer_cfg2_r15_present);
  TESTASSERT(not v1510->tdm_pattern_cfg_r15_present);
  TESTASSERT(not v1510->non_crit_ext_present);
  TESTASSERT(v1510->nr_radio_bearer_cfg1_r15.size() == 8); // TODO: Get a better way for oct_string comparison
  TESTASSERT(v1510->nr_radio_bearer_cfg1_r15[0] == 20);
  TESTASSERT(v1510->nr_radio_bearer_cfg1_r15[7] == 0);

  return 0;
}

int failed_dl_ccch_unpack()
{
  uint8_t  rrc_msg[] = {0xa5, 0xa8, 0xd8, 0x10, 0x0e, 0xc8, 0x02};
  cbit_ref bref(&rrc_msg[0], sizeof(rrc_msg));

  asn1::rrc::dl_ccch_msg_s msg;

  TESTASSERT(msg.unpack(bref) == SRSASN_SUCCESS);

  TESTASSERT(test_pack_unpack_consistency(msg) == SRSASN_SUCCESS);

  return 0;
}

int unrecognized_ext_group_test()
{
  uint8_t rrc_msg[] = {0x00, 0x81, 0x19, 0x8c, 0x37, 0x91, 0x90, 0x10, 0x22, 0xc1, 0x29, 0x40, 0x48,
                       0x00, 0x82, 0x00, 0x32, 0x67, 0x29, 0x8a, 0x5a, 0xa8, 0x31, 0x00, 0x18, 0x01,
                       0x2e, 0x38, 0x03, 0x84, 0x28, 0xc5, 0xb0, 0x9d, 0x4b, 0x48, 0x00};
  // 0081198c3791901022c12940480082003267298a5aa8310018012e38038428c5b09d4b4800
  uint32_t rrc_msg_len = sizeof(rrc_msg);

  cbit_ref bref(&rrc_msg[0], sizeof(rrc_msg));

  bcch_dl_sch_msg_s dl_sch_msg;
  dl_sch_msg.unpack(bref);

  TESTASSERT(dl_sch_msg.msg.type() == bcch_dl_sch_msg_type_c::types::c1);
  TESTASSERT(dl_sch_msg.msg.c1().type() == bcch_dl_sch_msg_type_c::c1_c_::types::sys_info);

  TESTASSERT(test_pack_unpack_consistency(dl_sch_msg) == SRSASN_SUCCESS);

  return 0;
}

int v2x_test()
{
  // Suspected sl_v2x_preconfig message
  static uint8_t rrc_msg[] = {
      0x20, 0x98, 0x03, 0x5E, 0x5B, 0x5F, 0xB0, 0x00, 0x00, 0x00, 0x40, 0xA0, 0x00, 0x00, 0x00, 0xBF, 0xFF, 0xFE,
      0x54, 0x02, 0x54, 0x06, 0x97, 0xFF, 0xFF, 0xCA, 0x80, 0x4A, 0x92, 0x88, 0x01, 0x00, 0x06, 0x01, 0x30, 0x00,
      0x81, 0x84, 0xE0, 0x8C, 0x00, 0x10, 0xC2, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x82, 0x30, 0x0E, 0x02,
      0x22, 0xAC, 0x04, 0x41, 0x02, 0x16, 0x2C, 0x58, 0xB1, 0x62, 0xC1, 0x02, 0x16, 0x2C, 0x58, 0xB1, 0x62, 0xC1,
      0x02, 0x16, 0x2C, 0x58, 0xB1, 0x62, 0xC1, 0x02, 0x16, 0x2C, 0x58, 0xB1, 0x62, 0xC1, 0x02, 0x16, 0x2C, 0x58,
      0xB1, 0x62, 0xC1, 0x02, 0x16, 0x2C, 0x58, 0xB1, 0x62, 0xC1, 0x02, 0x16, 0x2C, 0x58, 0xB1, 0x62, 0xC1, 0x02,
      0x16, 0x2C, 0x58, 0xB1, 0x62, 0xC0, 0xA2, 0x80, 0x04, 0x30, 0x72, 0x19, 0xE8, 0x34, 0x32, 0x19, 0xE8, 0x34,
      0x32, 0x08, 0x9C, 0x42, 0x0E, 0x00, 0x8E, 0x00, 0x19, 0x10, 0x70, 0x04, 0x70, 0x01, 0x2C, 0x83, 0x80, 0x23,
      0x80, 0x01, 0xE4, 0x1C, 0x01, 0x1C, 0x00, 0x07, 0xA0, 0xE0, 0x08, 0xE0, 0x00, 0x79, 0x31, 0xC0, 0x47, 0x00,
      0x02, 0x88, 0x38, 0x02, 0x38, 0x00, 0x0F, 0x4C, 0x70, 0x11, 0xC0, 0x00, 0x52, 0x0E, 0x00, 0x8E, 0x00};
  // 2098035E5B5FB000000040A0000000BFFFFE5402540697FFFFCA804A92880100060130008184E08C0010C22000000000000282300E0222AC044102162C58B162C102162C58B162C102162C58B162C102162C58B162C102162C58B162C102162C58B162C102162C58B162C102162C58B162C0A28004307219E8343219E83432089C420E008E001910700470012C8380238001E41C011C0007A0E008E0007931C047000288380238000F4C7011C000520E008E00

  cbit_ref            bref(rrc_msg, sizeof(rrc_msg));
  sl_v2x_precfg_r14_s sl_preconf{};
  TESTASSERT(sl_preconf.unpack(bref) == SRSASN_SUCCESS);

  //  asn1::json_writer json_writer;
  //  sl_preconf.to_json(json_writer);
  //  printf("Content: %s\n", json_writer.to_string().c_str());

  TESTASSERT(test_pack_unpack_consistency(sl_preconf) == SRSASN_SUCCESS);

  return SRSASN_SUCCESS;
}

int test_rrc_conn_reconf_r15_2()
{
  uint8_t rrc_msg[] = {0x20, 0x16, 0x15, 0xC8, 0x40, 0x00, 0x03, 0xC2, 0x84, 0x18, 0x10, 0xA8, 0x04, 0xD7, 0x95, 0x14,
                       0xA2, 0x01, 0x02, 0x18, 0x9A, 0x01, 0x80, 0x14, 0x81, 0x0A, 0xCB, 0x84, 0x08, 0x00, 0xAD, 0x6D,
                       0xC4, 0x06, 0x08, 0xAF, 0x6D, 0xC7, 0xA0, 0xC0, 0x82, 0x00, 0x00, 0x0C, 0x38, 0x60, 0x20, 0x30,
                       0xC3, 0x00, 0x00, 0x10, 0x04, 0x40, 0x10, 0xC2, 0x3C, 0x2A, 0x06, 0x20, 0x30, 0x11, 0x10, 0x28,
                       0x13, 0xDA, 0x4E, 0x96, 0xDA, 0x80, 0x83, 0xA1, 0x00, 0xA4, 0x83, 0x00, 0x32, 0x7B, 0x08, 0x95,
                       0xAE, 0x00, 0x16, 0xA9, 0x00, 0xE0, 0x80, 0x84, 0x8C, 0x82, 0xBB, 0xB1, 0xB4, 0xBA, 0x18, 0x83,
                       0x36, 0xB7, 0x31, 0x98, 0x18, 0x98, 0x83, 0x36, 0xB1, 0xB1, 0x9A, 0x1B, 0x1B, 0x02, 0x33, 0xB8,
                       0x39, 0x39, 0x82, 0x80, 0x85, 0x7F, 0x80, 0x80, 0xAF, 0x03, 0x7F, 0x7F, 0x7D, 0x7D, 0x7F, 0x7F,
                       0x28, 0x05, 0xFB, 0x32, 0x7B, 0x08, 0xC0, 0x00, 0x01, 0xF8, 0x3E, 0x3C, 0xB1, 0xB2, 0x00, 0xC0,
                       0x30, 0x38, 0x1F, 0xFA, 0x9C, 0x08, 0x3E, 0xA2, 0x5F, 0x1C, 0xE1, 0xD0, 0x84};
  // 201615C8400003C2841810A804D79514A20102189A018014810ACB840800AD6DC40608AF6DC7A0C08200000C38602030C3000010044010C23C2A06203011102813DA4E96DA8083A100A48300327B0895AE0016A900E080848C82BBB1B4BA188336B7319818988336B1B19A1B1B0233B839398280857F8080AF037F7F7D7D7F7F2805FB327B08C00001F83E3CB1B200C030381FFA9C083EA25F1CE1D084

  cbit_ref      bref(rrc_msg, sizeof(rrc_msg));
  dl_dcch_msg_s recfg_msg;
  TESTASSERT(recfg_msg.unpack(bref) == SRSASN_SUCCESS);

  TESTASSERT(test_pack_unpack_consistency(recfg_msg) == SRSASN_SUCCESS);

  return SRSASN_SUCCESS;
}

int test_rrc_conn_reconf_v2()
{
  uint8_t rrc_msg[] = {0x20, 0x16, 0x00, 0x82, 0x00, 0x4A, 0x27, 0x50, 0x89, 0x30, 0x3C, 0x02, 0x07, 0x42, 0x02, 0x3E,
                       0x06, 0x00, 0x02, 0xF8, 0x39, 0x00, 0x07, 0x00, 0x1D, 0x52, 0x36, 0xC1, 0x01, 0x07, 0x07, 0x06,
                       0x73, 0x72, 0x73, 0x61, 0x70, 0x6E, 0x05, 0x01, 0xAC, 0x10, 0x00, 0x02, 0x27, 0x08, 0x80, 0x00,
                       0x0D, 0x04, 0x08, 0x08, 0x08, 0x08, 0x50, 0x0B, 0xF6, 0x02, 0xF8, 0x39, 0x00, 0x01, 0x1A, 0x26,
                       0xB1, 0x8F, 0x01, 0x13, 0x02, 0xF8, 0x39, 0x00, 0x01, 0x23, 0x05, 0xF4, 0x26, 0xB1, 0x8F, 0x01,
                       0x62, 0x7C, 0x1F, 0x50, 0x29, 0x8E, 0x90, 0xF1, 0xCC, 0x82, 0xA2, 0x60, 0x00, 0x12, 0xA0, 0x00};
  // 20160082004A275089303C020742023E060002F8390007001D5236C10107070673727361706E0501AC100002270880000D0408080808500BF602F83900011A26B18F011302F83900012305F426B18F01627C1F50298E90F1CC82A2600012A000

  cbit_ref      bref(rrc_msg, sizeof(rrc_msg));
  dl_dcch_msg_s recfg_msg;
  TESTASSERT(recfg_msg.unpack(bref) == SRSASN_SUCCESS);

  TESTASSERT(test_pack_unpack_consistency(recfg_msg) == SRSASN_SUCCESS);

  return SRSASN_SUCCESS;
}

int test_rrc_conn_reconf_r15_3()
{
  uint8_t rrc_msg[] = {
      0x20, 0x02, 0x94, 0x08, 0x80, 0x81, 0x88, 0x0c, 0x02, 0x30, 0x31, 0x01, 0x58, 0x49, 0x41, 0x04, 0x3a, 0x74, 0x13,
      0x90, 0x64, 0x12, 0x22, 0xe2, 0x05, 0x82, 0x01, 0x8e, 0x31, 0xbe, 0x82, 0x10, 0x76, 0x2d, 0xc0, 0xfd, 0x3b, 0xf8,
      0xe0, 0xc6, 0x58, 0x06, 0x10, 0x88, 0xc1, 0x04, 0x1a, 0x70, 0x90, 0x83, 0x5b, 0xb0, 0x6e, 0xe3, 0x7a, 0x5a, 0x4e,
      0x53, 0x30, 0x13, 0x49, 0xc6, 0xd6, 0x00, 0x00, 0x2f, 0x46, 0x32, 0x8d, 0x35, 0xfd, 0x23, 0xb8, 0x20, 0x10, 0x00,
      0x01, 0x11, 0x41, 0xf9, 0x01, 0x0a, 0x80, 0x04, 0x00, 0x00, 0x44, 0x50, 0x00, 0x40, 0x20, 0xda, 0x14, 0x0d, 0x88,
      0x85, 0x23, 0x01, 0x8c, 0xaa, 0x47, 0x1c, 0x8a, 0xc3, 0xb8, 0x40, 0x00, 0x05, 0xe9, 0xc3, 0x0c, 0xa3, 0x4c, 0xa9,
      0x94, 0x02, 0xa9, 0x99, 0xab, 0x73, 0x80, 0x80, 0x02, 0x74, 0x83, 0x37, 0x12, 0x6e, 0x34, 0xdc, 0x79, 0xb9, 0x13,
      0x76, 0x03, 0x2f, 0x82, 0x10, 0xa8, 0x0e, 0x80, 0x25, 0x00, 0x24, 0xfa, 0x10, 0x00, 0x09, 0xa1, 0x2e, 0x01, 0x93,
      0x08, 0xcb, 0x11, 0x2f, 0x98, 0x7d, 0xdc, 0x40, 0x08, 0x00, 0x00, 0x88, 0xa0, 0xfc, 0x90, 0x85, 0x40, 0x02, 0x00,
      0x00, 0x22, 0x28, 0x00, 0x24, 0x41, 0x2d, 0x0a, 0x06, 0xc4, 0x42, 0x91, 0x80, 0xc6, 0x55, 0x23, 0x8e, 0x45, 0x61,
      0xd6, 0x54, 0x02, 0x47, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x04, 0x00, 0x00, 0xb2, 0x70, 0xdc, 0x51, 0x08, 0x00, 0x07,
      0x49, 0x59, 0x48, 0x3a, 0x12, 0xc8, 0x0f, 0x48, 0x0f, 0x48, 0x00, 0x01, 0x20, 0x00, 0xc8, 0xa0, 0x6c, 0x44, 0x30,
      0x18, 0xc6, 0xa4, 0x32, 0x89, 0x90, 0xac, 0x11, 0x00, 0x1f, 0xf1, 0x14, 0x00, 0xe0, 0x02, 0x7f, 0xc8, 0x50, 0x03,
      0x80, 0x21, 0x15, 0x8a, 0x00, 0x70, 0x05, 0x22, 0xb5, 0x40, 0x0e, 0x00, 0xc4, 0x96, 0xa8, 0x01, 0xc0, 0x41, 0x10,
      0x04, 0x42, 0x42, 0x8c, 0x88, 0x53, 0x11, 0xc3, 0x2e, 0x22, 0x5f, 0x32, 0xa6, 0x50, 0x1a, 0xa6, 0x66, 0xad, 0xce,
      0x02, 0x00, 0x09, 0xd2, 0x0c, 0xdc, 0x49, 0xb8, 0xd3, 0x71, 0xe6, 0xe4, 0x4d, 0xd8, 0x09, 0x8f, 0x4b, 0x33, 0x55,
      0x54, 0x94, 0x1c, 0x00, 0x10, 0x40, 0xc2, 0x05, 0x0c, 0x1e, 0x9c, 0x40, 0x91, 0x42, 0xc6, 0x0d, 0x1c, 0x3f, 0xf0,
      0x8e, 0x00, 0x20, 0xe8, 0x35, 0x40, 0x30, 0x21, 0x17, 0x39, 0xaa, 0x01, 0x82, 0x73, 0x84, 0x4d, 0x50, 0x0c, 0x1b,
      0xa0, 0x20, 0x6a, 0x80, 0x61, 0x02, 0x0e, 0x83, 0x74, 0x03, 0x0a, 0x11, 0x73, 0x9b, 0xa0, 0x18, 0x67, 0x38, 0x44,
      0xdd, 0x00, 0xc3, 0xba, 0x02, 0x06, 0xe8, 0x06, 0x20, 0x26, 0xe5, 0x61, 0x41, 0x89, 0x0a, 0x39, 0x18, 0x50, 0x62,
      0x82, 0xae, 0x36, 0x14, 0x18, 0xb0, 0xb3, 0x89, 0x85, 0x06, 0x30, 0x2e, 0xe1, 0x61, 0x41, 0x8d, 0x0c, 0x38, 0x18,
      0x50, 0x63, 0x83, 0x2d, 0xf6, 0x14, 0x18, 0xf6, 0xf8, 0x65, 0x85, 0x06, 0x41, 0xd0, 0x10, 0x21, 0x40, 0x35, 0x0e,
      0x60, 0x93, 0x0a, 0x08, 0x12, 0x70, 0xc0, 0xa1, 0x08, 0x38, 0x9b, 0xc1, 0x84, 0x67, 0x3c, 0x8e, 0x92, 0x68, 0x29,
      0x34, 0x10, 0x80, 0x0c, 0x10, 0xac, 0x62, 0x4d, 0xc8, 0x9b, 0xc7, 0xfe, 0xa3, 0x19, 0x4a, 0x52, 0x89, 0x42, 0xe0,
      0x00, 0x10, 0xd8, 0x07, 0x04, 0xc0, 0x04, 0x20, 0xe3, 0xb0, 0x01, 0x80, 0x00, 0x00, 0x00, 0x04, 0xd4, 0x08, 0x90,
      0xde, 0x90, 0x08, 0x02, 0x00, 0x00, 0x9a, 0x81, 0x12, 0x43, 0xd2, 0x02, 0x00, 0x40, 0x00, 0x13, 0x50, 0x22, 0x4d,
      0x7a, 0x40, 0x60, 0x08, 0x00, 0x02, 0x6a, 0x04, 0x4a, 0x4f, 0x49, 0x84, 0x56, 0xaa, 0x2a, 0x02, 0x10, 0x00, 0x40,
      0x42, 0x00, 0x38, 0x10, 0xf4, 0xb8, 0xa4, 0x02, 0x10, 0x20, 0x80, 0x0e, 0x04, 0x3d, 0x2e, 0x29, 0x01, 0x04, 0x04,
      0x20, 0x03, 0x81, 0x0f, 0x4b, 0x8c, 0x40, 0x61, 0x02, 0x08, 0x00, 0xe0, 0x43, 0xd2, 0xe3, 0x10, 0xe1, 0x15, 0xaa,
      0x00, 0x70, 0x21, 0xe9, 0x90, 0x00, 0x88, 0x01, 0x80, 0x00, 0x81, 0x01, 0x80, 0xe0, 0x0e, 0x01, 0xc1, 0x30, 0x00,
      0xe0, 0x90, 0x00, 0x00, 0x00, 0x04, 0x00, 0x80, 0x03, 0x00, 0xa0, 0x1c, 0xc0, 0x50, 0x00, 0xc0, 0x37, 0x80, 0x80,
      0x10, 0x43, 0x93, 0x0a, 0x83, 0xc6, 0xff, 0xff, 0x84, 0x1f, 0xe1, 0xe4, 0xb0, 0x01, 0x54, 0x00, 0x07, 0x94, 0x01,
      0x39, 0x4c, 0xc5, 0x00, 0xc3, 0x23, 0x32, 0x07, 0x80, 0x81, 0x62, 0x68, 0x02, 0x01, 0x62, 0x20, 0x0a, 0x01, 0xf9,
      0xe1, 0xc1, 0x20, 0x22, 0x30, 0xac, 0x23, 0x00, 0x20, 0x00, 0x00, 0x20, 0x02, 0xbc, 0x84, 0x20, 0xe4, 0x21, 0x06,
      0xa0, 0x00, 0x00, 0xe2, 0x80, 0xa0, 0x3a, 0x6e, 0xc3, 0x0a, 0x00};

  // 200294088081880c02303101584941043a741390641222e20582018e31be8210762dc0fd3bf8e0c658061088c1041a7090835bb06ee37a5a4e53301349c6d600002f46328d35fd23b8201000011141f9010a800400004450004020da140d888523018caa471c8ac3b8400005e9c30ca34ca99402a999ab73808002748337126e34dc79b91376032f8210a80e80250024fa100009a12e019308cb112f987ddc4008000088a0fc90854002000022280024412d0a06c4429180c655238e4561d6540247fffffffffc040000b270dc510800074959483a12c80f480f4800012000c8a06c443018c6a4328990ac11001ff11400e0027fc850038021158a00700522b5400e00c496a801c041100442428c885311c32e225f32a6501aa666adce020009d20cdc49b8d371e6e44dd8098f4b335554941c001040c2050c1e9c409142c60d1c3ff08e0020e8354030211739aa018273844d500c1ba0206a8061020e8374030a11739ba018673844dd00c3ba0206e8062026e56141890a3918506282ae361418b0b3898506302ee161418d0c38185063832df61418f6f865850641d0102140350e60930a081270c0a108389bc184673c8e9268293410800c10ac624dc89bc7fea3194a528942e00010d80704c00420e3b0018000000004d40890de90080200009a811243d2020040001350224d7a40600800026a044a4f498456aa2a0210004042003810f4b8a4021020800e043d2e290104042003810f4b8c4061020800e043d2e310e115aa007021e9900088018000810180e00e01c13000e0900000000400800300a01cc05000c03780801043930a83c6ffff841fe1e4b0015400079401394cc500c323320780816268020162200a01f9e1c1202230ac23002000002002bc8420e42106a00000e280a03a6ec30a00

  cbit_ref      bref(rrc_msg, sizeof(rrc_msg));
  dl_dcch_msg_s recfg_msg;
  TESTASSERT(recfg_msg.unpack(bref) == SRSASN_SUCCESS);

  TESTASSERT(test_pack_unpack_consistency(recfg_msg) == SRSASN_SUCCESS);

  return SRSASN_SUCCESS;
}

int main()
{
  auto& asn1_logger = srslog::fetch_basic_logger("ASN1", false);
  asn1_logger.set_level(srslog::basic_levels::debug);
  asn1_logger.set_hex_dump_max_size(-1);

  // Start the log backend.
  srslog::init();

  TESTASSERT(test_generic() == 0);
  TESTASSERT(test_json_printer() == 0);
  TESTASSERT(test_compare_eq() == 0);
  TESTASSERT(test_mib_msg() == 0);
  TESTASSERT(test_bcch_dl_sch_msg() == 0);
  TESTASSERT(test_bcch_dl_sch_msg2() == 0);
  TESTASSERT(test_bcch_dl_sch_msg3() == 0);
  TESTASSERT(test_dl_dcch_msg() == 0);
  TESTASSERT(ue_rrc_conn_recfg_r15_v10_test() == 0);
  TESTASSERT(failed_dl_ccch_unpack() == 0);
  TESTASSERT(unrecognized_ext_group_test() == 0);
  TESTASSERT(v2x_test() == 0);
  TESTASSERT(test_rrc_conn_reconf_r15_2() == 0);
  TESTASSERT(test_rrc_conn_reconf_v2() == 0);
  TESTASSERT(test_rrc_conn_reconf_r15_3() == 0);

  srslog::flush();

  printf("Success\n");
  return 0;
}
