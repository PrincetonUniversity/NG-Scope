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
#include "ngscope/hdr/dciLib/dci_sink_client.h"
#include "ngscope/hdr/dciLib/dci_sink_sock.h"
#include "ngscope/hdr/dciLib/dci_sink_dci_recv.h"
#include "ngscope/hdr/dciLib/dci_sink_ring_buffer.h"

extern bool go_exit;
extern ngscope_dci_sink_CA_t dci_CA_buf;

// connect server
int sock_connectServer_w_config_udp(char serv_IP[40], int serv_port){
	int sockfd;
    char buffer[1400];
    struct sockaddr_in servaddr;

    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    
	sock_setnonblocking(sockfd);

    // Set server address
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(serv_port);
    //servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");//服务器IP地址
	if (inet_pton(AF_INET, serv_IP, &(servaddr.sin_addr)) <= 0) {
        perror("Invalid server IP address");
        exit(EXIT_FAILURE);
    } 

	buffer[0] = (char)0xCC;	buffer[1] = (char)0xCC;	
	buffer[2] = (char)0xCC;	buffer[3] = (char)0xCC;	
	
	// Socket 
	sendto(sockfd, (char *)buffer, 4, 0, (const struct sockaddr *) &servaddr, sizeof(servaddr));
	sendto(sockfd, (char *)buffer, 4, 0, (const struct sockaddr *) &servaddr, sizeof(servaddr));

	int nof_serv_pkt = 0;
	while(!go_exit){
		unsigned int len =  sizeof(servaddr);
		int recvLen = recvfrom(sockfd, (char *)buffer, 1400, 0, (struct sockaddr *) &servaddr, &len);
		if(recvLen > 0){
			if(buffer[0] == (char)0xAA && buffer[1] == (char)0xAA && \
				buffer[2] == (char)0xAA &&	buffer[3] == (char)0xAA){
				nof_serv_pkt++;
			}	
			if(nof_serv_pkt >= 2){
				break;
			}
		}
		usleep(100000);
	}

	return sockfd;
}

// close the connection with the server 
// notify the server about it
int sock_close_and_notify_udp(int sockfd){
    char buffer[1400];
    struct sockaddr_in servaddr;
   	int PORT = 6767; 

    // Set server address
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");//服务器IP地址
  
	buffer[0] = (char)0xFF;	buffer[1] = (char)0xFF;	
	buffer[2] = (char)0xFF;	buffer[3] = (char)0xFF;	

	sendto(sockfd, (char *)buffer, 4, 0, (const struct sockaddr *) &servaddr, sizeof(servaddr));
	sendto(sockfd, (char *)buffer, 4, 0, (const struct sockaddr *) &servaddr, sizeof(servaddr));
	sendto(sockfd, (char *)buffer, 4, 0, (const struct sockaddr *) &servaddr, sizeof(servaddr));

	return 0;
}

void* dci_sink_client_thread(void* p){
	char serv_IP[40] = "127.0.0.1";
	int serv_port = 6767;
    char recvBuf[1400];
	struct sockaddr_in cliaddr;

	// connect the server
	int sockfd = sock_connectServer_w_config_udp(serv_IP, serv_port);
	socklen_t len;
	while(!go_exit){
		if(go_exit) break;
	    int buf_idx = 0;
        int recvLen = 0;

        recvLen = recvfrom(sockfd, (char *)recvBuf, 1400, MSG_WAITALL, (struct sockaddr*) &cliaddr, &len);
        if(recvLen > 0){
            while(!go_exit){
				int ret = ngscope_dci_sink_recv_buffer(&dci_CA_buf, recvBuf, buf_idx, recvLen);
                if(ret < 0){
                    // ignore the buffer
                    printf("recvLen: %d buf_idx: %d \n\n", recvLen, buf_idx);
                    buf_idx = recvLen;
                    break;
                }else if(ret == recvLen){
                    break;
                }else{
                    buf_idx = ret;
                }
            }
            //offset = shift_recv_buffer(recvBuf, buf_idx, recvLen);
            //printf("recvLen: %d buf_idx: %d \n\n", recvLen, buf_idx);
        }
	}
	// close the connection with server
	sock_close_and_notify_udp(sockfd);

	printf("hell world\n");
	return NULL;
}
