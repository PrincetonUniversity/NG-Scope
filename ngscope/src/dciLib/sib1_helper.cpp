
#include "ngscope/hdr/dciLib/sib1_helper.h"
#include "srsran/asn1/rrc.h"

int get_sib1_params(const uint8_t* payload, const uint32_t len, tdd_config* config)
{
    bool all_zero = true;
    for (int i = 0; i < len / 8; ++i) {
        if (payload[i] != 0x0) {
            all_zero = false;
            break;
        }
    }
    if (all_zero) {
        printf("ERROR: PDSCH payload is all zeros");
        return SRSRAN_ERROR;
    }
    printf("Decoding SIB 1...\n");
    asn1::rrc::bcch_dl_sch_msg_s dlsch_msg;
    asn1::cbit_ref dlsch_bref(payload, len / 8);
    asn1::SRSASN_CODE err = dlsch_msg.unpack(dlsch_bref);
    asn1::rrc::sib_type1_s sib1 = dlsch_msg.msg.c1().sib_type1();
    printf("SIB 1 Decoded.\n");


    // Extract the values after ensuring they are present
    if (sib1.tdd_cfg_present) {
        config->sf_config = sib1.tdd_cfg.sf_assign.to_number();
        config->tdd_special_sf = sib1.tdd_cfg.special_sf_patterns.to_number();
    } else {
        printf("TDD configuration not present in SIB1\n");
        return SRSRAN_ERROR;
    }

    asn1::json_writer js;
    (sib1).to_json(js);
    //printf("Decoded SIB1: %s\n", js.to_string().c_str());
    printf("Extracted sf_config: %d, tdd_special_sf: %d\n", config->sf_config, config->tdd_special_sf);    

  return SRSRAN_SUCCESS;
}

    