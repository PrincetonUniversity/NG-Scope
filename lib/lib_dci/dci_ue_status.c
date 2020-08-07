#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>

#include "time_period.h"

#include "status_main.h"
#include "ue_cell_status.h"
#include "dci_ue_status.h"

extern bool go_exit;

srslte_config_t main_config;            // configuration (which is stored inside the configuration file .cfg)

extern srslte_ue_cell_usage ue_cell_usage;  // CELL ue usage 
extern lteCCA_status_t	    ue_status_t;    // ue status

extern pthread_mutex_t mutex_exit;
extern pthread_mutex_t mutex_usage;

void* dci_ue_status_update(void* p){
    int thd_idx	    = *(int *)p;
	uint32_t 	start_time_ms 	= timestamp_ms();	
    printf("dci ue status thd idx:%d\n", thd_idx);

    /* init the status structure */
    lteCCA_status_init(&ue_status_t);

    /* set the raw logging configurations */
    lteCCA_setRawLog_all(&ue_status_t, &(main_config.rawLog_config)); 

    /* fill in the fileDescriptors according to the raw log configurations */
    lteCCA_fill_fileDescriptor(&ue_status_t, main_config.usrp_config);

	int repeat_flag 		= main_config.rawLog_config.repeat_flag;
	int repeat_log_time_ms 	= main_config.rawLog_config.repeat_log_time_ms;
	int total_log_time_ms 	= main_config.rawLog_config.total_log_time_ms;

    /* enter the main loop */
	if(repeat_flag == 1){
		// We need to repeat the recording 
		while(true){
			pthread_mutex_lock( &mutex_exit);
			if(go_exit){
				pthread_mutex_unlock( &mutex_exit);
				break;
			}
			pthread_mutex_unlock( &mutex_exit);
			while(true){
				// log for repeat_log_time_ms
				pthread_mutex_lock( &mutex_exit);
				if(go_exit){
					pthread_mutex_unlock( &mutex_exit);
					break;
				}
				pthread_mutex_unlock( &mutex_exit);
			
				pthread_mutex_lock( &mutex_usage);
				lteCCA_status_update(&ue_status_t, &ue_cell_usage);
				pthread_mutex_unlock( &mutex_usage);

				uint32_t curr_time_ms   = timestamp_ms();
				uint32_t time_elapse_ms = curr_time_ms - start_time_ms;
				if(time_elapse_ms >= repeat_log_time_ms){
					break;
				}
				/*Check the status every 0.5ms*/
				usleep(5e2);
			}
			lteCCA_update_fileDescriptor(&ue_status_t, main_config.usrp_config);
		}
	}else{
		if(total_log_time_ms > 0){
			// we will automatically stop after the total log time
			while(true){
				pthread_mutex_lock( &mutex_exit);
				if(go_exit){
					pthread_mutex_unlock( &mutex_exit);
					break;
				}
				pthread_mutex_unlock( &mutex_exit);
			
				pthread_mutex_lock( &mutex_usage);
				lteCCA_status_update(&ue_status_t, &ue_cell_usage);
				pthread_mutex_unlock( &mutex_usage);
				uint32_t curr_time_ms   = timestamp_ms();
				uint32_t time_elapse_ms = curr_time_ms - start_time_ms;
				if(time_elapse_ms >= total_log_time_ms){
					break;
				}
				/*Check the status every 0.5ms*/
				usleep(5e2);
			}
		}else{
			// we don't repeat and we don't stop until ctrl-c is fired
			while(true){
				pthread_mutex_lock( &mutex_exit);
				if(go_exit){
					pthread_mutex_unlock( &mutex_exit);
					break;
				}
				pthread_mutex_unlock( &mutex_exit);
			
				pthread_mutex_lock( &mutex_usage);
				lteCCA_status_update(&ue_status_t, &ue_cell_usage);
				pthread_mutex_unlock( &mutex_usage);

				/*Check the status every 0.5ms*/
				usleep(5e2);
			}
		}
	}
	lteCCA_status_exit(&ue_status_t);
    pthread_exit(NULL);
}
