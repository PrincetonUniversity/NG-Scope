#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <poll.h>

int main(){
    int client_sockfd;
    char* masterIP = NULL;

    struct sockaddr_in remote_addr; //服务器端网络地址结构体
    memset(&remote_addr,0,sizeof(remote_addr)); //数据初始化--清零
    remote_addr.sin_family=AF_INET; //设置为IP通信

    if(masterIP == NULL){
        //remote_addr.sin_addr.s_addr=inet_addr("127.0.0.1");//服务器IP地址
        remote_addr.sin_addr.s_addr=inet_addr("192.168.1.52");//服务器IP地址
    }else{
        remote_addr.sin_addr.s_addr=inet_addr( masterIP );//服务器IP地址
    }
    remote_addr.sin_port=htons(6767); //服务器端口号

    /*创建客户端套接字--IPv4协议，面向连接通信，TCP协议*/
    if((client_sockfd=socket(PF_INET,SOCK_STREAM,0))<0)
    {
        printf("socket error");
        return 0;
    }

    /*将套接字绑定到服务器的网络地址上*/
    if(connect(client_sockfd,(struct sockaddr *)&remote_addr,sizeof(struct sockaddr))<0)
    {
        printf("connect error");
        return 0;
    }
    float a = 0.1;

    send(client_sockfd, &a, sizeof(float), 0);
    return client_sockfd;
}
