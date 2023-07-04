#include <string.h>
#include "srsran/srsran.h"
#include "srsran/phy/ue/ngscope_dci.h"

/* Combination of unpack and translate to grant */
// Downlink first
int srsran_ngscope_unpack_dl_dci_2grant(srsran_ue_dl_t*     q,
                                        srsran_dl_sf_cfg_t* sf,
                                        srsran_ue_dl_cfg_t* cfg,
                                        srsran_pdsch_cfg_t* pdsch_cfg,
                                        srsran_dci_msg_t* dci_msg,
                                        srsran_dci_dl_t* dci_dl,
                                        srsran_pdsch_grant_t* dci_dl_grant)
{
    if (srsran_dci_msg_unpack_pdsch(&q->cell, sf, &cfg->cfg.dci, dci_msg, dci_dl)) {
        //ERROR("Unpacking DL DCI");
        return SRSRAN_ERROR;
    }
    if (srsran_ue_dl_dci_to_pdsch_grant_wo_mimo_yx(q, sf, cfg, dci_dl, dci_dl_grant)) {
        //ERROR("Translate DL DCI to grant");
        return SRSRAN_ERROR;
    }
    return SRSRAN_SUCCESS;
}
//
//void copy_single_dl_dci(ngscope_dci_msg_t* 		dci_array,
//						srsran_dci_location_t 	loc,
//                        float decode_prob, 
//						float corr,
//                        srsran_dci_dl_t* 		dci_dl,
//                        srsran_pdsch_grant_t* 	dci_dl_grant)
//{
//    dci_array->rnti    = dci_dl->rnti;
//    dci_array->prb     = dci_dl_grant->nof_prb;
//    dci_array->harq    = dci_dl->pid;
//    dci_array->nof_tb  = dci_dl_grant->nof_tb;
//    dci_array->dl      = true;
// 
//    dci_array->decode_prob  = decode_prob;
//    dci_array->corr         = corr;
//
//    dci_array->loc       	= loc;
//   
//    // transport block 1
//    dci_array->tb[0].mcs      = dci_dl_grant->tb[0].mcs_idx;
//    dci_array->tb[0].tbs      = dci_dl_grant->tb[0].tbs;
//    dci_array->tb[0].rv       = dci_dl_grant->tb[0].rv;
//    //dci_array[i][j].tb[0].ndi      = dci_dl_grant->tb[0].ndi;
//    dci_array->tb[0].ndi      = dci_dl->tb[0].ndi;
//
//    if(dci_dl_grant->nof_tb > 1){
//       // transport block 1
//        dci_array->tb[1].mcs      = dci_dl_grant->tb[1].mcs_idx;
//        dci_array->tb[1].tbs      = dci_dl_grant->tb[1].tbs;
//        dci_array->tb[1].rv       = dci_dl_grant->tb[1].rv;
//        //dci_array->tb[1].ndi      = dci_dl_grant->tb[1].ndi;
//    	dci_array->tb[1].ndi      = dci_dl->tb[1].ndi;
//    }
//    return;
//}


void srsran_ngscope_dci_into_array_dl(ngscope_dci_msg_t dci_array[][MAX_CANDIDATES_ALL],
                                        int i, int j,
										srsran_dci_location_t loc,
                                        float decode_prob, float corr,
                                        srsran_dci_dl_t* dci_dl,
                                        srsran_pdsch_grant_t* dci_dl_grant)
{
    dci_array[i][j].rnti    = dci_dl->rnti;
    dci_array[i][j].prb     = dci_dl_grant->nof_prb;
    dci_array[i][j].harq    = dci_dl->pid;
    dci_array[i][j].nof_tb  = dci_dl_grant->nof_tb;
    dci_array[i][j].dl      = true;
 
    dci_array[i][j].decode_prob      = decode_prob;
    dci_array[i][j].corr             = corr;

    dci_array[i][j].loc       	= loc;
   
    // transport block 1
    dci_array[i][j].tb[0].mcs      = dci_dl_grant->tb[0].mcs_idx;
    dci_array[i][j].tb[0].tbs      = dci_dl_grant->tb[0].tbs;
    dci_array[i][j].tb[0].rv       = dci_dl_grant->tb[0].rv;
    //dci_array[i][j].tb[0].ndi      = dci_dl_grant->tb[0].ndi;
    dci_array[i][j].tb[0].ndi      = dci_dl->tb[0].ndi;

    if(dci_dl_grant->nof_tb > 1){
       // transport block 1
        dci_array[i][j].tb[1].mcs      = dci_dl_grant->tb[1].mcs_idx;
        dci_array[i][j].tb[1].tbs      = dci_dl_grant->tb[1].tbs;
        dci_array[i][j].tb[1].rv       = dci_dl_grant->tb[1].rv;
        //dci_array[i][j].tb[1].ndi      = dci_dl_grant->tb[1].ndi;
    	dci_array[i][j].tb[1].ndi      = dci_dl->tb[1].ndi;
    }
    return;
}

//Uplink
int srsran_ngscope_unpack_ul_dci_2grant(srsran_ue_dl_t*     q,
                                        srsran_dl_sf_cfg_t* sf,
                                        srsran_ue_dl_cfg_t* cfg,
                                        srsran_pdsch_cfg_t* pdsch_cfg,
                                        srsran_dci_msg_t* dci_msg,
                                        srsran_dci_ul_t* dci_ul,
                                        srsran_pusch_grant_t* dci_ul_grant)
{

    srsran_ul_sf_cfg_t ul_sf;
    ZERO_OBJECT(ul_sf);
    ul_sf.tdd_config = sf->tdd_config;
    ul_sf.tti        = sf->tti;
    // set the hopping config
    srsran_pusch_hopping_cfg_t ul_hopping = {.n_sb = 1, .hopping_offset = 0, .hop_mode = 1};


    if (srsran_dci_msg_unpack_pusch(&q->cell, sf, &cfg->cfg.dci, dci_msg, dci_ul)) {
        //ERROR("Unpacking UL DCI");
        return SRSRAN_ERROR;
    }

    if (srsran_ra_ul_dci_to_grant(&q->cell, &ul_sf, &ul_hopping, dci_ul, dci_ul_grant)) {
        //ERROR("Translate UL DCI to uplink grant");
        return SRSRAN_ERROR;
    }
    return SRSRAN_SUCCESS;
}


void srsran_ngscope_dci_into_array_ul(ngscope_dci_msg_t dci_array[][MAX_CANDIDATES_ALL],
                                        int i, int j,
										srsran_dci_location_t loc,
                                        float decode_prob, float corr,
                                        srsran_dci_ul_t* dci_ul,
                                        srsran_pusch_grant_t* dci_ul_grant)
{
    dci_array[i][j].rnti    = dci_ul->rnti;
    dci_array[i][j].prb     = dci_ul_grant->L_prb;
    dci_array[i][j].harq    = 0;
    dci_array[i][j].nof_tb  = 1;
    dci_array[i][j].dl      = false;

    // transport block 1
    dci_array[i][j].tb[0].mcs      = dci_ul_grant->tb.mcs_idx;
    dci_array[i][j].tb[0].tbs      = dci_ul_grant->tb.tbs;
    dci_array[i][j].tb[0].rv       = dci_ul_grant->tb.rv;
    //dci_array[i][j].tb[0].ndi      = dci_ul_grant->tb.ndi;

    dci_array[i][j].loc       	= loc;
	
    dci_array[i][j].phich.n_dmrs   =  dci_ul->n_dmrs;
    dci_array[i][j].phich.n_prb_tilde   = dci_ul_grant->n_prb_tilde[0];

    dci_array[i][j].decode_prob      = decode_prob;
    dci_array[i][j].corr             = corr;

    return;
}

   
int srsran_ngscope_dci_prune_ret(ngscope_dci_per_sub_t* q)
{
    int nof_dl_msg  = q->nof_dl_dci;
    ngscope_dci_msg_t  dl_msg[MAX_DCI_PER_SUB];
    if(nof_dl_msg > MAX_DCI_PER_SUB){
        printf("\n\n\n\n nof_dl_msg:%d cannot be larger than MAX DCI_PER SUB \n\n\n\n", nof_dl_msg);
        return 0; 
    }
    int cnt = 0;
    for(int i=0; i<nof_dl_msg; i++){
        switch (q->dl_msg[i].format){
            case SRSRAN_DCI_FORMAT2:
                if(q->dl_msg[i].corr > 0.3){
                    memcpy(&dl_msg[cnt], &q->dl_msg[i], sizeof(ngscope_dci_msg_t));
                    cnt++;
                }
                break;
            case SRSRAN_DCI_FORMAT1C:
                // SI-RNTI Paging-RNTI RA-RNTI
                if( (q->dl_msg[i].rnti >= 0xFFF4) || (q->dl_msg[i].rnti <= 0x0A)){
                    memcpy(&dl_msg[cnt], &q->dl_msg[i], sizeof(ngscope_dci_msg_t));
                    cnt++;
                }
                break;
            case SRSRAN_DCI_FORMAT1:
                if( q->dl_msg[i].decode_prob > 75){
                    memcpy(&dl_msg[cnt], &q->dl_msg[i], sizeof(ngscope_dci_msg_t));
                    cnt++;
                }
                break;
            case SRSRAN_DCI_FORMAT1A:
            case SRSRAN_DCI_FORMAT1B:
                if( (q->dl_msg[i].corr > 0.5) && (q->dl_msg[i].decode_prob > 75)){
                    memcpy(&dl_msg[cnt], &q->dl_msg[i], sizeof(ngscope_dci_msg_t));
                    cnt++;
                }
                break;
            default:
                printf("Non-recongized DCI format:%d! i:%d nof_dl_msg:%d\n", q->dl_msg[i].format, i, nof_dl_msg);
                break;
        }
    } 
    memset(q->dl_msg, 0, nof_dl_msg * sizeof(ngscope_dci_msg_t));
    memcpy(q->dl_msg, dl_msg, cnt * sizeof(ngscope_dci_msg_t));
    
    q->nof_dl_dci = cnt;
    return 0;
}

int srsran_ngscope_dci_prune(ngscope_tree_t* q,
								uint32_t sf_idx)
{
    //printf("nof_location:%d nof_cce:%d sf_idx:%d \n", nof_location, nof_cce, sf_idx);
    uint32_t ncce = 0;
    for(int i=0; i<q->nof_location; i++){
        ncce = q->dci_location[i].ncce;
        //printf("ncce:%d \n", ncce);
        for(int j=0; j<MAX_NOF_FORMAT+1; j++){
            uint16_t rnti = q->dci_array[j][i].rnti;
            if(rnti > 0){// not empty
                // Rule 1: corr based cuting: and decode-prob based pruning
                if (!isnormal(q->dci_array[j][i].corr) || q->dci_array[j][i].corr < 0.5f || q->dci_array[j][i].decode_prob < 75) {
                    ZERO_OBJECT(q->dci_array[j][i]);
					continue;
                }

                // Rule 2: RNTI and its location should match
                bool loc_match = srsran_ngscope_space_match_yx(rnti,
                                    q->nof_cce, sf_idx, ncce, ngscope_index_to_format(j));
                if(loc_match == false){
                    ZERO_OBJECT(q->dci_array[j][i]);
					continue;
                }
            }
        }
    }
    //printf("ncce:%d \n", ncce);
    //int nof_node = srsran_ngscope_tree_non_empty_nodes(dci_array, nof_location);
    //printf("after pruning, there are %d non-empty nodes!\n\n", nof_node);

    return SRSRAN_SUCCESS;
}


