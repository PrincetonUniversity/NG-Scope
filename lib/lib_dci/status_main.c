#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#include "srslte/phy/ue/lte_scope.h"
#include "srslte/phy/ue/LTESCOPE_GLOBAL.h"
#include "tbs_256QAM_tables.h"
#include "status_main.h"

#define TTI_TO_IDX(i) (i%NOF_LOG_SF)


// --> display single message
static void display_subframe_dci_msg(srslte_ue_cell_usage* q, uint16_t index){

    for(int i=0;i<q->nof_cell;i++){
        printf("|%d\t%d\t%d\t",q->cell_status[i].sf_status[index].tti,
                              q->cell_status[i].sf_status[index].cell_dl_prb,
                              q->cell_status[i].sf_status[index].cell_ul_prb);
    }
    printf("\n");
    return;
}

static void log_subframe_raw_dl_msg(srslte_ue_cell_usage* q, lteCCA_rawLog_setting_t* config, uint16_t index){
    int		nof_cell = q->nof_cell;
    int		nof_msg;
    uint16_t	tti;
    FILE*	FD;
    srslte_subframe_status* sf_stat;
    
    for(int cell_idx=0;cell_idx<nof_cell;cell_idx++){
	sf_stat = &(q->cell_status[cell_idx].sf_status[index]);
	nof_msg = sf_stat->nof_msg_dl;
	tti	= sf_stat->tti;
	if(config->dl_single_file){
	    FD	= config->log_dl_fd[0];
	}else{
	    FD	= config->log_dl_fd[cell_idx];
	}

	for(int i=0;i<nof_msg;i++){
	    int mcs_idx;
	    int tbs = 0;
	    if(sf_stat->dl_mcs_tb1[i] > 0){
		if(sf_stat->dl_mcs_tb1[i] > 27){
		    mcs_idx = dl_mcs_tbs_idx_table2[27];
		}else{
		    mcs_idx = dl_mcs_tbs_idx_table2[sf_stat->dl_mcs_tb1[i]];
		}
		//printf("mcs:%d mcs_idx:%d prb:%d\n",sf_stat->dl_mcs_tb1[i], mcs_idx,sf_stat->dl_nof_prb[i]);
		tbs 	+= tbs_table_256QAM[mcs_idx][sf_stat->dl_nof_prb[i]];	
	    }
	    if(sf_stat->dl_mcs_tb2[i] > 0){
		if(sf_stat->dl_mcs_tb2[i] > 27){
		    mcs_idx = dl_mcs_tbs_idx_table2[27];
		}else{
		    mcs_idx = dl_mcs_tbs_idx_table2[sf_stat->dl_mcs_tb2[i]];
		}
		//printf("mcs:%d mcs_idx:%d prb:%d\n",sf_stat->dl_mcs_tb2[i], mcs_idx,sf_stat->dl_nof_prb[i]);
		tbs 	+= tbs_table_256QAM[mcs_idx][sf_stat->dl_nof_prb[i]];	
	    }

	    fprintf(FD, "%d\t%d\t%d\t",tti, sf_stat->dl_rnti_list[i],cell_idx);
	    fprintf(FD, "%d\t%d\t",sf_stat->cell_dl_prb, sf_stat->dl_nof_prb[i]);
	    fprintf(FD, "%d\n", tbs);

	    //	fprintf(FD, "%d\t%d\t",sf_stat->cell_dl_prb, sf_stat->dl_nof_prb[i]);
	    //	fprintf(FD, "%d\t%d\t",sf_stat->dl_mcs_tb1[i], sf_stat->dl_mcs_tb2[i]);
	    //	fprintf(FD, "%d\t%d\t",sf_stat->dl_tbs_tb1[i], sf_stat->dl_tbs_tb2[i]);
	    //	fprintf(FD, "%d\t%d\n",sf_stat->dl_tbs_hm_tb1[i], sf_stat->dl_tbs_hm_tb2[i]);
	}
        
    }
    return;
}

static void log_subframe_raw_ul_msg(srslte_ue_cell_usage* q, lteCCA_rawLog_setting_t* config, uint16_t index){
    int		nof_cell = q->nof_cell;
    int		nof_msg;
    uint16_t	tti;
    FILE*	FD;
    srslte_subframe_status* sf_stat;

    for(int cell_idx=0;cell_idx<nof_cell;cell_idx++){
		sf_stat = &(q->cell_status[cell_idx].sf_status[index]);

		nof_msg = sf_stat->nof_msg_ul;
		tti	= sf_stat->tti;

		if(config->dl_single_file){
			FD	= config->log_ul_fd[0];
		}else{
			FD	= config->log_ul_fd[cell_idx];
		}

		for(int i=0;i<nof_msg;i++){
			int mcs_idx;
			int tbs = 0;
			if(sf_stat->ul_mcs[i] > 0){
				mcs_idx = dl_mcs_tbs_idx_table2[sf_stat->ul_mcs[i]];
				tbs 	+= tbs_table_256QAM[mcs_idx][sf_stat->ul_nof_prb[i]];	
			}
			fprintf(FD, "%d\t%d\t%d\t",tti, sf_stat->ul_rnti_list[i],cell_idx);
			fprintf(FD, "%d\t%d\t",sf_stat->cell_ul_prb, sf_stat->ul_nof_prb[i]);
			fprintf(FD, "%d\n", tbs);
			//fprintf(FD, "%d\t%d\t%d\t\n",sf_stat->ul_mcs[i], sf_stat->ul_tbs[i], sf_stat->ul_tbs_hm[i]);
		}
    }
    return;
}

static void log_subframe_raw_dci_msg(lteCCA_rawLog_setting_t* q, srslte_ue_cell_usage* cell_usage, uint16_t index){
    if(q->log_dl_flag){
	    log_subframe_raw_dl_msg(cell_usage, q, index);
    }

    if(q->log_ul_flag){
	    log_subframe_raw_ul_msg(cell_usage, q, index);
    }
}

int single_subframe_status_update(lteCCA_status_t* q, srslte_ue_cell_usage* cell_usage, uint16_t index){

    if(q->display_flag){
	printf("dis\n");
	    display_subframe_dci_msg(cell_usage, index);
    }
    printf("log\n");
    log_subframe_raw_dci_msg(&(q->rawLog_setting), cell_usage, index);

    return 0; 
}

/* Check whether the value of the headers are valide or not */
bool check_valid_header(uint16_t header){
    if(header < NOF_LOG_SF){
	return true;
    }else{
	return false;
    }
}

int multi_subframe_status_update(lteCCA_status_t* q, srslte_ue_cell_usage* cell_usage, uint16_t start_idx, uint16_t end_idx){
    int		nof_sf;
    uint16_t	idx;

    //transform the index 
    if(start_idx > end_idx){
	end_idx += NOF_LOG_SF;
    }

    // Calculate the number of subrames we need to update
    nof_sf	= (end_idx - start_idx);
    if(nof_sf > 100){
	printf("WARNNING: we update too many subframes at one time -> %d subframes together!\n",nof_sf);
    }

    // update the subframe status
    for(uint16_t i=start_idx+1; i<=end_idx; i++){
	idx	= TTI_TO_IDX(i);
	single_subframe_status_update(q, cell_usage, idx);
    } 

    return 0;
}

int lteCCA_status_update(lteCCA_status_t* q, srslte_ue_cell_usage* cell_status){
    bool sync_flag = cell_status->sync_flag;

    /* we update the status only when all of the cells are synchronized */
    if(sync_flag == false){
	return 1;
    }

    /* Update the status*/
    uint16_t status_header, cell_status_header;

    status_header	= q->status_header;
    cell_status_header	= cell_status->header; 
   
    /* Check whether the value of two headers are valid or not */ 
    if( (check_valid_header(status_header) == false) || 
	 (check_valid_header(cell_status_header) == false) ){
	printf("ERROR: Wrong header value -- Header must be positive integer and smaller than %d!\n", NOF_LOG_SF);
	return 1;
    }

    /* Nothing to be updated if the headers are the same*/
    if(status_header == cell_status_header){
	return 1;
    } 
    
    /* We update the status when these two headers are not the same */
    if(q->active_flag == false){
	//-> 1: update a single subframe when start
	q->active_flag = true;
	single_subframe_status_update(q, cell_status, cell_status_header);
    }else{
	//-> 2: update all the subframes between status header and cell status header
	multi_subframe_status_update(q, cell_status, status_header, cell_status_header);
    }

    /* update the status header */ 
    q->status_header   = cell_status_header;

    return 0;
}

/* init the status structure*/
static void init_rawLog_setting(lteCCA_rawLog_setting_t* q){
    q->nof_cell		= 1;
    q->nameFile_time	= false;

    q->log_dl_flag	= false;
    q->dl_single_file	= true;
    
    q->log_ul_flag	= false;
    q->ul_single_file	= true;

    for(int i=0; i<MAX_NOF_USRP; i++){
	q->log_dl_fd[i] = NULL;
	q->log_ul_fd[i] = NULL;
    }    
}

int lteCCA_status_init(lteCCA_status_t* q){
    q->status_header = 0;
    q->active_flag  = false;
    q->display_flag = true;

    init_rawLog_setting(&(q->rawLog_setting));
    return 0;
}
int lteCCA_status_exit(lteCCA_status_t* q){
    lteCCA_rawLog_setting_t* config = &(q->rawLog_setting);

    FILE* FD;
    int nof_cell    = config->nof_cell;
	for(int i=0;i<nof_cell;i++){
		if(config->log_dl_flag){
			FD = config->log_dl_fd[i];
			if( FD != NULL){
				fclose(FD);
			}
		}
    	if(config->log_ul_flag){
			FD = config->log_ul_fd[i];
			if( FD != NULL){
				fclose(FD);
			}
		}
	}
}
void lteCCA_setRawLog_all(lteCCA_status_t* q, rawLog_config_t* log_config){
    lteCCA_rawLog_setting_t* config; 
    config  = &(q->rawLog_setting);

    config->nof_cell	    = log_config->nof_cell;
    config->nameFile_time   = log_config->nameFile_time;
    config->log_dl_flag	    = log_config->log_dl_flag;
    config->log_ul_flag	    = log_config->log_ul_flag;
    config->dl_single_file  = log_config->dl_single_file;
    config->ul_single_file  = log_config->ul_single_file;
}
void lteCCA_setRawLog_log_dl_flag(lteCCA_status_t* q, bool flag){
    q->rawLog_setting.log_dl_flag   = flag; 
}
void lteCCA_setRawLog_dl_single_file(lteCCA_status_t* q, bool flag){
    q->rawLog_setting.dl_single_file  = flag; 
}
void lteCCA_setRawLog_log_ul_flag(lteCCA_status_t* q, bool flag){
    q->rawLog_setting.log_ul_flag   = flag; 
}
void lteCCA_setRawLog_ul_single_file(lteCCA_status_t* q, bool flag){
    q->rawLog_setting.ul_single_file  = flag; 
}
void lteCCA_setRawLog_nof_cell(lteCCA_status_t* q, uint16_t nof_cell){
    q->rawLog_setting.nof_cell = nof_cell; 
}

/* fill up the file descriptors 
 * NOTE: this function must be called after seting the raw log configurations */
int lteCCA_fill_fileDescriptor(lteCCA_status_t* q, usrp_config_t* usrp_config){
    struct tm *newtime;
    time_t  t1;
    t1	= time(NULL);
    newtime = localtime(&t1);
 
    lteCCA_rawLog_setting_t* config = &(q->rawLog_setting);

    char fileName[128];
    char fileName1[128];

    FILE* FD;
    int nof_cell    = config->nof_cell;
	system("mkdir ./dci_log");
    /*  Downlink dci messages file handling*/
    if(config->log_dl_flag){ 
	if(config->dl_single_file){
	    if(config->nameFile_time){
		strftime(fileName, 128, "./dci_log/dci_raw_log_dl_all_%Y_%m_%d_%H_%M_%S.dciLog",newtime);
		FD = fopen(fileName,"w+");
	    }else{
		FD = fopen("./dci_log/dci_raw_log_dl_all.dciLog","w+");
	    }
	    if(FD == NULL){
		printf("ERROR: fail to open log file!\n");
		exit(0);
	    }
	    config->log_dl_fd[0]    = FD;
	}else{ 
	    for(int i=0;i<nof_cell;i++){	
		sprintf(fileName, "./dci_log/dci_raw_log_dl_cell_rf_%lld_N_%d",
					      usrp_config[i].rf_freq, usrp_config[i].N_id_2);
		if(config->nameFile_time){
		    strftime(fileName1, 128, "_%Y_%m_%d_%H_%M_%S.dciLog",newtime);
		}else{	
		    strcpy(fileName1, ".dciLog");
		}
		strcat(fileName, fileName1);
		FD = fopen(fileName,"w+");
		if(FD == NULL){
		    printf("ERROR: fail to open log file!\n");
		    exit(0);
		}
		config->log_dl_fd[i]    = FD;
	    }
	}    
    }

    /*  Uplink dci messages file handling*/
    if(config->log_ul_flag){ 
	if(config->ul_single_file){
	    if(config->nameFile_time){
		strftime(fileName, 128, "./dci_log/dci_raw_log_ul_all_%Y_%m_%d_%H_%M_%S.dciLog",newtime);
		FD = fopen(fileName,"w+");
	    }else{	
		FD = fopen("./dci_raw_log_ul_all.dciLog","w+");
	    }
	    if(FD == NULL){
		printf("ERROR: fail to open log file!\n");
		exit(0);
	    }
	    config->log_dl_fd[0]    = FD;
	}else{ 
	    for(int i=0;i<nof_cell;i++){	
	        sprintf(fileName, "./dci_log/dci_raw_log_ul_cell_rf_%lld_N_%d",
					      usrp_config[i].rf_freq, usrp_config[i].N_id_2);
		if(config->nameFile_time){
		    strftime(fileName1, 128, "_%Y_%m_%d_%H_%M_%S.dciLog",newtime);
		}else{	
		    strcpy(fileName1, ".dciLog");
		}
		strcat(fileName, fileName1);
		FD = fopen(fileName,"w+");
		if(FD == NULL){
		    printf("ERROR: fail to open log file!\n");
		    exit(0);
		}
		config->log_ul_fd[i]    = FD;
	    }
	}    
    }
    return 0;
}
/* Update the fileDescriptor */
int lteCCA_update_fileDescriptor(lteCCA_status_t* q, usrp_config_t* usrp_config){
    struct tm *newtime;
    time_t  t1;
    t1	= time(NULL);
    newtime = localtime(&t1);
 

    lteCCA_rawLog_setting_t* config = &(q->rawLog_setting);

    char fileName[128];
    char fileName1[128];

    FILE* FD;
    int nof_cell    = config->nof_cell;

    /*  Downlink dci messages file handling*/
    if(config->log_dl_flag){ 
	if(config->dl_single_file){
	    if(config->nameFile_time){
		strftime(fileName, 128, "./dci_log/dci_raw_log_dl_all_%Y_%m_%d_%H_%M_%S.dciLog",newtime);
		FD = config->log_dl_fd[0];
		fclose(FD);
		FD = fopen(fileName,"w+");
		if(FD == NULL){
		    printf("ERROR: fail to open log file!\n");
		    exit(0);
		}
		config->log_dl_fd[0]    = FD;
	    }
	}else{ 
	    for(int i=0;i<nof_cell;i++){	
		sprintf(fileName, "./dci_log/dci_raw_log_dl_cell_rf_%lld_N_%d",
					      usrp_config[i].rf_freq, usrp_config[i].N_id_2);
		if(config->nameFile_time){
		    strftime(fileName1, 128, "_%Y_%m_%d_%H_%M_%S.dciLog",newtime);
		    strcat(fileName, fileName1);
		    FD = config->log_dl_fd[i];
		    fclose(FD);
		    FD = fopen(fileName,"w+");
		    if(FD == NULL){
			printf("ERROR: fail to open log file!\n");
			exit(0);
		    }
		    config->log_dl_fd[i]    = FD;
		}
	    }
	}    
    }

    /*  Uplink dci messages file handling*/
    if(config->log_ul_flag){ 
	if(config->ul_single_file){
	    if(config->nameFile_time){
		strftime(fileName, 128, "./dci_log/dci_raw_log_ul_all_%Y_%m_%d_%H_%M_%S.dciLog",newtime);
		FD = config->log_dl_fd[0];
		fclose(FD);
		FD = fopen(fileName,"w+");
		if(FD == NULL){
		    printf("ERROR: fail to open log file!\n");
		    exit(0);
		}
		config->log_dl_fd[0]    = FD;
	    }
	}else{ 
	    for(int i=0;i<nof_cell;i++){	
	        sprintf(fileName, "./dci_log/dci_raw_log_ul_cell_rf_%lld_N_%d",
					      usrp_config[i].rf_freq, usrp_config[i].N_id_2);
		if(config->nameFile_time){
		    strftime(fileName1, 128, "_%Y_%m_%d_%H_%M_%S.dciLog",newtime);
		    strcat(fileName, fileName1);
		    FD = config->log_dl_fd[i];
		    fclose(FD);
		    FD = fopen(fileName,"w+");
		    if(FD == NULL){
			printf("ERROR: fail to open log file!\n");
			exit(0);
		    }
		    config->log_ul_fd[i]    = FD;
		}
	    }
	}    
    }
    return 0;
}
