#ifndef SERV_SOCK
#define SERV_SOCK
typedef struct {
    int  pkt_intval;  // inter-packet interval
    int  con_time_s;  // connection time (s)
    int  nof_pkt;     // number of packets to be received

    int  local_rf_enable; // whether the local RF is enabled
    char servIP[100];     // server ip 
    //char servIP[50];     // server ip 
    char *if_name;    // interface name
}sock_parm_t;

void remote_server_1ms(sock_parm_t* sock_parm);
#endif
