#include <stdio.h>
#include <stdlib.h>

#include "ngscope.h"

bool go_exit = false;

int main()
{
  // hello_world();
  prog_args_t prog_args;

  args_default(&prog_args);

  prog_args.nof_rf_dev       = 1;
  prog_args.log_dl           = false;
  prog_args.log_ul           = false;
  prog_args.rnti             = 0xFFFF;
  prog_args.remote_enable    = false;
  prog_args.decode_single_ue = false;
  prog_args.decode_SIB       = false;

  prog_args.rf_index       = 0;
  prog_args.rf_freq        = 2127500000;
  prog_args.rf_freq_vec[0] = 2127500000;
  prog_args.force_N_id_2   = -1;
  prog_args.nof_decoder    = 1;
  prog_args.disable_plots  = true;

  pthread_t main_thd;
  pthread_create(&main_thd, NULL, task_scheduler_thread, (void*)(&prog_args));

  return 1;
}
