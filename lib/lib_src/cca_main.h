#ifndef CCA_MAIN
#define CCA_MAIN
typedef struct {
    char servIP[100];
    int  servPort;
    int  con_time_us;
}cca_server_t;

int  cca_main(cca_server_t* server_t);
#endif
