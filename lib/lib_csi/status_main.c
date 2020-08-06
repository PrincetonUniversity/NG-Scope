#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
 
#include "srslte/phy/ue/lte_scope.h"
#include "srslte/phy/ue/LTESCOPE_GLOBAL.h"

#include "status_main.h"

#define TTI_TO_IDX(i) (i%NOF_LOG_SF)

extern bool go_exit;
extern pthread_mutex_t mutex_exit;

extern bool rf_ready[MAX_NOF_USRP];
extern bool all_RF_ready;
extern pthread_mutex_t mutex_RF_ready;

extern int  cell_prb_G[MAX_NOF_USRP];
extern int  nof_tx_ant_G[MAX_NOF_USRP];
extern pthread_mutex_t mutex_RF_config;


static bool check_parm_flag(lteCCA_rawLog_setting_t* q){
    bool all_check = true;
    for(int i=0;i<q->nof_cell;i++){
	if( (q->tx_ant_flag[i] == false) || (q->prb_flag[i] == false)){
	    all_check = false;
	    break;
	}
    }
    return all_check; 
}

static void log_single_subframe_csi(srslte_ue_cell_usage* q, lteCCA_rawLog_setting_t* config, uint16_t index){
    // first we check whether we need to forward CSI or not
     
    // return immediately if we don't need to record anything
    if(config->log_amp_flag == false && config->log_phase_flag == false
	    && (config->forward_flag != 1)){
	printf("all flag false return immediately!forward_flag:%d\n", config->forward_flag);
	return;
    }

    // we need all the tx antennas are all set
    pthread_mutex_lock(&mutex_RF_ready);
    if(all_RF_ready == false){
	pthread_mutex_unlock(&mutex_RF_ready);
	printf("RF not ready return immediately!\n");
	return;
    }
    pthread_mutex_unlock(&mutex_RF_ready);

    int		nof_cell = q->nof_cell;
    uint16_t	tti;

    FILE*	FD_amp; 
    FILE*	FD_phase;

    float	*csi_amp;
    float	*csi_phase;

    uint16_t    nof_prb;	// The max PRB each carrier has
    uint16_t    nof_tx_ant;    // Tx antenna number 
    uint16_t    nof_rx_ant;    // Rx antenna number 

    uint16_t    *nof_prb_v;	// The max PRB each carrier has
    uint16_t    *nof_tx_ant_v;    // Tx antenna number 
    uint16_t    *nof_rx_ant_v;    // Rx antenna number 

    uint16_t	file_format;

    file_format	    = config->format;

    nof_prb_v	    = q->max_cell_prb;
    nof_tx_ant_v    = q->nof_tx_ant;
    nof_rx_ant_v    = q->nof_rx_ant;

    int ds_subcarrier	= config->down_sample_subcarrier;
    int ds_subframe	= config->down_sample_subframe;

    
    srslte_subframe_status* sf_stat;
    
    if( config->log_amp_flag || config->log_phase_flag){
	for(int cell_idx=0;cell_idx<nof_cell;cell_idx++){
	    sf_stat	    = &(q->cell_status[cell_idx].sf_status[index]);
	    tti	    = sf_stat->tti;

	    // down sample the subframe 
	    if( tti%ds_subframe == 0 ){
		FD_amp	    = config->log_amp_fd[cell_idx];
		FD_phase    = config->log_phase_fd[cell_idx];

		nof_prb	    = nof_prb_v[cell_idx];
		nof_tx_ant  = nof_tx_ant_v[cell_idx];
		nof_rx_ant  = nof_rx_ant_v[cell_idx];

		for(int i=0;i<nof_tx_ant;i++){
		    for(int j=0;j<nof_rx_ant;j++){
			csi_amp	    = sf_stat->csi_amp[i][j];
			csi_phase   = sf_stat->csi_phase[i][j];

			if(config->log_amp_flag){
			    for(int k=0;k<nof_prb*12;k++){
				if( k%ds_subcarrier == 0 ){
				    switch(file_format){
					case 0:
					    fprintf(FD_amp,   "%f\t",csi_amp[k]);
					    break;
					case 1:
					    fprintf(FD_amp,   "%f,",csi_amp[k]);
					    break;
					default:
					    printf("UNKNOWN file format!\n");
					    exit(0);
				    }
				}
			    }
			    fprintf(FD_amp,"\n");
			}

			if(config->log_phase_flag){
			    for(int k=0;k<nof_prb*12;k++){
				if( k%ds_subcarrier == 0 ){
				    switch(file_format){
					case 0:
					    fprintf(FD_phase, "%f\t",csi_phase[k]);
					    break;
					case 1:
					    fprintf(FD_phase, "%f,",csi_phase[k]);
					    break;
					default:
					    printf("UNKNOWN file format!\n");
					    exit(0);
				    }
				}
			    }
			    fprintf(FD_phase,"\n");
			}
		    }
		}
	    }
	}
    }

    if(config->forward_flag == 1){
	//printf("\n\n We forward CSI\n");

	float buffer[350]; // it takes 1400 bytes to store 350 float number
	int   count = 0;
    	for(int cell_idx=0;cell_idx<nof_cell;cell_idx++){
	    sf_stat	= &(q->cell_status[cell_idx].sf_status[index]);
	    tti		= sf_stat->tti;

	    // down sample the subframe 
	    if( tti%ds_subframe == 0 ){
		nof_prb	    = nof_prb_v[cell_idx];
		nof_tx_ant  = nof_tx_ant_v[cell_idx];
		nof_rx_ant  = nof_rx_ant_v[cell_idx];
		for(int i=0;i<nof_tx_ant;i++){
		    for(int j=0;j<nof_rx_ant;j++){
			csi_amp	    = sf_stat->csi_amp[i][j];
			csi_phase   = sf_stat->csi_phase[i][j];
			for(int k=0;k<nof_prb*12;k++){
			    if( k%ds_subcarrier == 0 ){
				buffer[count] = csi_amp[k];
				count++;
				if(count % 350 == 0){
				    //printf("forward 350 byte!\n");
				    send(config->forward_sock, buffer, 350*sizeof(float), 0);
				}
				count = count % 350;
			    }
			}
			for(int k=0;k<nof_prb*12;k++){
			    if( k%ds_subcarrier == 0 ){
				buffer[count] = csi_phase[k];
				count++;
				if(count % 350 == 0){
				    //printf("forward 350 byte!\n");
				    send(config->forward_sock, buffer, 350*sizeof(float), 0);
				}
				count = count % 350;
			    }
			}
		    }
		}
	    }
	}	
	if(count > 0){
	    send(config->forward_sock, buffer, count*sizeof(float), 0);
	}
    }
    return;
}

int single_subframe_status_update(lteCCA_status_t* q, srslte_ue_cell_usage* cell_usage, uint16_t index){

    log_single_subframe_csi(cell_usage, &(q->rawLog_setting), index);

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

    q->forward_flag	= false;
    q->forward_sock	= 0;
    q->format		= 0;

    q->nof_cell		= 1;

    q->log_amp_flag	= false;
    
    q->log_phase_flag	= false;
    
    q->down_sample_subcarrier	= 1;	// 1 means no downsample
    q->down_sample_subframe	= 1;	// 1 means no downsample

    for(int i=0; i<MAX_NOF_USRP; i++){
	q->nof_prb[i]	    = 0;
	q->prb_flag[i]	    = 0;
    
	q->nof_tx_ant[i]    = 0;
	q->tx_ant_flag[i]   = false;

	q->nof_rx_ant[i]    = 0;

	q->log_amp_fd[i] = NULL;
	q->log_phase_fd[i] = NULL;
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

    if(config->log_amp_flag){ 
	for(int i=0;i<nof_cell;i++){	
	    FD = config->log_amp_fd[i];
	    if(	FD != NULL){
		fclose(FD);
	    }
	}
    }
    
    if(config->log_phase_flag){ 
	for(int i=0;i<nof_cell;i++){	
	    FD = config->log_phase_fd[i];
	    if(	FD != NULL){
		fclose(FD);
	    }
	}
    }
    // close the remote demo PC
    if(config->forward_flag){
	char output[100];
	for(int i=0;i<10;i++){
	    output[i] = 0xFF;
	}
	send(config->forward_sock, output, 10, 0);
    }
    return 0;
}
int lteCCA_forward_init(lteCCA_status_t* q, csi_forward_config_t* forward_config){
    if(forward_config->forward_flag == 1){
	printf("\n\n\n\n Connecting to server!\n");
	char masterIP[100];
	strcpy(masterIP, forward_config->remoteIP);		

	int client_sockfd;
	struct sockaddr_in remote_addr; //服务器端网络地址结构体
	memset(&remote_addr,0,sizeof(remote_addr)); //数据初始化--清零
	remote_addr.sin_family=AF_INET; //设置为IP通信

	if(masterIP == NULL){
	    remote_addr.sin_addr.s_addr=inet_addr("127.0.0.1");//服务器IP地址
	}else{
	    remote_addr.sin_addr.s_addr=inet_addr( masterIP );//服务器IP地址
	}
	remote_addr.sin_port=htons(6767); //服务器端口号

	/*创建客户端套接字--IPv4协议，面向连接通信，TCP协议*/
	if((client_sockfd=socket(PF_INET,SOCK_STREAM,0))<0)
	{
	    printf("socket error\n\n\n\n");
	    return 0;
	}

	/*将套接字绑定到服务器的网络地址上*/
	if(connect(client_sockfd,(struct sockaddr *)&remote_addr,sizeof(struct sockaddr))<0)
	{
	    printf(" connect error\n\n\n\n");
	    return 0;
	}else{
	    printf("Connection with server succeed!\n\n\n\n");
	}
	    
	q->rawLog_setting.forward_flag = true;
	q->rawLog_setting.forward_sock = client_sockfd;
    }else{
	printf("\n\n\n\n We don't forward CSI\n\n\n\n");
    }

    return 1;
} 
void lteCCA_setRawLog_all(lteCCA_status_t* q, csiLog_config_t* log_config){
    lteCCA_rawLog_setting_t* config; 
    config  = &(q->rawLog_setting);
    
    config->format	    = log_config->format;
    config->nof_cell	    = log_config->nof_cell;
    config->log_amp_flag    = log_config->log_amp_flag;
    config->log_phase_flag  = log_config->log_phase_flag;

    config->down_sample_subcarrier  = log_config->down_sample_subcarrier;
    config->down_sample_subframe    = log_config->down_sample_subframe;

    for(int i=0;i<config->nof_cell;i++){
	config->nof_rx_ant[i]	= log_config->rx_ant[i];
    }
}

void lteCCA_setRawLog_prb(lteCCA_status_t* q, uint16_t prb, int cell_idx){
    q->rawLog_setting.nof_prb[cell_idx]		= prb;
    q->rawLog_setting.prb_flag[cell_idx]	= true;
}
void lteCCA_setRawLog_txAnt(lteCCA_status_t* q, uint16_t tx_ant, int cell_idx){
    q->rawLog_setting.nof_tx_ant[cell_idx]	= tx_ant;
    q->rawLog_setting.tx_ant_flag[cell_idx]	= true;
}
void lteCCA_setRawLog_rxAnt(lteCCA_status_t* q, uint16_t rx_ant, int cell_idx){
    q->rawLog_setting.nof_rx_ant[cell_idx]	= rx_ant;
}


void lteCCA_setRawLog_log_amp_flag(lteCCA_status_t* q, bool flag){
    q->rawLog_setting.log_amp_flag   = flag; 
}

void lteCCA_setRawLog_log_phase_flag(lteCCA_status_t* q, bool flag){
    q->rawLog_setting.log_phase_flag   = flag; 
}

void lteCCA_setRawLog_nof_cell(lteCCA_status_t* q, uint16_t nof_cell){
    q->rawLog_setting.nof_cell = nof_cell; 
}

/* fill up the file descriptors 
 * NOTE: this function must be called after seting the raw log configurations */
int lteCCA_fill_fileDescriptor(lteCCA_status_t* q, usrp_config_t* usrp_config, csiLog_config_t* csi_config){
    struct tm *newtime;
    time_t  t1;
    t1	= time(NULL);
    newtime = localtime(&t1);
 
    lteCCA_rawLog_setting_t* config = &(q->rawLog_setting);

    bool    RF_ready;

    char fileName[128];
    char fileName1[128];

    FILE* FD;
    int nof_cell;
    int *nof_prb;
    int *nof_tx_ant;
    int *nof_rx_ant;

    lteCCA_setRawLog_all(q, csi_config);
    config = &(q->rawLog_setting);

    nof_cell	= config->nof_cell;

    // We need to set all the parameters, but some parameters isnot set until the RF is ready   
    // so we need to wait until all the necessary parameters are set  
    while(true){
	pthread_mutex_lock( &mutex_exit);
        if(go_exit){
            pthread_mutex_unlock( &mutex_exit);
            break;
        }
        pthread_mutex_unlock( &mutex_exit);
	
	pthread_mutex_lock(&mutex_RF_ready);
	RF_ready = all_RF_ready;
	pthread_mutex_unlock(&mutex_RF_ready);

	if(RF_ready == false){
	    printf("RF not ready, sleep and wait!\n");
	    sleep(1); // wait if the parameters are not ready yet (RF is not fully started)
	}else{
	    char buffer[200];
	    for(int i=0;i<100;i++){
		buffer[i] = 0xAA;
	    }
	    buffer[100] = nof_cell & 0xFF;
	    buffer[101] = q->rawLog_setting.down_sample_subcarrier & 0xFF;
	    buffer[102] = q->rawLog_setting.down_sample_subframe & 0xFF;
	    int offset	= 103;
	    // set tx antennas and cell prbs when the parameters are ready   
	    for(int i=0;i<nof_cell;i++){
		int tmp	    = cell_prb_G[i];
		buffer[offset]	= tmp & 0xFF;
		offset++;

		tmp  = nof_tx_ant_G[i];
		buffer[offset]	= tmp & 0xFF;
		offset++;

		tmp  = q->rawLog_setting.nof_rx_ant[i];
		buffer[offset]	= tmp & 0xFF;
		offset++;
		

		lteCCA_setRawLog_prb(q, cell_prb_G[i], i); 
		lteCCA_setRawLog_txAnt(q, nof_tx_ant_G[i], i);
	    }
	    send(q->rawLog_setting.forward_sock, buffer, offset+1, 0);

	    break;
	}
    }
    
    nof_prb	= q->rawLog_setting.nof_prb;
    nof_tx_ant	= q->rawLog_setting.nof_tx_ant;
    nof_rx_ant	= q->rawLog_setting.nof_rx_ant;
    
    printf("RF READY: open files!\n");
    /*  Downlink dci messages file handling*/
    if(config->log_amp_flag){ 
	for(int i=0;i<nof_cell;i++){	
	    sprintf(fileName, "./csi_amp_usrpIdx_%d_freq_%lld_N_%d_PRB_%d_TX_%d_RX_%d",
		    i, usrp_config[i].rf_freq, usrp_config[i].N_id_2, nof_prb[i], nof_tx_ant[i], nof_rx_ant[i]);
	    strftime(fileName1, 128, "_%Y_%m_%d_%H_%M_%S.csiLog",newtime);
	    strcat(fileName, fileName1);
	    FD = fopen(fileName,"w+");
	    if(FD == NULL){
		printf("ERROR: fail to open log file!\n");
		exit(0);
	    }
	    config->log_amp_fd[i]    = FD;
	}
    }

    /*  Downlink dci messages file handling*/
    if(config->log_phase_flag){ 
	for(int i=0;i<nof_cell;i++){	
	    sprintf(fileName, "./csi_phase_usrpIdx_%d_freq_%lld_N_%d_PRB_%d_TX_%d_RX_%d",
		    i, usrp_config[i].rf_freq, usrp_config[i].N_id_2, nof_prb[i], nof_tx_ant[i], nof_rx_ant[i]);
	    strftime(fileName1, 128, "_%Y_%m_%d_%H_%M_%S.csiLog",newtime);
	    strcat(fileName, fileName1);
	    FD = fopen(fileName,"w+");
	    if(FD == NULL){
		printf("ERROR: fail to open log file!\n");
		exit(0);
	    }
	    config->log_phase_fd[i]    = FD;
	}
    }
    return 0;
}

void clear_char_array(char* array, int size){
    for(int i=0;i<size;i++){
	array[i] = '\0';
    }
}
int lteCCA_fill_fileDescriptor_folder(lteCCA_status_t* q, usrp_config_t* usrp_config, csiLog_config_t* csi_config){
    struct tm *newtime;
    time_t  t1;
    t1	= time(NULL);
    newtime = localtime(&t1);
 
    lteCCA_rawLog_setting_t* config = &(q->rawLog_setting);

    bool    RF_ready;

    char fileName[128];
    char fileName1[128];

    FILE* FD;
    int nof_cell;
    int *nof_prb;
    int *nof_tx_ant;
    int *nof_rx_ant;

    lteCCA_setRawLog_all(q, csi_config);
    config = &(q->rawLog_setting);

    nof_cell	= config->nof_cell;

    // We need to set all the parameters, but some parameters isnot set until the RF is ready   
    // so we need to wait until all the necessary parameters are set  
    while(true){
	pthread_mutex_lock( &mutex_exit);
        if(go_exit){
            pthread_mutex_unlock( &mutex_exit);
            break;
        }
        pthread_mutex_unlock( &mutex_exit);
	
	pthread_mutex_lock(&mutex_RF_ready);
	RF_ready = all_RF_ready;
	pthread_mutex_unlock(&mutex_RF_ready);

	if(RF_ready == false){
	    printf("RF not ready, sleep and wait!\n");
	    sleep(1); // wait if the parameters are not ready yet (RF is not fully started)
	}else{
	    char buffer[200];
	    for(int i=0;i<100;i++){
		buffer[i] = 0xAA;
	    }
	    buffer[100] = nof_cell & 0xFF;
	    buffer[101] = q->rawLog_setting.down_sample_subcarrier & 0xFF;
	    buffer[102] = q->rawLog_setting.down_sample_subframe & 0xFF;
	    int offset	= 103;
	    // set tx antennas and cell prbs when the parameters are ready   
	    for(int i=0;i<nof_cell;i++){
		int tmp	    = cell_prb_G[i];
		buffer[offset]	= tmp & 0xFF;
		offset++;

		tmp  = nof_tx_ant_G[i];
		buffer[offset]	= tmp & 0xFF;
		offset++;

		tmp  = q->rawLog_setting.nof_rx_ant[i];
		buffer[offset]	= tmp & 0xFF;
		offset++;

		lteCCA_setRawLog_prb(q, cell_prb_G[i], i); 
		lteCCA_setRawLog_txAnt(q, nof_tx_ant_G[i], i);
	    }
	    send(q->rawLog_setting.forward_sock, buffer, offset+1, 0);

	    break;
	}
    }
    
    nof_prb	= q->rawLog_setting.nof_prb;
    nof_tx_ant	= q->rawLog_setting.nof_tx_ant;
    nof_rx_ant	= q->rawLog_setting.nof_rx_ant;
    
    printf("RF READY: open files!\n");


    /* we open the folder for storing the csi logs*/    
    char folderName[128];
    char tmp[128];
    strcpy(folderName,"mkdir ");
    strftime(tmp, 128, "./csi_log_%Y_%m_%d_%H_%M_%S",newtime);
    strcat(folderName, tmp);
    system(folderName);

    /*  Downlink dci messages file handling*/
    if(config->log_amp_flag){ 
	for(int i=0;i<nof_cell;i++){	
	    sprintf(fileName1, "/csi_amp_usrpIdx_%d_freq_%lld_N_%d_PRB_%d_TX_%d_RX_%d.csiLog",
		    i, usrp_config[i].rf_freq, usrp_config[i].N_id_2, nof_prb[i], nof_tx_ant[i], nof_rx_ant[i]);

	    strcpy(fileName, tmp);
	    strcat(fileName, fileName1);
	    FD = fopen(fileName,"w+");
	    clear_char_array(fileName, 128);

	    if(FD == NULL){
		printf("ERROR: fail to open log file!\n");
		exit(0);
	    }
	    config->log_amp_fd[i]    = FD;
	}
    }

    /*  Downlink dci messages file handling*/
    if(config->log_phase_flag){ 
	for(int i=0;i<nof_cell;i++){	
	    sprintf(fileName1, "/csi_phase_usrpIdx_%d_freq_%lld_N_%d_PRB_%d_TX_%d_RX_%d.csiLog",
		    i, usrp_config[i].rf_freq, usrp_config[i].N_id_2, nof_prb[i], nof_tx_ant[i], nof_rx_ant[i]);

	    strcpy(fileName, tmp);
	    strcat(fileName, fileName1);
	    FD = fopen(fileName,"w+");
	    clear_char_array(fileName, 128);

	    if(FD == NULL){
		printf("ERROR: fail to open log file!\n");
		exit(0);
	    }
	    config->log_phase_fd[i]    = FD;
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

    int *nof_prb    = q->rawLog_setting.nof_prb;
    int *nof_tx_ant = q->rawLog_setting.nof_tx_ant;
    int *nof_rx_ant = q->rawLog_setting.nof_rx_ant;
 
    // wait until all the tx antennas are set  
    while(true){
	if(check_parm_flag(&(q->rawLog_setting)) == false){
	    sleep(1);
	}else{
	    break;
	}	
    }

    /*  Downlink dci messages file handling*/
    if(config->log_amp_flag){ 
	
	for(int i=0;i<nof_cell;i++){	
	    // Close old file 
	    FD	    = config->log_amp_fd[i];
	    fclose(FD);
	
	    // open new file
	    sprintf(fileName, "./csi_amp_usrpIdx_%d_freq_%lld_N_%d_PRB_%d_TX_%d_RX_%d",
		    i, usrp_config[i].rf_freq, usrp_config[i].N_id_2, nof_prb[i], nof_tx_ant[i], nof_rx_ant[i]);

	    strftime(fileName1, 128, "_%Y_%m_%d_%H_%M_%S.csiLog",newtime);
	    strcat(fileName, fileName1);
	    FD = fopen(fileName,"w+");
	    if(FD == NULL){
		printf("ERROR: fail to open log file!\n");
		exit(0);
	    }
	    config->log_amp_fd[i]    = FD;
	}
    }

    /*  Downlink dci messages file handling*/
    if(config->log_phase_flag){ 
	for(int i=0;i<nof_cell;i++){	
	    // Close old file 
	    FD	    = config->log_phase_fd[i];
	    fclose(FD);

	    // open new file
	    sprintf(fileName, "./csi_phase_usrpIdx_%d_freq_%lld_N_%d_PRB_%d_TX_%d_RX_%d",
		    i, usrp_config[i].rf_freq, usrp_config[i].N_id_2, nof_prb[i], nof_tx_ant[i], nof_rx_ant[i]);
	    strftime(fileName1, 128, "_%Y_%m_%d_%H_%M_%S.csiLog",newtime);
	    strcat(fileName, fileName1);
	    FD = fopen(fileName,"w+");
	    if(FD == NULL){
		printf("ERROR: fail to open log file!\n");
		exit(0);
	    }
	    config->log_phase_fd[i]    = FD;
	}
    }
    return 0;
}

/* Update the fileDescriptor */
int lteCCA_update_fileDescriptor_folder(lteCCA_status_t* q, usrp_config_t* usrp_config){
    struct tm *newtime;
    time_t  t1;
    t1	= time(NULL);
    newtime = localtime(&t1);
 

    lteCCA_rawLog_setting_t* config = &(q->rawLog_setting);

    char fileName[128];
    char fileName1[128];

    FILE* FD;
    int nof_cell    = config->nof_cell;

    int *nof_prb    = q->rawLog_setting.nof_prb;
    int *nof_tx_ant = q->rawLog_setting.nof_tx_ant;
    int *nof_rx_ant = q->rawLog_setting.nof_rx_ant;
 
    // wait until all the tx antennas are set  
    while(true){
	if(check_parm_flag(&(q->rawLog_setting)) == false){
	    sleep(1);
	}else{
	    break;
	}	
    }

    /* we open the folder for storing the csi logs*/    
    char folderName[128];
    char tmp[128];
    strcpy(folderName,"mkdir ");
    strftime(tmp, 128, "./csi_log_%Y_%m_%d_%H_%M_%S",newtime);
    strcat(folderName, tmp);
    system(folderName);

    /*  Downlink dci messages file handling*/
    if(config->log_amp_flag){ 
	
	for(int i=0;i<nof_cell;i++){	
	    // Close old file 
	    FD	    = config->log_amp_fd[i];
	    fclose(FD);
	
	    // open new file
	    sprintf(fileName1, "/csi_amp_usrpIdx_%d_freq_%lld_N_%d_PRB_%d_TX_%d_RX_%d.csiLog",
		    i, usrp_config[i].rf_freq, usrp_config[i].N_id_2, nof_prb[i], nof_tx_ant[i], nof_rx_ant[i]);
	    strcpy(fileName, tmp);
	    strcat(fileName, fileName1);
	    FD = fopen(fileName,"w+");
	    clear_char_array(fileName, 128);
	    if(FD == NULL){
		printf("ERROR: fail to open log file!\n");
		exit(0);
	    }
	    config->log_amp_fd[i]    = FD;
	}
    }

    /*  Downlink dci messages file handling*/
    if(config->log_phase_flag){ 
	for(int i=0;i<nof_cell;i++){	
	    // Close old file 
	    FD	    = config->log_phase_fd[i];
	    fclose(FD);

	    // open new file
	    sprintf(fileName1, "/csi_phase_usrpIdx_%d_freq_%lld_N_%d_PRB_%d_TX_%d_RX_%d.csiLog",
		    i, usrp_config[i].rf_freq, usrp_config[i].N_id_2, nof_prb[i], nof_tx_ant[i], nof_rx_ant[i]);
	    strcpy(fileName, tmp);
	    strcat(fileName, fileName1);
	    FD = fopen(fileName,"w+");
	    clear_char_array(fileName, 128);

	    if(FD == NULL){
		printf("ERROR: fail to open log file!\n");
		exit(0);
	    }
	    config->log_phase_fd[i]    = FD;
	}
    }
    return 0;
}
