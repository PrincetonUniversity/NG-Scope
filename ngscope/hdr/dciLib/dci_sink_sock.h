#ifndef DCI_SINK_SOCK_HH
#define DCI_SINK_SOCK_HH
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "dci_sink_def.h"

typedef struct{
	struct sockaddr_in 	client_addr[MAX_CLIENT];
	int 				nof_client;
    pthread_mutex_t     mutex;
}client_list_t;

//remote file sink / server
typedef struct{
	client_list_t client_list; // the list that stores all the clients
	int sink_port; // the port of the server
	int sink_sockfd; // serv sock filedescriptor
}ngscope_dci_sink_serv_t;


void sock_setnonblocking(int sockfd);
bool sock_same_sock_addr(struct sockaddr_in* a, struct sockaddr_in* b);

bool sock_init_dci_sink(ngscope_dci_sink_serv_t* q, int port);
void sock_update_client_list_addr(client_list_t* q, struct sockaddr_in* addr);

int sock_send_config(ngscope_dci_sink_serv_t* q, cell_config_t* cell_config);
int sock_send_single_dci(ngscope_dci_sink_serv_t* q, ue_dci_t* ue_dci, int proto_v);

struct sockaddr_in sock_create_serv_addr(char serv_IP[40], int serv_port);
#endif
