/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2014 Ben Wojtowicz
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "srsran/asn1/liblte_common.h"

/*******************************************************************************
                              DEFINES
*******************************************************************************/

/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/

/*******************************************************************************
                              GLOBAL VARIABLES
*******************************************************************************/

/*******************************************************************************
                              FUNCTIONS
*******************************************************************************/

/*********************************************************************
    Name: liblte_value_2_bits

    Description: Converts a value to a bit string
*********************************************************************/
void liblte_value_2_bits(uint32 value, uint8** bits, uint32 N_bits)
{
  uint32 i;

  for (i = 0; i < N_bits; i++) {
    (*bits)[i] = (value >> (N_bits - i - 1)) & 0x1;
  }
  *bits += N_bits;
}

/*********************************************************************
    Name: liblte_bits_2_value

    Description: Converts a bit string to a value
*********************************************************************/
uint32 liblte_bits_2_value(uint8** bits, uint32 N_bits)
{
  uint32 value = 0;
  uint32 i;

  for (i = 0; i < N_bits; i++) {
    value |= (*bits)[i] << (N_bits - i - 1);
  }
  *bits += N_bits;

  return (value);
}

/*********************************************************************
    Name: liblte_pack

    Description: Pack a bit array into a byte array
*********************************************************************/
void liblte_pack(LIBLTE_BIT_MSG_STRUCT* bits, LIBLTE_BYTE_MSG_STRUCT* bytes)
{
  uint8_t* bit_ptr = bits->msg;
  uint32_t i;

  for (i = 0; i < bits->N_bits / 8; i++) {
    bytes->msg[i] = liblte_bits_2_value(&bit_ptr, 8);
  }
  bytes->N_bytes = bits->N_bits / 8;
  if (bits->N_bits % 8 > 0) {
    bytes->msg[bytes->N_bytes] = liblte_bits_2_value(&bit_ptr, bits->N_bits % 8);
    bytes->N_bytes++;
  }
}

/*********************************************************************
    Name: liblte_unpack

    Description: Unpack a byte array into a bit array
*********************************************************************/
void liblte_unpack(LIBLTE_BYTE_MSG_STRUCT* bytes, LIBLTE_BIT_MSG_STRUCT* bits)
{
  uint8_t* bit_ptr = bits->msg;
  uint32_t i;

  for (i = 0; i < bytes->N_bytes; i++) {
    liblte_value_2_bits(bytes->msg[i], &bit_ptr, 8);
  }
  bits->N_bits = bytes->N_bytes * 8;
}

/*********************************************************************
    Name: liblte_pack

    Description: Pack a bit array into a byte array
*********************************************************************/
void liblte_pack(uint8_t* bits, uint32_t n_bits, uint8_t* bytes)
{
  uint8_t* bit_ptr = bits;
  uint32_t i;

  for (i = 0; i < n_bits / 8; i++) {
    bytes[i] = liblte_bits_2_value(&bit_ptr, 8);
  }
  if (n_bits % 8 > 0) {
    bytes[n_bits / 8] = liblte_bits_2_value(&bit_ptr, n_bits % 8);
  }
}

/*********************************************************************
    Name: liblte_unpack

    Description: Unpack a byte array into a bit array
*********************************************************************/
void liblte_unpack(uint8_t* bytes, uint32_t n_bytes, uint8_t* bits)
{
  uint8_t* bit_ptr = bits;
  uint32_t i;

  for (i = 0; i < n_bytes; i++) {
    liblte_value_2_bits(bytes[i], &bit_ptr, 8);
  }
}

/*********************************************************************
    Name: liblte_align_up

    Description: Aligns a pointer to a multibyte boundary
*********************************************************************/
void liblte_align_up(uint8_t** ptr, uint32_t align)
{
  while ((uint64_t)(*ptr) % align > 0) {
    (*ptr)++;
  }
}

/*********************************************************************
    Name: liblte_align_up_zero

    Description:  Aligns a pointer to a multibyte boundary and zeros
                  bytes skipped
*********************************************************************/
void liblte_align_up_zero(uint8_t** ptr, uint32_t align)
{
  while ((uint64_t)(*ptr) % align > 0) {
    **ptr = 0;
    (*ptr)++;
  }
}
