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

#include "srsran/srsran.h"
#include <srsran/phy/phch/pusch_cfg.h>
#include <srsran/phy/utils/random.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "srsran/srsran.h"

static srsran_cell_t cell = {
    .nof_prb         = 6,                 // nof_prb
    .nof_ports       = 1,                 // nof_ports
    .id              = 0,                 // cell_id
    .cp              = SRSRAN_CP_NORM,    // cyclic prefix
    .phich_length    = SRSRAN_PHICH_NORM, // PHICH length
    .phich_resources = SRSRAN_PHICH_R_1_6 // PHICH resources
};

static srsran_uci_offset_cfg_t uci_cfg = {
    .I_offset_cqi = 6,
    .I_offset_ri  = 2,
    .I_offset_ack = 9,
};

static srsran_uci_data_t uci_data_tx = {};

uint32_t     L_rb          = 2;
uint32_t     tbs           = 0;
uint32_t     subframe      = 10;
srsran_mod_t modulation    = SRSRAN_MOD_QPSK;
uint32_t     rv_idx        = 0;
int          freq_hop      = -1;
int          riv           = -1;
uint32_t     mcs_idx       = 0;
bool         enable_64_qam = false;

void usage(char* prog)
{
  printf("Usage: %s [csrnfvmtF] \n", prog);
  printf("\n\tCell specific parameters:\n");
  printf("\t\t-n number of PRB [Default %d]\n", cell.nof_prb);
  printf("\t\t-c cell id [Default %d]\n", cell.id);

  printf("\n\tGrant parameters:\n");
  printf("\t\t-m MCS index (0-28) [Default %d]\n", mcs_idx);
  printf("\t\t-F frequency hopping [Default %d]\n", freq_hop);
  printf("\t\t-L L_rb [Default %d]\n", L_rb);
  printf("\t\t-R RIV [Default %d]\n", riv);
  printf("\t\t-r rv_idx (0-3) [Default %d]\n", rv_idx);

  printf("\n\tCQI/RI/ACK Reporting indexes parameters:\n");
  printf("\t\t-p I_offset_cqi (0-15) [Default %d]\n", uci_cfg.I_offset_cqi);
  printf("\t\t-p I_offset_ri (0-15) [Default %d]\n", uci_cfg.I_offset_ri);
  printf("\t\t-p I_offset_ack (0-15) [Default %d]\n", uci_cfg.I_offset_ack);

  printf("\n\tCQI/RI/ACK Reporting contents:\n");
  printf("\t\t-p cqi (none, wideband) [Default none]\n");
  printf("\t\t-p ri (0-1) (zeros, ones, random) [Default none]\n");
  printf("\t\t-p uci_ack [Default none]\n");

  printf("\n\tOther parameters:\n");
  printf("\t\t-p enable_64qam [Default %s]\n", enable_64_qam ? "enabled" : "disabled");
  printf("\t\t-s number of subframes [Default %d]\n", subframe);
  printf("\t-v [set srsran_verbose to debug, default none]\n");
}

void parse_extensive_param(char* param, char* arg)
{
  int ext_code = SRSRAN_SUCCESS;
  if (!strcmp(param, "I_offset_cqi")) {
    uci_cfg.I_offset_cqi = (uint32_t)strtol(arg, NULL, 10);
    if (uci_cfg.I_offset_cqi > 15) {
      ext_code = SRSRAN_ERROR;
    }
  } else if (!strcmp(param, "I_offset_ri")) {
    uci_cfg.I_offset_ri = (uint32_t)strtol(arg, NULL, 10);
    if (uci_cfg.I_offset_ri > 15) {
      ext_code = SRSRAN_ERROR;
    }
  } else if (!strcmp(param, "I_offset_ack")) {
    uci_cfg.I_offset_ack = (uint32_t)strtol(arg, NULL, 10);
    if (uci_cfg.I_offset_ack > 15) {
      ext_code = SRSRAN_ERROR;
    }
  } else if (!strcmp(param, "cqi")) {
    if (!strcmp(arg, "wideband")) {
      uci_data_tx.cfg.cqi.type                    = SRSRAN_CQI_TYPE_WIDEBAND;
      uci_data_tx.value.cqi.wideband.wideband_cqi = (uint8_t)(0x0f);
      uci_data_tx.cfg.cqi.data_enable             = true;
    } else if (!strcmp(arg, "none")) {
      uci_data_tx.cfg.cqi.data_enable = false;
    } else {
      ext_code = SRSRAN_ERROR;
    }
  } else if (!strcmp(param, "ri")) {
    uci_data_tx.value.ri = (uint8_t)strtol(arg, NULL, 10);
    if (uci_data_tx.value.ri > 1) {
      ext_code = SRSRAN_ERROR;
    } else {
      uci_data_tx.cfg.cqi.ri_len = 1;
    }
  } else if (!strcmp(param, "uci_ack")) {
    uci_data_tx.cfg.ack[0].nof_acks = SRSRAN_MIN((uint32_t)strtol(arg, NULL, 10), SRSRAN_UCI_MAX_ACK_BITS);
  } else if (!strcmp(param, "enable_64qam")) {
    enable_64_qam ^= true;
  } else {
    ext_code = SRSRAN_ERROR;
  }

  if (ext_code) {
    ERROR("Error parsing parameter '%s' and argument '%s'", param, arg);
    exit(ext_code);
  }
}

void parse_args(int argc, char** argv)
{
  int opt;
  while ((opt = getopt(argc, argv, "msLFrncpvf")) != -1) {
    switch (opt) {
      case 'm':
        mcs_idx = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 's':
        subframe = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'R':
        riv = (int)strtol(argv[optind], NULL, 10);
        break;
      case 'L':
        L_rb = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'F':
        freq_hop = (int)strtol(argv[optind], NULL, 10);
        break;
      case 'r':
        rv_idx = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'n':
        cell.nof_prb = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'c':
        cell.id = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'p':
        parse_extensive_param(argv[optind], argv[optind + 1]);
        optind++;
        break;
      case 'v':
        increase_srsran_verbose_level();
        break;
      default:
        usage(argv[0]);
        exit(-1);
    }
  }
}

int main(int argc, char** argv)
{
  srsran_random_t        random_h = srsran_random_init(0);
  srsran_chest_ul_res_t  chest_res;
  srsran_pusch_t         pusch_tx;
  srsran_pusch_t         pusch_rx;
  uint8_t*               data       = NULL;
  uint8_t*               data_rx    = NULL;
  cf_t*                  sf_symbols = NULL;
  int                    ret        = -1;
  struct timeval         t[3];
  srsran_pusch_cfg_t     cfg;
  srsran_softbuffer_tx_t softbuffer_tx;
  srsran_softbuffer_rx_t softbuffer_rx;
  srsran_crc_t           crc_tb;

  ZERO_OBJECT(uci_data_tx);
  ZERO_OBJECT(crc_tb);

  bzero(&cfg, sizeof(srsran_pusch_cfg_t));

  srsran_dci_ul_t dci;
  ZERO_OBJECT(dci);

  parse_args(argc, argv);

  dci.freq_hop_fl = freq_hop;
  if (riv >= 0) {
    dci.type2_alloc.riv = (uint32_t)riv;
  } else {
    dci.type2_alloc.riv = srsran_ra_type2_to_riv(L_rb, 0, cell.nof_prb);
  }
  dci.tb.mcs_idx = mcs_idx;

  srsran_ul_sf_cfg_t ul_sf;
  ZERO_OBJECT(ul_sf);
  ul_sf.tti = 0;

  srsran_pusch_hopping_cfg_t ul_hopping = {.n_sb = 1, .hopping_offset = 0, .hop_mode = 1};

  if (srsran_ra_ul_dci_to_grant(&cell, &ul_sf, &ul_hopping, &dci, &cfg.grant)) {
    ERROR("Error computing resource allocation");
    return ret;
  }

  cfg.grant.n_prb_tilde[0] = cfg.grant.n_prb[0];
  cfg.grant.n_prb_tilde[1] = cfg.grant.n_prb[1];

  if (srsran_pusch_init_ue(&pusch_tx, cell.nof_prb)) {
    ERROR("Error creating PUSCH object");
    goto quit;
  }
  if (srsran_pusch_set_cell(&pusch_tx, cell)) {
    ERROR("Error creating PUSCH object");
    goto quit;
  }
  if (srsran_pusch_init_enb(&pusch_rx, cell.nof_prb)) {
    ERROR("Error creating PUSCH object");
    goto quit;
  }
  if (srsran_pusch_set_cell(&pusch_rx, cell)) {
    ERROR("Error creating PUSCH object");
    goto quit;
  }

  uint16_t rnti = 62;
  dci.rnti      = rnti;
  cfg.rnti      = rnti;

  uint32_t nof_re = SRSRAN_NRE * cell.nof_prb * 2 * SRSRAN_CP_NSYMB(cell.cp);
  sf_symbols      = srsran_vec_cf_malloc(nof_re);
  if (!sf_symbols) {
    perror("malloc");
    exit(-1);
  }

  data = srsran_vec_u8_malloc(150000);
  if (!data) {
    perror("malloc");
    exit(-1);
  }

  data_rx = srsran_vec_u8_malloc(150000);
  if (!data_rx) {
    perror("malloc");
    exit(-1);
  }

  if (srsran_softbuffer_tx_init(&softbuffer_tx, 100)) {
    ERROR("Error initiating soft buffer");
    goto quit;
  }

  if (srsran_softbuffer_rx_init(&softbuffer_rx, 100)) {
    ERROR("Error initiating soft buffer");
    goto quit;
  }

  srsran_chest_ul_res_init(&chest_res, cell.nof_prb);
  srsran_chest_ul_res_set_identity(&chest_res);

  cfg.enable_64qam     = enable_64_qam;
  uint64_t decode_us   = 0;
  uint64_t decode_bits = 0;

  for (int n = 0; n < subframe; n++) {
    ret = SRSRAN_SUCCESS;

    /* Configure PUSCH */
    ul_sf.tti      = (uint32_t)n;
    cfg.uci_offset = uci_cfg;

    srsran_softbuffer_tx_reset(&softbuffer_tx);
    srsran_softbuffer_rx_reset(&softbuffer_rx);

    // Generate random data
    for (uint32_t i = 0; i < cfg.grant.tb.tbs / 8; i++) {
      data[i] = (uint8_t)srsran_random_uniform_int_dist(random_h, 0, 255);
    }
    // Attach CRC for making sure TB with 0 CRC are detected
    srsran_crc_attach_byte(&crc_tb, data, cfg.grant.tb.tbs - 24);

    for (uint32_t a = 0; a < uci_data_tx.cfg.ack[0].nof_acks; a++) {
      uci_data_tx.value.ack.ack_value[a] = (uint8_t)srsran_random_uniform_int_dist(random_h, 0, 1);
    }

    srsran_pusch_data_t pdata;
    pdata.ptr          = data;
    pdata.uci          = uci_data_tx.value;
    cfg.uci_cfg        = uci_data_tx.cfg;
    cfg.softbuffers.tx = &softbuffer_tx;

    if (srsran_pusch_encode(&pusch_tx, &ul_sf, &cfg, &pdata, sf_symbols)) {
      ERROR("Error encoding TB");
      exit(-1);
    }
    if (rv_idx > 0) {
      cfg.grant.tb.rv = rv_idx;
      if (srsran_pusch_encode(&pusch_tx, &ul_sf, &cfg, &pdata, sf_symbols)) {
        ERROR("Error encoding TB");
        exit(-1);
      }
    }

    srsran_pusch_res_t pusch_res = {};
    pusch_res.data               = data_rx;
    cfg.softbuffers.rx           = &softbuffer_rx;
    memcpy(&cfg.uci_cfg, &uci_data_tx.cfg, sizeof(srsran_uci_cfg_t));

    gettimeofday(&t[1], NULL);
    int r = srsran_pusch_decode(&pusch_rx, &ul_sf, &cfg, &chest_res, sf_symbols, &pusch_res);
    gettimeofday(&t[2], NULL);
    if (r) {
      printf("Error returned while decoding\n");
      ret = SRSRAN_ERROR;
    }

    if (memcmp(data_rx, data, (size_t)cfg.grant.tb.tbs / 8) != 0) {
      printf("Unmatched data detected\n");
      ret = SRSRAN_ERROR;
    } else {
      INFO("Rx Data is Ok");
    }

    if (uci_data_tx.cfg.ack[0].nof_acks) {
      if (!pusch_res.uci.ack.valid) {
        printf("Invalid UCI ACK bit\n");
        ret = SRSRAN_ERROR;
      } else if (memcmp(uci_data_tx.value.ack.ack_value,
                        pusch_res.uci.ack.ack_value,
                        uci_data_tx.cfg.ack[0].nof_acks) != 0) {
        printf("UCI ACK bit error:\n");
        printf("\tTx: ");
        srsran_vec_fprint_byte(stdout, uci_data_tx.value.ack.ack_value, uci_data_tx.cfg.ack[0].nof_acks);
        printf("\tRx: ");
        srsran_vec_fprint_byte(stdout, pusch_res.uci.ack.ack_value, cfg.uci_cfg.ack[0].nof_acks);
        ret = SRSRAN_ERROR;
      } else {
        INFO("Rx ACK (%d bits) is Ok: ", uci_data_tx.cfg.ack[0].nof_acks);
        if (get_srsran_verbose_level() >= SRSRAN_VERBOSE_INFO) {
          srsran_vec_fprint_byte(stdout, uci_data_tx.value.ack.ack_value, uci_data_tx.cfg.ack[0].nof_acks);
        }
      }
    }

    if (uci_data_tx.cfg.cqi.ri_len) {
      if (uci_data_tx.value.ri != pusch_res.uci.ri) {
        printf("UCI RI bit error: %d != %d\n", uci_data_tx.value.ri, pusch_res.uci.ri);
        ret = SRSRAN_ERROR;
      } else {
        INFO("Rx RI is Ok");
      }
    }

    if (uci_data_tx.cfg.cqi.data_enable) {
      uci_data_tx.value.cqi.data_crc = pusch_res.uci.cqi.data_crc;
      if (memcmp(&uci_data_tx.value.cqi, &pusch_res.uci.cqi, sizeof(pusch_res.uci.cqi)) != 0) {
        printf("CQI Decode failed at subframe %d\n", n);
        ret = SRSRAN_ERROR;
      } else {
        INFO("Rx CQI is Ok (crc=%d, wb_cqi=%d)", pusch_res.uci.cqi.data_crc, pusch_res.uci.cqi.wideband.wideband_cqi);
      }
    }

    if (ret) {
      goto quit;
    }

    get_time_interval(t);
    printf("DECODED OK in %d:%d (TBS: %d bits, TX: %.2f Mbps, Processing: %.2f Mbps)\n",
           (int)t[0].tv_sec,
           (int)t[0].tv_usec,
           cfg.grant.tb.tbs,
           (float)cfg.grant.tb.tbs / 1000,
           (float)cfg.grant.tb.tbs / t[0].tv_usec);
    decode_us += t[0].tv_usec;
    decode_bits += cfg.grant.tb.tbs;
  }

  printf("Decoded Rate: %f Mbps\n", (double)decode_bits / (double)decode_us);
quit:
  srsran_chest_ul_res_free(&chest_res);
  srsran_pusch_free(&pusch_tx);
  srsran_pusch_free(&pusch_rx);
  srsran_softbuffer_tx_free(&softbuffer_tx);
  srsran_softbuffer_rx_free(&softbuffer_rx);
  srsran_random_free(random_h);
  if (sf_symbols) {
    free(sf_symbols);
  }
  if (data) {
    free(data);
  }
  if (data_rx) {
    free(data_rx);
  }
  if (ret) {
    printf("Error\n");
  } else {
    printf("Ok\n");
  }
  exit(ret);
}
