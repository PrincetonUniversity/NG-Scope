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
#include <string.h>
#include <poll.h>
#include "usrp_sock.h"
extern int RNTI_ID_DONE;
extern bool go_exit;

int accept_slave_connect(int* server_fd, int* client_fd_vec){
    int server_sockfd;//服务器端套接字
    int client_sockfd;//客户端套接字
    int nof_sock = 0;
    struct sockaddr_in my_addr;   //服务器网络地址结构体
    struct sockaddr_in remote_addr; //客户端网络地址结构体
    unsigned int sin_size;
    memset(&my_addr,0,sizeof(my_addr)); //数据初始化--清零
    my_addr.sin_family=AF_INET; //设置为IP通信
    my_addr.sin_addr.s_addr=INADDR_ANY;//服务器IP地址--允许连接到所有本地地址上
    my_addr.sin_port=htons(6767); //服务器端口号

    /*创建服务器端套接字--IPv4协议，面向连接通信，TCP协议*/
    if((server_sockfd=socket(PF_INET,SOCK_STREAM,0))<0)
    {
        perror("socket error");
        return 0;
    }

    *server_fd = server_sockfd;
    int flags = fcntl(server_sockfd, F_GETFL, 0);
    fcntl(server_sockfd, F_SETFL, flags | O_NONBLOCK);

    /*将套接字绑定到服务器的网络地址上*/
    if(bind(server_sockfd,(struct sockaddr *)&my_addr,sizeof(struct sockaddr))<0)
    {
        perror("bind error");
        return 0;
    }

    /*监听连接请求--监听队列长度为5*/
    if(listen(server_sockfd,5)<0)
    {
        perror("listen error");
        return 0;
    };

    sin_size=sizeof(struct sockaddr_in);
    int start = time(NULL);
    int ellaps = 0;
    while (true){
        /*等待客户端连接请求到达*/
        client_sockfd=accept(server_sockfd,(struct sockaddr *)&remote_addr, &sin_size);
        if(client_sockfd > 0){
            printf("accept client %s/n",inet_ntoa(remote_addr.sin_addr));
            client_fd_vec[nof_sock] = client_sockfd;
            nof_sock += 1;
        }
        ellaps = time(NULL);
        if( (ellaps - start > MAX_WAIT_TIME_S) || (nof_sock >= MAX_NOF_USRP))
            break;
    }
    return nof_sock;
}

int connect_server(char *masterIP){
    int client_sockfd;
    struct sockaddr_in remote_addr; //服务器端网络地址结构体
    memset(&remote_addr,0,sizeof(remote_addr)); //数据初始化--清零
    remote_addr.sin_family=AF_INET; //设置为IP通信
    if(masterIP == NULL){
	remote_addr.sin_addr.s_addr=inet_addr("192.168.1.10");//服务器IP地址
    }else{
	remote_addr.sin_addr.s_addr=inet_addr( masterIP );//服务器IP地址
    }
    remote_addr.sin_port=htons(6767); //服务器端口号

    /*创建客户端套接字--IPv4协议，面向连接通信，TCP协议*/
    if((client_sockfd=socket(PF_INET,SOCK_STREAM,0))<0)
    {
        perror("socket error");
        return 0;
    }

    /*将套接字绑定到服务器的网络地址上*/
    if(connect(client_sockfd,(struct sockaddr *)&remote_addr,sizeof(struct sockaddr))<0)
    {
        perror("connect error");
        return 0;
    }
    return client_sockfd;
}


void info_server(int client_fd, bool flag){
    char sock_buf[100] ={0};
    sock_buf[0] = 0xFF;
    if(flag)
	sock_buf[1] = 0x11;
    else
	sock_buf[1] = 0xFF;
	
    /**** tell all slaves that our RF is ready *****/
    while(true){
        if(send(client_fd, sock_buf, 2, 0) < 0){
            perror("SOCKET send error\n");
        }else{
            return;
        }
    }
}

bool hear_server(int client_fd, usrp_cmd_t cmd){
    int len;
    char sock_buf[100] ={0};

    /**** receive answer from all our server *****/
    while(true){
        len = recv(client_fd, sock_buf, 100, 0);
        if( (len > 0) && (sock_buf[0] = 0xFF)){
	    switch (cmd){
		case RF_READY:
		    if( sock_buf[1] == 0x11)
			return true;
		    else
			return false;		
		case DCI_DONE:
		    if( sock_buf[1] == 0x22)
			return true;
		    else
			return false;		
		case CON_CLOSE:
		    if( sock_buf[1] == 0x33)
			return true;
		    else
			return false;		
		default:
		    printf("Command not supported!\n");
		    return false;
	    }
	}
    }
    return true;
}

void sync_server(int client_fd, usrp_cmd_t cmd){
    if(hear_server(client_fd, cmd)){
	info_server(client_fd, true);
    }else{
	info_server(client_fd, false);
    }
    return;
}

void* handle_server_cmd(void *fd){
    int sock_fd = *(int *)fd;

    int len;
    char sock_buf[100] ={0};
    struct pollfd poll_fds[ 1 ];

    poll_fds[ 0 ].fd = sock_fd;
    poll_fds[ 0 ].events = POLLIN;
    struct timespec timeout;
    timeout.tv_sec = 10;
    timeout.tv_nsec = 0;

    while(true){
        ppoll( poll_fds, 1, &timeout, NULL );
        if ( poll_fds[ 0 ].revents & POLLIN ) {
            len = recv(sock_fd, sock_buf, 100, 0);
            if( (len > 0) && (sock_buf[0] = 0xFF)){
		switch (sock_buf[1]){
		    case 0x22:
			RNTI_ID_DONE = 1;
			//info_server(sock_fd, true);
			break;
		    case 0x33:
			RNTI_ID_DONE = 1;
			go_exit = true;
			info_server(sock_fd, true);
			close(sock_fd);
			break;
		    default:
			printf("Un-defined command type!\n");
			return NULL;
		}
		break;
            }
        }
    }
    
    return NULL;
}


void info_slave(int nof_sock, int* client_fd, usrp_cmd_t cmd){
    char sock_buf[100] ={0};
    sock_buf[0] = 0xFF;
    switch (cmd){
	case RF_READY:
	    sock_buf[1] = 0x11;
	    break;
	case DCI_DONE:
	    sock_buf[1] = 0x22;
	    break;
	case CON_CLOSE:
	    sock_buf[1] = 0x33;
	    break;
	default: 
	    printf("Un-defined command type!\n");
	    return;
    }

    /**** inform all slaves our command *****/
    if(nof_sock > 0){
        for (int i = 0; i < nof_sock; i++){
            if(send(client_fd[i], sock_buf, 2, 0) < 0){
                perror("SOCKET send error\n");
                return;
            }
        }
    }else{
        printf("NO slave USRPs!\n");
        return;
    }
}

bool hear_slave(int nof_sock, int* client_fd){
    int len;
    char sock_buf[100] ={0};
    
    bool sync_flag = true;
    /**** receive answer from all our slaves *****/
    if(nof_sock > 0){
        for (int i = 0; i < nof_sock; i++){
	    printf("Synchronizing with %d-th USRP\n",i);
            while(true){
		if(go_exit)
		    break;
                len = recv(client_fd[i], sock_buf, 100, 0);
                if( (len > 0) && (sock_buf[0] = 0xFF)){
		    if(sock_buf[1] == 0x11){
			printf("Succeed!\n");
			break;
		    }else if(sock_buf[1] == 0xFF){
			printf("Failed!\n");
			break;
		    }
		}
            }
        }
    }else{
	sync_flag = false;
        printf("NO slave USRPs\n");
    }
    return sync_flag;
}

bool sync_slave(int nof_sock, int* client_fd, usrp_cmd_t cmd){
    if(nof_sock > 0){
        info_slave(nof_sock, client_fd, cmd);
        if(!hear_slave(nof_sock, client_fd))
	    return false;
    }else{
	printf("NO slave USRP\n");
	return false;
    }
    return true;
}

void close_sockets(int nof_sock, int server_fd, int* client_fd){
    close(server_fd);
    if(nof_sock > 0){
        for (int i = 0; i < nof_sock; i++){
            close(client_fd[i]);
        }
    }else{
        printf("NO slave USRPs\n");
        return;
    }
}

