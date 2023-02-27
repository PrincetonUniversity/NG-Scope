#include "srsran/srsran.h"
#include <string.h>

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

void srsran_ngscope_dci_into_array_dl(ngscope_dci_msg_t dci_array[][MAX_CANDIDATES_ALL],
                                        int i, int j,
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

   
    // transport block 1
    dci_array[i][j].tb[0].mcs      = dci_dl_grant->tb[0].mcs_idx;
    dci_array[i][j].tb[0].tbs      = dci_dl_grant->tb[0].tbs;
    dci_array[i][j].tb[0].rv       = dci_dl_grant->tb[0].rv;
    //dci_array[i][j].tb[0].ndi      = dci_dl_grant->tb[0].ndi;

    if(dci_dl_grant->nof_tb > 1){
       // transport block 1
        dci_array[i][j].tb[1].mcs      = dci_dl_grant->tb[1].mcs_idx;
        dci_array[i][j].tb[1].tbs      = dci_dl_grant->tb[1].tbs;
        dci_array[i][j].tb[1].rv       = dci_dl_grant->tb[1].rv;
        //dci_array[i][j].tb[1].ndi      = dci_dl_grant->tb[1].ndi;
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

    dci_array[i][j].phich.n_dmrs   =  dci_ul->n_dmrs;
    dci_array[i][j].phich.n_prb_tilde   = dci_ul_grant->n_prb_tilde[0];

    dci_array[i][j].decode_prob      = decode_prob;
    dci_array[i][j].corr             = corr;

    return;
}
