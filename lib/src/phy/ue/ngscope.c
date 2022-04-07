#include "srsran/srsran.h"
#include <string.h>
#include <unistd.h>

#include "srsran/phy/ue/ngscope_st.h"
#include "srsran/phy/ue/ngscope_tree.h"
#include "srsran/phy/ue/ngscope_dci.h"

int format_to_index(srsran_dci_format_t format){
    switch(format){
        case SRSRAN_DCI_FORMAT0:
            return 0;
            break;
        case SRSRAN_DCI_FORMAT1:
            return 1;
            break;
        case SRSRAN_DCI_FORMAT1A:
            return 2;
            break;
        case SRSRAN_DCI_FORMAT1C:
            return 3;
            break;
        case SRSRAN_DCI_FORMAT2:
            return 4;
            break;
        default:
            printf("Format not recegnized!\n");
            break;
    }
    return -1;
}

///* Copy the DCI message from the dci array to the dci per subframe struct
// * After copying, the DCI message inside the dci array will be deleted!
// */
//static void copy_dci_fromArray2PerSub(ngscope_dci_msg_t dci_array[][MAX_CANDIDATES_ALL],
//                                        ngscope_dci_per_sub_t* dci_per_sub,
//                                        int format, 
//                                        int idx)
//{
//    if( (format  < 0) || (idx<0) || (idx >= MAX_CANDIDATES_ALL)){
//        ERROR("Format or IDX is invalid!\n");
//        return;
//    }
//    if(format == 0){
//        // Format 0 uplink
//        dci_per_sub->ul_msg[dci_per_sub->nof_ul_dci] = dci_array[format][idx];
//        dci_per_sub->nof_ul_dci++;
//    }else{
//        // Downlink dci messages
//        dci_per_sub->dl_msg[dci_per_sub->nof_dl_dci] = dci_array[format][idx];
//        ZERO_OBJECT(dci_array[format][idx]);
//    }
//    return;
//}
                                 
/* Yaxiong's dci search function */
int srsran_ngscope_search_all_space_array_yx(srsran_ue_dl_t*        q,
                                             srsran_dl_sf_cfg_t*    sf,
                                             srsran_ue_dl_cfg_t*    cfg,
                                             srsran_pdsch_cfg_t*    pdsch_cfg,
                                             ngscope_dci_msg_t      dci_array[][MAX_CANDIDATES_ALL],
                                             srsran_dci_location_t  dci_location[MAX_CANDIDATES_ALL],
                                             ngscope_dci_per_sub_t* dci_per_sub)
{
  int ret = SRSRAN_ERROR;
  int nof_location = 0;
  int nof_ul_dci = 0;
  int nof_dl_dci = 0;

  // Generate the whole search space
  //srsran_dci_location_t dci_location[MAX_CANDIDATES_ALL] = {0};

  // We only search 5 formats
  //int MAX_NOF_FORMAT = 4;  

  //dci configuration
  srsran_dci_cfg_t dci_cfg = cfg->cfg.dci;

  srsran_dci_msg_t      dci_msg[MAX_NOF_FORMAT];

  srsran_dci_ul_t       dci_ul[MAX_NOF_FORMAT]; 
  srsran_pusch_grant_t  dci_ul_grant[MAX_NOF_FORMAT]; 

  srsran_dci_dl_t       dci_dl[MAX_NOF_FORMAT];
  srsran_pdsch_grant_t  dci_dl_grant[MAX_NOF_FORMAT];
               
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

  nof_location = srsran_ngscope_search_space_block_yx(&q->pdcch, sf->cfi, dci_location);
  //int nof_cce = srsran_pdcch_get_nof_cce_yx(&q->pdcch, sf->cfi);

  //printf("TTI:%d NOF CCE:%d nof location:%d\n", sf->tti, nof_cce, nof_location);
  // Test purpose
  //srsran_ngscope_tree_check_nodes(dci_location, 5);
  //int nof_loc = 0;
  //while(nof_loc < nof_location){
  //  for(int i=0;i<15;i++){
  //      if(nof_loc >= nof_location) break;
  //      printf("%d %d %d | ",dci_location[nof_loc].L,dci_location[nof_loc].ncce, dci_location[nof_loc].checked);
  //      nof_loc++;
  //  }
  //  printf("\n\n");
  //}


  int loc_idx = 0;
  int blk_idx = 0;
  while(loc_idx < nof_location){
      for(int i=0;i<15;i++){
        //printf("%d-th ncce:%d L:%d | ", i, dci_location[i].ncce, dci_location[i].L);
        if(loc_idx > nof_location) break; 
        if(dci_location[loc_idx].checked){
            loc_idx++;
            continue;
        }

        search_space.loc[0] = dci_location[loc_idx]; 

        // Search all the formats in this location
        int nof_dci = srsran_ngscope_search_in_space_yx(q, sf, &search_space, &dci_cfg, dci_msg);
       
        nof_ul_dci = 0;
        nof_dl_dci = 0;
     
        if(nof_dci > 0){
            for(int j=0;j<nof_dci;j++){
                if(dci_msg[j].format == SRSRAN_DCI_FORMAT0){
                    //Upack the uplink dci to uplink grant
                    if(srsran_ngscope_unpack_ul_dci_2grant(q, sf, cfg, pdsch_cfg, &dci_msg[j],  
                                    &dci_ul[nof_ul_dci], &dci_ul_grant[nof_ul_dci]) == SRSRAN_SUCCESS){
                        srsran_ngscope_dci_into_array_ul(dci_array, 0, loc_idx, 
                                        dci_msg[j].decode_prob, dci_msg[j].corr, &dci_ul[nof_ul_dci], &dci_ul_grant[nof_ul_dci]);
                        //dci_array[0][loc_idx].decode_prob      = dci_msg[j].decode_prob;
                        //dci_array[0][loc_idx].corr             = dci_msg[j].corr;
                    }
                }else{
                    // Upack the downlink dci to downlink grant
                    if(srsran_ngscope_unpack_dl_dci_2grant(q, sf, cfg, pdsch_cfg, &dci_msg[j], 
                                    &dci_dl[nof_dl_dci], &dci_dl_grant[nof_dl_dci]) == SRSRAN_SUCCESS){
                        int format_idx = format_to_index(dci_msg[j].format);
                        srsran_ngscope_dci_into_array_dl(dci_array, format_idx, loc_idx, 
                                dci_msg[j].decode_prob, dci_msg[j].corr, &dci_dl[nof_dl_dci], &dci_dl_grant[nof_dl_dci]);
                        //dci_array[format_idx][loc_idx].decode_prob      = dci_msg[j].decode_prob;
                        //dci_array[format_idx][loc_idx].corr             = dci_msg[j].corr;
                    }
                } 
            }
        }
        int matched_format = -1;
        int matched_root = -1;
        srsran_ngscope_tree_CP_match(dci_array, nof_location, blk_idx, loc_idx, &matched_root, &matched_format);

        if( matched_root >= 0){
            printf("Matched root_idx:%d format:%d L:%d NCCE:%d rnti:%d PRB:%d\n", 
                matched_root, matched_format, dci_location[matched_root].L, dci_location[matched_root].ncce,
                dci_array[matched_format][matched_root].rnti, dci_array[matched_format][matched_root].prb); 

            // copy the matched dci message to the results
            srsran_ngscope_tree_copy_dci_fromArray2PerSub(dci_array, dci_per_sub, matched_format, matched_root);

            // check the locations 
            srsran_ngscope_tree_check_nodes(dci_location, matched_root);

            // delete the messages in the dci array 
            srsran_ngscope_tree_clear_dciArray_nodes(dci_array, matched_root);
        }
        dci_location[loc_idx].checked = true;
        loc_idx++;
      }
      blk_idx++;
  } 

  int nof_node = srsran_ngscope_tree_non_empty_nodes(dci_array, nof_location);
  printf("after matching, there are %d non-empty nodes!\n\n", nof_node);
  srsran_ngscope_tree_solo_nodes(dci_array, dci_location, dci_per_sub, nof_location);

//  // Plot the decoded results!
//  // The plot is block based --> we plot each block in one row

  loc_idx = 0;
  while(loc_idx < nof_location){
    for(int i=0;i<15;i++){
        printf("|%d %d %.2f| ", dci_location[loc_idx].L, dci_location[loc_idx].ncce, dci_location[loc_idx].mean_llr);        
        for(int j=0;j<MAX_NOF_FORMAT+1;j++){
            printf("{%d %d %1.2f %1.2f}-", dci_array[j][loc_idx].rnti, dci_array[j][loc_idx].prb, dci_array[j][loc_idx].decode_prob, dci_array[j][loc_idx].corr);
        }
        //if( (i==0) || (i==2) || (i==6)) printf("\n");
        if( (i%2 == 0) ) printf("\n");

        loc_idx++;
        if(loc_idx>= nof_location){
            break;
        }
    }
    printf("\n\n");
  }
  printf("\n");

  //usleep(800);
  return ret;
}


/* Yaxiong's dci search function */
int srsran_ngscope_search_all_space_yx(srsran_ue_dl_t*     q,
                                 srsran_dl_sf_cfg_t* sf,
                                 srsran_ue_dl_cfg_t* cfg,
                                 srsran_pdsch_cfg_t* pdsch_cfg)
{
  int ret = SRSRAN_ERROR;
  int nof_location = 0;
  int nof_ul_dci = 0;
  int nof_dl_dci = 0;

  // Generate the whole search space
  srsran_dci_location_t dci_location[MAX_CANDIDATES_ALL];

  // We only search 5 formats
  //int MAX_NOF_FORMAT = 4;  

  int rnti_array[MAX_NOF_FORMAT][MAX_CANDIDATES_ALL] = {0};
  int format_array[MAX_NOF_FORMAT][MAX_CANDIDATES_ALL] = {0};
  int prb_array[MAX_NOF_FORMAT][MAX_CANDIDATES_ALL] = {0};

  //dci configuration
  srsran_dci_cfg_t dci_cfg = cfg->cfg.dci;

  srsran_dci_msg_t      dci_msg[MAX_NOF_FORMAT];

  srsran_dci_ul_t       dci_ul[MAX_NOF_FORMAT]; 
  srsran_pusch_grant_t  dci_ul_grant[MAX_NOF_FORMAT]; 

  srsran_dci_dl_t       dci_dl[MAX_NOF_FORMAT];
  srsran_pdsch_grant_t  dci_dl_grant[MAX_NOF_FORMAT];
               
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

  nof_location = srsran_ngscope_search_space_block_yx(&q->pdcch, sf->cfi, dci_location);
  int nof_cce = srsran_pdcch_get_nof_cce_yx(&q->pdcch, sf->cfi);

  printf("TTI:%d NOF CCE:%d nof location:%d\n", sf->tti, nof_cce, nof_location);

  for(int i=0;i<nof_location;i++){
    //printf("%d-th ncce:%d L:%d | ", i, dci_location[i].ncce, dci_location[i].L);
    search_space.loc[0] = dci_location[i]; 

    // Search all the formats in this location
    int nof_dci = srsran_ngscope_search_in_space_yx(q, sf, &search_space, &dci_cfg, dci_msg);
   
    nof_ul_dci = 0;
    nof_dl_dci = 0;
 
    if(nof_dci > 0){
        for(int j=0;j<nof_dci;j++){
            if(dci_msg[j].format == SRSRAN_DCI_FORMAT0){
                //Upack the uplink dci to uplink grant
                if(srsran_ngscope_unpack_ul_dci_2grant(q, sf, cfg, pdsch_cfg, &dci_msg[j], &dci_ul[nof_ul_dci], &dci_ul_grant[nof_ul_dci]) == SRSRAN_SUCCESS){
                    rnti_array[j][i]    = dci_msg[j].rnti;
                    format_array[j][i]  = dci_msg[j].format;
                    prb_array[j][i]     = dci_ul_grant[nof_ul_dci].L_prb;
                    nof_ul_dci += 1;
                }
            }else{
                // Upack the downlink dci to downlink grant
                if(srsran_ngscope_unpack_dl_dci_2grant(q, sf, cfg, pdsch_cfg, &dci_msg[j], &dci_dl[nof_dl_dci], &dci_dl_grant[nof_dl_dci]) == SRSRAN_SUCCESS){
                    rnti_array[j][i]    = dci_msg[j].rnti;
                    format_array[j][i]  = dci_msg[j].format;
                    prb_array[j][i] = dci_dl_grant[nof_dl_dci].nof_prb;
                    nof_dl_dci += 1;
                }
            } 
        }
    }
  }
 
  // Plot the decoded results!
  // The plot is block based --> we plot each block in one row
  int loc_idx = 0;
  while(loc_idx < nof_location){
    for(int i=0;i<15;i++){
        printf("{%d-%d} ", dci_location[loc_idx].L, dci_location[loc_idx].ncce);        
        for(int j=0;j<MAX_NOF_FORMAT;j++){
            printf("{%d %d %d}-", rnti_array[j][loc_idx], format_array[j][loc_idx], prb_array[j][loc_idx]);
        }
        loc_idx++;
        if(loc_idx>= nof_location){
            break;
        }
    }
    printf("\n\n");
  }
  printf("\n");

  return ret;
}

