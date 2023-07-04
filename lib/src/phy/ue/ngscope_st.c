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

#include "srsran/srsran.h"
#include "srsran/phy/ue/ngscope_st.h"

int ngscope_format_to_index(srsran_dci_format_t format){
    switch(format){
        case SRSRAN_DCI_FORMAT0:
            return 0;
            break;
        case SRSRAN_DCI_FORMAT1:
            return 1;
            break;
        case SRSRAN_DCI_FORMAT1A:
            return 2;
            break;
        case SRSRAN_DCI_FORMAT1C:
            return 3;
            break;
        case SRSRAN_DCI_FORMAT2:
            return 4;
            break;
        default:
            printf("Format not recegnized!\n");
            break;
    }
    return -1;
}
int ngscope_push_dci_to_per_sub(ngscope_dci_per_sub_t* q, ngscope_dci_msg_t* msg){
	if(msg->dl){
		// push dl msg to its position, only when we have space
		if(q->nof_dl_dci < MAX_DCI_PER_SUB){
			memcpy(&q->dl_msg[q->nof_dl_dci], msg, sizeof(ngscope_dci_msg_t));
			q->nof_dl_dci++;
		}
	}else{
		if(q->nof_ul_dci < MAX_DCI_PER_SUB){
			memcpy(&q->ul_msg[q->nof_ul_dci], msg, sizeof(ngscope_dci_msg_t));
			q->nof_ul_dci++;
		}
	}
	return 0;
}

int ngscope_enqueue_ul_reTx_dci_msg(ngscope_dci_per_sub_t* q, uint16_t targetRNTI){
	ngscope_dci_msg_t msg;
	msg.rnti 	= targetRNTI;
	msg.prb		= 0;
	msg.harq 	= 0;
	msg.nof_tb 	= 1;
	msg.dl 		= false;
	msg.decode_prob 	= 100;
	msg.corr 	= 1;
	msg.format 	= 0;
	msg.tb[0].mcs 	= 0;
	msg.tb[0].tbs 	= 0;
	msg.tb[0].rv  	= 1;
	msg.tb[0].ndi 	= 0;

	ngscope_push_dci_to_per_sub(q, &msg);
	return 0;
}

int ngscope_rnti_inside_dci_per_sub_dl(ngscope_dci_per_sub_t* q, uint16_t targetRNTI){
	if(q->nof_dl_dci > 0){
		for(int	i=0; i<q->nof_dl_dci; i++){
			if(q->dl_msg[i].rnti == targetRNTI){
				return i;	
			}
		}
	}
	return -1;
}

int ngscope_rnti_inside_dci_per_sub_ul(ngscope_dci_per_sub_t* q, uint16_t targetRNTI){
	if(q->nof_dl_dci > 0){
		for(int	i=0; i<q->nof_ul_dci; i++){
			if(q->ul_msg[i].rnti == targetRNTI){
				return i;	
			}
		}
	}
	return -1;
}

srsran_dci_format_t ngscope_index_to_format(int index){
    switch(index){
        case 0:
            return SRSRAN_DCI_FORMAT0;
            break;
        case 1:
            return SRSRAN_DCI_FORMAT1;
            break;
        case 2:
            return SRSRAN_DCI_FORMAT1A;
            break;
        case 3:
            return SRSRAN_DCI_FORMAT1C;
            break;
        case 4:
            return SRSRAN_DCI_FORMAT2;
            break;
        default:
            printf("Format not recegnized!\n");
            break;
    }
    return SRSRAN_DCI_FORMAT0;
}

void srsran_ngscope_print_dci_per_sub(ngscope_dci_per_sub_t* q){
	printf("DL-> ");
	for(int i=0; i<q->nof_dl_dci; i++){
		printf("%d ", q->dl_msg[i].rnti);
	}
	printf("UL-> ");
	for(int i=0; i<q->nof_ul_dci; i++){
		printf("%d ", q->ul_msg[i].rnti);
	}

	printf("\n");

	return;
}


