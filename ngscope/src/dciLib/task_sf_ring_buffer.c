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

#include "ngscope/hdr/dciLib/task_sf_ring_buffer.h"

int task_sf_ring_buffer_init(task_tmp_buffer_t* q, int max_num_samples){
	/********************** Set up the tmp buffer **********************/
    q->header  	= 0;
    q->tail     = 0;
    q->len 		= 0;
	q->full 	= false;
    //task_tmp_buffer.nof_buf  = 0;
    // init the buffer
    for(int i=0; i<MAX_TMP_BUFFER; i++){
        for (int j = 0; j < SRSRAN_MAX_PORTS; j++) {
            q->sf_buf[i].IQ_buffer[j] = srsran_vec_cf_malloc(max_num_samples);
        }
    }
	return 0;
}

int task_sf_ring_buffer_free(task_tmp_buffer_t* q){
    for(int i=0; i<MAX_TMP_BUFFER; i++){
        for (int j = 0; j < SRSRAN_MAX_PORTS; j++) {
            free(q->sf_buf[i].IQ_buffer[j]);
        }
    }
	return 0; 
}

int task_sf_ring_buffer_put(task_tmp_buffer_t* q,
							cf_t* buffers[SRSRAN_MAX_CHANNELS],
							uint32_t sfn, 
							uint32_t sf_idx,
							int rf_nof_rx_ant,
							int max_num_samples)
{
	// we do nothing if the buffer is full
	if(q->full) return 0;

	/* Now we put the data into the buffer */

	//printf("Nof buffer:%d %d\n", task_tmp_buffer.header, task_tmp_buffer.nof_buf);
	q->sf_buf[q->header].sf_idx   = sf_idx;
	q->sf_buf[q->header].sfn      = sfn;
	for(int p=0; p<rf_nof_rx_ant; p++){
		memcpy(q->sf_buf[q->header].IQ_buffer[p], 
								buffers[p], max_num_samples*sizeof(cf_t));
	}   
	
	/*After puting data into the buffer, we need to update the corresponding parameters*/
	q->len++;
	q->header++;
	q->header = q->header % MAX_TMP_BUFFER; // advance the header first
	if(q->len == MAX_TMP_BUFFER){
		q->full = true;  // the buffer is full
	}
	return 1;
}

// tail contains the newest data
int task_sf_ring_buffer_get(task_tmp_buffer_t* q){
	q->len--;
	q->tail++; 
	q->tail = q->tail % MAX_TMP_BUFFER;

	if(q->len < MAX_TMP_BUFFER){
		q->full = false;
	}
	return 0;
}

bool task_sf_ring_buffer_full(task_tmp_buffer_t* q){
	return q->full; 
}

bool task_sf_ring_buffer_empty(task_tmp_buffer_t* q){
	if(q->len > 0){
		return false;
	}else{
		return true;
	}
}

int task_sf_ring_buffer_len(task_tmp_buffer_t* q){
	return q->len;
}

