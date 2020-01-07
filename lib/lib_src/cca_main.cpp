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

#include "client.hh"
#include "cca_main.h"
#include "config.h"

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
	
    Client client(client_socket, server_address);

    // tick first to init the connection
    struct pollfd poll_fds[ 1 ];
    poll_fds[ 0 ].fd = client_socket.get_sock();
    poll_fds[ 0 ].events = POLLIN;
    struct timespec timeout;
    timeout.tv_sec = 1;
    timeout.tv_nsec = 0;

    while (true){
	/* wait for incoming packet OR expiry of timer */
	ppoll( poll_fds, 1, &timeout, NULL );
	if ( poll_fds[ 0 ].revents & POLLIN ) {
	    client.recv();
	}
    }
    close(client_socket.get_sock());
    return 0;
}

