#ifndef SRSLTE_UE_CELL_STATUS_H
#define SRSLTE_UE_CELL_STATUS_H

#include "srslte/phy/ue/lte_scope.h"
#include "srslte/phy/ue/LTESCOPE_GLOBAL.h"

#define MAX_TTI 10240
#define NOF_RECORDED_SF 32
#define NOF_LOG_SF 320
#define MAX_NOF_CA 3
#define NOF_REPORT_SF 20

#define EMPTY_CELL_LEN  30
#define AVE_UE_DL_PRB   30
#define AVE_UE_PHY_RATE 10
#define AVE_UE_RATE 30

#define NOF_BITS_PER_PKT 12000
#define CA_DEACTIVE_TIME 80

typedef struct SRSLTE_API{
    float   rsrq;	// per-subframe
    float   rsrp0;
    float   rsrp1;
    float   noise;
}srslte_subframe_rf_status;

typedef struct SRSLTE_API{
    // Cell status information
    uint32_t tti;	    // the current tti
    uint16_t cell_dl_prb;   // the amount of utilized dl PRB
    uint16_t cell_ul_prb;   // the amount of utilized ul PRB
    uint8_t  nof_msg_dl;    // number of downlink messages stored
    uint8_t  nof_msg_ul;    // number of uplink messages stored

    float   rsrq;	// per-subframe
    float   rsrp0;
    float   rsrp1;
    float   noise;
    /*	    UE status information	*/

    // -->  Donwlink control information 
    uint16_t	dl_rnti_list[MAX_NOF_MSG_PER_SF];

    uint32_t	dl_nof_prb[MAX_NOF_MSG_PER_SF];

    uint8_t	dl_mcs_tb1[MAX_NOF_MSG_PER_SF];
    uint8_t	dl_mcs_tb2[MAX_NOF_MSG_PER_SF];
    int		dl_tbs_tb1[MAX_NOF_MSG_PER_SF];
    int		dl_tbs_tb2[MAX_NOF_MSG_PER_SF];
    int		dl_tbs_hm_tb1[MAX_NOF_MSG_PER_SF];
    int		dl_tbs_hm_tb2[MAX_NOF_MSG_PER_SF];
    
    uint8_t	dl_rv_tb1[MAX_NOF_MSG_PER_SF];
    uint8_t	dl_rv_tb2[MAX_NOF_MSG_PER_SF];
    bool	dl_ndi_tb1[MAX_NOF_MSG_PER_SF];
    bool	dl_ndi_tb2[MAX_NOF_MSG_PER_SF];
    
    // -->  Uplink control information 
    uint16_t	ul_rnti_list[MAX_NOF_MSG_PER_SF];
    uint32_t	ul_nof_prb[MAX_NOF_MSG_PER_SF];
    uint8_t	ul_mcs[MAX_NOF_MSG_PER_SF];
    int		ul_tbs[MAX_NOF_MSG_PER_SF];
    int		ul_tbs_hm[MAX_NOF_MSG_PER_SF];
    uint8_t	ul_rv[MAX_NOF_MSG_PER_SF];
    bool	ul_ndi[MAX_NOF_MSG_PER_SF];

}srslte_subframe_status;

typedef struct SRSLTE_API{
    uint16_t                header; // each carrier has its own header

    //TODO I temporaly delete this member 
    //uint32_t                last_active;  // the time (ms) the UE is active
    uint16_t                dci_touched;    // the index of the most recent decoding subframe

    int                     last_rate_tbs1, last_rate_tbs2;
    int                     last_rate_hm1, last_rate_hm2;

    bool                    dci_token[NOF_LOG_SF];
    srslte_subframe_status  sf_status[NOF_LOG_SF];

}srslte_cell_status;

typedef struct SRSLTE_API{
    //TODO I temporaly delete target RNTI
    //uint16_t    targetRNTI;

    bool	printFlag;

    bool        logFlag;
    FILE*       dci_fd;

    bool        remote_flag;
    int         remote_sock;

    bool	sync_flag;	    // the indicator of cell synchronization

    uint16_t            nof_cell;   // the number of carriers (cell towers) we montiors 

    uint16_t            header;     //indicator of the maximum tti 
    uint16_t            stat_header;     //indicator of the which subframe the status has been processed

    uint16_t            max_cell_prb[MAX_NOF_CA];   // The max PRB each carrier has

    bool                cell_triggered[MAX_NOF_CA]; // Is this cell triggered for a specific UE

    uint16_t            nof_thread[MAX_NOF_CA];  // nof threads that are created for decoding each cell

    srslte_cell_status  cell_status[MAX_NOF_CA];

}srslte_ue_cell_usage;

int cell_status_init(srslte_ue_cell_usage* q);
int cell_status_reset(srslte_ue_cell_usage* q);

int cell_status_ask_for_dci_token(srslte_ue_cell_usage* q, int ca_idx, uint32_t tti);
int cell_status_return_dci_token(srslte_ue_cell_usage* q, int ca_idx, uint32_t tti,
                                 srslte_dci_subframe_t* dci_list,
				   srslte_subframe_rf_status* rf_status);

int cell_status_set_nof_cells(srslte_ue_cell_usage* q, uint16_t nof_cells);
int cell_status_set_prb(srslte_ue_cell_usage* q, uint16_t nof_prb, int ca_idx);
int cell_status_set_nof_thread(srslte_ue_cell_usage* q, uint16_t nof_thread, int ca_idx);

#endif
