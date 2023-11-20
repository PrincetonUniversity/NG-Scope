#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <unistd.h>

#include "parse_args.h"
#include "task_scheduler_lib.h"
#include "task_scheduler_main.h"
extern bool go_exit;

void* task_scheduler_thread(void* p)
{
  prog_args_t* prog_args     = (prog_args_t*)p;
  int          nof_decoder   = prog_args->nof_decoder;
  int          rf_idx        = prog_args->rf_index;
  uint32_t     rf_nof_rx_ant = prog_args->rf_nof_rx_ant;
  uint32_t     max_num_samples;

  printf("TASK scheduler\n");
  // Define some important structure
  srsran_ue_sync_t ue_sync;
  srsran_rf_t      radio;
  srsran_cell_t    cell;
  srsran_ue_mib_t  ue_mib;
  srsran_ue_dl_t   ue_dl;

  /************** Setting up the UE sync buffer ******************/
  cf_t* sync_buffer[SRSRAN_MAX_PORTS] = {NULL};
  cf_t* buffers[SRSRAN_MAX_CHANNELS]  = {};

  for (int j = 0; j < rf_nof_rx_ant; j++) {
    sync_buffer[j] = srsran_vec_cf_malloc(max_num_samples);
  }
  // Set the buffer for ue_sync
  for (int p = 0; p < SRSRAN_MAX_PORTS; p++) {
    buffers[p] = sync_buffer[p];
  }
  /************** END OF setting up the UE sync buffer ******************/

  /******************* INIT the task scheduler  *********************/
  printf("INIT the Task Scheduler!\n");
  task_scheduler_init(*prog_args, &radio, &cell, &ue_sync, &ue_mib, &ue_dl, rf_nof_rx_ant, sync_buffer);
  max_num_samples = 3 * SRSRAN_SF_LEN_PRB(cell.nof_prb); /// Length in complex samples

  /*******  Define and Init the downlink decoding parameters and configurations ****/
  srsran_dl_sf_cfg_t dl_sf_cfg;
  srsran_ue_dl_cfg_t ue_dl_cfg;
  srsran_pdsch_cfg_t pdsch_cfg;
  // srsran_chest_dl_cfg_t chest_pdsch_cfg = {};

  ZERO_OBJECT(dl_sf_cfg);
  ZERO_OBJECT(ue_dl_cfg);
  ZERO_OBJECT(pdsch_cfg);

  srsran_softbuffer_rx_t rx_softbuffers[SRSRAN_MAX_CODEWORDS];
  task_init_decode_config(&dl_sf_cfg, &ue_dl_cfg, &pdsch_cfg, &cell, prog_args, rx_softbuffers);

  /* Here are some common parameters that we will use during the decoding */
  int      sf_cnt       = 0;
  uint32_t sfn          = 0;
  uint32_t last_tti     = 0;
  uint32_t tti          = 0;
  bool     decode_pdcch = false;
  uint32_t sf_idx       = 0;
  int      ret          = 0;

  /**********   THE MAIN LOOP    *********/
  printf("Enter the Main Loop ....\n");
  while (!go_exit && (sf_cnt < prog_args->nof_subframes || prog_args->nof_subframes == -1)) {
    /*  Get the subframe data and put it into the buffer */
    ret = srsran_ue_sync_zerocopy(&ue_sync, buffers, max_num_samples);

    if (ret < 0) {
      ERROR("Error calling srsran_ue_sync_work()");
    } else if (ret == 1) {
      sf_idx = srsran_ue_sync_get_sfidx(&ue_sync);
      sf_cnt++;

      /********************* SFN handling *********************/
      if ((sf_idx == 0) || (decode_pdcch == false)) {
        // update SFN when sf_idx is 0
        uint32_t sfn_tmp = 0;
        task_ue_mib_decode_sfn(&ue_mib, &cell, &sfn_tmp, decode_pdcch);

        // if the decoded snf is non-zero
        if (sfn_tmp > 0) {
          decode_pdcch = true;
          if (sfn != sfn_tmp) {
            sfn = sfn_tmp;
            printf("UPDATE SFN: current sfn-> %d decoded sfn-> %d\n", sfn, sfn_tmp);
            printf("******   DCI decoding start for Cell %d    ******\n", rf_idx);
            printf("**************************************************");
          }
        }
      }
      /******************* END OF SFN handling *******************/

      tti = sfn * 10 + sf_idx;

      /***************** Decode the PDCCH *********/
      if (decode_pdcch) { // We only decode when we got the SFN
        if ((last_tti != 10239) && (last_tti + 1 != tti)) {
          printf("Last tti:%d current tti:%d\n", last_tti, tti);
        }
        last_tti = tti;

        uint32_t mi_set_len;
        mi_set_len = 1;
        // Blind search PHICH mi value
        if (mi_set_len == 1) {
          srsran_ue_dl_set_mi_auto(&ue_dl);
        }

        ret = 0;
        if ((ret = srsran_ue_dl_decode_fft_estimate(&ue_dl, &dl_sf_cfg, &ue_dl_cfg)) < 0) {
          return NULL;
        }

        /*************  Handing the SFN and sf_idx ***********/
        if ((sf_idx == 9)) {
          sfn++; // we increase the sfn incase MIB decoding failed
          if (sfn == 1024) {
            sfn = 0;
          }
        }
      }
    }
  }

  //--> Deal with the exit and free memory

  // wait until its our turn to close
  printf("wait for scheduler to be ready!\n");

  // wait_for_scheduler_ready_to_close(rf_idx);
  // task_scheduler_up[rf_idx] = false;

  /* Wait for the decoder thread to finish*/
  /* for (int i = 0; i < nof_decoder; i++) { */
  /*   // Tell the decoder thread to exit in case */
  /*   // they are still waiting for the signal */
  /*   printf("Signling %d-th decoder!\n", i); */
  /*   pthread_cond_signal(&sf_buffer[rf_idx][i].sf_cond); */
  /* } */

  /* for (int i = 0; i < nof_decoder; i++) { */
  /*   pthread_join(dci_thd[i], NULL); */
  /* } */

  // free the ue dl and the related buffer
  /* for (int i = 0; i < nof_decoder; i++) { */
  /*   srsran_ue_dl_free(&dci_decoder[i].ue_dl); */
  /*   // free the buffer */
  /*   for (int j = 0; j < rf_nof_rx_ant; j++) { */
  /*     free(sf_buffer[rf_idx][i].IQ_buffer[j]); */
  /*   } */
  /* } */

  // free the ue mib decoder
  srsran_ue_mib_free(&ue_mib);

  // free the ue_sync
  srsran_ue_sync_free(&ue_sync);

  // free the sync buffer
  for (int j = 0; j < rf_nof_rx_ant; j++) {
    free(sync_buffer[j]);
  }

  // Stop the Radio USRP
  radio_stop(&radio);

  // close the tmp buffer handling thread
  // pthread_join(tmp_buf_thd, NULL);

  // task_sf_ring_buffer_free(&task_tmp_buffer[rf_idx]);
  //
  //     for(int i=0; i<MAX_TMP_BUFFER; i++){
  //         for (int j = 0; j < SRSRAN_MAX_PORTS; j++) {
  //             free(task_tmp_buffer.sf_buf[i].IQ_buffer[j]);
  //         }
  //     }
  //

  /* pthread_mutex_lock(&scheduler_close_mutex); */
  /* task_scheduler_closed[rf_idx] = true; */
  /* pthread_mutex_unlock(&scheduler_close_mutex); */

  printf("TASK-Scheduler of %d-th RF devices CLOSED!\n", rf_idx);

  return NULL;
}
