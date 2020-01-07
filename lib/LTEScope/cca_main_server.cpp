/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2015 Software Radio Systems Limited
 *
 * \section LICENSE
 *
 * This file is part of the srsLTE library.
 *
 * srsLTE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsLTE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <iostream>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/poll.h>

#include <pthread.h>
#include <semaphore.h>
#include "srslte/common/gen_mch_tables.h"
#include "srslte/common/crash_handler.h"
#include <srslte/phy/common/phy_common.h>
#include "srslte/phy/io/filesink.h"
#include "srslte/srslte.h"
#include "srslte/phy/ue/lte_scope.h"
#include "srslte/phy/ue/ue_list.h"
#include "srslte/phy/ue/ue_cell_status.h"

#define ENABLE_AGC_DEFAULT

extern "C"{
#include "srslte/phy/rf/rf.h"
#include "srslte/phy/rf/rf_utils.h"
#include "dci_decode_multi_usrp.h"
#include "read_cfg.h"
}
#include "cca_main.h"
#include "usrp_sock.h"
#include "socket.hh"
#define PRINT_CHANGE_SCHEDULIGN

extern float mean_exec_time;

//enum receiver_state { DECODE_MIB, DECODE_PDSCH} state; 
bool go_exit = false; 
bool exit_heartBeat = false;
srslte_ue_cell_usage ue_cell_usage;
enum receiver_state state[MAX_NOF_USRP]; 
srslte_ue_sync_t ue_sync[MAX_NOF_USRP]; 
prog_args_t prog_args[MAX_NOF_USRP]; 
srslte_ue_list_t ue_list[MAX_NOF_USRP];
srslte_cell_t cell[MAX_NOF_USRP];  
srslte_rf_t rf[MAX_NOF_USRP]; 
uint32_t system_frame_number[MAX_NOF_USRP] = { 0, 0, 0, 0, 0 }; // system frame number
int free_order[MAX_NOF_USRP*4] = {0};

pthread_mutex_t mutex_exit = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_usage = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_free_order = PTHREAD_MUTEX_INITIALIZER;


uint16_t targetRNTI_const = 0;
void sig_int_handler(int signo)
{
  printf("SIGINT received. Exiting...\n");
  if (signo == SIGINT) {
    go_exit = true;
  } else if (signo == SIGSEGV) {
    exit(1);
  }
}

int main(int argc, char **argv) {
    int server_sock;
    int client_sock;
    int nof_sock = accept_slave_connect(&server_sock, &client_sock);
    printf("%d client connected\n", nof_sock);
    int efd;
    struct epoll_event ev, events[1];

    srslte_lteCCA_rate lteCCA_rate;
    FILE* FD = fopen("time.txt","w+"); 
    efd = epoll_create(4); //创建epoll实例
    if (efd == -1) {
	printf("create epoll fail \r\n");	
	return 0;
    }
    ev.data.fd = client_sock;
    ev.events = EPOLLIN;
    epoll_ctl(efd, EPOLL_CTL_ADD, client_sock, &ev); //添加到epoll监听队列中
    while(true){
	int recv_len = recv(client_sock, &lteCCA_rate, sizeof(srslte_lteCCA_rate), 0);
	if(recv_len > 0){
	    printf("probe rate:%d rate_hm:%d ue rate:%d ue_rate_hm:%d\n",
			lteCCA_rate.probe_rate,
			lteCCA_rate.probe_rate_hm,
			lteCCA_rate.ue_rate,
			lteCCA_rate.ue_rate_hm);
	    uint64_t curr_time = Socket::timestamp();  
	    fprintf(FD,"%ld\n",curr_time); 
	}
	//int nfds = epoll_wait(efd, events, 1, 10000);
	//if(nfds > 0){
	//    for(int i=0;i<nfds;i++){
	//	if( (events[i].data.fd == server_sock) && (events[i].events & POLLIN) ){
	//	    int recv_len = recv(server_sock, &lteCCA_rate, sizeof(srslte_lteCCA_rate), 0);
	//	    printf("probe rate:%d rate_hm:%d ue rate:%d ue_rate_hm:%d\n",
	//			lteCCA_rate.probe_rate,
	//			lteCCA_rate.probe_rate_hm,
	//			lteCCA_rate.ue_rate,
	//			lteCCA_rate.ue_rate_hm);
	//	    uint64_t curr_time = Socket::timestamp();  
	//	    fprintf(FD,"%ld\n",curr_time); 
	//	}
	//    }
	//}
    }
    close(server_sock);
    printf("\nBye MAIN FUNCTION!\n");
    exit(0);
}

