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

#include "ngscope/hdr/dciLib/skip_tti.h"

int skip_tti_init(task_skip_tti_t* q){
	q->nof_tti = 0;

	for(int i=0; i<MAX_NOF_SKIP_TTI; i++){
		q->sf[i] 		= 0;
		q->sf_idx[i] 	= 0;
	}

	return 0;
}

int skip_tti_put(task_skip_tti_t* q, uint32_t sf, uint32_t sf_idx){
	if(q->nof_tti == MAX_NOF_SKIP_TTI){
		//printf("\n Too many skippped subframes!! we recommend adjust the hyperparameters, for example, increasing the number of decoders!\n");
		return 0;
	}
	q->sf[q->nof_tti] 		= sf;
	q->sf_idx[q->nof_tti] 	= sf_idx;
	q->nof_tti++;
	return 1;	
}
uint32_t skip_tti_get(task_skip_tti_t* q){
	if(q->nof_tti == 0){
		printf("ERROR: no skip tti found!\n");
		return 0;
	}

	uint32_t tti = q->sf[q->nof_tti-1] * 10 + q->sf_idx[q->nof_tti-1];
	q->sf[q->nof_tti-1] 	= 0;
	q->sf_idx[q->nof_tti-1] = 0;

	q->nof_tti--;
	return tti;
}

int skip_tti_empty(task_skip_tti_t* q){
	if(q->nof_tti ==0){
		return true;
	}else{
		return false;
	}
}
