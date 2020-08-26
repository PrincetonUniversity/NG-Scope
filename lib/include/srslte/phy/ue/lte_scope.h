#ifndef SRSLTE_LTE_SCOPE_H
#define SRSLTE_LTE_SCOPE_H

#include <stdbool.h>

#include "srslte/phy/ch_estimation/chest_dl.h"
#include "srslte/phy/dft/ofdm.h"
#include "srslte/phy/common/phy_common.h"

#include "srslte/phy/phch/dci.h"
#include "srslte/phy/phch/pcfich.h"
#include "srslte/phy/phch/pdcch.h"
#include "srslte/phy/phch/pdsch.h"
#include "srslte/phy/phch/pmch.h"
#include "srslte/phy/phch/pdsch_cfg.h"
#include "srslte/phy/phch/phich.h"
#include "srslte/phy/phch/ra.h"
#include "srslte/phy/phch/regs.h"
#include "srslte/phy/sync/cfo.h"

#include "srslte/phy/ue/ue_dl.h"

#include "srslte/phy/utils/vector.h"
#include "srslte/phy/utils/debug.h"

#include "srslte/phy/ue/LTESCOPE_GLOBAL.h"

#include "srslte/config.h"

#define UE_INACTIVE_TIME_LIMIT 2000 // One UE that has been inactive for 3000-ms/3-s are removed
#define UE_REAPPEAR_TIME_LIMIT 30   // The same rnti must appear within 30-ms to be an active UE
#define UE_REAPPEAR_NO_LIMIT 2      // The rnti must appear for 2 times
#define HIGH_PROB_THR 90	    // The higher threshold for decoding probability (strict for entering)
#define DECODE_PROB_THR 75	    // The lower threshold of probability for letting in the message of active UE
#define MAX_CANDIDATES_ALL 180
// Threshold for LLR -- filtering out those locations with low decoding confidence
#define LLR_THR 0.7
#define NOF_UE_ALL_FORMATS 6
const static srslte_dci_format_t ue_all_formats[] = {
    SRSLTE_DCI_FORMAT0,
    SRSLTE_DCI_FORMAT1,
    SRSLTE_DCI_FORMAT1A,
    SRSLTE_DCI_FORMAT1B,
    SRSLTE_DCI_FORMAT1C,
    SRSLTE_DCI_FORMAT2,
    SRSLTE_DCI_FORMAT2A
};

typedef struct SRSLTE_API {
  // Yaxiong's Modification: RNTI list
  uint8_t   active_ue_list[65536];
  uint16_t  ue_cnt[65536];
  uint16_t  ue_dl_cnt[65536];
  uint16_t  ue_ul_cnt[65536];

  uint32_t  ue_last_active[65536];
  uint32_t  ue_enter_time[65536];

  uint16_t  max_freq_ue;
  uint16_t  max_dl_freq_ue;
  uint16_t  max_ul_freq_ue;
  uint16_t  nof_active_ue;
} srslte_ue_list_t;

typedef struct SRSLTE_API {
    uint8_t   active_ue_list[65536];
    uint16_t  max_freq_ue;
    uint16_t  nof_active_ue;
} srslte_active_ue_list_t;

typedef struct SRSLTE_API {
  uint32_t L;    // Aggregation level
  uint32_t ncce; // Position of first CCE of the dci
  float    mean_llr;
  bool	   checked;
} srslte_dci_location_paws_t;

typedef struct SRSLTE_API {
    srslte_dci_location_paws_t loc_L0[8];
    srslte_dci_location_paws_t loc_L1[4];
    srslte_dci_location_paws_t loc_L2[2];
    srslte_dci_location_paws_t loc_L3[1];
} srslte_dci_location_blk_paws_t;



typedef struct SRSLTE_API{
    /* RNTI Format SFN FN*/
    uint16_t    rnti;
    uint32_t    tti;
    bool	downlink;
    bool	off_tree; // decoded from tree based structure

    float       decode_prob;  // decoding probability

    srslte_dci_format_t format;
    /* Location of the dci message inside the control channel*/
    uint32_t L;    // Aggregation level
    uint32_t ncce; // Position of first CCE of the dci
    int      nof_cce; // Total number of CCE inside the control channel
    uint32_t cfi;    // CFI indicator (number of OFDM symbols the control channel has)
    uint32_t cif;    // carrier indicator field (indicating the index of carrier)

    /* Information contained inside the dci message including
    HARQ pid, the MCS index, redundancy version and NDI */

    uint32_t harq_pid; // ID of the HARQ
    uint32_t nof_prb;  // nof prb allocated

    uint32_t mcs_idx_tb1;
    int      rv_tb1;
    bool     ndi_tb1;
    int      tbs_tb1;
    int      tbs_hm_tb1;

    uint32_t mcs_idx_tb2;
    int      rv_tb2;
    bool     ndi_tb2;
    int      tbs_tb2;
    int      tbs_hm_tb2;

    uint16_t  max_freq_ue;
    uint16_t  max_dl_freq_ue;
    uint16_t  max_ul_freq_ue;

    uint32_t  max_freq_ue_cnt;

    uint16_t  nof_active_ue;
    uint8_t   active;
    uint32_t  my_ul_cnt;
    uint32_t  my_dl_cnt;
}srslte_dci_msg_paws;

typedef struct SRSLTE_API{
    srslte_dci_msg_paws downlink_msg[MAX_NOF_MSG_PER_SF];  
    uint32_t	dl_msg_cnt;
    srslte_dci_msg_paws uplink_msg[MAX_NOF_MSG_PER_SF];  
    uint32_t	ul_msg_cnt;
}srslte_dci_subframe_t;

typedef struct SRSLTE_API{
    uint16_t cell_dl_prb;  // Number of total busy dl prbs of this cell 
    uint16_t cell_ul_prb;  // Number of total busy dl prbs of this cell 
    uint16_t ue_dl_prb;    // Allocated prbs for the ue downlink traffic
    uint16_t ue_ul_prb;	   // Allocated prbs for the ue uplink traffic
}srslte_subframe_bw_usage;

/* Yaxiong's modification -- dci decoder */
SRSLTE_API int srslte_dci_decoder_yx(srslte_ue_dl_t* q,
                                    srslte_active_ue_list_t* active_ue_list,
                                    srslte_dci_subframe_t* dci_msg_out,
                                    uint32_t    tti);
#endif
