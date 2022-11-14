#ifndef THREAD_EXIT_H
#define THREAD_EXIT_H
#include <stdio.h>

#include "ngscope_def.h"
bool wait_for_ALL_RF_DEV_close();
bool wait_for_scheduler_ready_to_close(int rf_idx);
bool wait_for_decoder_ready_to_close(int  rf_idx, int  decoder_idx);
#endif

