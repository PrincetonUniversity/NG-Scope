/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2012-2014 Ben Wojtowicz
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#ifndef SRSRAN_LIBLTE_COMMON_H
#define SRSRAN_LIBLTE_COMMON_H

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*******************************************************************************
                              DEFINES
*******************************************************************************/

// TODO: This was chosen arbitrarily
#define LIBLTE_ASN1_OID_MAXSUBIDS 128

// Caution these values must match SRSRAN_ ones in common.h
#define LIBLTE_MAX_MSG_SIZE_BITS 102048
#define LIBLTE_MAX_MSG_SIZE_BYTES 12237
#define LIBLTE_MSG_HEADER_OFFSET 1020

// Macro to make it easier to convert defines into strings
#define LIBLTE_CASE_STR(code)                                                                                          \
  case code:                                                                                                           \
    return #code

/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/

typedef int8_t   int8;
typedef uint8_t  uint8;
typedef int16_t  int16;
typedef uint16_t uint16;
typedef int32_t  int32;
typedef uint32_t uint32;
typedef int64_t  int64;
typedef uint64_t uint64;

typedef enum {
  LIBLTE_SUCCESS = 0,
  LIBLTE_ERROR_INVALID_INPUTS,
  LIBLTE_ERROR_ENCODE_FAIL,
  LIBLTE_ERROR_DECODE_FAIL,
  LIBLTE_ERROR_INVALID_CRC,
  LIBLTE_ERROR_N_ITEMS
} LIBLTE_ERROR_ENUM;
static const char liblte_error_text[LIBLTE_ERROR_N_ITEMS][64] = {
    "Invalid inputs",
    "Encode failure",
    "Decode failure",
};

#define LIBLTE_STRING_LEN 128

typedef void* LIBLTE_ASN1_OPEN_TYPE_STRUCT;

typedef struct {
  uint32_t numids;                           // number of subidentifiers
  uint32_t subid[LIBLTE_ASN1_OID_MAXSUBIDS]; // subidentifier values
} LIBLTE_ASN1_OID_STRUCT;

typedef struct {
  bool data_valid;
  bool data;
} LIBLTE_BOOL_MSG_STRUCT;

typedef struct {
  uint32 N_bits;
  uint8  header[LIBLTE_MSG_HEADER_OFFSET];
  uint8  msg[LIBLTE_MAX_MSG_SIZE_BITS];
} LIBLTE_BIT_MSG_STRUCT __attribute__((aligned(8)));

struct alignas(8) LIBLTE_BYTE_MSG_STRUCT
{
  uint32 N_bytes;
  uint8  header[LIBLTE_MSG_HEADER_OFFSET];
  uint8  msg[LIBLTE_MAX_MSG_SIZE_BYTES];
};

/*******************************************************************************
                              DECLARATIONS
*******************************************************************************/

/*********************************************************************
    Name: liblte_value_2_bits

    Description: Converts a value to a bit string
*********************************************************************/
void liblte_value_2_bits(uint32 value, uint8** bits, uint32 N_bits);

/*********************************************************************
    Name: liblte_bits_2_value

    Description: Converts a bit string to a value
*********************************************************************/
uint32 liblte_bits_2_value(uint8** bits, uint32 N_bits);

/*********************************************************************
    Name: liblte_pack

    Description: Pack a bit array into a byte array
*********************************************************************/
void liblte_pack(LIBLTE_BIT_MSG_STRUCT* bits, LIBLTE_BYTE_MSG_STRUCT* bytes);

/*********************************************************************
    Name: liblte_unpack

    Description: Unpack a byte array into a bit array
*********************************************************************/
void liblte_unpack(LIBLTE_BYTE_MSG_STRUCT* bytes, LIBLTE_BIT_MSG_STRUCT* bits);

/*********************************************************************
    Name: liblte_pack

    Description: Pack a bit array into a byte array
*********************************************************************/
void liblte_pack(uint8_t* bits, uint32_t n_bits, uint8_t* bytes);

/*********************************************************************
    Name: liblte_unpack

    Description: Unpack a byte array into a bit array
*********************************************************************/
void liblte_unpack(uint8_t* bytes, uint32_t n_bytes, uint8_t* bits);

/*********************************************************************
    Name: liblte_align_up

    Description: Aligns a pointer to a multibyte boundary
*********************************************************************/
void liblte_align_up(uint8_t** ptr, uint32_t align);

/*********************************************************************
    Name: liblte_align_up_zero

    Description:  Aligns a pointer to a multibyte boundary and zeros
                  bytes skipped
*********************************************************************/
void liblte_align_up_zero(uint8_t** ptr, uint32_t align);

#endif // SRSRAN_LIBLTE_COMMON_H
