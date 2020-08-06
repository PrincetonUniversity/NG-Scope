#ifndef SRSLTE_UE_CELL_STATUS_H
#define SRSLTE_UE_CELL_STATUS_H

#include "srslte/phy/ue/lte_scope.h"
#include "srslte/phy/ue/LTESCOPE_GLOBAL.h"

#define MAX_TTI 10240
#define NOF_RECORDED_SF 32
#define NOF_LOG_SF 40
#define MAX_NOF_CA 3
#define NOF_REPORT_SF 20

#define EMPTY_CELL_LEN  30
#define AVE_UE_DL_PRB   30
#define AVE_UE_PHY_RATE 10
#define AVE_UE_RATE 30

#define NOF_BITS_PER_PKT 12000
#define CA_DEACTIVE_TIME 80

#define MAX_NOF_ANT 4
typedef struct SRSLTE_API{

    // Cell status information
    uint32_t tti;	    // the current tti
    //float		    ***csi_amp;		// 3d csi amp matrix
    //float		    ***csi_phase;	// 3d csi phase matrix
    float   *csi_amp[4][4];		// 3d csi amp matrix
    float   *csi_phase[4][4];	// 3d csi phase matrix

}srslte_subframe_status;

typedef struct SRSLTE_API{
    uint16_t                header; // each carrier has its own header
    uint16_t                dci_touched;    // the index of the most recent decoding subframe
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
    uint16_t		nof_tx_ant[MAX_NOF_CA];	    // Tx antenna number 
    uint16_t		nof_rx_ant[MAX_NOF_CA];	    // Rx antenna number 

    uint16_t            nof_thread[MAX_NOF_CA];  // nof threads that are created for decoding each cell

    srslte_cell_status  cell_status[MAX_NOF_CA];

}srslte_ue_cell_usage;

int cell_status_init(srslte_ue_cell_usage* q);
int cell_status_reset(srslte_ue_cell_usage* q);

int cell_status_allocate_csi_matrix(srslte_ue_cell_usage* q);
int cell_status_exit_free_csi_matrix(srslte_ue_cell_usage* q);

int cell_status_ask_for_dci_token(srslte_ue_cell_usage* q, int ca_idx, uint32_t tti);
int cell_status_return_dci_token(srslte_ue_cell_usage* q, srslte_ue_dl_t* ue_dl, uint16_t ca_idx, uint32_t tti);

int cell_status_set_nof_cells(srslte_ue_cell_usage* q, uint16_t nof_cells);
int cell_status_set_nof_thread(srslte_ue_cell_usage* q, uint16_t nof_thread, int ca_idx);
int cell_status_set_prb(srslte_ue_cell_usage* q, uint16_t nof_prb, int ca_idx);
int cell_status_set_rx_ant(srslte_ue_cell_usage* q, uint16_t nof_rx_ant, int ca_idx);
int cell_status_set_tx_ant(srslte_ue_cell_usage* q, uint16_t nof_tx_ant, int ca_idx);


#endif
