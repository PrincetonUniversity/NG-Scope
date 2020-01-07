#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <string>
#include <poll.h>

#include "acker.hh"
#include "serv_sock.h"
#include "config.h"

extern bool go_exit;

void remote_server_1ms(sock_parm_t* sock_config){
   
    int pkt_intval = sock_config->pkt_intval;
    int con_time_s = sock_config->con_time_s;
    int nof_pkt	   = sock_config->nof_pkt;
 
    Socket client_socket;
    bool server;
    int sender_id = getpid();
    Socket::Address server_address( UNKNOWN );
    server = false;
    
    client_socket.bind( Socket::Address( "0.0.0.0", 9003 ) );
    
    if( sock_config->servIP != NULL){
	const char *servIP = sock_config->servIP;
	server_address = Socket::Address( servIP, 9001 );
    }else{
	server_address = Socket::Address( "18.217.108.190", 9001 );
    }
	
    if( sock_config->if_name != NULL){
	char *if_name = sock_config->if_name;
	client_socket.bind_to_device(if_name);
    }
 
    Acker acker(client_socket, client_socket, server_address, server, sender_id );

    acker.set_pkt_intval(pkt_intval);
    acker.set_con_time(con_time_s);
    acker.set_nof_pkt(nof_pkt);

    printf("notify_config\n");
    acker.notify_config(false);
    printf("recv\n");
    acker.recv_noACK();
    close(client_socket.get_sock());
    return;
}

