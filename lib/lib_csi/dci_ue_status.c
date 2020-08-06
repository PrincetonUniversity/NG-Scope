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
#include "time_period.h"

#include "srsgui/srsgui.h"

extern bool go_exit;
srslte_config_t main_config;            // configuration (which is stored inside the configuration file .cfg)

extern srslte_ue_cell_usage ue_cell_usage;  // CELL ue usage 
extern lteCCA_status_t	    ue_status_t;    // ue status

extern pthread_mutex_t mutex_csi;
extern pthread_mutex_t mutex_exit;
extern pthread_mutex_t mutex_usage;

void* dci_ue_status_update(void* p){
    int thd_idx	    = *(int *)p;
    printf("dci ue status thd idx:%d\n", thd_idx);

    /* init the status structure */
    lteCCA_status_init(&ue_status_t);

    printf("Enter forward init --> try to connect to remote PC\n");
    lteCCA_forward_init(&ue_status_t, &(main_config.forward_config));
    
    /* fill in the fileDescriptors according to the raw log configurations */
    printf("DCI_UE_STATUS: fill in the descriptor\n");
    //lteCCA_fill_fileDescriptor(&ue_status_t, main_config.usrp_config, &(main_config.csiLog_config));
    lteCCA_fill_fileDescriptor_folder(&ue_status_t, main_config.usrp_config, &(main_config.csiLog_config));
    printf("DCI_UE_STATUS: enter LOOP...\n");

    /* enter the main loop */
    uint32_t start_time_ms	= timestamp_ms();

    int repeat_flag		= main_config.csiLog_config.repeat_flag;
    int repeat_log_intval_s	= main_config.csiLog_config.repeat_log_intval_s;
    int repeat_pause_intval_s	= main_config.csiLog_config.repeat_pause_intval_s;

    int trace_count = 0;

    while(true){
	pthread_mutex_lock( &mutex_exit);
	if(go_exit){
	    pthread_mutex_unlock( &mutex_exit);
	    break;
	}
	pthread_mutex_unlock( &mutex_exit);
	start_time_ms	= timestamp_ms();
	trace_count++;

	printf("\n\n We begin to log %d-th csi now .... \n\n", trace_count);
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
	    if(repeat_flag == 1){	    
		uint32_t curr_time_ms	= timestamp_ms();
		uint32_t time_elapse_ms	= curr_time_ms - start_time_ms;
		if(time_elapse_ms >= repeat_log_intval_s * 1000){
		    lteCCA_update_fileDescriptor_folder(&ue_status_t, main_config.usrp_config);
		    //lteCCA_update_fileDescriptor(&ue_status_t, main_config.usrp_config);
		    break;
		}
	    }
	    usleep(5e2);
	}
	if(repeat_flag == 1){
	    printf("\n\nWe finish logging and are going to pause for %d seconds! \n", repeat_pause_intval_s);
	    for(int i=0;i<repeat_pause_intval_s;i++){

		pthread_mutex_lock( &mutex_exit);
		if(go_exit){
		    pthread_mutex_unlock( &mutex_exit);
		    break;
		}
		pthread_mutex_unlock( &mutex_exit);

		if(i%5 == 0){
		    printf("We have slep for %d seconds!\n", i);
		}
		sleep(1);
	    }
	}
    }
    lteCCA_status_exit(&ue_status_t);

    pthread_exit(NULL);
}


//plot_waterfall_t poutfft[MAX_NOF_USRP];
//
//plot_real_t pcos_sim;
//
//#define PLOT_X_POINT 1024
//
//static void update_cos_sim(float* sim_vec, float sim, int N){
//    for(int i=0;i<(N-1);i++){
//        sim_vec[i] = sim_vec[i+1];
//    }
//    sim_vec[N-1] = sim;
//}
//static float point_multiply(float* amp_a, float* amp_b, int N){
//    float ret = 0;
//    for(int i=0;i<N;i++){
//        ret += amp_a[i] * amp_b[i];
//    }
//    return ret;
//}
//static float vector_norm(float* amp, int N){
//    float ret, sum=0;
//    for(int i=0;i<N;i++){
//        sum += amp[i] * amp[i];
//    }
//    ret = sqrt(sum);
//    return ret;
//}
//static float cal_cos_sim(float* amp_a, float* amp_b, int N){
//    float m_v, norm_v_a, norm_v_b;
//
//    m_v     = point_multiply(amp_a, amp_b, N);
//    norm_v_a= vector_norm(amp_a, N);
//    norm_v_b= vector_norm(amp_b, N);
//
//    float cos_sim = m_v / (norm_v_a * norm_v_b);
//    return cos_sim;
//}
//
//static void get_csi_amp(cf_t* csi, float* amp, int nof_point){
//    for(int i=0;i<nof_point;i++){
//        amp[i] = cabs(csi[i]);
//    }
//}
//static void copy_amp_vec(float* dest, float* source, int N){
//    for(int i=0; i<N; i++){
//        dest[i] = source[i];
//    }
//}
//static void clear_csi_amp(float* amp, int N){
//    for(int i=0;i<N;i++){
//        amp[i] = 0;
//    }
//}
//
//void *plot_thread_run(void *arg) {
//    int     nof_prb = 100;
//    float   csi_amp[1200];
//    float   cosine_sim;
//    float   csi_log[2][100*12];
//    float   cos_sim[PLOT_X_POINT];
//
//
//    int     cos_cnt = 0;
//
//    int nof_usrp = main_config.nof_usrp;
//
//    sdrgui_init();
//
//    plot_real_init(&pcos_sim);
//    plot_real_setTitle(&pcos_sim, "Cosine similarity");
//    plot_real_setYAxisScale(&pcos_sim, 0, 1);
//
//    for(int i=0;i<nof_usrp;i++){
//        plot_waterfall_init(&poutfft[i], SRSLTE_NRE * nof_prb, 100);
//        plot_waterfall_setTitle(&poutfft[i], "CSI Magnitude");
//        plot_waterfall_setPlotYAxisScale(&poutfft[i], 0, 7);
//        plot_waterfall_setSpectrogramZAxisScale(&poutfft[i], 0, 7);
//    }
//
//    while(true){
//        pthread_mutex_lock( &mutex_csi);
//        for(int i=0;i<nof_usrp;i++){
//            cf_t*   csi_ptr = csi_mat[i];
//            if(creal(csi_ptr[0]) != 0 && creal(csi_ptr[1] != 0)){
//                get_csi_amp(csi_ptr, csi_amp, SRSLTE_NRE * nof_prb);
//                plot_waterfall_appendNewData(&poutfft[i], csi_amp, SRSLTE_NRE * nof_prb);
//                reset_csi(csi_ptr, SRSLTE_NRE * nof_prb);
//                copy_amp_vec(csi_log[i], csi_amp, SRSLTE_NRE * nof_prb);
//            }
//        }
//        pthread_mutex_unlock( &mutex_csi);
//        if( (csi_log[0][0] != 0) && (csi_log[0][1] != 0)
//            && (csi_log[1][0] != 0) && (csi_log[1][1] != 0)){
//            cosine_sim = cal_cos_sim(csi_log[0], csi_log[1], SRSLTE_NRE * nof_prb);
//            clear_csi_amp(csi_log[0], SRSLTE_NRE * nof_prb);
//            clear_csi_amp(csi_log[1], SRSLTE_NRE * nof_prb);
//            if(cos_cnt <= PLOT_X_POINT){
//                cos_sim[cos_cnt] = cosine_sim;
//                cos_cnt ++;
//            }else{
//                update_cos_sim(cos_sim, cosine_sim, PLOT_X_POINT);
//            }
//            plot_real_setNewData(&pcos_sim, cos_sim, PLOT_X_POINT);
//        }
//
//        usleep(400);
//    }
//
//    return NULL;
//}
//
//void init_plots() {
//
////    pthread_attr_t attr;
//  //  struct sched_param param;
//  //  param.sched_priority = 0;
//  //  pthread_attr_init(&attr);
//  //  pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
//  //  pthread_attr_setschedparam(&attr, &param);
//    if (pthread_create(&plot_thread, NULL, plot_thread_run, NULL)) {
//            perror("pthread_create");
//            exit(-1);
//    }
//}
//
