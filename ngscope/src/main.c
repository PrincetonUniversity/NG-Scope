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
#include <getopt.h>
#include "srsran/common/crash_handler.h"
#include "srsran/srsran.h"
#include "ngscope/hdr/dciLib/radio.h"
#include "ngscope/hdr/dciLib/task_scheduler.h"
#include "ngscope/hdr/dciLib/dci_decoder.h"
#include "ngscope/hdr/dciLib/load_config.h"
#include "ngscope/hdr/dciLib/ngscope_main.h"
#include "ngscope/hdr/dciLib/asn_decoder.h"

#define DEFAULT_SIB_OUTPUT "decoded_sibs"
#define DEFAULT_DCI_OUTPUT "dci_output"


bool go_exit = false;

void sig_int_handler(int signo)
{
  printf("SIGINT received. Exiting...\n");

  if (signo == SIGINT) {
    go_exit = true;
  } else if (signo == SIGSEGV) {
    exit(1);
  }
}

void print_help()
{
  printf("NG-Scope usage: ngscope [OPTIONS]\n");
  printf("  -c <Config File>\t\t[Mandatory] NG-Scope configuration file.\n");
  printf("  -s <SIB Output File>\t\t[Optional] Ouput file where the decoded SIB messages will be stored.\n");
  printf("  -o <DCI Output Folder>\t[Optional] Ouput folder where DCI logs will be stored.\n");
  printf("  -h\t\t\t\t[Optional] Show this menu.\n");
}

int main(int argc, char** argv){
    ngscope_config_t config;
    int c;
    /* Variables tahtw ill hold the command line arguments */
    char * config_path = NULL;
    char * sib_path = NULL;
    char * out_path = NULL;

    /* Parsing command line arguments */
    while ((c = getopt (argc, argv, "c:s:o:h")) != -1) {
      switch (c) {
        case 'c':
          config_path = optarg;
          break;
        case 's':
          sib_path = optarg;
          break;
        case 'o':
          out_path = optarg;
          break;
        case 'h':
          print_help();
          return 0;
        case '?':
          if (optopt == 'c')
            fprintf (stderr, "Option -%c requires an argument.\n", optopt);
          if (optopt == 's')
            fprintf (stderr, "Option -%c requires an argument.\n", optopt);
          if (optopt == 'o')
            fprintf (stderr, "Option -%c requires an argument.\n", optopt);
          else {
            print_help();
            return 1;
          }
        default:
          print_help();
          return 1;
        }
    }
    /* Check that the config file has been provided */
    if(config_path == NULL) {
      print_help();
      return 1;
    }
    printf("Configuration file: %s\n", config_path);
    /* Check SIB output */
    if(sib_path == NULL) {
      sib_path = DEFAULT_SIB_OUTPUT;
      printf("SIB output file not specified (using '%s')\n", sib_path);
    }
    else
      printf("Decoded SIB file: %s\n", sib_path);
    /* Check DCI output */
    if(out_path == NULL) {
      out_path = DEFAULT_DCI_OUTPUT;
      printf("DCI logs folder not specified (using '%s')\n", out_path);
    }
    else
      printf("DCI logs folder: %s\n", out_path);

    /* Signal handlers */
    srsran_debug_handle_crash(argc, argv);
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigprocmask(SIG_UNBLOCK, &sigset, NULL);
    signal(SIGINT, sig_int_handler);


    /* Load the configurations */
    ngscope_read_config(&config, config_path);
    /* Set DCI logs output folder path  */
    config.dci_logs_path = out_path;
    config.sib_logs_path = sib_path;

    ngscope_main(&config);
    return 1;
}
