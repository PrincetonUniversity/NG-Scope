#ifndef SRSRAN_NGSCOPE_ST_H
#define SRSRAN_NGSCOPE_ST_H

#define MAX_CANDIDATES_ALL 180
#define MAX_NOF_FORMAT 4
#define MAX_DCI_PER_SUB 8
#define LLR_RATIO 0.3f

typedef struct{
    uint32_t mcs;
    uint32_t tbs;
    uint32_t rv;
    //bool     ndi;
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
    // information of the transport block
    ngscope_dci_tb_t    tb[2];

    // parameters stored for decoding phich
    ngscope_dci_phich_t phich;
}ngscope_dci_msg_t;


typedef struct SRSRAN_API {
    ngscope_dci_msg_t  dl_msg[MAX_DCI_PER_SUB];
    uint32_t           nof_dl_dci;

    ngscope_dci_msg_t  ul_msg[MAX_DCI_PER_SUB];
    uint32_t           nof_ul_dci;
} ngscope_dci_per_sub_t;


#endif
