#ifndef SIB1_HELPER_H
#define SIB1_HELPER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "srsran/phy/phch/ra.h"
#include <stdint.h>

typedef struct {
    int sf_config;
    int tdd_special_sf;
} tdd_config;

int get_sib1_params(const uint8_t* payload, const uint32_t len, tdd_config* config);



#ifdef __cplusplus
}
#endif

#endif // SIB1_HELPER_H