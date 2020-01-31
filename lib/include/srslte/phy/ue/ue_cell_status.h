#ifndef SRSLTE_UE_CELL_STATUS_H
#define SRSLTE_UE_CELL_STATUS_H

#include "srslte/phy/ue/lte_scope.h"

#define MAX_TTI 10240
#define NOF_RECORDED_SF 32
#define NOF_LOG_SF 320
#define MAX_NOF_CA 3
#define NOF_REPORT_SF 20

#define EMPTY_CELL_LEN 	30
#define AVE_UE_DL_PRB	30
#define AVE_UE_PHY_RATE 30
#define AVE_UE_RATE 30

#define NOF_BITS_PER_PKT 12000
#define CA_DEACTIVE_TIME 80 

typedef struct SRSLTE_API{
    uint16_t cell_dl_prb;
    uint16_t cell_ul_prb;

    uint16_t ue_dl_prb;
    uint16_t ue_ul_prb;

    uint32_t tti;
    uint32_t mcs_idx_tb1;
    int      tbs_tb1;
    int      tbs_hm_tb1;
    int      rv_tb1;
    bool     ndi_tb1;

    uint32_t mcs_idx_tb2;
    int      tbs_tb2;
    int      tbs_hm_tb2;
    int      rv_tb2;
    bool     ndi_tb2;

}srslte_subframe_status;

typedef struct SRSLTE_API{
    uint16_t		    header;
    uint32_t		    last_active;  // the time (ms) the UE is active 
    uint16_t		    dci_touched;

    int			    last_rate_tbs1, last_rate_tbs2;
    int			    last_rate_hm1, last_rate_hm2;
    bool		    dci_token[NOF_LOG_SF];
    srslte_subframe_status  sf_status[NOF_LOG_SF];
}srslte_cell_status;



typedef struct SRSLTE_API{
    uint16_t		targetRNTI; 
    bool		logFlag;
    bool		printFlag;
    FILE*		FD;
    bool		remote_flag;
    int			remote_sock;

    uint16_t		nof_cell;

    uint16_t		header;	    //indicator of the maximum tti 
    uint16_t		max_cell_prb[MAX_NOF_CA]; 
    bool		cell_triggered[MAX_NOF_CA]; 
    uint16_t		nof_thread[MAX_NOF_CA];  // nof threads that are created for decoding each cell 

    srslte_cell_status  cell_status[MAX_NOF_CA];
    
}srslte_ue_cell_usage;
    
typedef struct SRSLTE_API{
    int probe_rate;
    int probe_rate_hm;
    int full_load;
    int full_load_hm;
    int ue_rate;
    int ue_rate_hm;
    int cell_usage;
}srslte_lteCCA_rate;

// Init the rnti list
SRSLTE_API int srslte_UeCell_init(srslte_ue_cell_usage* q);
SRSLTE_API int srslte_UeCell_reset(srslte_ue_cell_usage* q); 
SRSLTE_API int srslte_UeCell_ask_for_dci_token(srslte_ue_cell_usage* q, int ca_idx, uint32_t tti);
SRSLTE_API int srslte_UeCell_return_dci_token(srslte_ue_cell_usage* q, int ca_idx, uint32_t tti,
                                    srslte_dci_msg_paws* ue_dci, srslte_subframe_bw_usage* bw_usage);
SRSLTE_API int srslte_UeCell_get_status(srslte_ue_cell_usage* q, uint32_t last_tti, uint32_t* current_tti, bool* ca_active,
                                        int cell_dl_prb[][NOF_REPORT_SF], int ue_dl_prb[][NOF_REPORT_SF],
                                        int mcs_tb1[][NOF_REPORT_SF], int mcs_tb2[][NOF_REPORT_SF],
                                        int tbs[][NOF_REPORT_SF], int tbs_hm[][NOF_REPORT_SF]);
SRSLTE_API int srslte_UeCell_get_maxPRB(srslte_ue_cell_usage* q, int* cellMaxPrb); 
SRSLTE_API int srslte_UeCell_get_nof_cell(srslte_ue_cell_usage* q);
SRSLTE_API uint16_t srslte_UeCell_get_targetRNTI(srslte_ue_cell_usage* q);

SRSLTE_API int srslte_UeCell_set_remote_sock(srslte_ue_cell_usage* q,  int sock);
SRSLTE_API int srslte_UeCell_set_remote_flag(srslte_ue_cell_usage* q,  bool flag);
SRSLTE_API int srslte_UeCell_set_logFlag(srslte_ue_cell_usage* q, bool logFlag);
SRSLTE_API int srslte_UeCell_set_file_descriptor(srslte_ue_cell_usage* q,  FILE* FD);
SRSLTE_API int srslte_UeCell_set_printFlag(srslte_ue_cell_usage* q, bool printFlag); 
SRSLTE_API int srslte_UeCell_set_nof_cells(srslte_ue_cell_usage* q, uint16_t nof_cells);
SRSLTE_API int srslte_UeCell_set_targetRNTI(srslte_ue_cell_usage* q, uint16_t targetRNTI);
SRSLTE_API int srslte_UeCell_set_prb(srslte_ue_cell_usage* q, uint16_t nof_prb, int ca_idx);
SRSLTE_API int srslte_UeCell_set_nof_thread(srslte_ue_cell_usage* q, uint16_t nof_thread, int ca_idx); 
//SRSLTE_API 


#endif
