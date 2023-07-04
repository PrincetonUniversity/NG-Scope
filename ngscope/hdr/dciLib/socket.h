#ifndef NGSCOPE_SOCKET_H
#define NGSCOPE_SOCKET_H

#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/socket.h>

#include "srsran/srsran.h"

#define MAX_CLIENT 5
#ifdef __cplusplus
extern "C" {
#endif
int connectServer();

int accept_slave_connect(int* server_fd, int* client_fd_vec, int portNum);

#endif
