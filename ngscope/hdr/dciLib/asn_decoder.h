#ifndef __ASN_DECODER__
#define __ASN_DECODER__

#include <stdint.h>

typedef enum { MIB_4G, SIB_4G, MIB_5G, SIB_5G } PayloadType;

int init_asn_decoder(const char * path);
int push_asn_payload(uint8_t * payload, int len, PayloadType type, uint32_t tti);
void terminate_asn_decoder();

#endif