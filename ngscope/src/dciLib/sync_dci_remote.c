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
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>

#include "srsran/srsran.h"

#include "ngscope/hdr/dciLib/sync_dci_remote.h"

void dci_sync_init(ngscope_dci_sync_t* dci_sync){
	dci_sync->time_stamp = 0;
	dci_sync->tti 		= 0;
	dci_sync->cell_idx 	= 0;
	//dci_sync->ul_reTx 	= 0;
	dci_sync->proto_v 	= 0;

	dci_sync->downlink 	= false;
	dci_sync->uplink 	= false;

	memset(&dci_sync->dl_dci, 0, sizeof(ngscope_dci_msg_t));
	memset(&dci_sync->ul_dci, 0, sizeof(ngscope_dci_msg_t));

	return;
}

uint8_t get_ul_dl(ngscope_dci_sync_t dci_sync){
	uint8_t dl_ul = 0;
	uint8_t dl=0x00;
	uint8_t ul=0x00;
	if(dci_sync.downlink){
		dl = 0x01;
	}
	if(dci_sync.uplink){
		ul = 0x02;
	}
	dl_ul = (dl | ul) & 0xFF;

	return dl_ul;
}

int sync_dci_ver1(char* buf, int buf_idx, ngscope_dci_sync_t dci_sync){
	uint8_t dl_ul = 0;

	/***************************************
	Indicate whether we have downlink or uplink dci messages  
	00: no uplink and no downlink
	01: only downlink
	10: only uplink
	11: both downlink and uplink
	**************************************/
	dl_ul = get_ul_dl(dci_sync);
	memcpy(&buf[buf_idx], &dl_ul, sizeof(uint8_t));
	buf_idx += 1;
	//printf("tti:%d dl_ul:%d buf_idx:%d\n", dci_sync.tti, dl_ul, buf_idx);

	if(dci_sync.downlink){
		ngscope_dci_msg_t dci = dci_sync.dl_dci;
		/* indicate the retransmission */
		uint8_t reTx = 0;
		if(dci.tb[0].rv > 0 || dci.tb[1].rv > 0){
			reTx = 1;
		}
		memcpy(&buf[buf_idx], &reTx, sizeof(uint8_t));
		buf_idx += 1;

		/* Transport block size */
		uint32_t tbs    = dci.tb[0].tbs;
		//tbs             += dci.tb[1].tbs;
		if(dci.nof_tb > 1){
			tbs += dci.tb[1].tbs;
		}

		//printf("tbs->:%d ->", tbs);
		tbs             = htonl(tbs);
		//printf("%d \n", tbs);
		memcpy(&buf[buf_idx], &tbs, sizeof(uint32_t));
		buf_idx += 4;
	}

	if(dci_sync.uplink){
		ngscope_dci_msg_t dci = dci_sync.ul_dci;

		/*  retransmission or not */
		//uint8_t reTx = dci_sync.ul_reTx;
		uint8_t reTx = 0;
		if(dci.tb[0].rv > 0 || dci.tb[1].rv > 0){
			reTx = 1;
		}
		memcpy(&buf[buf_idx], &reTx, sizeof(uint8_t));
		buf_idx += 1;

		/* Transport block size */
		uint32_t tbs    = 0;
		// the tbs should be non-zero only when it is not a reTx
		if(reTx == 0){
			tbs = dci.tb[0].tbs;
			//tbs             += dci.tb[1].tbs;
			if(dci.nof_tb > 1){
				// theoretically uplink has only one TB, put it here anyway
				tbs += dci.tb[1].tbs;
			}
			tbs             = htonl(tbs);
		}
		memcpy(&buf[buf_idx], &tbs, sizeof(uint32_t));
		buf_idx += 4;
	}
	return buf_idx;
}


int ngscope_sync_dci_remote(int sock, ngscope_dci_sync_t dci_sync)
{
    if(sock <= 0){
		//printf("ERROR: sock not set!\n\n");
		return 0;
	}
    char buf[100];
	/**********************************************
	Common part of the data:
	preamble: 			0xAA 0xAA 0xAA 0xAA
	protocol version:  	8 bits
	timestamp:  		64 bits
	tti: 				16 bits
	cell_idx:			8 bits 
	**********************************************/
	// preamble here
    buf[0] = 0xAA;buf[1] = 0xAA;buf[2] = 0xAA;buf[3] = 0xAA;
    int     buf_idx = 4; 

	/************ Protocol Version ************/
	memcpy(&buf[buf_idx], &dci_sync.proto_v, sizeof(uint8_t));
	buf_idx += 1;

    /************ Time stamp ***************/
    uint32_t lower_t = htonl( (uint32_t)dci_sync.time_stamp); 
    uint32_t upper_t = htonl( dci_sync.time_stamp >> 32); 
    memcpy(&buf[buf_idx], &lower_t, sizeof(uint32_t));
    buf_idx += 4;
    memcpy(&buf[buf_idx], &upper_t, sizeof(uint32_t));
    buf_idx += 4;

    /*************    	TTI 		**************/
    uint16_t tx_tti = dci_sync.tti;
    memcpy(&buf[buf_idx], &tx_tti, sizeof(uint16_t));
    buf_idx += 2;

	/************ which cell tower (cell_idx) ************/
	memcpy(&buf[buf_idx], &dci_sync.cell_idx, sizeof(uint8_t));
	buf_idx += 1;

	switch(dci_sync.proto_v){
		case 0:
			buf_idx = sync_dci_ver1(buf, buf_idx, dci_sync);
			break;
		default:
			printf("ERROR: unknown protocol version!\n");
			break;
	}

	/* Send the data to remote server via UDP */
	struct sockaddr_in servaddr;
   	int PORT = 6767; 
    // Set server address
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");//服务器IP地址
	int ret = sendto(sock, (char *)buf, buf_idx, MSG_CONFIRM, (const struct sockaddr *) &servaddr, sizeof(servaddr));

	/* Send the data to remote server via TCP */
    //int ret = send(sock, buf, buf_idx, 0); 

	if(ret == -1){
    	//printf("Error sending: %i\n",errno);
	}
    return ret;
}

int ngscope_sync_config_remote(int sock, uint16_t* prb_cell, uint8_t nof_cell){
    if(sock <= 0){
		//printf("ERROR: sock not set!\n\n");
		return 0;
	}
    char buf[100];
	// preamble here
    buf[0] = 0xBB;buf[1] = 0xBB;buf[2] = 0xBB;buf[3] = 0xBB;
    int     buf_idx = 4; 

	// copy nof cell
	memcpy(&buf[buf_idx], &nof_cell, sizeof(uint8_t));
    buf_idx += 1; 

	// know the nof_prb for each cell
	for(int i=0; i<nof_cell; i++){
		memcpy(&buf[buf_idx], &prb_cell[i], sizeof(uint16_t));
    	buf_idx += 2; 
	}
	/* Send the data to remote server via UDP */
	struct sockaddr_in servaddr;
   	int PORT = 6767; 
    // Set server address
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");//服务器IP地址
	int ret = sendto(sock, (char *)buf, buf_idx, MSG_CONFIRM, (const struct sockaddr *) &servaddr, sizeof(servaddr));


	/* Send the data to remote server via TCP */
    //int ret = send(sock, buf, buf_idx, 0); 

	if(ret == -1){
    	printf("Error sending: %i\n",errno);
	}
	printf("SEND config %d bytes!\n", ret);
	return ret;
}
