#ifndef SRSRAN_NGSCOPE_ST_H
#define SRSRAN_NGSCOPE_ST_H

#define MAX_CANDIDATES_ALL 180
#define MAX_NOF_FORMAT 4
#define MAX_DCI_PER_SUB 10
#define LLR_RATIO 0.3f

#define PDCCH_FORMAT_NOF_BITS(i) ((1 << i) * 72)

#include "srsran/phy/phch/dci.h"
typedef struct{
    uint32_t mcs;
    uint32_t tbs;
    uint32_t rv;
    bool     ndi;
}ngscope_dci_tb_t;


/* only used for decoding phich*/
typedef struct{
    uint32_t n_dmrs;
    uint32_t n_prb_tilde;
}ngscope_dci_phich_t;


typedef struct{
    uint16_t rnti;
    uint32_t prb;
    uint32_t harq;
    int      nof_tb;
    bool     dl;
    float    decode_prob;
    float    corr; 

    srsran_dci_format_t format;
    // information of the transport block
    ngscope_dci_tb_t    tb[2];

    // parameters stored for decoding phich
    ngscope_dci_phich_t phich;
	srsran_dci_location_t loc;
}ngscope_dci_msg_t;


typedef struct SRSRAN_API {
    ngscope_dci_msg_t  dl_msg[MAX_DCI_PER_SUB];
    uint32_t           nof_dl_dci;

    ngscope_dci_msg_t  ul_msg[MAX_DCI_PER_SUB];
    uint32_t           nof_ul_dci;

	uint64_t 			timestamp;
} ngscope_dci_per_sub_t;

int ngscope_push_dci_to_per_sub(ngscope_dci_per_sub_t* q, ngscope_dci_msg_t* msg);

int ngscope_enqueue_ul_reTx_dci_msg(ngscope_dci_per_sub_t* q, uint16_t targetRNTI);
int ngscope_rnti_inside_dci_per_sub_dl(ngscope_dci_per_sub_t* q, uint16_t targetRNTI);
int ngscope_rnti_inside_dci_per_sub_ul(ngscope_dci_per_sub_t* q, uint16_t targetRNTI);

int ngscope_format_to_index(srsran_dci_format_t format);

srsran_dci_format_t ngscope_index_to_format(int index);
void srsran_ngscope_print_dci_per_sub(ngscope_dci_per_sub_t* q);



#endif
