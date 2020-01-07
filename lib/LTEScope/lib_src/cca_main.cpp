#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <vector>
#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <string>
#include <poll.h>

#include "client.hh"
#include "cca_main.h"
#include "config.h"
extern bool go_exit;
extern pthread_mutex_t mutex_exit;

void* cca_main_multi(void* p){
    cca_server_t* server_t = (cca_server_t *)p; 
     
    Socket client_socket;
    int sender_id = getpid();
    Socket::Address server_address( UNKNOWN );
    client_socket.bind( Socket::Address( "0.0.0.0", 9003 ) );
    if( server_t->servIP != NULL){
	const char *servIP = server_t->servIP;
	server_address = Socket::Address( servIP, 9001 );
    }else{
	server_address = Socket::Address( "18.217.108.190", 9001 );
    }
	
    FILE* FD = fopen("./data/client_log","w+");
    Client client(client_socket, server_address, FD);
    client.set_cell_num_prb(); 
    int tfd;    //定时器描述符
    int efd;    //epoll描述符
    uint64_t tvalue;
    struct epoll_event ev, events[2];
    struct itimerspec time_intv; //用来存储时间

    tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);   //创建定时器 non-block
    if(tfd == -1) {
        printf("create timer fd fail \r\n");
	pthread_exit(NULL);
    }

    time_intv.it_value.tv_sec = server_t->con_time_s; 
    time_intv.it_value.tv_nsec = server_t->con_time_ns;
    time_intv.it_interval.tv_sec = 0;   // non periodic  
    time_intv.it_interval.tv_nsec = 0;

    efd = epoll_create(2); //创建epoll实例
    if (efd == -1) {
        printf("create epoll fail \r\n");
        close(tfd);
	pthread_exit(NULL);
    }
    
    ev.data.fd = tfd; 
    ev.events = EPOLLIN;    //监听定时器读事件，当定时器超时时，定时器描述符可读。
    epoll_ctl(efd, EPOLL_CTL_ADD, tfd, &ev); //添加到epoll监听队列中

    ev.data.fd = client_socket.get_sock();
    ev.events = EPOLLIN;    //监听定时器读事件，当定时器超时时，定时器描述符可读。
    epoll_ctl(efd, EPOLL_CTL_ADD, client_socket.get_sock(), &ev); //添加到epoll监听队列中

    bool exit_loop = false;
    client.set_blk_ack(3);
    client.init_connection();
    timerfd_settime(tfd, 0, &time_intv, NULL);  //启动定时器
    while (true){
	pthread_mutex_lock( &mutex_exit);
        if(go_exit){
            pthread_mutex_unlock( &mutex_exit);
            break;
        }
        pthread_mutex_unlock( &mutex_exit);
    
	int nfds = epoll_wait(efd, events, 2, 0);
	if(nfds < 0){
	    printf("Epoll wait failure!\n");
	    continue;
	}
	if(nfds == 0){
	    continue;
	}	
	for(int i=0;i<nfds;i++){
	    if( (events[i].data.fd == client_socket.get_sock()) &&
		   (events[i].events & POLLIN) ){
		client.recv();
	    }
	    if( (events[i].data.fd == tfd) &&
		   (events[i].events & POLLIN) ){
		int ret = read(tfd, &tvalue, sizeof(uint64_t));
		if( ret == -1){
		    printf("Error in read time fd!\n");
		}
		exit_loop = true;	
	    }
	}
	if( exit_loop){
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
    printf("garbage collection\n");
    while (true){
	int nfds = epoll_wait(efd, events, 2, 0);
	if(nfds < 0){
	    printf("Epoll wait failure!\n");
	    continue;
	}
	for(int i=0;i<nfds;i++){
	    if( (events[i].data.fd == client_socket.get_sock()) &&
		   (events[i].events & POLLIN) ){
		client.recv();
	    }
	    if( (events[i].data.fd == tfd) &&
		   (events[i].events & POLLIN) ){
		int ret = read(tfd, &tvalue, sizeof(uint64_t));
		if( ret == -1){
		    printf("Error in read time fd!\n");
		}
		exit_loop = true;	
	    }
	}
	if( exit_loop){
	   break;
	}
    }
    close(tfd);
    close(client_socket.get_sock());
    pthread_exit(NULL);
}

int cca_main(cca_server_t* server_t){
   
    Socket client_socket;
    int sender_id = getpid();
    Socket::Address server_address( UNKNOWN );
    client_socket.bind( Socket::Address( "0.0.0.0", 9003 ) );
    if( server_t->servIP != NULL){
	const char *servIP = server_t->servIP;
	server_address = Socket::Address( servIP, 9001 );
    }else{
	server_address = Socket::Address( "18.217.108.190", 9001 );
    }
	
    FILE* FD = fopen("./data/client_log","w+");
    Client client(client_socket, server_address, FD);
    client.set_cell_num_prb(); 
    int tfd;    //定时器描述符
    int tfd_QAM;    //定时器描述符
    int efd;    //epoll描述符
    uint64_t tvalue;
    struct epoll_event ev, events[3];
    struct itimerspec time_intv, time_intv_QAM; //用来存储时间

    tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);   //创建定时器 non-block
    if(tfd == -1) {
        printf("create timer fd fail \r\n");
        return 0;
    }
    tfd_QAM = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);   //创建定时器 non-block
    if(tfd == -1) {
        printf("create timer fd fail \r\n");
        return 0;
    }

    time_intv.it_value.tv_sec = server_t->con_time_s; 
    time_intv.it_value.tv_nsec = server_t->con_time_ns;
    time_intv.it_interval.tv_sec = 0;   // non periodic  
    time_intv.it_interval.tv_nsec = 0;

    time_intv_QAM.it_value.tv_sec = 0; 
    time_intv_QAM.it_value.tv_nsec = 2e8;
    time_intv_QAM.it_interval.tv_sec = 0;   // non periodic  
    time_intv_QAM.it_interval.tv_nsec = 0;


    efd = epoll_create(3); //创建epoll实例
    if (efd == -1) {
        printf("create epoll fail \r\n");
        close(tfd);
        return 0;
    }
    
    ev.data.fd = tfd; 
    ev.events = EPOLLIN;    //监听定时器读事件，当定时器超时时，定时器描述符可读。
    epoll_ctl(efd, EPOLL_CTL_ADD, tfd, &ev); //添加到epoll监听队列中
    
    ev.data.fd = tfd_QAM; 
    ev.events = EPOLLIN;    //监听定时器读事件，当定时器超时时，定时器描述符可读。
    epoll_ctl(efd, EPOLL_CTL_ADD, tfd_QAM, &ev); //添加到epoll监听队列中


    ev.data.fd = client_socket.get_sock();
    ev.events = EPOLLIN;    //监听定时器读事件，当定时器超时时，定时器描述符可读。
    epoll_ctl(efd, EPOLL_CTL_ADD, client_socket.get_sock(), &ev); //添加到epoll监听队列中

    bool exit_loop = false;
    client.init_connection();
    client.set_blk_ack(7);
    client.set_overhead_factor(0.17);

    timerfd_settime(tfd, 0, &time_intv, NULL);  //启动定时器
    timerfd_settime(tfd_QAM, 0, &time_intv_QAM, NULL);  //启动定时器
    while (true){
	pthread_mutex_lock( &mutex_exit);
        if(go_exit){
            pthread_mutex_unlock( &mutex_exit);
            break;
        }
        pthread_mutex_unlock( &mutex_exit);
    
	int nfds = epoll_wait(efd, events, 3, 0);
	if(nfds < 0){
	    printf("Epoll wait failure!\n");
	    continue;
	}
	if(nfds == 0){
	    continue;
	}	
	for(int i=0;i<nfds;i++){
	    if( (events[i].data.fd == client_socket.get_sock()) &&
		   (events[i].events & POLLIN) ){
		client.recv();
	    }
	    if( (events[i].data.fd == tfd) &&
		   (events[i].events & POLLIN) ){
		// Connection time out
		int ret = read(tfd, &tvalue, sizeof(uint64_t));
		if( ret == -1){
		    printf("Error in read time fd!\n");
		}
		exit_loop = true;	
	    }
	    if( (events[i].data.fd == tfd_QAM) &&
		   (events[i].events & POLLIN) ){
		//client.set_256QAM(true);
	    }
	}
	if( exit_loop){
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
    printf("garbage collection\n");
    while (true){
	int nfds = epoll_wait(efd, events, 2, 0);
	if(nfds < 0){
	    printf("Epoll wait failure!\n");
	    continue;
	}
	for(int i=0;i<nfds;i++){
	    if( (events[i].data.fd == client_socket.get_sock()) &&
		   (events[i].events & POLLIN) ){
		client.recv();
	    }
	    if( (events[i].data.fd == tfd) &&
		   (events[i].events & POLLIN) ){
		int ret = read(tfd, &tvalue, sizeof(uint64_t));
		if( ret == -1){
		    printf("Error in read time fd!\n");
		}
		exit_loop = true;	
	    }
	}
	if( exit_loop){
	   break;
	}
    }

    close(tfd);
    close(client_socket.get_sock());
    return 0;
}

