#include <string.h>
#include <unistd.h>

#include "srsran/phy/ngscope/ngscope.h"
#include "srsran/phy/ngscope/ngscope_dci.h"
#include "srsran/phy/ngscope/ngscope_search_space.h"
#include "srsran/phy/ngscope/ngscope_st.h"
#include "srsran/phy/ngscope/ngscope_tree.h"
#include "srsran/phy/ngscope/ue_dl.h"

/* Yaxiong's dci search function */
int srsran_ngscope_search_all_space_array_yx(srsran_pdcch_t*        q,
                                             srsran_cell_t*         cell,
                                             srsran_dl_sf_cfg_t*    sf,
                                             srsran_ue_dl_cfg_t*    cfg,
                                             srsran_pdsch_cfg_t*    pdsch_cfg,
                                             ngscope_dci_per_sub_t* dci_per_sub,
                                             ngscope_tree_t*        tree,
                                             uint16_t               targetRNTI)
{
  int ret = SRSRAN_ERROR;

  dci_per_sub->nof_dl_dci = 0;
  dci_per_sub->nof_ul_dci = 0;

  // dci configuration
  srsran_dci_cfg_t dci_cfg = cfg->cfg.dci;

  // dci_cfg.multiple_csi_request_enabled 	= true;

  //printf("multicsi:%d cif_enable:%d srs:%d not_ue_ss:%d", dci_cfg.multiple_csi_request_enabled,\
  	dci_cfg.cif_enabled, dci_cfg.srs_request_enabled, dci_cfg.is_not_ue_ss);

  srsran_dci_msg_t dci_msg[MAX_NOF_FORMAT];

  dci_blind_search_t search_space;
  ZERO_OBJECT(search_space);

  // search_space.formats[0] = SRSRAN_DCI_FORMAT0;
  search_space.formats[0] = SRSRAN_DCI_FORMAT1;
  search_space.formats[1] = SRSRAN_DCI_FORMAT1A;
  search_space.formats[2] = SRSRAN_DCI_FORMAT1C;
  search_space.formats[3] = SRSRAN_DCI_FORMAT2;

  search_space.nof_locations = 1;

  if (cell.nof_ports == 1) {
    // if the cell has only 1 antenna, it doesn't support MIMO
    search_space.nof_formats = 4;
  } else {
    search_space.nof_formats = MAX_NOF_FORMAT;
  }

  // ngscope_tree_t tree;
  ngscope_tree_init(tree);
  ngscope_tree_set_locations(tree, &q->pdcch, sf->cfi);

  uint32_t nof_cce;
  nof_cce = srsran_pdcch_get_nof_cce_yx(&q->pdcch, sf->cfi);
  ngscope_tree_set_cce(tree, nof_cce);

  // printf("ngscope: TTI:%d NOF CCE:%d nof location:%d\n", sf->tti, nof_cce,
  // tree->nof_location);

  // Test purpose
  // srsran_ngscope_tree_check_nodes(dci_location, 5);

  int loc_idx = 0, blk_idx = 0, cnt = 0;

  int found_dci = 0;
  // printf("enter while!\n");
  while (loc_idx < tree->nof_location) {
    // printf("inside while!\n");
    for (int i = 0; i < 15; i++) {
      if (loc_idx > tree->nof_location)
        break;

      if (tree->dci_location[loc_idx].checked || tree->dci_location[loc_idx].mean_llr < LLR_RATIO) {
        // skip the location, if 1) it has been checked 2) its llr ratio
        // is too small printf("check:%d
        // mean_llr::%f\n",dci_location[loc_idx].checked,
        // dci_location[loc_idx].mean_llr);
        loc_idx++;
        continue;
      }
      cnt++;

      // printf("TTI:%d NOF CCE:%d CFI:%d nof location:%d\n", sf->tti,
      // sf->cfi, nof_cce, tree->nof_location);
      search_space.loc[0] = tree->dci_location[loc_idx];

      // Search for the paging information first, the dci_cfg for the
      // paging messages and normal dci messages are entirely different
      if (search_space.loc[0].ncce == 0) {
        // first search for the paging
        dci_cfg.multiple_csi_request_enabled = false;
        int nof_dci = srsran_ngscope_search_in_space_yx(q, sf, &search_space, &dci_cfg, dci_msg);

        // Unpack the dci messages
        unpack_dci_message_vec(q, sf, cfg, pdsch_cfg, dci_msg, nof_dci, loc_idx, tree);

        int format_idx = srsran_ngscope_tree_find_rnti_range(tree, loc_idx, 0xFFF4, 0xFFFF);
        if (format_idx >= 0) {
          // if we found the corresponding rnti, we will check the
          // nodes, So the following search won't touch those checked
          // nodes
          copy_dci_to_output(tree, dci_per_sub, format_idx, loc_idx);
          found_dci += 1;
        }
      }

      // redo the check in case we found the rnti, if we found the rnti,
      // the corresponding locations should be checked and thus skipped
      if (tree->dci_location[loc_idx].checked) {
        // skip the location, if 1) it has been checked 2) its llr ratio
        // is too small
        loc_idx++;
        continue;
      }

      // printf("we enter normal dci message decoding !\n");
      //  Now we search for the normal dci messages
      dci_cfg.multiple_csi_request_enabled = true;

      // Search all the formats in this location
      int nof_dci = srsran_ngscope_search_in_space_yx(q, sf, &search_space, &dci_cfg, dci_msg);

      // Unpack the dci messages
      unpack_dci_message_vec(q, sf, cfg, pdsch_cfg, dci_msg, nof_dci, loc_idx, tree);

      // child parent matching
      found_dci += child_parent_match(tree, dci_per_sub, loc_idx, blk_idx, targetRNTI);
      // printf("prune done!\n");

      tree->dci_location[loc_idx].checked = true;
      // printf("set dci location done!\n");

      loc_idx++;
      // printf("loc ++ done!\n");
    } // end of for
    blk_idx++;
  } // end of while

  srsran_ngscope_tree_copy_rnti(tree, dci_per_sub, targetRNTI);
  // srsran_ngscope_dci_prune_ret(dci_per_sub);

  // srsran_ngscope_tree_prune_tree(tree.dci_array, nof_location);
  // srsran_ngscope_tree_copy_rnti(&tree, dci_per_sub, targetRNTI);

  // ngscope_dci_msg_t ue_dci = srsran_ngscope_tree_find_rnti(&tree,
  // targetRNTI); if(ue_dci.rnti > 0){
  //   printf("FOUND RNTI: tti:%d L:%d ncce:%d format:%d\n", sf->tti,
  //   ue_dci.loc.L, ue_dci.loc.ncce, ue_dci.format);
  // }

  srsran_ngscope_dci_prune(tree, sf->tti % 10);

  // int nof_node = srsran_ngscope_tree_non_empty_nodes(tree);
  // printf("TTI:%d Searching %d location, found %d dci, left %d non-empty
   // nodes!\n",\
  		sf->tti, cnt, found_dci, nof_node);
  // srsran_ngscope_print_dci_per_sub(dci_per_sub);

  //////srsran_ngscope_tree_solo_nodes(dci_array, dci_location, dci_per_sub,
  /// nof_location);
  // if(nof_node > 0){
  // srsran_ngscope_tree_plot_multi(tree.dci_array, tree.dci_location,
  // nof_location); srsran_ngscope_tree_plot_multi(&tree);
  //}

  // srsran_ngscope_dci_prune(tree, sf->tti % 10);
  // nof_node = srsran_ngscope_tree_non_empty_nodes(dci_array, nof_location);
  // printf("after pruning, there are %d non-empty nodes!\n\n", nof_node);

  // if(nof_node > 0){
  //   srsran_ngscope_tree_plot_multi(dci_array, dci_location, nof_location);
  // }

  //  // Plot the decoded results!
  //  // The plot is block based --> we plot each block in one row
  // srsran_ngscope_tree_plot_multi(dci_array, dci_location, nof_location);
  // usleep(800);
  // printf("\n");

  return found_dci;
}

/* Yaxiong's dci search function */
int srsran_ngscope_search_all_space_array_signleUE_yx(srsran_ue_dl_t*        q,
                                                      srsran_dl_sf_cfg_t*    sf,
                                                      srsran_ue_dl_cfg_t*    cfg,
                                                      srsran_pdsch_cfg_t*    pdsch_cfg,
                                                      ngscope_dci_per_sub_t* dci_per_sub,
                                                      uint16_t               targetRNTI)
{
  int ret = SRSRAN_ERROR;
  // int nof_location = 0;
  int nof_ul_dci = 0;
  int nof_dl_dci = 0;

  dci_per_sub->nof_dl_dci = 0;
  dci_per_sub->nof_ul_dci = 0;
  // Generate the whole search space
  // srsran_dci_location_t dci_location[MAX_CANDIDATES_ALL] = {0};

  // We only search 5 formats
  // int MAX_NOF_FORMAT = 4;

  // dci configuration
  srsran_dci_cfg_t dci_cfg = cfg->cfg.dci;

  // dci_cfg.multiple_csi_request_enabled 	= true;

  //printf("multicsi:%d cif_enable:%d srs:%d not_ue_ss:%d", dci_cfg.multiple_csi_request_enabled,\
  	dci_cfg.cif_enabled, dci_cfg.srs_request_enabled, dci_cfg.is_not_ue_ss);

  srsran_dci_msg_t dci_msg[MAX_NOF_FORMAT];

  srsran_dci_ul_t      dci_ul[MAX_NOF_FORMAT];
  srsran_pusch_grant_t dci_ul_grant[MAX_NOF_FORMAT];

  srsran_dci_dl_t      dci_dl[MAX_NOF_FORMAT];
  srsran_pdsch_grant_t dci_dl_grant[MAX_NOF_FORMAT];

  // ngscope_dci_msg_t     dci_array[MAX_NOF_FORMAT+1][MAX_CANDIDATES_ALL];
  // srsran_dci_location_t dci_location[MAX_CANDIDATES_ALL];
  // for(int i=0; i<MAX_NOF_FORMAT+1; i++){
  //   for(int j=0; j<MAX_CANDIDATES_ALL; j++){
  //   	ZERO_OBJECT(dci_array[i][j]);
  //   }
  // }
  // for(int i=0; i<MAX_CANDIDATES_ALL; i++){
  //   ZERO_OBJECT(dci_location[i]);
  // }

  ngscope_tree_t tree;
  ngscope_tree_init(&tree);

  dci_blind_search_t search_space;
  ZERO_OBJECT(search_space);

  // search_space.formats[0] = SRSRAN_DCI_FORMAT0;
  search_space.formats[0] = SRSRAN_DCI_FORMAT1;
  search_space.formats[1] = SRSRAN_DCI_FORMAT1A;
  search_space.formats[2] = SRSRAN_DCI_FORMAT1C;
  search_space.formats[3] = SRSRAN_DCI_FORMAT2;

  search_space.nof_locations = 1;

  if (q->cell.nof_ports == 1) {
    // if the cell has only 1 antenna, it doesn't support MIMO
    search_space.nof_formats = 4;
  } else {
    search_space.nof_formats = MAX_NOF_FORMAT;
  }
  uint32_t mi_set_len;
  if (q->cell.frame_type == SRSRAN_TDD && !sf->tdd_config.configured) {
    mi_set_len = 3;
  } else {
    mi_set_len = 1;
  }

  // Currently we assume FDD only
  srsran_ue_dl_set_mi_auto(q);
  if ((ret = srsran_ue_dl_decode_fft_estimate(q, sf, cfg)) < 0) {
    ERROR("ERROR decode FFT\n");
    return ret;
  }

  ngscope_tree_set_locations(&tree, &q->pdcch, sf->cfi);
  // printf("NOF_LOC:%d cfi:%d nof_cce: %d %d %d\n", tree.nof_location,
  // sf->cfi, q->pdcch.nof_cce[0], q->pdcch.nof_cce[1], q->pdcch.nof_cce[2]);
  // srsran_ngscope_tree_plot_loc(&tree);

  int loc_idx = 0;
  int blk_idx = 0;
  int cnt     = 0;
  // int found_dci = 0;
  // printf("enter while!\n");
  while (loc_idx < tree.nof_location) {
    // printf("inside while!\n");
    for (int i = 0; i < 15; i++) {
      // printf("%d-th ncce:%d L:%d | ", i, dci_location[i].ncce,
      // dci_location[i].L);
      if (loc_idx > tree.nof_location)
        break;
      // if(dci_location[loc_idx].checked){
      if (tree.dci_location[loc_idx].checked || tree.dci_location[loc_idx].mean_llr < LLR_RATIO) {
        // skip the location, if 1) it has been checked 2) its llr ratio
        // is too small printf("check:%d
        // mean_llr::%f\n",dci_location[loc_idx].checked,
        // dci_location[loc_idx].mean_llr);
        loc_idx++;
        continue;
      }
      cnt++;
      search_space.loc[0] = tree.dci_location[loc_idx];

      // Search all the formats in this location
      int nof_dci = srsran_ngscope_search_in_space_yx(q, sf, &search_space, &dci_cfg, dci_msg);
      /*****************************************************************
       * 	dci_msg -> dci
       *****************************************************************/
      nof_ul_dci = 0;
      nof_dl_dci = 0;

      if (nof_dci > 0) {
        for (int j = 0; j < nof_dci; j++) {
          if (dci_msg[j].format == SRSRAN_DCI_FORMAT0) {
            // Upack the uplink dci to uplink grant
            if (srsran_ngscope_unpack_ul_dci_2grant(
                    q, sf, cfg, pdsch_cfg, &dci_msg[j], &dci_ul[nof_ul_dci], &dci_ul_grant[nof_ul_dci]) ==
                SRSRAN_SUCCESS) {
              srsran_ngscope_dci_into_array_ul(tree.dci_array,
                                               0,
                                               loc_idx,
                                               tree.dci_location[loc_idx],
                                               dci_msg[j].decode_prob,
                                               dci_msg[j].corr,
                                               &dci_ul[nof_ul_dci],
                                               &dci_ul_grant[nof_ul_dci]);
            }
          } else {
            // Upack the downlink dci to downlink grant
            if (srsran_ngscope_unpack_dl_dci_2grant(
                    q, sf, cfg, pdsch_cfg, &dci_msg[j], &dci_dl[nof_dl_dci], &dci_dl_grant[nof_dl_dci]) ==
                SRSRAN_SUCCESS) {
              int format_idx = ngscope_format_to_index(dci_msg[j].format);
              srsran_ngscope_dci_into_array_dl(tree.dci_array,
                                               format_idx,
                                               loc_idx,
                                               tree.dci_location[loc_idx],
                                               dci_msg[j].decode_prob,
                                               dci_msg[j].corr,
                                               &dci_dl[nof_dl_dci],
                                               &dci_dl_grant[nof_dl_dci]);
            }
          }
        }
      }
      // printf("prune done!\n");
      tree.dci_location[loc_idx].checked = true;
      // printf("set dci location done!\n");
      loc_idx++;
      // printf("loc ++ done!\n");
    } // end of for
    blk_idx++;
  } // end of while

  srsran_ngscope_tree_copy_rnti(&tree, dci_per_sub, targetRNTI);
  return ret;
}

/* Yaxiong's dci search function */
int srsran_ngscope_decode_dci_singleUE_yx(srsran_ue_dl_t*        q,
                                          srsran_dl_sf_cfg_t*    sf,
                                          srsran_ue_dl_cfg_t*    cfg,
                                          srsran_pdsch_cfg_t*    pdsch_cfg,
                                          ngscope_dci_per_sub_t* dci_per_sub,
                                          uint16_t               targetRNTI)
{
  srsran_ue_decode_dci_yx(q, sf, cfg, pdsch_cfg, dci_per_sub, targetRNTI);
  return 1;
}

/* Decoding SIB messages */
int srsran_ngscope_decode_SIB_yx(srsran_ue_dl_t*     q,
                                 srsran_dl_sf_cfg_t* dl_sf,
                                 srsran_ue_dl_cfg_t* ue_dl_cfg,
                                 srsran_pdsch_cfg_t* pdsch_cfg,
                                 bool                acks[SRSRAN_MAX_CODEWORDS],
                                 uint8_t*            data[SRSRAN_MAX_CODEWORDS])
{
  int      n                   = 0;
  int      tm_tmp              = ue_dl_cfg->cfg.tm;
  bool     enable_256qam       = ue_dl_cfg->cfg.pdsch.use_tbs_index_alt;
  bool     csi_request_enabled = ue_dl_cfg->cfg.dci.multiple_csi_request_enabled;
  uint16_t rnti                = pdsch_cfg->rnti;

  // Adjust the configurations for decoding SIB messages
  ue_dl_cfg->cfg.tm                               = 1;
  ue_dl_cfg->cfg.pdsch.use_tbs_index_alt          = false;
  ue_dl_cfg->cfg.dci.multiple_csi_request_enabled = false;
  pdsch_cfg->rnti                                 = SRSRAN_SIRNTI;

  n = srsran_ue_dl_find_and_decode(q, dl_sf, ue_dl_cfg, pdsch_cfg, data, acks);

  // Restore the configurations
  ue_dl_cfg->cfg.tm                               = tm_tmp;
  ue_dl_cfg->cfg.pdsch.use_tbs_index_alt          = enable_256qam;
  ue_dl_cfg->cfg.dci.multiple_csi_request_enabled = csi_request_enabled;
  pdsch_cfg->rnti                                 = rnti;

  return n;
}
