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


#include "ngscope/hdr/dciLib/thread_exit.h"
extern bool dci_decoder_up[MAX_NOF_RF_DEV][MAX_NOF_DCI_DECODER];
extern bool task_scheduler_up[MAX_NOF_RF_DEV];

//bool dci_decoder_closed[MAX_NOF_RF_DEV][MAX_NOF_DCI_DECODER] = {{true}};
extern bool task_scheduler_closed[MAX_NOF_RF_DEV];
extern pthread_mutex_t     scheduler_close_mutex;

bool wait_for_ALL_RF_DEV_close(){
	while(true){
		bool ret = true;
		// wait until the task schedulers of all RF devs are closed
    	pthread_mutex_lock(&scheduler_close_mutex);
		for(int i=0; i<MAX_NOF_RF_DEV; i++){
			if(task_scheduler_closed[i]==false){
				ret = false;
			}
		}
    	pthread_mutex_unlock(&scheduler_close_mutex);
		usleep(10);
		if(ret) break;
	}
	return true;
}


bool wait_for_scheduler_ready_to_close(int rf_idx){
	bool ret = false;
	while(!ret){
		ret = true;
    	pthread_mutex_lock(&scheduler_close_mutex);
		for(int i=rf_idx+1; i<MAX_NOF_RF_DEV; i++){
			if(task_scheduler_closed[i] == false){
				ret = false;
			}
		}
    	pthread_mutex_unlock(&scheduler_close_mutex);
		usleep(10);
	}
	return true;
}


bool wait_for_decoder_ready_to_close(int  rf_idx, int  decoder_idx){
	bool ret = false;
	while(!ret){
		ret = true;
		// the task scheduler must be ready to close
		if(task_scheduler_up[rf_idx] == false){
			// its our turn to close to dci decoder
			for(int i=decoder_idx+1; i<MAX_NOF_DCI_DECODER; i++){
				if(dci_decoder_up[rf_idx][i] == true){
					ret = false;
				}
			}
		}else{
			ret = false;
		}
		usleep(10);
	}
	return true;
}
