#ifndef _ULSCHE_H_
#define _ULSCHE_H_

#include <iostream>
#include <vector>
#include <mutex>
#include <memory>
#include <map>
#include <algorithm>
// #include <tuple>

#include "srsran/srsran.h"
#include "srsran/asn1/rrc/si.h"
#include "srsran/asn1/rrc/bcch_msg.h"
#include "srsran/support/srsran_assert.h"
#include "srsran/asn1/asn1_utils.h"

#include "sfTask.h"
#include "common_type_define.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DCIUL {
        srsran_pusch_grant_t pusch_grant;
        srsran_dci_ul_t dci_ul;
        int nof_ack;
}DCIUL;

class ULSch{
public:
    ULSch() { }
    ~ULSch() { }
    
    void pushULDCI(uint32_t tti, int tdd_config, uint16_t rnti, srsran_dci_ul_t dci_ul);
    void pushULDCI(uint32_t tti, int tdd_config, uint16_t rnti, srsran_pusch_grant_t pusch_grant, srsran_dci_ul_t dci_ul, int nof_ack);
    std::vector<std::pair<uint16_t, srsran_dci_ul_t>> getUlDCIVec(uint32_t tti);
    std::vector<std::pair<uint16_t, DCIUL>> getULDCIstruct(uint32_t tti);
    void deleteDciUL(uint32_t tti);

    void addUlAckNum(uint32_t tti, int tdd_config, uint16_t rnti);
    int getUlAckNum(uint32_t tti, uint16_t rnti);
    void deleteUlAckNum(uint32_t tti);

private:
    std::mutex ulschMutex;
    std::mutex ulAckMutex;
    std::map <uint32_t, std::vector<std::pair<uint16_t, srsran_dci_ul_t>>> ulDCIDatabase;
    std::map <uint32_t, std::vector<std::pair<uint16_t, DCIUL>>> ulDCImap;
    std::map <uint32_t, std::vector<uint16_t>> ulACKmap;

    uint32_t getTargetTti(uint32_t curtti, int tdd_config);
    uint32_t getTargetTtiAck(uint32_t curtti, int tdd_config);
};


#ifdef __cplusplus
}
#endif

#endif