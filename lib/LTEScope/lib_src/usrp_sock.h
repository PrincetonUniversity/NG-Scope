#include <stdint.h>

#define MAX_USRP_NUM 1
#define MAX_WAIT_TIME_S 10
typedef enum{
    RF_READY,
    DCI_DONE,
    CON_CLOSE
}usrp_cmd_t;


int accept_slave_connect(int* server_fd, int* client_fd_vec);
int connect_server(char* masterIP);


