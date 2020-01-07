#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include "srslte/phy/ue/ue_dl.h"
#include "tbs_256QAM_tables.h"
#include "srslte/phy/ue/rnti_prune.h"
#include "srslte/phy/ue/bin_tree.h"
#include "srslte/phy/ue/lte_scope.h"

#include <string.h>
#include <srslte/srslte.h>

/***********************************************************
* Yaxiong's Modifications --
***********************************************************/
#define PDCCH_FORMAT_NOF_BITS(i)        ((1<<i)*72)
#define NOF_CCE(cfi)  ((cfi>0&&cfi<4)?q->nof_cce[cfi-1]:0)
#define NOF_REGS(cfi) ((cfi>0&&cfi<4)?q->nof_regs[cfi-1]:0)
float mean_llr(srslte_pdcch_t* q, int ncce, int l){
    float mean = 0;
    for (int j=0;j<PDCCH_FORMAT_NOF_BITS(l);j++) {
	    mean += fabsf(q->llr[ncce * 72 + j]);
    }
    return mean;
}
uint32_t srslte_pdcch_location_blocks_yx(srslte_pdcch_t* q, uint32_t cfi, srslte_dci_location_blk_paws_t* blk){
    uint32_t nof_cce = NOF_CCE(cfi);
    int nof_L3 = nof_cce / 8;

    // fill in the blocks with full L3 ncces 
    for(int i=0;i< nof_cce/8;i++){
	memset(&blk[i], 0, sizeof(srslte_dci_location_blk_paws_t));
	// L = 3
	blk[i].loc_L3[0].L	= 3;
	blk[i].loc_L3[0].ncce	= 8 * i;
	blk[i].loc_L3[0].checked	= false;
	blk[i].loc_L3[0].mean_llr	= mean_llr(q, 8*i, 3);
	for(int j=0;j<2;j++){
	    blk[i].loc_L2[j].L    = 2;
	    blk[i].loc_L2[j].ncce = 8 * i + 4 * j;
	    blk[i].loc_L2[j].checked	= false;
	    blk[i].loc_L2[j].mean_llr	= mean_llr(q, 8 * i + 4 * j, 2);
	}
	for(int j=0;j<4;j++){
	    blk[i].loc_L1[j].L    = 1;
	    blk[i].loc_L1[j].ncce = 8 * i + 2 * j;
	    blk[i].loc_L1[j].checked	= false;
	    blk[i].loc_L1[j].mean_llr	= mean_llr(q, 8 * i + 2 * j, 1);
	}
	for(int j=0;j<8;j++){
	    blk[i].loc_L0[j].L    = 0;
	    blk[i].loc_L0[j].ncce = 8 * i + j;
	    blk[i].loc_L0[j].checked	= false;
	    blk[i].loc_L0[j].mean_llr	= mean_llr(q, 8 * i + j, 0);
	}
    }
    // handle the rest cces if it cannot fill in one L3 block
    int rem = nof_cce % 8;
    if( rem != 0){
	memset(&blk[nof_L3], 0, sizeof(srslte_dci_location_blk_paws_t));
	for(int i=0;i<rem;i++){
	    blk[nof_L3].loc_L0[i].L    = 0;
	    blk[nof_L3].loc_L0[i].ncce = 8 * nof_L3 + i;
	    blk[nof_L3].loc_L0[i].checked	= false;
	    blk[nof_L3].loc_L0[i].mean_llr	= mean_llr(q, 8*nof_L3+i, 0);
	}	
	if( rem / 2 > 0){
	    for(int i=0;i< rem/2; i++){
		blk[nof_L3].loc_L1[i].L    = 1;
		blk[nof_L3].loc_L1[i].ncce = 8 * nof_L3 + 2 * i;
		blk[nof_L3].loc_L1[i].checked	= false;
		blk[nof_L3].loc_L1[i].mean_llr	= mean_llr(q, 8*nof_L3+2*i, 1);
	    }
	}
	if( rem / 4 > 0){
	    for(int i=0;i< rem/4; i++){
		blk[nof_L3].loc_L2[i].L    = 2;
		blk[nof_L3].loc_L2[i].ncce = 8 * nof_L3 + 4 * i;
		blk[nof_L3].loc_L2[i].checked	= false;
		blk[nof_L3].loc_L2[i].mean_llr	= mean_llr(q, 8*nof_L3+4*i, 2);
	    }
	}
	return nof_L3+1;
    }else{
	return nof_L3;
    }
}
uint32_t srslte_pdcch_search_space_all_yx(srslte_pdcch_t* q, uint32_t cfi, srslte_dci_location_paws_t* c)
{

    uint32_t nof_cce = NOF_CCE(cfi);
    uint32_t i, j, L, k;
    int l;
    k = 0;
    for (l = 3; l >= 0; l--) {
        L = (1 << l);
        for (i = 0; i < nof_cce / (L); i++) {
            int ncce = (L) * (i % (nof_cce / (L)));
            float mean = 0;
            for (j=0;j<PDCCH_FORMAT_NOF_BITS(l);j++) {
                    mean += fabsf(q->llr[ncce * 72 + j]);
            }
            mean /= PDCCH_FORMAT_NOF_BITS(l);
            if(mean > LLR_THR){
                c[k].checked = 0;
                c[k].L = l;
                c[k].ncce = ncce;
                c[k].mean_llr = mean;
                k++;
            }else{
                continue;
            }
        }
    }
    return k;
}

uint32_t srslte_pdcch_ue_locations_ncce_check(uint32_t nof_cce, uint32_t nsubframe, uint16_t rnti, uint32_t this_ncce) {
    int l; // this must be int because of the for(;;--) loop
    uint32_t i, L, m;
    uint32_t Yk, ncce;
    const int nof_candidates[4] = { 6, 6, 2, 2};

    // Compute Yk for this subframe
    Yk = rnti;
    for (m = 0; m < nsubframe+1; m++) {
        Yk = (39827 * Yk) % 65537;
    }

    // All aggregation levels from 8 to 1
    for (l = 3; l >= 0; l--) {
        L = (1 << l);
        // For all candidates as given in table 9.1.1-1
        for (i = 0; i < nof_candidates[l]; i++) {
            if (nof_cce >= L) {
                ncce = L * ((Yk + i) % (nof_cce / L));
                // Check if candidate fits in c vector and in CCE region
                if (ncce + L <= nof_cce)
                {
                    if (ncce==this_ncce) {
                        return 1; // cce matches
                    }
                }
            }
        }
    }
    // Commom search space
    for (l = 3; l > 1; l--) {
        L = (1 << l);
        for (i = 0; i < SRSLTE_MIN(nof_cce, 16) / (L); i++) {
            ncce = (L) * (i % (nof_cce / (L)));
            if (ncce + L <= nof_cce){
                if (ncce==this_ncce) {
                    return 1; // cce matches
                }
            }
        }
    }

    return 0;
}
static void crc_set_mask_rnti(uint8_t *crc, uint16_t rnti) {
  uint32_t i;
  uint8_t mask[16];
  uint8_t *r = mask;

  DEBUG("Mask CRC with RNTI 0x%x\n", rnti);

  srslte_bit_unpack(rnti, &r, 16);
  for (i = 0; i < 16; i++) {
    crc[i] = (crc[i] + mask[i]) % 2;
  }
}

static float dci_decode_out_RNTI_prob(srslte_pdcch_t *q, float *e, uint8_t *data, uint32_t E, uint32_t nof_bits, uint16_t* RNTI) {
  uint16_t p_bits, crc_res, c_rnti;
  uint8_t *x;
  uint8_t tmp[3 * (SRSLTE_DCI_MAX_BITS + 16)];
  uint8_t tmp2[10 * (SRSLTE_DCI_MAX_BITS + 16)];
  uint8_t check[(SRSLTE_DCI_MAX_BITS + 16)];
  srslte_convcoder_t encoder;
  int poly[3] = { 0x6D, 0x4F, 0x57 };

  if (q         != NULL         &&
      data      != NULL         &&
      E         <= q->max_bits   &&
      nof_bits  <= SRSLTE_DCI_MAX_BITS) // &&
          //(nof_bits +16)*3 <= E) // check if it is possbile to store the dci msg in the available cces
  {
    bzero(q->rm_f, sizeof(float)*3 * (SRSLTE_DCI_MAX_BITS + 16));

    /* unrate matching */
    srslte_rm_conv_rx(e, E, q->rm_f, 3 * (nof_bits + 16));

    /* viterbi decoder */
    srslte_viterbi_decode_f(&q->decoder, q->rm_f, data, nof_bits + 16);

    if (SRSLTE_VERBOSE_ISDEBUG()) {
      srslte_bit_fprint(stdout, data, nof_bits + 16);
    }

    x = &data[nof_bits];
    p_bits = (uint16_t) srslte_bit_pack(&x, 16);
    crc_res = ((uint16_t) srslte_crc_checksum(&q->crc, data, nof_bits) & 0xffff);
    c_rnti = p_bits ^ crc_res;
    DEBUG("p_bits: 0x%x, crc_checksum: 0x%x, crc_rem: 0x%x\n", p_bits, crc_res, c_rnti);

    *RNTI = c_rnti;

    // re-encoding the packet
    encoder.K = 7;
    encoder.R = 3;
    encoder.tail_biting = true;
    memcpy(encoder.poly, poly, 3 * sizeof(int));

    memcpy(check,data,nof_bits);

    srslte_crc_attach(&q->crc, check, nof_bits);
    crc_set_mask_rnti(&check[nof_bits], c_rnti);

    srslte_convcoder_encode(&encoder, check, tmp, nof_bits + 16);
    srslte_rm_conv_tx(tmp, 3 * (nof_bits + 16), tmp2, E);

    float parcheck = 0.0;
    for (int i=0;i<E;i++) {
            //parcheck += ((((e[i]*32+127.5)>127.5)?1:0)==tmp2[i]);
            parcheck += (((e[i]>0)?1:0)==tmp2[i]);
    }
    parcheck = 100*parcheck/E;
   //printf("find a matach for target RNTI! decode_prob:%f\n", parcheck);
    //if (parcheck > 90) printf("nb %d:, p_bits: 0x%x, crc_checksum: 0x%x, crc_rem: 0x%x with prob %.2f\n", nof_bits, p_bits, crc_res, c_rnti, parcheck);
    return parcheck;
  } else {
    fprintf(stderr, "Invalid parameters: E: %d, max_bits: %d, nof_bits: %d\n", E, q->max_bits, nof_bits);
    return -1.0;
  }
}

bool srslte_dci_location_isvalid_yx(srslte_dci_location_paws_t *c) {
  if (c->L <= 3 && c->ncce <= 87) {
    return true;
  } else {
    return false;
  }
}

int srslte_ra_tbs_from_idx_yx(uint32_t tbs_idx, uint32_t n_prb) {
  if (tbs_idx < 34 && n_prb > 0 && n_prb <= SRSLTE_MAX_PRB) {
    return tbs_table_256QAM[tbs_idx][n_prb - 1];
  } else {
    return SRSLTE_ERROR;
  }
}

int get_256QAM_tbs(uint32_t mcs_idx, uint32_t nof_prb)
{
    uint32_t tbs_idx;
    if(mcs_idx < 28){
	tbs_idx = dl_mcs_tbs_idx_table2[mcs_idx];
	return srslte_ra_tbs_from_idx_yx(tbs_idx, nof_prb);
    }else{
	return 0;
    }
}
static int fill_dci_msg_ul_yx(srslte_ra_ul_dci_t* grant, srslte_dci_msg_paws* dci_msg_paws)
{
    dci_msg_paws->harq_pid    = 0;

    dci_msg_paws->mcs_idx_tb1 = grant->mcs_idx;
    dci_msg_paws->rv_tb1      = grant->rv_idx;
    dci_msg_paws->ndi_tb1     = grant->ndi;
    dci_msg_paws->tbs_hm_tb1  = 0;

    dci_msg_paws->mcs_idx_tb2 = 0;
    dci_msg_paws->rv_tb2      = 0;
    dci_msg_paws->ndi_tb2     = 0;
    dci_msg_paws->tbs_hm_tb2  = 0;

    return 0;
}
static int fill_dci_msg_dl_yx(srslte_ra_dl_dci_t* grant, srslte_dci_msg_paws* dci_msg_paws, uint32_t nof_prb)
{
    dci_msg_paws->harq_pid    = grant->harq_process;

    dci_msg_paws->mcs_idx_tb1 = grant->mcs_idx;
    dci_msg_paws->rv_tb1      = grant->rv_idx;
    dci_msg_paws->ndi_tb1     = grant->ndi;
    dci_msg_paws->tbs_hm_tb1  = get_256QAM_tbs(grant->mcs_idx, nof_prb);

    if(grant->tb_en[1]){
        dci_msg_paws->mcs_idx_tb2 = grant->mcs_idx_1;
        dci_msg_paws->rv_tb2      = grant->rv_idx_1;
	dci_msg_paws->ndi_tb2     = grant->ndi_1;
        dci_msg_paws->tbs_hm_tb2  = get_256QAM_tbs(grant->mcs_idx_1, nof_prb);
    }else{
        dci_msg_paws->mcs_idx_tb2 = 0;
        dci_msg_paws->rv_tb2      = 0;
	dci_msg_paws->ndi_tb2     = 0;
        dci_msg_paws->tbs_hm_tb2  = 0;
    }
    return 0;
}
int srslte_pdcch_decode_msg_yx(srslte_pdcch_t* q,  uint32_t cfi, uint32_t tti, bool loc_check,
    srslte_dci_format_t format, srslte_dci_location_paws_t* location, srslte_dci_msg_paws* dci_msg_paws)
{
    srslte_dci_msg_t  *msg = malloc(sizeof(srslte_dci_msg_t));
    srslte_ra_dl_dci_t dci_dl;
    srslte_ra_ul_dci_t dci_ul;
    srslte_ra_dl_grant_t grant;
    srslte_ra_ul_grant_t grant_ul;

    uint32_t nof_prb = q->cell.nof_prb;
    uint32_t nof_ports = q->cell.nof_ports;
    uint16_t decoded_rnti;
    float prob;
    //msg->location = *location;
    msg->format  = format;
    msg->nof_bits = 0;

    if (q != NULL && msg != NULL && srslte_dci_location_isvalid_yx(location)) {
	if (location->ncce * 72 + PDCCH_FORMAT_NOF_BITS(location->L) > NOF_CCE(cfi) * 72) {
	  ERROR("Invalid location: nCCE: %d, L: %d, NofCCE: %d\n", location->ncce, location->L, NOF_CCE(cfi));
	} else {
        //ret = SRSLTE_SUCCESS;

	    uint32_t nof_bits = srslte_dci_format_sizeof(format, nof_prb, nof_ports);
	    uint32_t e_bits   = PDCCH_FORMAT_NOF_BITS(location->L);

	    prob = dci_decode_out_RNTI_prob(q, &q->llr[location->ncce * 72], msg->data, e_bits, nof_bits, &decoded_rnti);

	    if (prob > DECODE_PROB_THR) {
		// decoding is successful, we need to filter out some invalid DCI messages
		msg->nof_bits = nof_bits;
		if(prob > 97 && decoded_rnti != 65524){
		    //printf("%d %d \n",tti, decoded_rnti);
		}
	       // 1-st check: format differentiation
		srslte_dci_format_t decoded_format = (msg->data[0] == 0) ? SRSLTE_DCI_FORMAT0 : SRSLTE_DCI_FORMAT1A;
		if ( msg->format == SRSLTE_DCI_FORMAT0 && decoded_format != SRSLTE_DCI_FORMAT0){
		    //printf("Target format 0, but the decoded format is not 0\n");
		    free(msg);return SRSLTE_ERROR;
		}
		if ( msg->format == SRSLTE_DCI_FORMAT1A && decoded_format != SRSLTE_DCI_FORMAT1A){
		    //printf("Target format 1A, but the decoded format is not 1A\n");
		    free(msg);return SRSLTE_ERROR;
		}
		if ( decoded_rnti < 0x000A || decoded_rnti >0xFFF3){
		    //printf("Msg rnti exceed range!\n");
		    free(msg);return SRSLTE_ERROR;
		}
		if(loc_check){
		    // 2-nd check: msg location (ncce) should match with its UE-specific search space
		    if (srslte_pdcch_ue_locations_ncce_check(NOF_CCE(cfi), tti % 10, decoded_rnti, location->ncce)== 0) {
			//printf("Msg location check fails!\n");
			free(msg);return SRSLTE_ERROR;
		    }
		}
		if( msg->format == SRSLTE_DCI_FORMAT0){
		    if(srslte_dci_msg_unpack_pusch(msg, &dci_ul, nof_prb) == SRSLTE_SUCCESS){
			if(srslte_ra_ul_dci_to_grant(&dci_ul, nof_prb, 0, &grant_ul) == SRSLTE_SUCCESS){
			    dci_msg_paws->downlink      = false;
			    dci_msg_paws->format        = format;
			    dci_msg_paws->cfi           = cfi;
			    dci_msg_paws->tti           = tti;
			    dci_msg_paws->nof_cce       = NOF_CCE(cfi);
			    dci_msg_paws->decode_prob   = prob ;
			    dci_msg_paws->rnti          = decoded_rnti;
			    dci_msg_paws->nof_prb	    = grant_ul.L_prb;
			    dci_msg_paws->L             = location->L;
			    dci_msg_paws->ncce          = location->ncce;
			    dci_msg_paws->tbs_tb1	    = grant_ul.mcs.tbs;
			    dci_msg_paws->tbs_tb2	    = 0;
			    fill_dci_msg_ul_yx(&dci_ul, dci_msg_paws);
			}
		    }
		    free(msg);
		    //return SRSLTE_ERROR;
		    return SRSLTE_SUCCESS;
		}else{
		    bzero(&dci_dl, sizeof(srslte_ra_dl_dci_t));
		    // unpack the msg
		    if (srslte_dci_msg_unpack_pdsch(msg, &dci_dl, nof_prb, nof_ports, true) == SRSLTE_SUCCESS){
			// convert the unpacked msg to dl grant
			if(srslte_ra_dl_dci_to_grant(&dci_dl, nof_prb, decoded_rnti, &grant) == SRSLTE_SUCCESS){
			    dci_msg_paws->downlink      = true;
			    dci_msg_paws->format        = format;
			    dci_msg_paws->cfi           = cfi;
			    dci_msg_paws->tti           = tti;
			    dci_msg_paws->nof_cce       = NOF_CCE(cfi);
			    dci_msg_paws->decode_prob   = prob ;
			    dci_msg_paws->rnti          = decoded_rnti;
			    dci_msg_paws->L             = location->L;
			    dci_msg_paws->ncce          = location->ncce;
			    dci_msg_paws->nof_prb	    = grant.nof_prb;
			    dci_msg_paws->tbs_tb1	    = grant.mcs[0].tbs;

			    if(dci_dl.tb_en[1]){ 
				dci_msg_paws->tbs_tb2   = grant.mcs[1].tbs;
			    }else{
				dci_msg_paws->tbs_tb2   = grant.mcs[1].tbs;
			    }

			    fill_dci_msg_dl_yx(&dci_dl, dci_msg_paws, grant.nof_prb);
			    free(msg);
			    return SRSLTE_SUCCESS;
			}
			free(msg);return SRSLTE_ERROR;
		    }
		    free(msg);return SRSLTE_ERROR;
		}
	    }
	}
  } else {
    ERROR("Invalid parameters, location=%d,%d format=%d\n", location->ncce, location->L, (int)msg->format);
  }
  free(msg);return SRSLTE_ERROR;
}

int srslte_decode_dci_single_location_no_prune_yx(srslte_ue_dl_t*     q,
                                uint32_t cfi, uint32_t tti,
				srslte_tree_ele* tree_ele)
{
    int ret, count=0;
    srslte_dci_format_t format;
    // Loop over all formats
    for(int i=0;i<NOF_UE_ALL_FORMATS;i++){
	//if(i==0){continue;};
        //if (i != 2 && i != 5){continue;}  // We skip the uplink dci and format 1C
        //if (i!=0 && i != 2 && i != 5){continue;}  // We skip the uplink dci and format 1C
        format = ue_all_formats[i];
        ret = srslte_pdcch_decode_msg_yx(&(q->pdcch),  cfi, tti, false, format, 
					    &tree_ele->location, &tree_ele->dci_msg[i]);
        if (ret == SRSLTE_SUCCESS){
            count++;
        }
    }
    tree_ele->nof_msg = count;
    return count;
}

int srslte_prune_tree(srslte_tree_ele* bin_tree, srslte_dci_msg_paws* decode_ret,
                            srslte_active_ue_list_t* active_ue_list,
                            int nof_cce, uint32_t tti)
{
    int msg_cnt = 0;
    for(int i=0;i<15;i++){  
	if(bin_tree[i].cleared || (bin_tree[i].nof_msg <= 0)){
	    continue;
	}
	if(dci_msg_location_pruning(active_ue_list, bin_tree[i].dci_msg, &decode_ret[msg_cnt], tti) > 0){
	    if(srslte_pdcch_ue_locations_ncce_check(nof_cce, tti % 10, decode_ret[msg_cnt].rnti, decode_ret[msg_cnt].ncce)){
		traversal_children_clear(bin_tree, i);
		msg_cnt++;	
	    }
	}
    }
    return msg_cnt;
}

int srslte_decode_dci_single_location_blk_yx(srslte_ue_dl_t*     q,
                                uint32_t cfi, uint32_t tti,
                               srslte_active_ue_list_t* active_ue_list,
                               srslte_dci_location_blk_paws_t* loc_blk,
                               srslte_dci_msg_paws* dci_decode_ret)
{
    int decoded_msg = 0;
    int nof_cce = ((cfi>0&&cfi<4)?q->pdcch.nof_cce[cfi-1]:0); 

    srslte_dci_msg_paws decode_ret;
    srslte_tree_ele bin_tree[15];

    srslte_tree_blk2tree(loc_blk, bin_tree);
    
    //printf("NOF CCE:%d\n", nof_cce);
    for(int i=0;i<15;i++){
	//printf("decoding %d-th element in the tree!\n", i);
	if(bin_tree[i].cleared){
	    //printf("Skip %d-th elements in the tree due to cleared!\n", i);
	    continue;
	}
	if(bin_tree[i].location.mean_llr < LLR_THR){ 
	    //printf("Skip %d-th elements in the tree due to low llr\n", i);
	    //if(bin_tree[i].location.L == 0){ 
	    //printf("Skip %d-th elements in the tree due to low llr\n", i);
	    continue;
	    //}
	}
	if( srslte_decode_dci_single_location_no_prune_yx(q, cfi, tti, &bin_tree[i]) > 0){
	    if( prune_tree_element(bin_tree, i, &decode_ret) > 0){
		printf(" ---  We find a match, ");
		if(srslte_pdcch_ue_locations_ncce_check(nof_cce, tti % 10, decode_ret.rnti, decode_ret.ncce)){
		    printf("and we past the ncce location test \n");
		    memcpy(&dci_decode_ret[decoded_msg], &decode_ret, sizeof(srslte_dci_msg_paws));
		    dci_decode_ret[decoded_msg].off_tree = true;
		    decoded_msg++; 
		}else{
		    printf("and we fail in the ncce location test \n");
		}
	    }	
	}
    }
    int num = srslte_prune_tree(bin_tree, &dci_decode_ret[decoded_msg], active_ue_list, nof_cce, tti);
    decoded_msg += num;
    return decoded_msg;
}
int srslte_decode_dci_single_location_yx(srslte_ue_dl_t*     q,
                                uint32_t cfi, uint32_t tti,
                               srslte_active_ue_list_t* active_ue_list,
                               srslte_dci_location_paws_t* location,
                               srslte_dci_msg_paws* dci_decode_ret)
{
    //nof_ue_all_formats ue_all_formats

    srslte_dci_msg_paws dci_msg_yx[NOF_UE_ALL_FORMATS];
    memset(dci_msg_yx, 0, NOF_UE_ALL_FORMATS * sizeof(srslte_dci_msg_paws));

    int ret, count=0;
    srslte_dci_format_t format;

    // Loop over all formats
    for(int i=0;i<NOF_UE_ALL_FORMATS;i++){
	//if(i==0){continue;};
        //if (i != 2 && i != 5){continue;}  // We skip the uplink dci and format 1C
        //if (i!=0 && i != 2 && i != 5){continue;}  // We skip the uplink dci and format 1C
        format = ue_all_formats[i];
        ret = srslte_pdcch_decode_msg_yx(&(q->pdcch),  cfi, tti, true, format, location, &dci_msg_yx[i]);
        if (ret == SRSLTE_SUCCESS){
            count++;
        }
    }
    if (count > 0){
        return dci_msg_location_pruning(active_ue_list, dci_msg_yx, dci_decode_ret, tti);
    }
    return 0;
}

int extract_UL_DL_dci_msg(srslte_dci_msg_paws* dci_msg_in, srslte_dci_msg_paws* dci_msg_out, uint32_t nof_msg, bool downlink)
{
    int count = 0;
    for(int i=0;i<nof_msg;i++){
	if(dci_msg_in[i].downlink == downlink){
	    memcpy(&dci_msg_out[count], &dci_msg_in[i], sizeof(srslte_dci_msg_paws));
	    count++;
	}
    }  
    return count; 
} 
int srslte_decode_dci_all_loc_block_yx(srslte_ue_dl_t*     q,
                                uint32_t cfi, uint32_t tti,
                               srslte_active_ue_list_t* active_ue_list,
			       srslte_dci_location_blk_paws_t* loc_blocks,
                               srslte_dci_subframe_t* dci_msg_subframe,
                               uint32_t nof_locations)
{
    srslte_dci_msg_paws dci_msg_vector[15];

    int msg_cnt = 0, output_msg_cnt = 0, cnt = 0;
    int CELL_MAX_PRB = q->cell.nof_prb;

    for(int i=0;i<nof_locations;i++){
	//printf("decoding %d-th location blocks!\n",i);
	cnt = srslte_decode_dci_single_location_blk_yx(q, cfi, tti, active_ue_list, &loc_blocks[i], &dci_msg_vector[msg_cnt]);
	if( cnt > 0){
	    msg_cnt += cnt;
	}	
    }

    printf("before pruning %d messages decoded!\n", msg_cnt);
    dci_msg_list_display(dci_msg_vector, msg_cnt);

    srslte_subframe_prune_dl_ul_all(dci_msg_vector, active_ue_list, dci_msg_subframe, CELL_MAX_PRB, msg_cnt);

    output_msg_cnt = dci_msg_subframe->dl_msg_cnt + dci_msg_subframe->ul_msg_cnt; 
    return output_msg_cnt;
}

int srslte_decode_dci_wholeSF_yx(srslte_ue_dl_t*     q,
                                uint32_t cfi, uint32_t tti,
                               srslte_active_ue_list_t* active_ue_list,
                               srslte_dci_location_paws_t* locations,
                               srslte_dci_subframe_t* dci_msg_subframe,
                               uint32_t nof_locations)
{
    srslte_dci_msg_paws dci_msg_vector[10];
    memset(dci_msg_vector, 0, 10 * sizeof(srslte_dci_msg_paws));

    srslte_dci_msg_paws dci_msg_downlink[10];
    srslte_dci_msg_paws dci_msg_uplink[10];

    srslte_dci_msg_paws dci_decode_ret;


    int msg_cnt = 0, msg_cnt_ul=0, msg_cnt_dl=0;
    int output_msg_cnt = 0;
    int cnt, nof_prb;
    int CELL_MAX_PRB = q->cell.nof_prb;


    /* Decode all the possible locations and 
     * store the results in the dci_msg_vector*/
    for(int i=0;i<nof_locations;i++){
        if(locations[i].checked == 1){continue;} // skip the locations that has already been checked
        cnt = srslte_decode_dci_single_location_yx(q, cfi, tti, active_ue_list, &locations[i], &dci_decode_ret);
        if (cnt > 0){
            dci_msg_vector[msg_cnt] = dci_decode_ret;
            msg_cnt++;
            check_decoded_locations(locations, nof_locations,i);
        }
    }
    
    msg_cnt_dl = extract_UL_DL_dci_msg(dci_msg_vector, dci_msg_downlink, msg_cnt, true);
    msg_cnt_ul = extract_UL_DL_dci_msg(dci_msg_vector, dci_msg_uplink, msg_cnt, false);

    /* Prune the downlink dci messages */
    nof_prb = nof_prb_one_subframe(dci_msg_downlink, msg_cnt_dl);
    if(nof_prb > CELL_MAX_PRB){
        //prune_msg_cnt_dl = dci_subframe_pruning(active_ue_list, dci_msg_downlink, dci_msg_prune_dl, q->cell.nof_prb, msg_cnt_dl);
        dci_msg_subframe->dl_msg_cnt = dci_subframe_pruning(active_ue_list, 
			dci_msg_downlink, dci_msg_subframe->downlink_msg, q->cell.nof_prb, msg_cnt_dl);
    }else{
	dci_msg_subframe->dl_msg_cnt = msg_cnt_dl;
        memcpy(dci_msg_subframe->downlink_msg, dci_msg_downlink, msg_cnt_dl*sizeof(srslte_dci_msg_paws));
    }

    /* Prune the downlink dci messages */
    nof_prb = nof_prb_one_subframe(dci_msg_uplink, msg_cnt_ul);
    if(nof_prb > CELL_MAX_PRB){
	dci_msg_subframe->ul_msg_cnt = dci_subframe_pruning(active_ue_list, 
			dci_msg_uplink, dci_msg_subframe->uplink_msg, q->cell.nof_prb, msg_cnt_ul);
    }else{
	dci_msg_subframe->ul_msg_cnt = msg_cnt_ul;
        memcpy(dci_msg_subframe->uplink_msg, dci_msg_uplink, msg_cnt_ul*sizeof(srslte_dci_msg_paws));
    }
    output_msg_cnt = dci_msg_subframe->dl_msg_cnt + dci_msg_subframe->ul_msg_cnt; 
    return output_msg_cnt;
}

/* The dci decoding function output the message in mssage array dci_msg_out
*  return the number of decoded dci messages        */
int srslte_dci_decoder_yx(  srslte_ue_dl_t*     q,
                            srslte_active_ue_list_t* active_ue_list,
                            srslte_dci_subframe_t* dci_msg_subframe,
                            uint32_t    tti)
{
    uint32_t cfi;
    uint32_t sf_idx = tti%10;
    int ret = SRSLTE_ERROR;
    if ((ret = srslte_ue_dl_decode_fft_estimate_mbsfn(q, sf_idx, &cfi, SRSLTE_SF_NORM)) < 0) {
        return ret;
    }

    float noise_estimate = srslte_chest_dl_get_noise_estimate(&q->chest);
    if (srslte_pdcch_extract_llr_multi(&q->pdcch, q->sf_symbols_m, q->ce_m, noise_estimate, sf_idx, cfi)) {
        fprintf(stderr, "Error extracting LLRs\n");
        return SRSLTE_ERROR;
    }

    int nof_dci_msg = 0;
    uint32_t nof_locations = 0;

    srslte_dci_location_paws_t locations[MAX_CANDIDATES_ALL];
    // prepare the search space
    nof_locations = srslte_pdcch_search_space_all_yx(&(q->pdcch), cfi, locations);
    if( nof_locations == 0){
        return 0;
    }
    //decode all possible locations
    nof_dci_msg = srslte_decode_dci_wholeSF_yx(q, cfi, tti, active_ue_list, locations, dci_msg_subframe, nof_locations);
// 
//    srslte_dci_location_blk_paws_t loc_blocks[12];
//    nof_locations = srslte_pdcch_location_blocks_yx(&(q->pdcch), cfi, loc_blocks);
//
//    //for(int i=0;i<nof_locations;i++){
//    //    printf("%d-th block -- L:%d ncce:%d |", i, loc_blocks[i].loc_L3[0].L, loc_blocks[i].loc_L3[0].ncce);
//    //    for(int j=0;j<2;j++){
//    //        printf("|L:%d ncce:%d |",loc_blocks[i].loc_L2[j].L, loc_blocks[i].loc_L2[j].ncce);
//    //    }
//    //    for(int j=0;j<4;j++){
//    //        printf("|L:%d ncce:%d |",loc_blocks[i].loc_L1[j].L, loc_blocks[i].loc_L1[j].ncce);
//    //    }
//    //    for(int j=0;j<8;j++){
//    //        printf("|L:%d ncce:%d |",loc_blocks[i].loc_L0[j].L, loc_blocks[i].loc_L0[j].ncce);
//    //    }
//    //    printf("\n");
//    //}
//
//    // decode all possible locations
//    nof_dci_msg = srslte_decode_dci_all_loc_block_yx(q, cfi, tti, active_ue_list, loc_blocks, dci_msg_subframe, nof_locations);
//    printf("nof_dci_msg:%d\n",nof_dci_msg);
    return nof_dci_msg;
}


