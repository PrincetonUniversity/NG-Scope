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

#include "srsran/common/crash_handler.h"

#include "srsran/srsran.h"
#include "ngscope/hdr/dciLib/dci_sink_client.h"
#include "ngscope/hdr/dciLib/dci_sink_sock.h"
#include "ngscope/hdr/dciLib/dci_sink_dci_recv.h"
#include "ngscope/hdr/dciLib/dci_sink_ring_buffer.h"

bool go_exit = false;
ngscope_dci_sink_CA_t dci_CA_buf;

void sig_int_handler(int signo)
{
  printf("SIGINT received. Exiting...\n");
  if (signo == SIGINT) {
    go_exit = true;
  } else if (signo == SIGSEGV) {
    exit(1);
  }
}

int main(int argc, char** argv){
    srsran_debug_handle_crash(argc, argv);

    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigprocmask(SIG_UNBLOCK, &sigset, NULL);
    signal(SIGINT, sig_int_handler);

    pthread_t test_thd;
    pthread_create(&test_thd, NULL, dci_sink_client_thread, NULL);
	pthread_join(test_thd, NULL);

	printf("abs");
    return 1;
}
