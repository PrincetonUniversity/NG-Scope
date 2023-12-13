#ifndef __LIBASN4G_H__
#define __LIBASN4G_H__

#include <stdint.h>

int bcch_dl_sch_decode_4g(FILE * fd, uint8_t * payload, int len);
int mib_decode_4g(FILE * fd, uint8_t * payload, int len);

# endif
