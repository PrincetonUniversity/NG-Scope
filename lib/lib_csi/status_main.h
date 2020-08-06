#ifndef STATUS_MATH_H
#define STATUS_MATH_H

#include "srslte/phy/ue/lte_scope.h"
#include "srslte/phy/ue/LTESCOPE_GLOBAL.h"

#include "load_config.h"
#include "ue_cell_status.h"

typedef struct{

    bool	forward_flag;
    int		forward_sock;

    uint16_t	format;

    uint16_t	nof_cell;

    int		nof_prb[MAX_NOF_USRP];
    bool	prb_flag[MAX_NOF_USRP];

    int		nof_rx_ant[MAX_NOF_USRP];
    int		nof_tx_ant[MAX_NOF_USRP];
    bool	tx_ant_flag[MAX_NOF_USRP];

    bool	log_amp_flag;
    FILE*	log_amp_fd[MAX_NOF_USRP];	

    bool	log_phase_flag;
    FILE*	log_phase_fd[MAX_NOF_USRP];	

    int		down_sample_subcarrier;
    int		down_sample_subframe;
 
}lteCCA_rawLog_setting_t;

typedef struct{
    uint16_t    status_header;
    bool        active_flag;
    bool	display_flag;

    lteCCA_rawLog_setting_t rawLog_setting;
}lteCCA_status_t;


// the main function
int lteCCA_status_update(lteCCA_status_t* q, srslte_ue_cell_usage* cell_status);


int lteCCA_status_init(lteCCA_status_t* q);
int lteCCA_status_exit(lteCCA_status_t* q);

int lteCCA_forward_init(lteCCA_status_t* q, csi_forward_config_t* forward_config);

int lteCCA_fill_fileDescriptor(lteCCA_status_t* q, usrp_config_t* usrp_config, csiLog_config_t* csi_config);
int lteCCA_fill_fileDescriptor_folder(lteCCA_status_t* q, usrp_config_t* usrp_config, csiLog_config_t* csi_config);

int lteCCA_update_fileDescriptor(lteCCA_status_t* q, usrp_config_t* usrp_config);
int lteCCA_update_fileDescriptor_folder(lteCCA_status_t* q, usrp_config_t* usrp_config);


// Functions for setting the parameters
void lteCCA_setRawLog_prb(lteCCA_status_t* q, uint16_t prb, int cell_idx);
void lteCCA_setRawLog_txAnt(lteCCA_status_t* q, uint16_t tx_ant, int cell_idx);
void lteCCA_setRawLog_rxAnt(lteCCA_status_t* q, uint16_t rx_ant, int cell_idx);

void lteCCA_setRawLog_all(lteCCA_status_t* q, csiLog_config_t* log_config);
void lteCCA_setRawLog_log_amp_flag(lteCCA_status_t* q, bool flag);
void lteCCA_setRawLog_log_phase_flag(lteCCA_status_t* q, bool flag);
void lteCCA_setRawLog_nof_cell(lteCCA_status_t* q, uint16_t nof_cell);

#endif
