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
    printf("dci ue status thd idx:%d\n", thd_idx);

    /* init the status structure */
    lteCCA_status_init(&ue_status_t);

    /* set the raw logging configurations */
    lteCCA_setRawLog_all(&ue_status_t, &(main_config.rawLog_config)); 

    /* fill in the fileDescriptors according to the raw log configurations */
    lteCCA_fill_fileDescriptor(&ue_status_t, main_config.usrp_config);

    /* enter the main loop */
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

	/*Check the status every 0.5s*/
	usleep(5e2);
    }
    pthread_exit(NULL);
}
