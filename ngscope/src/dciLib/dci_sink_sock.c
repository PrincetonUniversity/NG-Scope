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
#include <fcntl.h>
#include <stdbool.h>

#include "ngscope/hdr/dciLib/dci_sink_sock.h"

//set sock in non-block mode
void sock_setnonblocking(int sockfd) {
    int flag = fcntl(sockfd, F_GETFL, 0);
    if (flag < 0) {
        perror("fcntl F_GETFL fail");
        return;
    }

    if (fcntl(sockfd, F_SETFL, flag | O_NONBLOCK) < 0) {
        perror("fcntl F_SETFL fail");
    }
	return;
}

// check whether two socks are the same 
bool sock_same_sock_addr(struct sockaddr_in* a, struct sockaddr_in* b){
	if(a->sin_family == b->sin_family && a->sin_port == b->sin_port){
		if(a->sin_addr.s_addr == b->sin_addr.s_addr){
			bool match = true;
			for(int i=0; i<8; i++){
				if(a->sin_zero[i] != b->sin_zero[i]){
					match = false;
				}
			}
			if(match) 
				return true;
		}
	}
	return false;
}


/***** CLIENT and Server handling *******/
void sock_init_client_list(client_list_t* q){
	for(int i=0; i<MAX_CLIENT; i++){
		memset(&q->client_addr[i], 0, sizeof(struct sockaddr_in));
	}
	q->nof_client 	= 0;
	// init the mutex
	pthread_mutex_init(&q->mutex, NULL);
	return;
}

bool sock_init_dci_sink(ngscope_dci_sink_serv_t* q, int port){
	struct sockaddr_in servaddr;

	// we must set the port first
	q->sink_port = port;

	// Create UDP socket
    q->sink_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (q->sink_sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
	
	// non-blocking
	sock_setnonblocking(q->sink_sockfd);

	// Set server address
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(q->sink_port);
    
    // Bind socket to server address
    if (bind(q->sink_sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

	// init the client list
	sock_init_client_list(&q->client_list);

	return true;
}

void sock_update_client_list_addr(client_list_t* q, struct sockaddr_in* addr){
    pthread_mutex_lock(&q->mutex);
	// if the vectoris full, return
	if(q->nof_client >= MAX_CLIENT){
		printf("nof client is more than the max nof client!\n");
    	pthread_mutex_unlock(&q->mutex);
		return;
	}

	// check if the addr is already inside the vector
	for(int i=0; i< q->nof_client; i++){
		if(sock_same_sock_addr(&q->client_addr[i], addr)){
			// the client address is already inside the vector
			printf("the client address is already inside the vector\n");
    		pthread_mutex_unlock(&q->mutex);
			return;
		}
	}

	// copy the addr to the vector
	memcpy(&q->client_addr[q->nof_client], addr, sizeof(struct sockaddr_in));

	// update the client number
	q->nof_client++;
	
	printf("We have %d  client!\n", q->nof_client);

    pthread_mutex_unlock(&q->mutex);

	return;
}

int sock_send_single_dci(ngscope_dci_sink_serv_t* q, ue_dci_t* ue_dci, int proto_v){

	char buf[100];
	/**********************************************
	Common part of the data:
	preamble: 			0xAA 0xAA 0xAA 0xAA
	protocol version:  	8 bits
	ue_dci_t: 			size varies
	**********************************************/
	// preamble here
    buf[0] = 0xAA;buf[1] = 0xAA;buf[2] = 0xAA;buf[3] = 0xAA;
    int     buf_idx = 4; 

	/************ Protocol Version ************/
	memcpy(&buf[buf_idx], &proto_v, sizeof(uint8_t));
	buf_idx += 1;

	/*********** UE DCI ***************/
	memcpy(&buf[buf_idx], ue_dci, sizeof(ue_dci_t));
	buf_idx += sizeof(ue_dci_t);
	
	/* Send the data to all client via UDP */
    pthread_mutex_lock(&q->client_list.mutex);
	for(int i=0; i< q->client_list.nof_client; i++){
		int ret = sendto(q->sink_sockfd, (char *)buf, buf_idx, MSG_CONFIRM, 
				(const struct sockaddr *) &q->client_list.client_addr[i], sizeof(struct sockaddr));
		if(ret < 0){
			printf("ERROR in sending via socket!\n");
		}
	}
    pthread_mutex_unlock(&q->client_list.mutex);

	return 1;
}

int sock_send_config(ngscope_dci_sink_serv_t* q, cell_config_t* cell_config){
	char buf[100];
	// preamble here
    buf[0] = 0xBB;buf[1] = 0xBB;buf[2] = 0xBB;buf[3] = 0xBB;
    int     buf_idx = 4; 

	/*********** CELL CONFIG ***************/
	memcpy(&buf[buf_idx], cell_config, sizeof(cell_config_t));
	buf_idx += sizeof(cell_config_t);

	/* Send the config to all client via UDP */
    pthread_mutex_lock(&q->client_list.mutex);
	for(int i=0; i< q->client_list.nof_client; i++){
		int ret = sendto(q->sink_sockfd, (char *)buf, buf_idx, MSG_CONFIRM, 
				(const struct sockaddr *) &q->client_list.client_addr[i], sizeof(struct sockaddr));
		if(ret < 0){
			printf("ERROR in sending via socket!\n");
		}
	}
    pthread_mutex_unlock(&q->client_list.mutex);

	return 1;
}

struct sockaddr_in sock_create_serv_addr(char serv_IP[40], int serv_port){
    struct sockaddr_in servaddr;

    // Set server address
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(serv_port);
    //servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");//服务器IP地址
	if (inet_pton(AF_INET, serv_IP, &(servaddr.sin_addr)) <= 0) {
        perror("Invalid server IP address");
        exit(EXIT_FAILURE);
    } 

	return servaddr;
}

