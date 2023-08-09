#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <unistd.h>

#include "srsran/srsran.h"
#include "srsran/common/crash_handler.h"
#include "srsran/phy/rf/rf.h"
#include "sibscope/headers/cell_scan.h"

#define MHZ 1000000
#define SAMP_FREQ 1920000
#define FLEN 9600
#define FLEN_PERIOD 0.005

#define MAX_EARFCN 1000

int cellsearch_recv_wrapper(void* h, void* data, uint32_t nsamples, srsran_timestamp_t* t)
{
  DEBUG(" ----  Receive %d samples  ---- ", nsamples);
  return srsran_rf_recv_with_time((srsran_rf_t*)h, data, nsamples, 1, NULL, NULL);
}


int cell_scan(srsran_rf_t * rf,
            cell_search_cfg_t * cell_detect_config,
            struct cells * results,
            int max_cells,
            int band)
{
    int n;
    srsran_ue_cellsearch_t cs;
    srsran_ue_cellsearch_result_t found_cells[3];
    int nof_freqs;
    srsran_earfcn_t channels[MAX_EARFCN];
    uint32_t freq;
    uint32_t n_found_cells = 0;
    extern bool go_exit;
    srsran_cell_t cell;
    int i;


    /* Set gain to 50 */
    srsran_rf_set_rx_gain(rf, 50);

    /* Initialize CellSearch structure */
    if (srsran_ue_cellsearch_init(&cs, cell_detect_config->max_frames_pss, cellsearch_recv_wrapper, (void*)rf)) {
        ERROR("Error initiating UE cell detect");
        return -1;
    }

    if (cell_detect_config->max_frames_pss) {
        srsran_ue_cellsearch_set_nof_valid_frames(&cs, cell_detect_config->nof_valid_pss_frames);
    }

    /* Supress RF messages */
    srsran_rf_suppress_stdout(rf);
    /* Find frequencies for the selected band with no earfcn limits (-1) */
    nof_freqs = srsran_band_get_fd_band(band, channels, -1, -1, MAX_EARFCN);
    if (nof_freqs < 0) {
        ERROR("Error getting EARFCN list");
        return -1;
    }

    /* For each of the frequencies in the selected band */
    for (freq = 0; freq < nof_freqs && !go_exit && n_found_cells < max_cells; freq++) {
        printf(
            "[%3d/%d]: Looking for PSS at %.2f MHz...\n", freq, nof_freqs, channels[freq].fd);
        fflush(stdout);

        /* Clear CellSearchResult structures (3) */
        bzero(found_cells, 3 * sizeof(srsran_ue_cellsearch_result_t));
        
        /* Adjust Radio parameters */
        /* Set the RX frequency */
        srsran_rf_set_rx_freq(rf, 0, (double)channels[freq].fd * MHZ);
        /* Set sampling rate */
        srsran_rf_set_rx_srate(rf, SRSRAN_CS_SAMP_FREQ);

        /* Start RX streaming */
        srsran_rf_start_rx_stream(rf, false);

        /* Search for cells */
        n = srsran_ue_cellsearch_scan(&cs, found_cells, NULL);
        if (n < 0) {
            ERROR("Error searching cell");
            return -1;
        } else if (n > 0) {
            for (i = 0; i < 3; i++) {
                /* If pagging success rate is larger than 2, try to decode the MIB */
                if (found_cells[i].psr > 2.0) {
                    /* Set cell parameters for MIB decoding */
                    cell.id = found_cells[i].cell_id;
                    cell.cp = found_cells[i].cp;
                    int ret = rf_mib_decoder(rf, 1, cell_detect_config, &cell, NULL);
                    if (ret == SRSRAN_UE_MIB_FOUND) {
                        /* Cell is added to the list of valid detected cells if the number of ports is larger than 0 */
                        if (cell.nof_ports > 0) {
                            results[n_found_cells].cell = cell;
                            results[n_found_cells].freq = channels[freq].fd;
                            results[n_found_cells].dl_earfcn = channels[freq].id;
                            results[n_found_cells].power = found_cells[i].peak;
                            n_found_cells++;
                        }
                    }
                    else if (ret < 0) {
                        ERROR("Error decoding MIB");
                        return -1;
                    }
                }
            }
        }
    }

    srsran_ue_cellsearch_free(&cs);
    srsran_rf_stop_rx_stream(rf);
    return n_found_cells;
}