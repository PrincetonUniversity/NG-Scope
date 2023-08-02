#ifndef __ASN_DECODER__
#define __ASN_DECODER__

#include <stdint.h>

int init_asn_decoder(const char * path);
int push_asn_payload(uint8_t * payload, int len, uint32_t tti);
void terminate_asn_decoder();

#endif