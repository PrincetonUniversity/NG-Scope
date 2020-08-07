#ifndef STATUS_MATH_H
#define STATUS_MATH_H

#include "srslte/phy/ue/lte_scope.h"
#include "srslte/phy/ue/LTESCOPE_GLOBAL.h"

#include "load_config.h"
#include "ue_cell_status.h"

typedef struct{
    uint16_t	nof_cell;
    bool	nameFile_time;    
    bool	log_dl_flag;
    bool	dl_single_file;
    FILE*	log_dl_fd[MAX_NOF_USRP];	

    bool	log_ul_flag;
    bool	ul_single_file;
    FILE*	log_ul_fd[MAX_NOF_USRP];	

}lteCCA_rawLog_setting_t;

typedef struct{
    uint16_t    status_header;
    bool        active_flag;
    bool	display_flag;

    lteCCA_rawLog_setting_t rawLog_setting;

}lteCCA_status_t;

int lteCCA_status_update(lteCCA_status_t* q, srslte_ue_cell_usage* cell_status);

int lteCCA_status_init(lteCCA_status_t* q);
int lteCCA_status_exit(lteCCA_status_t* q);

int lteCCA_fill_fileDescriptor(lteCCA_status_t* q, usrp_config_t* usrp_config);
int lteCCA_update_fileDescriptor(lteCCA_status_t* q, usrp_config_t* usrp_config);

void lteCCA_setRawLog_all(lteCCA_status_t* q, rawLog_config_t* log_config);

void lteCCA_setRawLog_log_dl_flag(lteCCA_status_t* q, bool flag);
void lteCCA_setRawLog_dl_single_file(lteCCA_status_t* q, bool flag);
void lteCCA_setRawLog_log_ul_flag(lteCCA_status_t* q, bool flag);
void lteCCA_setRawLog_ul_single_file(lteCCA_status_t* q, bool flag);
void lteCCA_setRawLog_nof_cell(lteCCA_status_t* q, uint16_t nof_cell);

#endif
