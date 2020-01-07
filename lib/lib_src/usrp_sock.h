#include <stdint.h>

#define MAX_NOF_USRP 4
#define MAX_WAIT_TIME_S 5
typedef enum{
    RF_READY,
    DCI_DONE,
    CON_CLOSE
}usrp_cmd_t;

void info_server(int client_fd, bool flag);
int accept_slave_connect(int* server_fd, int* client_fd_vec);
int connect_server(char* masterIP);

/* slave function */
void sync_server(int client_fd, usrp_cmd_t cmd);
void* handle_server_cmd(void *fd);

/* master function */
bool sync_slave(int nof_sock, int* client_fd, usrp_cmd_t cmd);
void close_sockets(int nof_sock, int server_fd, int* client_fd);

