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
			memcpy(&q->ul_msg[q->nof_dl_dci], msg, sizeof(ngscope_dci_msg_t));
			q->nof_ul_dci++;
		}
	}
	return 0;
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
