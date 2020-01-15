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
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <errno.h>

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
#include "client.hh"
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
srslte_lteCCA_rate lteCCA_rate;

int main(int argc, char **argv) {
    int opt;
    int con_time_s = 20;
    int con_time_ns = 0;
   
    while ((opt = getopt(argc, argv, "tc")) != -1) {
        switch (opt) {
        case 't':
            con_time_s = atoi(argv[optind]);
            break;
        default:
            printf("no input found\n");
            break;
        }
    }

    srslte_lteCCA_rate lteCCA_rate;

    FILE* FD	    = fopen("client_log","w+"); 
    FILE* FD_rate   = fopen("CCA_rate_log","w+"); 

    // Connection (with AWS server) parameters
    char AWS_servIP[100];
    strcpy(AWS_servIP, "13.58.172.157");

    // Set up UDP with AWS server
    Socket AWS_client_socket;
    int sender_id = getpid();
    Socket::Address AWS_server_address( UNKNOWN );
    AWS_client_socket.bind( Socket::Address( "0.0.0.0", 9003 ) );
    AWS_server_address = Socket::Address( AWS_servIP, 9001 );
    Client client(AWS_client_socket, AWS_server_address, FD);

    // Set up Epoll 
    int efd;
    struct epoll_event ev, events[4];
    int tfd, tfd_QAM;
    struct itimerspec time_intv, time_intv_QAM; //用来存储时间

    // Create timer     
    tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);   //创建定时器 non-block
    if(tfd == -1) {
        printf("create timer fd fail \r\n");
        return 0;
    }
   
    time_intv.it_value.tv_sec = con_time_s;
    time_intv.it_value.tv_nsec = con_time_ns;
    time_intv.it_interval.tv_sec = 0;   // non periodic
    time_intv.it_interval.tv_nsec = 0;

   
    // Create epoll 
    efd = epoll_create(4); //创建epoll实例
    if (efd == -1) {
	printf("create epoll fail \r\n");	
	return 0;
    }
    ev.data.fd = tfd;
    ev.events = EPOLLIN;    //监听定时器读事件，当定时器超时时，定时器描述符可读。
    epoll_ctl(efd, EPOLL_CTL_ADD, tfd, &ev); //添加到epoll监听队列中

    ev.data.fd = AWS_client_socket.get_sock();
    ev.events = EPOLLIN;    //监听定时器读事件，当定时器超时时，定时器描述符可读。
    epoll_ctl(efd, EPOLL_CTL_ADD, AWS_client_socket.get_sock(), &ev); //添加到epoll监听队列中

    bool log_flag = false;
    bool exit_loop = false;

    // start the link
    client.init_connection();
    timerfd_settime(tfd, 0, &time_intv, NULL);  //启动定时器 for connection
    while(true){
	int nfds = epoll_wait(efd, events, 4, 10000);
	if(nfds > 0){
	    for(int i=0;i<nfds;i++){
		// Handle the ack from AWS server
		// timeout for the connection
		if( (events[i].data.fd == AWS_client_socket.get_sock()) && (events[i].events & POLLIN) ){
		    client.recv_noRF_noACK(); 
		    log_flag = true;
		}

		if( (events[i].data.fd == tfd) && (events[i].events & POLLIN) ){
		    exit_loop = true; 
		}
	    }
	}
	if(exit_loop){
	    break;
	}
    }
    client.close_connection();
    // garbage collection -- 0.5s
    time_intv.it_value.tv_sec = 0;
    time_intv.it_value.tv_nsec = 5e8;
    time_intv.it_interval.tv_sec = 0;   // non periodic
    time_intv.it_interval.tv_nsec = 0;
    timerfd_settime(tfd, 0, &time_intv, NULL);  //启动定时器
    exit_loop = false;
    printf("\n\n Enter garbage collection...\n\n");
    while (true){
        int nfds = epoll_wait(efd, events, 2, 0);
        if(nfds < 0){
            printf("Epoll wait failure!\n");
            continue;
        }
        for(int i=0;i<nfds;i++){
            if( (events[i].data.fd == AWS_client_socket.get_sock()) && (events[i].events & POLLIN) ){
                client.recv_noRF_noACK();
            }
            if( (events[i].data.fd == tfd) && (events[i].events & POLLIN) ){
                exit_loop = true;
            }
        }
        if( exit_loop ){
           break;
        }
    }
    close(tfd);
    close(AWS_client_socket.get_sock());

    printf("\nBye MAIN FUNCTION!\n");
    exit(0);
}

