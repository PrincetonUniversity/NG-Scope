#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>

#include "ngscope/hdr/dciLib/status_tracker.h"
#include "ngscope/hdr/dciLib/status_plot.h"
#include "ngscope/hdr/dciLib/ngscope_def.h"
#include "ngscope/hdr/dciLib/parse_args.h"
#include "ngscope/hdr/dciLib/dci_log.h"
#include "ngscope/hdr/dciLib/time_stamp.h"
#include "ngscope/hdr/dciLib/socket.h"
#include "ngscope/hdr/dciLib/cell_status.h"
#include "ngscope/hdr/dciLib/sync_dci_remote.h"
#include "ngscope/hdr/dciLib/load_config.h"
#include "ngscope/hdr/dciLib/thread_exit.h"

#include "ngscope/hdr/dciLib/dci_sink_def.h"
#include "ngscope/hdr/dciLib/dci_sink_serv.h"
#include "ngscope/hdr/dciLib/dci_sink_sock.h"

//#define TTI_TO_IDX(i) (i%CELL_STATUS_RING_BUF_SIZE)

extern bool go_exit;

//Waiting for the rf to be ready 
extern pthread_mutex_t     cell_mutex;
extern srsran_cell_t       cell_vec[MAX_NOF_RF_DEV];

//  DCI-Decoder <--- DCI buffer ---> Status-Tracker
extern dci_ready_t                  dci_ready;
extern ngscope_status_buffer_t      dci_buffer[MAX_DCI_BUFFER];

//  Status-Tracker <--- DCI buffer ---> (DCI-Ring-Buffer -- Cell-Status-Tracker)
extern ngscope_status_buffer_t      cell_stat_buffer[MAX_DCI_BUFFER];
extern dci_ready_t               	cell_stat_ready;

//  Status-Tracker <--- DCI buffer ---> (DCI-Ring-Buffer -- DCI-Logger)
extern ngscope_status_buffer_t      log_stat_buffer[MAX_DCI_BUFFER];
extern dci_ready_t               	log_stat_ready;

pthread_cond_t      plot_cond  = PTHREAD_COND_INITIALIZER;
pthread_mutex_t     plot_mutex = PTHREAD_MUTEX_INITIALIZER;
ngscope_plot_t      plot_data;

ngscope_dci_sink_serv_t dci_sink_serv;

extern bool task_scheduler_closed[MAX_NOF_RF_DEV];

void wait_for_radio(ngscope_status_tracker_t* q, int nof_dev){
    bool radio_ready = true;
    while(true){
        if(go_exit) break;

        radio_ready = true;
        pthread_mutex_lock(&cell_mutex);
        for(int i=0; i<nof_dev; i++){
            if(cell_vec[i].nof_prb == 0){
                radio_ready = false;
                break;
            }
        }
        pthread_mutex_unlock(&cell_mutex);
        if(radio_ready){
            break;
        }else{
            usleep(10000);
        }
    }     
    pthread_mutex_lock(&cell_mutex);
    for(int i=0; i<nof_dev; i++){
        q->cell_prb[i] = cell_vec[i].nof_prb;
    }
    pthread_mutex_unlock(&cell_mutex);

    return;
}

void* status_tracker_thread(void* p){
    /* TODO log target status */
    //prog_args_t* prog_args = (prog_args_t*)p; 

	ngscope_config_t* config = (ngscope_config_t *)p;

    // Number of RF devices
    int nof_dev = config->nof_rf_dev;
    int nof_dci = 0;
    //int nof_prb = 0;
    int dis_plot = config->rf_config[0].disable_plot;

    uint16_t targetRNTI = config->rnti;
    int remote_enable   = config->remote_enable;

    printf("DIS_PLOT:%d nof_RF_DEV:%d \n", dis_plot, nof_dev);
    /* Init the status tracker */
    ngscope_status_tracker_t status_tracker;
    memset(&status_tracker, 0, sizeof(ngscope_status_tracker_t));

    /* INIT status_tracker */
    status_tracker.targetRNTI = targetRNTI;
    status_tracker.nof_cell   = nof_dev;

    // Container for store obtained csi  
    ngscope_status_buffer_t    dci_queue[MAX_DCI_BUFFER];

    /* Wait the Radio to be ready */
    wait_for_radio(&status_tracker, nof_dev);

	log_config_t dci_log_config;
	memcpy(&dci_log_config.config, config, sizeof(ngscope_config_t));

	uint16_t cell_prb[MAX_NOF_RF_DEV];	
	cell_config_t 	cell_config;

	cell_config.nof_cell 	= nof_dev;
	cell_config.rnti 		= targetRNTI;

	for(int i=0; i<nof_dev; i++){
		cell_prb[i] = status_tracker.cell_prb[i];
		cell_config.cell_prb[i] = status_tracker.cell_prb[i];
		dci_log_config.cell_prb[i] = status_tracker.cell_prb[i];
	}

	// remote can be any program on the same device or 
    // program running on other devices
    // We sync the decoded DCI with the remote ones
    if(remote_enable){    
		// init the dci_sink that stores all the client 
		sock_init_dci_sink(&dci_sink_serv, 6666);

		status_tracker.remote_sock = 1;

		pthread_t 	dci_sink_thd;
    	pthread_create(&dci_sink_thd, NULL, dci_sink_server_thread, (void*)(&cell_config));
    }

    printf("\n\n\n Radio is ready! \n\n"); 

	/* Create the cell status tracking thread */
    pthread_t 			cell_stat_thd;
	cell_status_info_t 	info;

	info.targetRNTI 	= targetRNTI;
	info.nof_cell 		= nof_dev;
	info.remote_sock 	= status_tracker.remote_sock;
	info.remote_enable 	= remote_enable;

	memcpy(info.cell_prb, status_tracker.cell_prb, nof_dev * sizeof(int));
    pthread_create(&cell_stat_thd, NULL, cell_status_thread, (void*)(&info));

	/* create the dci logging thread */
	pthread_t 	dci_log_thd;	
	if(ngscope_config_check_log(config)){
    	pthread_create(&dci_log_thd, NULL, dci_log_thread, (void*)(&dci_log_config));
	}

	FILE* fd = fopen("status_tracker.txt","w+");

    while(true){
        if(go_exit) break;
        // reset the dci queue 
        memset(dci_queue, 0, MAX_DCI_BUFFER * sizeof(ngscope_status_buffer_t));

		/* Data handling between DCI decoder and StatusTracker 
		 *  DCI-Decoder--> dci--> dci_buffer --> Status-Tracker */
        pthread_mutex_lock(&dci_ready.mutex);
        // Use while in case some corner case conditional wake up
        while(dci_ready.nof_dci <=0){
            pthread_cond_wait(&dci_ready.cond, &dci_ready.mutex);
        }
        // copy data from the dci buffer
        nof_dci     = dci_ready.nof_dci;
        memcpy(dci_queue, dci_buffer, nof_dci * sizeof(ngscope_status_buffer_t));

        // clean the dci buffer 
        memset(dci_buffer, 0, nof_dci * sizeof(ngscope_status_buffer_t));

        // reset the dci buffer
        dci_ready.header    = 0;
        dci_ready.nof_dci   = 0;
        pthread_mutex_unlock(&dci_ready.mutex);

        /* put the dci into the cell status buffer, later, the cell status handler will handle it
		 *  Status-Tracker -->  cell_stat_buffer --> Cell-Status-Tracker */
        pthread_mutex_lock(&cell_stat_ready.mutex);
		for(int i=0; i<nof_dci; i++){
			cell_stat_buffer[cell_stat_ready.header].dci_per_sub = dci_queue[i].dci_per_sub;
			cell_stat_buffer[cell_stat_ready.header].tti 		 = dci_queue[i].tti;
			cell_stat_buffer[cell_stat_ready.header].cell_idx 	 = dci_queue[i].cell_idx;

			cell_stat_ready.header = (cell_stat_ready.header + 1) % MAX_DCI_BUFFER;
			if(cell_stat_ready.nof_dci < MAX_DCI_BUFFER){
				cell_stat_ready.nof_dci++;
			}
		}
        //printf("TTI :%d ul_dci: %d dl_dci:%d nof_dci:%d\n", dci_ret.tti, dci_per_sub.nof_ul_dci, 
        //                                        dci_per_sub.nof_dl_dci, dci_ready.nof_dci);
        pthread_cond_signal(&cell_stat_ready.cond);
        pthread_mutex_unlock(&cell_stat_ready.mutex);
 
        /* put the dci into the log status buffer, later, the DCI logger will handle it
		 *  Status-Tracker -->  log_stat_buffer --> DCI-Logger */
        pthread_mutex_lock(&log_stat_ready.mutex);
		for(int i=0; i<nof_dci; i++){
			log_stat_buffer[log_stat_ready.header].dci_per_sub 	= dci_queue[i].dci_per_sub;
			log_stat_buffer[log_stat_ready.header].tti 		 	= dci_queue[i].tti;
			log_stat_buffer[log_stat_ready.header].cell_idx  	= dci_queue[i].cell_idx;

			log_stat_ready.header = (log_stat_ready.header + 1) % MAX_DCI_BUFFER;
			if(log_stat_ready.nof_dci < MAX_DCI_BUFFER){
				log_stat_ready.nof_dci++;
			}
		}
        //printf("TTI :%d ul_dci: %d dl_dci:%d nof_dci:%d\n", dci_ret.tti, dci_per_sub.nof_ul_dci, 
        //                                        dci_per_sub.nof_dl_dci, dci_ready.nof_dci);
        pthread_cond_signal(&log_stat_ready.cond);
        pthread_mutex_unlock(&log_stat_ready.mutex);
     
        //printf("Copy %d dci->", nof_dci); 
        for(int i=0; i<nof_dci; i++){
			fprintf(fd, "%d\t%d\n", dci_queue[i].tti, nof_dci);
            //printf(" %d-th dci: ul dci:%d dl_dci:%d  tti:%d IDX:%d\n", i, dci_queue[i].dci_per_sub.nof_ul_dci, \
            dci_queue[i].dci_per_sub.nof_dl_dci, dci_queue[i].tti, TTI_TO_IDX(dci_queue[i].tti));
        }
		//printf("\n");
    }
    printf("Close Status Tracker!\n");
	fclose(fd);    
	//close_and_notify_udp(status_tracker.remote_sock);
//
//    if(dis_plot == 0){
//        if (!pthread_kill(plot_thread, 0)) {
//          pthread_kill(plot_thread, SIGHUP);
//          pthread_join(plot_thread, NULL);
//        }
//    }
 	wait_for_ALL_RF_DEV_close();        

	// Wait for the cell status tracking thread to end
	pthread_join(cell_stat_thd, NULL);

	// Wait for the dci log thread to end
	if(ngscope_config_check_log(config)){
		pthread_join(dci_log_thd, NULL);
	}
    
    // close the remote socket
    if(status_tracker.remote_sock > 0){
        close(status_tracker.remote_sock);
    }

    //status_tracker_print_ue_freq(&status_tracker);

    printf("Status Tracker CLOSED!\n");
    return NULL;
}
