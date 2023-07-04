#include <unistd.h>
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


#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "ngscope/hdr/dciLib/dci_sink_ring_buffer.h"
#include "ngscope/hdr/dciLib/dci_sink_recv_dci.h"

// recv configurations from the dci_sink_serv
int recv_config(char* recvBuf, cell_config_t* cell_config, int buf_idx, int recvLen){
	if(buf_idx + (int)sizeof(cell_config_t) > recvLen){
		printf("recv_config: not enough bytes!\n");
		return buf_idx;
	}
    memcpy(cell_config, &recvBuf[buf_idx], sizeof(cell_config_t));
	buf_idx += sizeof(cell_config_t);

	int nof_cell = cell_config->nof_cell;
	printf("RNTI: %d NOF_CELL:%d  ", cell_config->rnti, nof_cell);
	for(int i=0; i< nof_cell; i++){
		printf("%d-th CELL PRB:%d ", i, cell_config->cell_prb[i]);
	}
	printf("\n");

	return buf_idx;
}

void print_ue_dci(ue_dci_t* q){
	printf("Cell_idx:%d tti:%d rnti:%d dl_tbs:%d ul_tbs:%d\n", 
			q->cell_idx, q->tti, q->rnti, q->dl_tbs, q->ul_tbs);
	return;
}
// We receive DCI from the buffer. The received dci is stored inside the ue_dci
int recv_dci(char* recvBuf, ue_dci_t* ue_dci, int buf_idx, int recvLen){
	if(buf_idx + 1 >= recvLen){
		printf("recv_dci: not enough bytes!\n");
		return buf_idx;
	}

	uint8_t 	proto_v;
	// get protocol buffer
	memcpy(&proto_v, &recvBuf[buf_idx], sizeof(uint8_t));
	buf_idx += 1;

	switch(proto_v){
		case 0:
			// protocol version 1 ue_dci_t
			if(buf_idx + (int)sizeof(ue_dci_t) > recvLen){
				printf("sizeof ue_dci_t:%d buf_idx:%d\n",(int)sizeof(ue_dci_t), buf_idx);
				printf("recv_one_dci: not enough bytes!\n");
				return 0;
			}
			memcpy(ue_dci, &recvBuf[buf_idx], sizeof(ue_dci_t)); 
			print_ue_dci(ue_dci);
			buf_idx += sizeof(ue_dci_t);
			break;
		default:
			printf("ERROR: unknown protocol version!\n");
			break;
	}

	return buf_idx;
}


// We receve the data inside the buffer
int ngscope_dci_sink_recv_buffer(ngscope_dci_sink_CA_t* q, char* recvBuf, int idx, int recvLen){
	int buf_idx = idx; 

	/* First, we check the preamble to know the types of data*/
	if( recvBuf[buf_idx] == (char)0xAA && recvBuf[buf_idx+1] == (char)0xAA && \
		recvBuf[buf_idx+2] == (char)0xAA && recvBuf[buf_idx+3] == (char)0xAA ){

		//printf("DCI received!\n");
		ue_dci_t ue_dci;
		buf_idx	+= 4;
		int buf_idx_before = buf_idx;
		printf("recvLen:%d buf_idx:%d \n", recvLen, buf_idx);
		buf_idx = recv_dci(recvBuf, &ue_dci, buf_idx, recvLen);

		if(buf_idx_before == buf_idx){
			// the decoding of the dci failed 
			buf_idx -= 4;
			return -1;
		}else{
			// insert the dci into the ring buffer
			ngscope_dciSink_ringBuf_insert_dci(q, &ue_dci); 
		}
	}else if( recvBuf[buf_idx] == (char)0xBB && recvBuf[buf_idx+1] == (char)0xBB && \
		recvBuf[buf_idx+2] == (char)0xBB && recvBuf[buf_idx+3] == (char)0xBB ){

		printf("Configuration received!\n");
		cell_config_t cell_config;
		buf_idx	+= 4;
		int buf_idx_before = buf_idx;
		buf_idx = recv_config(recvBuf, &cell_config, buf_idx, recvLen);
		// update the parameters inside the ring buffer 

		if(buf_idx_before == buf_idx){
			// the decoding of the dci failed 
			buf_idx -= 4;
			return -1;
		}else{
			ngscope_dciSink_ringBuf_update_config(q, &cell_config);	
		}
	}else if( recvBuf[buf_idx] == (char)0xFF && recvBuf[buf_idx+1] == (char)0xFF && \
		recvBuf[buf_idx+2] == (char)0xFF && recvBuf[buf_idx+3] == (char)0xFF ){
		printf("EXIT!\n");
		return -999;
	}else{
		printf("ERROR: Unknown preamble!\n");
		return -1;
	}
	return buf_idx;
}

