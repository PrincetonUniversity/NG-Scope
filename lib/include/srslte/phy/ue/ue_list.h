#ifndef SRSLTE_UE_LIST_H
#define SRSLTE_UE_LIST_H

#include "srslte/phy/ue/lte_scope.h"

// Init the rnti list
SRSLTE_API void srslte_init_ue_list(srslte_ue_list_t* q);
SRSLTE_API void srslte_update_ue_list_every_subframe(srslte_ue_list_t* q, uint32_t tti);
SRSLTE_API void fill_ue_statistics(srslte_ue_list_t* q, srslte_dci_msg_paws* dci_msg);
SRSLTE_API void srslte_enqueue_subframe_msg(srslte_dci_subframe_t* q, srslte_ue_list_t* ue_list, uint32_t tti);

// Enqueu one rnti into the list
SRSLTE_API void srslte_enqueue_ue(srslte_ue_list_t* q, uint16_t rnti, uint32_t tti, bool downlink);

// check whether the rnti is in the list or not
SRSLTE_API uint8_t srslte_check_ue_inqueue(srslte_ue_list_t* q, uint16_t rnti);

// copy active ue list from rnti list to ue_dl
SRSLTE_API void srslte_copy_active_ue_list(srslte_ue_list_t* q, srslte_active_ue_list_t* active_ue_list);

#endif
