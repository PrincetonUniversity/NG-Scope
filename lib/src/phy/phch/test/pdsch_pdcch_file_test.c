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
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "srsran/srsran.h"

char* input_file_name = NULL;

srsran_cell_t cell = {
    6,                 // nof_prb
    1,                 // nof_ports
    0,                 // cell_id
    SRSRAN_CP_NORM,    // cyclic prefix
    SRSRAN_PHICH_NORM, // PHICH length
    SRSRAN_PHICH_R_1,  // PHICH resources
    SRSRAN_FDD,

};

int flen;

uint32_t cfi  = 2;
uint16_t rnti = SRSRAN_SIRNTI;

int      max_frames = 10;
uint32_t sf_idx     = 0;

srsran_dci_format_t dci_format = SRSRAN_DCI_FORMAT1A;
srsran_filesource_t fsrc;
srsran_ue_dl_t      ue_dl;
cf_t*               input_buffer[SRSRAN_MAX_PORTS];

void usage(char* prog)
{
  printf("Usage: %s [rovfcenmps] -i input_file\n", prog);
  printf("\t-o DCI format [Default %s]\n", srsran_dci_format_string(dci_format));
  printf("\t-c cell.id [Default %d]\n", cell.id);
  printf("\t-s Start subframe_idx [Default %d]\n", sf_idx);
  printf("\t-f cfi [Default %d]\n", cfi);
  printf("\t-r rnti [Default 0x%x]\n", rnti);
  printf("\t-p cell.nof_ports [Default %d]\n", cell.nof_ports);
  printf("\t-n cell.nof_prb [Default %d]\n", cell.nof_prb);
  printf("\t-m max_frames [Default %d]\n", max_frames);
  printf("\t-e Set extended prefix [Default Normal]\n");
  printf("\t-v [set srsran_verbose to debug, default none]\n");
}

void parse_args(int argc, char** argv)
{
  int opt;
  while ((opt = getopt(argc, argv, "irovfcenmps")) != -1) {
    switch (opt) {
      case 'i':
        input_file_name = argv[optind];
        break;
      case 'c':
        cell.id = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 's':
        sf_idx = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'r':
        rnti = strtoul(argv[optind], NULL, 0);
        break;
      case 'm':
        max_frames = strtoul(argv[optind], NULL, 0);
        break;
      case 'f':
        cfi = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'n':
        cell.nof_prb = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'p':
        cell.nof_ports = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'o':
        dci_format = srsran_dci_format_from_string(argv[optind]);
        if (dci_format == SRSRAN_DCI_NOF_FORMATS) {
          ERROR("Error unsupported format %s", argv[optind]);
          exit(-1);
        }
        break;
      case 'v':
        increase_srsran_verbose_level();
        break;
      case 'e':
        cell.cp = SRSRAN_CP_EXT;
        break;
      default:
        usage(argv[0]);
        exit(-1);
    }
  }
  if (!input_file_name) {
    usage(argv[0]);
    exit(-1);
  }
}

int base_init()
{
  if (srsran_filesource_init(&fsrc, input_file_name, SRSRAN_COMPLEX_FLOAT_BIN)) {
    ERROR("Error opening file %s", input_file_name);
    exit(-1);
  }

  flen = SRSRAN_SF_LEN(srsran_symbol_sz(cell.nof_prb));

  input_buffer[0] = srsran_vec_cf_malloc(flen);
  if (!input_buffer[0]) {
    perror("malloc");
    exit(-1);
  }

  if (srsran_ue_dl_init(&ue_dl, input_buffer, cell.nof_prb, 1)) {
    ERROR("Error initializing UE DL");
    return -1;
  }
  if (srsran_ue_dl_set_cell(&ue_dl, cell)) {
    ERROR("Error initializing UE DL");
    return -1;
  }

  DEBUG("Memory init OK");
  return 0;
}

void base_free()
{
  srsran_filesource_free(&fsrc);
  srsran_ue_dl_free(&ue_dl);
  free(input_buffer[0]);
}

int main(int argc, char** argv)
{
  int  nof_frames;
  int  ret;
  bool acks[SRSRAN_MAX_TB];
  bzero(acks, sizeof(bool) * SRSRAN_MAX_TB);

  if (argc < 3) {
    usage(argv[0]);
    exit(-1);
  }
  parse_args(argc, argv);

  if (base_init()) {
    ERROR("Error initializing memory");
    exit(-1);
  }

  uint8_t* data[SRSRAN_MAX_CODEWORDS] = {malloc(100000)};
  if (!data[0]) {
    perror("malloc");
    exit(-1);
  }

  srsran_ue_dl_cfg_t ue_dl_cfg;
  ZERO_OBJECT(ue_dl_cfg);

  srsran_dl_sf_cfg_t dl_sf;
  ZERO_OBJECT(dl_sf);

  srsran_pdsch_cfg_t pdsch_cfg;
  ZERO_OBJECT(pdsch_cfg);

  srsran_softbuffer_rx_t softbuffer_rx;
  srsran_softbuffer_rx_init(&softbuffer_rx, cell.nof_prb);
  pdsch_cfg.softbuffers.rx[0] = &softbuffer_rx;
  pdsch_cfg.rnti              = rnti;

  ret        = -1;
  nof_frames = 0;
  do {
    srsran_filesource_read(&fsrc, input_buffer[0], flen);
    INFO("Reading %d samples sub-frame %d", flen, sf_idx);

    dl_sf.tti = sf_idx;
    ret       = srsran_ue_dl_find_and_decode(&ue_dl, &dl_sf, &ue_dl_cfg, &pdsch_cfg, data, acks);
    if (ret > 0) {
      printf("PDSCH Decoded OK!\n");
    } else if (ret == 0) {
      printf("No DCI dci found\n");
    } else if (ret < 0) {
      printf("Error decoding PDSCH\n");
    }
    sf_idx = (sf_idx + 1) % 10;
    nof_frames++;
  } while (nof_frames <= max_frames && ret == 0);

  base_free();
  if (data[0])
    free(data[0]);
  if (ret > 0) {
    exit(0);
  } else {
    exit(-1);
  }
}
