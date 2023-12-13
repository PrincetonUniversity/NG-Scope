#include <stdio.h>
#include <stdlib.h>
#include <BCCH-DL-SCH-Message.h>
#include <MasterInformationBlock.h>
#include "libasn4g.h"

int bcch_dl_sch_decode_4g(FILE * fd, uint8_t * payload, int len)
{
    asn_dec_rval_t rval;
    BCCH_DL_SCH_Message_t * msg = 0;

    /* Decode the input buffer */
    rval = uper_decode_complete(0, &asn_DEF_BCCH_DL_SCH_Message, (void **) &msg, payload, len);
    if(rval.code != RC_OK) {
        printf("Decoding error (%d bytes consumed)\n", (int) rval.consumed);
        return 1;
    }
    /* Print the decoded payload type as XML */
    xer_fprint(fd, &asn_DEF_BCCH_DL_SCH_Message, msg);

    ASN_STRUCT_FREE(asn_DEF_BCCH_DL_SCH_Message, msg);

    return 0; /* Decoding finished successfully */
}

int mib_decode_4g(FILE * fd, uint8_t * payload, int len)
{
    asn_dec_rval_t rval;
    MasterInformationBlock_t * msg = 0;

    /* Decode the input buffer */
    rval = uper_decode_complete(0, &asn_DEF_MasterInformationBlock, (void **) &msg, payload, len);
    if(rval.code != RC_OK) {
        printf("Decoding error (%d bytes consumed)\n", (int) rval.consumed);
        return 1;
    }
    /* Print the decoded payload type as XML */
    xer_fprint(fd, &asn_DEF_MasterInformationBlock, msg);

    ASN_STRUCT_FREE(asn_DEF_MasterInformationBlock, msg);

    return 0; /* Decoding finished successfully */
}
