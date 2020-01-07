#ifndef CCA_MAIN
#define CCA_MAIN
typedef struct {
    char servIP[100];
    int  servPort;
    int  con_time_s;
    int  con_time_ns;
}cca_server_t;

int  cca_main(cca_server_t* server_t);
void*  cca_main_multi(void* p);
#endif
