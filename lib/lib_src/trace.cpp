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
#include "trace.h"
using namespace std;
void init_trace(){
    /* clean the log */
    char clean_cmd[256];
    strcpy(clean_cmd, "sudo rm ./data/*");
    if(system(clean_cmd)){
        printf("clean failed!\n");
    }
    FILE *fp;
    fp = fopen("./data/config_log","w+");
    fclose(fp);
    return;
}
int move_file(char* sourceFile, char* destFile){
    ifstream source(sourceFile, ios::in);
    if( !source.is_open()){
        printf("Error open source file!\n");
        return 0;
    }
    ofstream dest(destFile, ios::out | ios::trunc);
    if( !dest.is_open()){
        printf("Error open destination file!\n");
        return 0;
    }
    dest << source.rdbuf();

    source.close();
    dest.close();
    return 1;
}

int copy_usrp_trace(int index, int usrp_idx){
    char source[256] = {0};
    char dest[256] = {0};
    strcpy(source, "./data/DCI_dl_log.txt");
    sprintf(dest,"./data/DCI_dl_log_%d_USRP%d.txt",index,usrp_idx);
    move_file(source, dest);

    strcpy(source, "./data/DCI_ul_log.txt");
    sprintf(dest,"./data/DCI_ul_log_%d_USRP%d.txt",index,usrp_idx);
    move_file(source, dest);
    return 1;
}

int copy_sock_trace(int index, int usrp_idx){
    char source[256] = {0};
    char dest[256] = {0};
    printf("MOVING SOCK TRACE\n\n\n");
    strcpy(source, "./data/client-ack-log");
    sprintf(dest,"./data/client-ack-log_%d_USRP%d",index, usrp_idx);
    move_file(source, dest);
    return 1;
}

int copy_sock_trace_multi(int loop_idx, int tr_idx){
    char source[256] = {0};
    char dest[256] = {0};
    printf("MOVING SOCK TRACE\n\n\n");
    strcpy(source, "./data/client-ack-log");
    sprintf(dest,"./data/client-ack-loop_%d_tr_%d",loop_idx, tr_idx);
    move_file(source, dest);
    return 1;
}


