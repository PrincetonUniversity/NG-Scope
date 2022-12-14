#include "srsran/srsran.h"
#include <string.h>
#include <unistd.h>

#include "srsran/phy/ue/ngscope_st.h"
#include "srsran/phy/ue/ngscope_tree.h"
#include "srsran/phy/ue/ngscope_dci.h"
#include "srsran/phy/ue/ngscope_search_space.h"

/* Yaxiong's dci search function */
int srsran_ngscope_search_all_space_array_yx(srsran_ue_dl_t*        q,
                                             srsran_dl_sf_cfg_t*    sf,
                                             srsran_ue_dl_cfg_t*    cfg,
                                             srsran_pdsch_cfg_t*    pdsch_cfg,
                                             //ngscope_dci_msg_t      dci_array[][MAX_CANDIDATES_ALL],
                                             //srsran_dci_location_t  dci_location[MAX_CANDIDATES_ALL],
                                             ngscope_dci_per_sub_t* dci_per_sub,
											 uint16_t targetRNTI)
{
  int ret = SRSRAN_ERROR;
  //int nof_location = 0;
  int nof_ul_dci = 0;
  int nof_dl_dci = 0;

  dci_per_sub->nof_dl_dci = 0;
  dci_per_sub->nof_ul_dci = 0;
  // Generate the whole search space
  //srsran_dci_location_t dci_location[MAX_CANDIDATES_ALL] = {0};

  // We only search 5 formats
  //int MAX_NOF_FORMAT = 4;  

  //dci configuration
  srsran_dci_cfg_t dci_cfg = cfg->cfg.dci;

  //dci_cfg.multiple_csi_request_enabled 	= true;

  //printf("multicsi:%d cif_enable:%d srs:%d not_ue_ss:%d", dci_cfg.multiple_csi_request_enabled,\
  	dci_cfg.cif_enabled, dci_cfg.srs_request_enabled, dci_cfg.is_not_ue_ss);
 
  srsran_dci_msg_t      dci_msg[MAX_NOF_FORMAT];

  srsran_dci_ul_t       dci_ul[MAX_NOF_FORMAT]; 
  srsran_pusch_grant_t  dci_ul_grant[MAX_NOF_FORMAT]; 

  srsran_dci_dl_t       dci_dl[MAX_NOF_FORMAT];
  srsran_pdsch_grant_t  dci_dl_grant[MAX_NOF_FORMAT];

  //ngscope_dci_msg_t     dci_array[MAX_NOF_FORMAT+1][MAX_CANDIDATES_ALL];
  //srsran_dci_location_t dci_location[MAX_CANDIDATES_ALL];
  //for(int i=0; i<MAX_NOF_FORMAT+1; i++){
  //  for(int j=0; j<MAX_CANDIDATES_ALL; j++){
  //  	ZERO_OBJECT(dci_array[i][j]);
  //  }
  //}
  //for(int i=0; i<MAX_CANDIDATES_ALL; i++){
  //  ZERO_OBJECT(dci_location[i]);
  //}

  ngscope_tree_t tree;
  ngscope_tree_init(&tree);

  dci_blind_search_t search_space;
  ZERO_OBJECT(search_space);

  //search_space.formats[0] = SRSRAN_DCI_FORMAT0;
  search_space.formats[0] = SRSRAN_DCI_FORMAT1;
  search_space.formats[1] = SRSRAN_DCI_FORMAT1A;
  search_space.formats[2] = SRSRAN_DCI_FORMAT1C;
  search_space.formats[3] = SRSRAN_DCI_FORMAT2;

  search_space.nof_locations = 1;

  if(q->cell.nof_ports == 1){
    // if the cell has only 1 antenna, it doesn't support MIMO
    search_space.nof_formats = 4;
  }else{
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
  //printf("NOF_LOC:%d cfi:%d nof_cce: %d %d %d\n", tree.nof_location, sf->cfi, q->pdcch.nof_cce[0], q->pdcch.nof_cce[1], q->pdcch.nof_cce[2]);
  //srsran_ngscope_tree_plot_loc(&tree);


  //nof_location = srsran_ngscope_search_space_block_yx(&q->pdcch, sf->cfi, dci_location);

  //uint32_t nof_cce = srsran_pdcch_get_nof_cce_yx(&q->pdcch, sf->cfi);
  //printf("TTI:%d NOF CCE:%d nof location:%d\n", sf->tti, nof_cce, tree.nof_location);

  // Test purpose
  //srsran_ngscope_tree_check_nodes(dci_location, 5);

//  int nof_loc = 0;
//  while(nof_loc < tree.nof_location){
//    for(int i=0;i<15;i++){
//        if(nof_loc >= tree.nof_location) break;
//        printf("%d %d %d %f| ",dci_location[nof_loc].L,dci_location[nof_loc].ncce, \
//			dci_location[nof_loc].checked, dci_location[nof_loc].mean_llr);
//        nof_loc++;
//    }
//    printf("\n\n");
//  }


  int loc_idx = 0;
  int blk_idx = 0;
  int cnt = 0;
  //int found_dci = 0;
  //printf("enter while!\n");
  while(loc_idx < tree.nof_location){
  	  //printf("inside while!\n");
      for(int i=0;i<15;i++){
        //printf("%d-th ncce:%d L:%d | ", i, dci_location[i].ncce, dci_location[i].L);
        if(loc_idx > tree.nof_location) break; 
        //if(dci_location[loc_idx].checked){
        if(tree.dci_location[loc_idx].checked || tree.dci_location[loc_idx].mean_llr < LLR_RATIO){
			//skip the location, if 1) it has been checked 2) its llr ratio is too small
			//printf("check:%d mean_llr::%f\n",dci_location[loc_idx].checked, dci_location[loc_idx].mean_llr);
            loc_idx++;
            continue;
        }
		cnt++;
        search_space.loc[0] = tree.dci_location[loc_idx]; 

        // Search all the formats in this location
        int nof_dci = srsran_ngscope_search_in_space_yx(q, sf, &search_space, &dci_cfg, dci_msg);

		//printf("search done! nof_dci:%d\n", nof_dci);

		/*****************************************************************
		* 	dci_msg -> dci
		*****************************************************************/
        nof_ul_dci = 0;
        nof_dl_dci = 0;
		bool found_targetRNTI = false;

        if(nof_dci > 0){
            for(int j=0;j<nof_dci;j++){
                if(dci_msg[j].format == SRSRAN_DCI_FORMAT0){
                    //Upack the uplink dci to uplink grant
                    if(srsran_ngscope_unpack_ul_dci_2grant(q, sf, cfg, pdsch_cfg, &dci_msg[j],  
                                    &dci_ul[nof_ul_dci], &dci_ul_grant[nof_ul_dci]) == SRSRAN_SUCCESS){
                        srsran_ngscope_dci_into_array_ul(tree.dci_array, 0, loc_idx, tree.dci_location[loc_idx],
                                        dci_msg[j].decode_prob, dci_msg[j].corr, &dci_ul[nof_ul_dci], &dci_ul_grant[nof_ul_dci]);
                    }
                }else{
                    // Upack the downlink dci to downlink grant
                    if(srsran_ngscope_unpack_dl_dci_2grant(q, sf, cfg, pdsch_cfg, &dci_msg[j], 
                                    &dci_dl[nof_dl_dci], &dci_dl_grant[nof_dl_dci]) == SRSRAN_SUCCESS){
                        int format_idx = ngscope_format_to_index(dci_msg[j].format);
                        srsran_ngscope_dci_into_array_dl(tree.dci_array, format_idx, loc_idx, tree.dci_location[loc_idx],
                                dci_msg[j].decode_prob, dci_msg[j].corr, &dci_dl[nof_dl_dci], &dci_dl_grant[nof_dl_dci]);
                    }
                } 
            }
            for(int j=0;j<MAX_NOF_FORMAT+1;j++){
				if(tree.dci_array[j][loc_idx].rnti == targetRNTI){
					//found_dci++;
					// copy the matched dci message to the results
					srsran_ngscope_tree_copy_dci_fromArray2PerSub(&tree, dci_per_sub, j, loc_idx);
					// check the locations 
					srsran_ngscope_tree_check_nodes(&tree, loc_idx);
					// delete the messages in the dci array (including matched root and its children)
					srsran_ngscope_tree_clear_dciArray_nodes(&tree, loc_idx);
					found_targetRNTI = true;
					break;
				}
			}
        }

		if(found_targetRNTI){
			tree.dci_location[loc_idx].checked = true;
			loc_idx++;
			continue;
		}

		/*****************************************************************
		* Matching child parent	in the tree
		*****************************************************************/
        int matched_format_vec[MAX_NOF_FORMAT+1] = {0};
        int matched_root    = -1;
        int nof_matched     = 0;
        srsran_ngscope_tree_CP_match(&tree, blk_idx, loc_idx, targetRNTI, &nof_matched, &matched_root, matched_format_vec);
        //srsran_ngscope_tree_CP_match(tree.dci_array, nof_location, blk_idx, loc_idx, &nof_matched, &matched_root, matched_format_vec);
         
        /* Pruning a single node. We can only decode one dci from each location, so we need to figure 
		out which format is correct if there are mutliple dci are decoded from that specific location. */
        if( nof_matched > 0){
            int format_idx      = matched_format_vec[0]; 
            int pruned_nof_dci  = 1;

            // Prune the dci messages, if we have more than 1 matched RNTI found 
            if(nof_matched > 1){
                // Prune the nodes since it is possible that two RNTIs are matched for one node
                pruned_nof_dci = srsran_ngscope_tree_prune_node(&tree, nof_matched, matched_root, targetRNTI, matched_format_vec, &format_idx);
            }
            
            for(int i=0; i< nof_matched; i++){
                //printf("TTI:%d Matched num:%d root_idx:%d format:%d L:%d NCCE:%d rnti:%d PRB:%d decode_prob:%f corr:%f\n", \
                    sf->tti, nof_matched, matched_root, matched_format_vec[i], dci_location[matched_root].L, dci_location[matched_root].ncce, \
                    dci_array[matched_format_vec[i]][matched_root].rnti, dci_array[matched_format_vec[i]][matched_root].prb, \
                       dci_array[matched_format_vec[i]][matched_root].decode_prob, dci_array[matched_format_vec[i]][matched_root].corr); 

                //bool space_match_specific = srsran_ngscope_ue_locations_ncce_check_ue_specific(nof_cce, sf->tti % 10,\
                                             dci_array[matched_format_vec[i]][matched_root].rnti, \
                                             dci_location[matched_root].ncce); 
 
                //bool space_match_common  = srsran_ngscope_ue_locations_ncce_check_common(nof_cce, sf->tti % 10, \
                                             dci_array[matched_format_vec[i]][matched_root].rnti, \
                                             dci_location[matched_root].ncce); 
               
                //printf(" space Matched? ue_specific:%d commom:%d\n", space_match_specific, space_match_common);
            }    

            
//            printf("TTI:%d Matched num:%d root_idx:%d format:%d L:%d NCCE:%d rnti:%d PRB:%d decode_prob:%f corr:%f\n", \
//                    sf->tti, nof_matched, matched_root, format_idx, dci_location[matched_root].L, dci_location[matched_root].ncce, \
//                    dci_array[format_idx][matched_root].rnti, dci_array[format_idx][matched_root].prb, \
//                       dci_array[format_idx][matched_root].decode_prob, dci_array[format_idx][matched_root].corr); 
//
            //bool space_match_specific = srsran_ngscope_ue_locations_ncce_check_ue_specific(nof_cce, sf->tti % 10,\
                                         dci_array[format_idx][matched_root].rnti, \
                                         dci_location[matched_root].ncce); 

            //bool space_match_common  = srsran_ngscope_ue_locations_ncce_check_common(nof_cce, sf->tti % 10, \
                                         dci_array[format_idx][matched_root].rnti, \
                                         dci_location[matched_root].ncce); 
           
            //printf("space Matched? ue_specific:%d commom:%d\n", space_match_specific, space_match_common);

            if(pruned_nof_dci == 1){
                bool space_match = true;
                if( (format_idx != 0) && (format_idx != 4)){
                    //TODO Do we need to use space match for other formats?
                    //space_match = srsran_ngscope_space_match_yx(dci_array[format_idx][matched_root].rnti, \
                    //                nof_cce, sf->tti % 10, dci_location[matched_root].ncce, ngscope_index_to_format(format_idx));
                }
                if(space_match){
					//found_dci++;
					//if(tree.dci_array[format_idx][matched_root].rnti == targetRNTI){
					//	printf("COPY  RNTI: tti:%d L:%d ncce:%d format:%d\n", sf->tti, tree.dci_array[format_idx][matched_root].loc.L, 
					//				tree.dci_array[format_idx][matched_root].loc.ncce, format_idx);
					//}

                    // copy the matched dci message to the results
                    srsran_ngscope_tree_copy_dci_fromArray2PerSub(&tree, dci_per_sub, format_idx, matched_root);
                    //printf("nof_dl_msg:%d nof_ul_msg:%d \n", dci_per_sub->nof_dl_dci, dci_per_sub->nof_dl_dci);
					
                    // check the locations 
                    srsran_ngscope_tree_check_nodes(&tree, matched_root);

                    // delete the messages in the dci array (including matched root and its children)
                    srsran_ngscope_tree_clear_dciArray_nodes(&tree, matched_root);
                }
            } 
        }
		printf("prune done!\n");
        tree.dci_location[loc_idx].checked = true;
		//printf("set dci location done!\n");
        loc_idx++;
		//printf("loc ++ done!\n");
      }//end of for
      blk_idx++;
  }//end of while 
  srsran_ngscope_tree_copy_rnti(&tree, dci_per_sub, targetRNTI);
  //srsran_ngscope_dci_prune_ret(dci_per_sub); 

  //srsran_ngscope_tree_prune_tree(tree.dci_array, nof_location);
  //srsran_ngscope_tree_copy_rnti(&tree, dci_per_sub, targetRNTI);

  //ngscope_dci_msg_t ue_dci = srsran_ngscope_tree_find_rnti(&tree, targetRNTI);
  //if(ue_dci.rnti > 0){
  //  printf("FOUND RNTI: tti:%d L:%d ncce:%d format:%d\n", sf->tti, ue_dci.loc.L, ue_dci.loc.ncce, ue_dci.format);
  //}

  int nof_node = srsran_ngscope_tree_non_empty_nodes(&tree);

  //printf("TTI:%d Searching %d location, found %d dci, left %d non-empty nodes!\n",\
  		sf->tti, cnt, found_dci, nof_node);

  //////srsran_ngscope_tree_solo_nodes(dci_array, dci_location, dci_per_sub, nof_location);
  if(nof_node > 0){
    //srsran_ngscope_tree_plot_multi(dci_array, dci_location, nof_location);
  }
  ////uint32_t nof_cce = srsran_pdcch_get_nof_cce_yx(&q->pdcch, sf->cfi);

  //srsran_ngscope_dci_prune(dci_array, dci_location, nof_location, nof_cce, sf->tti % 10, dci_per_sub);
  //nof_node = srsran_ngscope_tree_non_empty_nodes(dci_array, nof_location);
  //printf("after pruning, there are %d non-empty nodes!\n\n", nof_node);

  //if(nof_node > 0){
  //  srsran_ngscope_tree_plot_multi(dci_array, dci_location, nof_location);
  //}

//  // Plot the decoded results!
//  // The plot is block based --> we plot each block in one row
 //srsran_ngscope_tree_plot_multi(dci_array, dci_location, nof_location);
  //usleep(800);
  //printf("\n");

  return ret;
}


