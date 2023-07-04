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

//#include "srsran/srsran.h"

#include "ngscope/hdr/dciLib/dci_sink_serv.h"
#include "ngscope/hdr/dciLib/dci_sink_sock.h"
#include "ngscope/hdr/dciLib/dci_sink_recv_dci.h"

extern bool go_exit;
//extern client_list_t client_list;

extern ngscope_dci_sink_serv_t dci_sink_serv;

int push_client_to_vec(struct sockaddr_in client_addr[MAX_CLIENT], struct sockaddr_in* addr, int nof_client){
	// if the vectoris full, return
	if(nof_client >= MAX_CLIENT){
		printf("vector is full!\n");
		return nof_client;
	}

	// check if the addr is already inside the vector
	for(int i=0; i< nof_client; i++){
		if(sock_same_sock_addr(&client_addr[i], addr)){
			// the client address is already inside the vector
			printf("the client address is already inside the vector\n");
			return nof_client;
		}
	}

	// copy the addr to the vector
	memcpy(&client_addr[nof_client], addr, sizeof(struct sockaddr_in));

	return (nof_client+1);
}

void* dci_sink_server_thread(void* p){
	struct sockaddr_in servaddr, client_addr[MAX_CLIENT], cliaddr;
    int sockfd;
	int PORT = 6767, nof_client = 0;
    char recvBuf[1400];
	cell_config_t cell_config;
	cell_config = *(cell_config_t*)p;

	// Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
	
	// non-blocking
	sock_setnonblocking(sockfd);

	// Set server address
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);
    
    // Bind socket to server address
    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

	// Receive message from client
    unsigned int len = 0;
	int n;
    len = sizeof(cliaddr);
	// we try to build the connection first: wati for the client to send the connection request to us
	while(!go_exit){
		// recv data from client and get the client address
		n = recvfrom(sockfd, (char *)recvBuf, 1400, MSG_WAITALL, (struct sockaddr*) &cliaddr, &len);
		if(n > 0){
			printf("PORT: %d recv len:%d | %d %d \n", cliaddr.sin_port, n, recvBuf[0], recvBuf[1]);
			// we recevie the connection request from client
			if( recvBuf[0] == (char)0xCC && recvBuf[1] == (char)0xCC && recvBuf[2] == (char)0xCC 
							 && recvBuf[3] == (char)0xCC ){
				printf("Recv client connection request!\n");
				// push the client to the local list | skip if the client is already inside the list
				int new_client = push_client_to_vec(client_addr, &cliaddr, nof_client);
				printf("new client:%d nof_client:%d \n", new_client, nof_client);
				// New client is found!
				if(new_client > nof_client){
					nof_client = new_client;
					//if we receive a new client, push it to the global list
					sock_update_client_list_addr(&dci_sink_serv.client_list, &cliaddr);

					// tell the client that we recevied their request
					recvBuf[0] = (char)0xAA;recvBuf[1] = (char)0xAA;
					recvBuf[2] = (char)0xAA;recvBuf[3] = (char)0xAA;

					printf("Built connection with %d-th client!\n", nof_client);
					sendto(sockfd, (char *)recvBuf, 4, MSG_CONFIRM, (const struct sockaddr *) &cliaddr, len);
					sendto(sockfd, (char *)recvBuf, 4, MSG_CONFIRM, (const struct sockaddr *) &cliaddr, len);

					printf("tell the client about the configuration!\n");
					sock_send_config(&dci_sink_serv, &cell_config);
				}
			}
			// we receive the request to close the connection  
			if( recvBuf[0] == (char)0xFF && recvBuf[1] == (char)0xFF && recvBuf[2] == (char)0xFF 
							 && recvBuf[3] == (char)0xFF ){
				// TODO finish
			}

		}
		//sleep
		usleep(100000);
	}

    return NULL;
}
