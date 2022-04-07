/**
 * Copyright 2013-2021 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 *
 * srsRAN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsRAN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#include <complex.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "srsran/phy/utils/simd.h"
#include "srsran/phy/utils/vector_simd.h"

void srsran_vec_xor_bbb_simd(const uint8_t* x, const uint8_t* y, uint8_t* z, const int len)
{
  int i = 0;
#if SRSRAN_SIMD_B_SIZE
  if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(y) && SRSRAN_IS_ALIGNED(z)) {
    for (; i < len - SRSRAN_SIMD_B_SIZE + 1; i += SRSRAN_SIMD_B_SIZE) {
      simd_b_t a = srsran_simd_b_load((int8_t*)&x[i]);
      simd_b_t b = srsran_simd_b_load((int8_t*)&y[i]);

      simd_b_t r = srsran_simd_b_xor(a, b);

      srsran_simd_b_store((int8_t*)&z[i], r);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_B_SIZE + 1; i += SRSRAN_SIMD_B_SIZE) {
      simd_b_t a = srsran_simd_b_loadu((int8_t*)&x[i]);
      simd_b_t b = srsran_simd_b_loadu((int8_t*)&y[i]);

      simd_b_t r = srsran_simd_b_xor(a, b);

      srsran_simd_b_storeu((int8_t*)&z[i], r);
    }
  }
#endif /* SRSRAN_SIMD_B_SIZE */

  for (; i < len; i++) {
    z[i] = x[i] ^ y[i];
  }
}

int srsran_vec_dot_prod_sss_simd(const int16_t* x, const int16_t* y, const int len)
{
  int i      = 0;
  int result = 0;
#if SRSRAN_SIMD_S_SIZE
  simd_s_t simd_dotProdVal = srsran_simd_s_zero();
  if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(y)) {
    for (; i < len - SRSRAN_SIMD_S_SIZE + 1; i += SRSRAN_SIMD_S_SIZE) {
      simd_s_t a = srsran_simd_s_load(&x[i]);
      simd_s_t b = srsran_simd_s_load(&y[i]);

      simd_s_t z = srsran_simd_s_mul(a, b);

      simd_dotProdVal = srsran_simd_s_add(simd_dotProdVal, z);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_S_SIZE + 1; i += SRSRAN_SIMD_S_SIZE) {
      simd_s_t a = srsran_simd_s_loadu(&x[i]);
      simd_s_t b = srsran_simd_s_loadu(&y[i]);

      simd_s_t z = srsran_simd_s_mul(a, b);

      simd_dotProdVal = srsran_simd_s_add(simd_dotProdVal, z);
    }
  }
  srsran_simd_aligned short dotProdVector[SRSRAN_SIMD_S_SIZE];
  srsran_simd_s_store(dotProdVector, simd_dotProdVal);
  for (int k = 0; k < SRSRAN_SIMD_S_SIZE; k++) {
    result += dotProdVector[k];
  }
#endif /* SRSRAN_SIMD_S_SIZE */

  for (; i < len; i++) {
    result += (x[i] * y[i]);
  }

  return result;
}

void srsran_vec_sum_sss_simd(const int16_t* x, const int16_t* y, int16_t* z, const int len)
{
  int i = 0;
#if SRSRAN_SIMD_S_SIZE
  if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(y) && SRSRAN_IS_ALIGNED(z)) {
    for (; i < len - SRSRAN_SIMD_S_SIZE + 1; i += SRSRAN_SIMD_S_SIZE) {
      simd_s_t a = srsran_simd_s_load(&x[i]);
      simd_s_t b = srsran_simd_s_load(&y[i]);

      simd_s_t r = srsran_simd_s_add(a, b);

      srsran_simd_s_store(&z[i], r);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_S_SIZE + 1; i += SRSRAN_SIMD_S_SIZE) {
      simd_s_t a = srsran_simd_s_loadu(&x[i]);
      simd_s_t b = srsran_simd_s_loadu(&y[i]);

      simd_s_t r = srsran_simd_s_add(a, b);

      srsran_simd_s_storeu(&z[i], r);
    }
  }
#endif /* SRSRAN_SIMD_S_SIZE */

  for (; i < len; i++) {
    z[i] = x[i] + y[i];
  }
}

void srsran_vec_sub_sss_simd(const int16_t* x, const int16_t* y, int16_t* z, const int len)
{
  int i = 0;
#if SRSRAN_SIMD_S_SIZE
  if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(y) && SRSRAN_IS_ALIGNED(z)) {
    for (; i < len - SRSRAN_SIMD_S_SIZE + 1; i += SRSRAN_SIMD_S_SIZE) {
      simd_s_t a = srsran_simd_s_load(&x[i]);
      simd_s_t b = srsran_simd_s_load(&y[i]);

      simd_s_t r = srsran_simd_s_sub(a, b);

      srsran_simd_s_store(&z[i], r);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_S_SIZE + 1; i += SRSRAN_SIMD_S_SIZE) {
      simd_s_t a = srsran_simd_s_loadu(&x[i]);
      simd_s_t b = srsran_simd_s_loadu(&y[i]);

      simd_s_t r = srsran_simd_s_sub(a, b);

      srsran_simd_s_storeu(&z[i], r);
    }
  }
#endif /* SRSRAN_SIMD_S_SIZE */

  for (; i < len; i++) {
    z[i] = x[i] - y[i];
  }
}

void srsran_vec_sub_bbb_simd(const int8_t* x, const int8_t* y, int8_t* z, const int len)
{
  int i = 0;
#if SRSRAN_SIMD_B_SIZE
  if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(y) && SRSRAN_IS_ALIGNED(z)) {
    for (; i < len - SRSRAN_SIMD_B_SIZE + 1; i += SRSRAN_SIMD_B_SIZE) {
      simd_b_t a = srsran_simd_b_load(&x[i]);
      simd_b_t b = srsran_simd_b_load(&y[i]);

      simd_b_t r = srsran_simd_b_sub(a, b);

      srsran_simd_b_store(&z[i], r);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_S_SIZE + 1; i += SRSRAN_SIMD_S_SIZE) {
      simd_b_t a = srsran_simd_b_loadu(&x[i]);
      simd_b_t b = srsran_simd_b_loadu(&y[i]);

      simd_b_t r = srsran_simd_b_sub(a, b);

      srsran_simd_b_storeu(&z[i], r);
    }
  }
#endif /* SRSRAN_SIMD_S_SIZE */

  for (; i < len; i++) {
    z[i] = x[i] - y[i];
  }
}

void srsran_vec_prod_sss_simd(const int16_t* x, const int16_t* y, int16_t* z, const int len)
{
  int i = 0;
#if SRSRAN_SIMD_S_SIZE
  if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(y) && SRSRAN_IS_ALIGNED(z)) {
    for (; i < len - SRSRAN_SIMD_S_SIZE + 1; i += SRSRAN_SIMD_S_SIZE) {
      simd_s_t a = srsran_simd_s_load(&x[i]);
      simd_s_t b = srsran_simd_s_load(&y[i]);

      simd_s_t r = srsran_simd_s_mul(a, b);

      srsran_simd_s_store(&z[i], r);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_S_SIZE + 1; i += SRSRAN_SIMD_S_SIZE) {
      simd_s_t a = srsran_simd_s_loadu(&x[i]);
      simd_s_t b = srsran_simd_s_loadu(&y[i]);

      simd_s_t r = srsran_simd_s_mul(a, b);

      srsran_simd_s_storeu(&z[i], r);
    }
  }
#endif /* SRSRAN_SIMD_S_SIZE */

  for (; i < len; i++) {
    z[i] = x[i] * y[i];
  }
}

void srsran_vec_neg_sss_simd(const int16_t* x, const int16_t* y, int16_t* z, const int len)
{
  int i = 0;

#if SRSRAN_SIMD_S_SIZE
  if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(y) && SRSRAN_IS_ALIGNED(z)) {
    for (; i < len - SRSRAN_SIMD_S_SIZE + 1; i += SRSRAN_SIMD_S_SIZE) {
      simd_s_t a = srsran_simd_s_load(&x[i]);
      simd_s_t b = srsran_simd_s_load(&y[i]);

      simd_s_t r = srsran_simd_s_neg(a, b);

      srsran_simd_s_store(&z[i], r);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_S_SIZE + 1; i += SRSRAN_SIMD_S_SIZE) {
      simd_s_t a = srsran_simd_s_loadu(&x[i]);
      simd_s_t b = srsran_simd_s_loadu(&y[i]);

      simd_s_t r = srsran_simd_s_neg(a, b);

      srsran_simd_s_storeu(&z[i], r);
    }
  }
#endif /* SRSRAN_SIMD_S_SIZE */

  for (; i < len; i++) {
    z[i] = y[i] < 0 ? -x[i] : x[i];
  }
}

void srsran_vec_neg_bbb_simd(const int8_t* x, const int8_t* y, int8_t* z, const int len)
{
  int i = 0;

#if SRSRAN_SIMD_B_SIZE
  if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(y) && SRSRAN_IS_ALIGNED(z)) {
    for (; i < len - SRSRAN_SIMD_B_SIZE + 1; i += SRSRAN_SIMD_B_SIZE) {
      simd_b_t a = srsran_simd_b_load(&x[i]);
      simd_b_t b = srsran_simd_b_load(&y[i]);

      simd_b_t r = srsran_simd_b_neg(a, b);

      srsran_simd_b_store(&z[i], r);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_B_SIZE + 1; i += SRSRAN_SIMD_B_SIZE) {
      simd_b_t a = srsran_simd_b_loadu(&x[i]);
      simd_b_t b = srsran_simd_b_loadu(&y[i]);

      simd_b_t r = srsran_simd_b_neg(a, b);

      srsran_simd_b_storeu(&z[i], r);
    }
  }
#endif /* SRSRAN_SIMD_S_SIZE */
  for (; i < len; i++) {
    z[i] = y[i] < 0 ? -x[i] : x[i];
  }
}

#define SAVE_OUTPUT_16_SSE(j)                                                                                          \
  do {                                                                                                                 \
    int16_t  temp = (int16_t)_mm_extract_epi16(xVal, j);                                                               \
    uint16_t l    = (uint16_t)_mm_extract_epi16(lutVal, j);                                                            \
    y[l]          = temp;                                                                                              \
  } while (false)

/* No improvement with AVX */
void srsran_vec_lut_sss_simd(const short* x, const unsigned short* lut, short* y, const int len)
{
  int i = 0;
#ifdef LV_HAVE_SSE
  if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(lut)) {
    for (; i < len - 7; i += 8) {
      __m128i xVal   = _mm_load_si128((__m128i*)&x[i]);
      __m128i lutVal = _mm_load_si128((__m128i*)&lut[i]);

      SAVE_OUTPUT_16_SSE(0);
      SAVE_OUTPUT_16_SSE(1);
      SAVE_OUTPUT_16_SSE(2);
      SAVE_OUTPUT_16_SSE(3);
      SAVE_OUTPUT_16_SSE(4);
      SAVE_OUTPUT_16_SSE(5);
      SAVE_OUTPUT_16_SSE(6);
      SAVE_OUTPUT_16_SSE(7);
    }
  } else {
    for (; i < len - 7; i += 8) {
      __m128i xVal   = _mm_loadu_si128((__m128i*)&x[i]);
      __m128i lutVal = _mm_loadu_si128((__m128i*)&lut[i]);

      SAVE_OUTPUT_16_SSE(0);
      SAVE_OUTPUT_16_SSE(1);
      SAVE_OUTPUT_16_SSE(2);
      SAVE_OUTPUT_16_SSE(3);
      SAVE_OUTPUT_16_SSE(4);
      SAVE_OUTPUT_16_SSE(5);
      SAVE_OUTPUT_16_SSE(6);
      SAVE_OUTPUT_16_SSE(7);
    }
  }
#endif

  for (; i < len; i++) {
    y[lut[i]] = x[i];
  }
}

#define SAVE_OUTPUT_SSE_8(j)                                                                                           \
  do {                                                                                                                 \
    int8_t   temp = (int8_t)_mm_extract_epi8(xVal, j);                                                                 \
    uint16_t idx  = (uint16_t)_mm_extract_epi16(lutVal1, j);                                                           \
    y[idx]        = (char)temp;                                                                                        \
  } while (false)

#define SAVE_OUTPUT_SSE_8_2(j)                                                                                         \
  do {                                                                                                                 \
    int8_t   temp = (int8_t)_mm_extract_epi8(xVal, j + 8);                                                             \
    uint16_t idx  = (uint16_t)_mm_extract_epi16(lutVal2, j);                                                           \
    y[idx]        = (char)temp;                                                                                        \
  } while (false)

void srsran_vec_lut_bbb_simd(const int8_t* x, const unsigned short* lut, int8_t* y, const int len)
{
  int i = 0;
#ifdef LV_HAVE_SSE
  if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(lut)) {
    for (; i < len - 15; i += 16) {
      __m128i xVal    = _mm_load_si128((__m128i*)&x[i]);
      __m128i lutVal1 = _mm_load_si128((__m128i*)&lut[i]);
      __m128i lutVal2 = _mm_load_si128((__m128i*)&lut[i + 8]);

      SAVE_OUTPUT_SSE_8(0);
      SAVE_OUTPUT_SSE_8(1);
      SAVE_OUTPUT_SSE_8(2);
      SAVE_OUTPUT_SSE_8(3);
      SAVE_OUTPUT_SSE_8(4);
      SAVE_OUTPUT_SSE_8(5);
      SAVE_OUTPUT_SSE_8(6);
      SAVE_OUTPUT_SSE_8(7);

      SAVE_OUTPUT_SSE_8_2(0);
      SAVE_OUTPUT_SSE_8_2(1);
      SAVE_OUTPUT_SSE_8_2(2);
      SAVE_OUTPUT_SSE_8_2(3);
      SAVE_OUTPUT_SSE_8_2(4);
      SAVE_OUTPUT_SSE_8_2(5);
      SAVE_OUTPUT_SSE_8_2(6);
      SAVE_OUTPUT_SSE_8_2(7);
    }
  } else {
    for (; i < len - 15; i += 16) {
      __m128i xVal    = _mm_loadu_si128((__m128i*)&x[i]);
      __m128i lutVal1 = _mm_loadu_si128((__m128i*)&lut[i]);
      __m128i lutVal2 = _mm_loadu_si128((__m128i*)&lut[i + 8]);

      SAVE_OUTPUT_SSE_8(0);
      SAVE_OUTPUT_SSE_8(1);
      SAVE_OUTPUT_SSE_8(2);
      SAVE_OUTPUT_SSE_8(3);
      SAVE_OUTPUT_SSE_8(4);
      SAVE_OUTPUT_SSE_8(5);
      SAVE_OUTPUT_SSE_8(6);
      SAVE_OUTPUT_SSE_8(7);

      SAVE_OUTPUT_SSE_8_2(0);
      SAVE_OUTPUT_SSE_8_2(1);
      SAVE_OUTPUT_SSE_8_2(2);
      SAVE_OUTPUT_SSE_8_2(3);
      SAVE_OUTPUT_SSE_8_2(4);
      SAVE_OUTPUT_SSE_8_2(5);
      SAVE_OUTPUT_SSE_8_2(6);
      SAVE_OUTPUT_SSE_8_2(7);
    }
  }
#endif

  for (; i < len; i++) {
    y[lut[i]] = x[i];
  }
}

void srsran_vec_convert_if_simd(const int16_t* x, float* z, const float scale, const int len)
{
  int         i    = 0;
  const float gain = 1.0f / scale;

#ifdef LV_HAVE_SSE
  __m128 s = _mm_set1_ps(gain);
  if (SRSRAN_IS_ALIGNED(z)) {
    for (; i < len - 3; i += 4) {
      __m64* ptr = (__m64*)&x[i];
      __m128 fl  = _mm_cvtpi16_ps(*ptr);
      __m128 v   = _mm_mul_ps(fl, s);

      _mm_store_ps(&z[i], v);
    }
  } else {
    for (; i < len - 3; i += 4) {
      __m64* ptr = (__m64*)&x[i];
      __m128 fl  = _mm_cvtpi16_ps(*ptr);
      __m128 v   = _mm_mul_ps(fl, s);

      _mm_storeu_ps(&z[i], v);
    }
  }
#endif /* LV_HAVE_SSE */

  for (; i < len; i++) {
    z[i] = ((float)x[i]) * gain;
  }
}

void srsran_vec_convert_fi_simd(const float* x, int16_t* z, const float scale, const int len)
{
  int i = 0;

#if SRSRAN_SIMD_F_SIZE && SRSRAN_SIMD_S_SIZE
  simd_f_t s = srsran_simd_f_set1(scale);
  if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(z)) {
    for (; i < len - SRSRAN_SIMD_S_SIZE + 1; i += SRSRAN_SIMD_S_SIZE) {
      simd_f_t a = srsran_simd_f_load(&x[i]);
      simd_f_t b = srsran_simd_f_load(&x[i + SRSRAN_SIMD_F_SIZE]);

      simd_f_t sa = srsran_simd_f_mul(a, s);
      simd_f_t sb = srsran_simd_f_mul(b, s);

      simd_s_t i16 = srsran_simd_convert_2f_s(sa, sb);

      srsran_simd_s_store(&z[i], i16);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_S_SIZE + 1; i += SRSRAN_SIMD_S_SIZE) {
      simd_f_t a = srsran_simd_f_loadu(&x[i]);
      simd_f_t b = srsran_simd_f_loadu(&x[i + SRSRAN_SIMD_F_SIZE]);

      simd_f_t sa = srsran_simd_f_mul(a, s);
      simd_f_t sb = srsran_simd_f_mul(b, s);

      simd_s_t i16 = srsran_simd_convert_2f_s(sa, sb);

      srsran_simd_s_storeu(&z[i], i16);
    }
  }
#endif /* SRSRAN_SIMD_F_SIZE && SRSRAN_SIMD_S_SIZE */

  for (; i < len; i++) {
    z[i] = (int16_t)(x[i] * scale);
  }
}

void srsran_vec_convert_conj_cs_simd(const cf_t* x_, int16_t* z, const float scale, const int len_)
{
  int i = 0;

  const float* x   = (float*)x_;
  const int    len = len_ * 2;

#if SRSRAN_SIMD_F_SIZE && SRSRAN_SIMD_S_SIZE
  srsran_simd_aligned float scale_v[SRSRAN_SIMD_F_SIZE];
  for (uint32_t j = 0; j < SRSRAN_SIMD_F_SIZE; j++) {
    scale_v[j] = (j % 2 == 0) ? +scale : -scale;
  }

  simd_f_t s = srsran_simd_f_load(scale_v);
  if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(z)) {
    for (; i < len - SRSRAN_SIMD_S_SIZE + 1; i += SRSRAN_SIMD_S_SIZE) {
      simd_f_t a = srsran_simd_f_load(&x[i]);
      simd_f_t b = srsran_simd_f_load(&x[i + SRSRAN_SIMD_F_SIZE]);

      simd_f_t sa = srsran_simd_f_mul(a, s);
      simd_f_t sb = srsran_simd_f_mul(b, s);

      simd_s_t i16 = srsran_simd_convert_2f_s(sa, sb);

      srsran_simd_s_store(&z[i], i16);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_S_SIZE + 1; i += SRSRAN_SIMD_S_SIZE) {
      simd_f_t a = srsran_simd_f_loadu(&x[i]);
      simd_f_t b = srsran_simd_f_loadu(&x[i + SRSRAN_SIMD_F_SIZE]);

      simd_f_t sa = srsran_simd_f_mul(a, s);
      simd_f_t sb = srsran_simd_f_mul(b, s);

      simd_s_t i16 = srsran_simd_convert_2f_s(sa, sb);

      srsran_simd_s_storeu(&z[i], i16);
    }
  }
#endif /* SRSRAN_SIMD_F_SIZE && SRSRAN_SIMD_S_SIZE */

  for (; i < len; i++) {
    z[i] = (int16_t)(x[i] * scale);
    i++;
    z[i] = (int16_t)(x[i] * -scale);
  }
}

#define SRSRAN_IS_ALIGNED_SSE(PTR) (((size_t)(PTR)&0x0F) == 0)

void srsran_vec_convert_fb_simd(const float* x, int8_t* z, const float scale, const int len)
{
  int i = 0;

  // Force the use of SSE here instead of AVX since the implementations requires too many permutes across 128-bit
  // boundaries

#ifdef LV_HAVE_SSE
  __m128 s = _mm_set1_ps(scale);
  if (SRSRAN_IS_ALIGNED_SSE(x) && SRSRAN_IS_ALIGNED_SSE(z)) {
    for (; i < len - 16 + 1; i += 16) {
      __m128 a = _mm_load_ps(&x[i]);
      __m128 b = _mm_load_ps(&x[i + 1 * 4]);
      __m128 c = _mm_load_ps(&x[i + 2 * 4]);
      __m128 d = _mm_load_ps(&x[i + 3 * 4]);

      __m128 sa = _mm_mul_ps(a, s);
      __m128 sb = _mm_mul_ps(b, s);
      __m128 sc = _mm_mul_ps(c, s);
      __m128 sd = _mm_mul_ps(d, s);

      __m128i ai = _mm_cvttps_epi32(sa);
      __m128i bi = _mm_cvttps_epi32(sb);
      __m128i ci = _mm_cvttps_epi32(sc);
      __m128i di = _mm_cvttps_epi32(sd);
      __m128i ab = _mm_packs_epi32(ai, bi);
      __m128i cd = _mm_packs_epi32(ci, di);

      __m128i i8 = _mm_packs_epi16(ab, cd);

      _mm_store_si128((__m128i*)&z[i], i8);
    }
  } else {
    for (; i < len - 16 + 1; i += 16) {
      __m128 a = _mm_load_ps(&x[i]);
      __m128 b = _mm_load_ps(&x[i + 1 * 4]);
      __m128 c = _mm_load_ps(&x[i + 2 * 4]);
      __m128 d = _mm_load_ps(&x[i + 3 * 4]);

      __m128 sa = _mm_mul_ps(a, s);
      __m128 sb = _mm_mul_ps(b, s);
      __m128 sc = _mm_mul_ps(c, s);
      __m128 sd = _mm_mul_ps(d, s);

      __m128i ai = _mm_cvttps_epi32(sa);
      __m128i bi = _mm_cvttps_epi32(sb);
      __m128i ci = _mm_cvttps_epi32(sc);
      __m128i di = _mm_cvttps_epi32(sd);
      __m128i ab = _mm_packs_epi32(ai, bi);
      __m128i cd = _mm_packs_epi32(ci, di);

      __m128i i8 = _mm_packs_epi16(ab, cd);

      _mm_storeu_si128((__m128i*)&z[i], i8);
    }
  }
#endif

#ifdef HAVE_NEON
#pragma message "srsran_vec_convert_fb_simd not implemented in neon"
#endif /* HAVE_NEON */

  for (; i < len; i++) {
    z[i] = (int8_t)(x[i] * scale);
  }
}

float srsran_vec_acc_ff_simd(const float* x, const int len)
{
  int   i       = 0;
  float acc_sum = 0.0f;

#if SRSRAN_SIMD_F_SIZE
  simd_f_t simd_sum = srsran_simd_f_zero();

  if (SRSRAN_IS_ALIGNED(x)) {
    for (; i < len - SRSRAN_SIMD_F_SIZE + 1; i += SRSRAN_SIMD_F_SIZE) {
      simd_f_t a = srsran_simd_f_load(&x[i]);

      simd_sum = srsran_simd_f_add(simd_sum, a);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_F_SIZE + 1; i += SRSRAN_SIMD_F_SIZE) {
      simd_f_t a = srsran_simd_f_loadu(&x[i]);

      simd_sum = srsran_simd_f_add(simd_sum, a);
    }
  }

  srsran_simd_aligned float sum[SRSRAN_SIMD_F_SIZE];
  srsran_simd_f_store(sum, simd_sum);
  for (int k = 0; k < SRSRAN_SIMD_F_SIZE; k++) {
    acc_sum += sum[k];
  }
#endif

  for (; i < len; i++) {
    acc_sum += x[i];
  }

  return acc_sum;
}

cf_t srsran_vec_acc_cc_simd(const cf_t* x, const int len)
{
  int  i       = 0;
  cf_t acc_sum = 0.0f;

#if SRSRAN_SIMD_F_SIZE
  simd_f_t simd_sum = srsran_simd_f_zero();

  if (SRSRAN_IS_ALIGNED(x)) {
    for (; i < len - SRSRAN_SIMD_F_SIZE / 2 + 1; i += SRSRAN_SIMD_F_SIZE / 2) {
      simd_f_t a = srsran_simd_f_load((float*)&x[i]);

      simd_sum = srsran_simd_f_add(simd_sum, a);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_F_SIZE / 2 + 1; i += SRSRAN_SIMD_F_SIZE / 2) {
      simd_f_t a = srsran_simd_f_loadu((float*)&x[i]);

      simd_sum = srsran_simd_f_add(simd_sum, a);
    }
  }

  __attribute__((aligned(64))) cf_t sum[SRSRAN_SIMD_F_SIZE / 2];
  srsran_simd_f_store((float*)&sum, simd_sum);
  for (int k = 0; k < SRSRAN_SIMD_F_SIZE / 2; k++) {
    acc_sum += sum[k];
  }
#endif

  for (; i < len; i++) {
    acc_sum += x[i];
  }
  return acc_sum;
}

void srsran_vec_add_fff_simd(const float* x, const float* y, float* z, const int len)
{
  int i = 0;

#if SRSRAN_SIMD_F_SIZE
  if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(y) && SRSRAN_IS_ALIGNED(z)) {
    for (; i < len - SRSRAN_SIMD_F_SIZE + 1; i += SRSRAN_SIMD_F_SIZE) {
      simd_f_t a = srsran_simd_f_load(&x[i]);
      simd_f_t b = srsran_simd_f_load(&y[i]);

      simd_f_t r = srsran_simd_f_add(a, b);

      srsran_simd_f_store(&z[i], r);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_F_SIZE + 1; i += SRSRAN_SIMD_F_SIZE) {
      simd_f_t a = srsran_simd_f_loadu(&x[i]);
      simd_f_t b = srsran_simd_f_loadu(&y[i]);

      simd_f_t r = srsran_simd_f_add(a, b);

      srsran_simd_f_storeu(&z[i], r);
    }
  }
#endif

  for (; i < len; i++) {
    z[i] = x[i] + y[i];
  }
}

void srsran_vec_sub_fff_simd(const float* x, const float* y, float* z, const int len)
{
  int i = 0;

#if SRSRAN_SIMD_F_SIZE
  if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(y) && SRSRAN_IS_ALIGNED(z)) {
    for (; i < len - SRSRAN_SIMD_F_SIZE + 1; i += SRSRAN_SIMD_F_SIZE) {
      simd_f_t a = srsran_simd_f_load(&x[i]);
      simd_f_t b = srsran_simd_f_load(&y[i]);

      simd_f_t r = srsran_simd_f_sub(a, b);

      srsran_simd_f_store(&z[i], r);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_F_SIZE + 1; i += SRSRAN_SIMD_F_SIZE) {
      simd_f_t a = srsran_simd_f_loadu(&x[i]);
      simd_f_t b = srsran_simd_f_loadu(&y[i]);

      simd_f_t r = srsran_simd_f_sub(a, b);

      srsran_simd_f_storeu(&z[i], r);
    }
  }
#endif

  for (; i < len; i++) {
    z[i] = x[i] - y[i];
  }
}

cf_t srsran_vec_dot_prod_ccc_simd(const cf_t* x, const cf_t* y, const int len)
{
  int  i      = 0;
  cf_t result = 0;

#if SRSRAN_SIMD_CF_SIZE
  if (len >= SRSRAN_SIMD_CF_SIZE) {
    simd_cf_t avx_result = srsran_simd_cf_zero();
    if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(y)) {
      for (; i < len - SRSRAN_SIMD_CF_SIZE + 1; i += SRSRAN_SIMD_CF_SIZE) {
        simd_cf_t xVal = srsran_simd_cfi_load(&x[i]);
        simd_cf_t yVal = srsran_simd_cfi_load(&y[i]);

        avx_result = srsran_simd_cf_add(srsran_simd_cf_prod(xVal, yVal), avx_result);
      }
    } else {
      for (; i < len - SRSRAN_SIMD_CF_SIZE + 1; i += SRSRAN_SIMD_CF_SIZE) {
        simd_cf_t xVal = srsran_simd_cfi_loadu(&x[i]);
        simd_cf_t yVal = srsran_simd_cfi_loadu(&y[i]);

        avx_result = srsran_simd_cf_add(srsran_simd_cf_prod(xVal, yVal), avx_result);
      }
    }

    __attribute__((aligned(64))) float simd_dotProdVector[SRSRAN_SIMD_CF_SIZE];
    simd_f_t                           acc_re = srsran_simd_cf_re(avx_result);
    simd_f_t                           acc_im = srsran_simd_cf_im(avx_result);

    simd_f_t acc = srsran_simd_f_hadd(acc_re, acc_im);
    for (int j = 2; j < SRSRAN_SIMD_F_SIZE; j *= 2) {
      acc = srsran_simd_f_hadd(acc, acc);
    }
    srsran_simd_f_store(simd_dotProdVector, acc);
    __real__ result = simd_dotProdVector[0];
    __imag__ result = simd_dotProdVector[1];
  }
#endif

  for (; i < len; i++) {
    result += (x[i] * y[i]);
  }

  return result;
}

#ifdef ENABLE_C16
c16_t srsran_vec_dot_prod_ccc_c16i_simd(const c16_t* x, const c16_t* y, const int len)
{
  int   i      = 0;
  c16_t result = 0;

#if SRSRAN_SIMD_C16_SIZE
  simd_c16_t avx_result = srsran_simd_c16_zero();

  for (; i < len - SRSRAN_SIMD_C16_SIZE + 1; i += SRSRAN_SIMD_C16_SIZE) {
    simd_c16_t xVal = srsran_simd_c16i_load(&x[i]);
    simd_c16_t yVal = srsran_simd_c16i_load(&y[i]);

    avx_result = srsran_simd_c16_add(srsran_simd_c16_prod(xVal, yVal), avx_result);
  }

  __attribute__((aligned(256))) c16_t avx_dotProdVector[16] = {0};
  srsran_simd_c16i_store(avx_dotProdVector, avx_result);
  for (int k = 0; k < 16; k++) {
    result += avx_dotProdVector[k];
  }
#endif

  for (; i < len; i++) {
    result += (x[i] * y[i]) / (1 << 14);
  }

  return result;
}
#endif /* ENABLE_C16 */

cf_t srsran_vec_dot_prod_conj_ccc_simd(const cf_t* x, const cf_t* y, const int len)
{
  int  i      = 0;
  cf_t result = 0;

#if SRSRAN_SIMD_CF_SIZE
  if (len >= SRSRAN_SIMD_CF_SIZE) {
    simd_cf_t avx_result = srsran_simd_cf_zero();
    if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(y)) {
      for (; i < len - SRSRAN_SIMD_CF_SIZE + 1; i += SRSRAN_SIMD_CF_SIZE) {
        simd_cf_t xVal = srsran_simd_cfi_load(&x[i]);
        simd_cf_t yVal = srsran_simd_cfi_load(&y[i]);

        avx_result = srsran_simd_cf_add(srsran_simd_cf_conjprod(xVal, yVal), avx_result);
      }
    } else {
      for (; i < len - SRSRAN_SIMD_CF_SIZE + 1; i += SRSRAN_SIMD_CF_SIZE) {
        simd_cf_t xVal = srsran_simd_cfi_loadu(&x[i]);
        simd_cf_t yVal = srsran_simd_cfi_loadu(&y[i]);

        avx_result = srsran_simd_cf_add(srsran_simd_cf_conjprod(xVal, yVal), avx_result);
      }
    }

    __attribute__((aligned(64))) float simd_dotProdVector[SRSRAN_SIMD_CF_SIZE];
    simd_f_t                           acc_re = srsran_simd_cf_re(avx_result);
    simd_f_t                           acc_im = srsran_simd_cf_im(avx_result);

    simd_f_t acc = srsran_simd_f_hadd(acc_re, acc_im);
    for (int j = 2; j < SRSRAN_SIMD_F_SIZE; j *= 2) {
      acc = srsran_simd_f_hadd(acc, acc);
    }
    srsran_simd_f_store(simd_dotProdVector, acc);
    __real__ result = simd_dotProdVector[0];
    __imag__ result = simd_dotProdVector[1];
  }
#endif

  for (; i < len; i++) {
    result += x[i] * conjf(y[i]);
  }

  return result;
}

void srsran_vec_prod_cfc_simd(const cf_t* x, const float* y, cf_t* z, const int len)
{
  int i = 0;

#if SRSRAN_SIMD_CF_SIZE
  if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(y) && SRSRAN_IS_ALIGNED(z)) {
    for (; i < len - SRSRAN_SIMD_CF_SIZE + 1; i += SRSRAN_SIMD_CF_SIZE) {
      simd_f_t s = srsran_simd_f_load(&y[i]);

      simd_cf_t a = srsran_simd_cfi_load(&x[i]);
      simd_cf_t r = srsran_simd_cf_mul(a, s);
      srsran_simd_cfi_store(&z[i], r);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_F_SIZE + 1; i += SRSRAN_SIMD_F_SIZE) {
      simd_f_t s = srsran_simd_f_loadu(&y[i]);

      simd_cf_t a = srsran_simd_cfi_loadu(&x[i]);
      simd_cf_t r = srsran_simd_cf_mul(a, s);
      srsran_simd_cfi_storeu(&z[i], r);
    }
  }
#endif

  for (; i < len; i++) {
    z[i] = x[i] * y[i];
  }
}

void srsran_vec_prod_fff_simd(const float* x, const float* y, float* z, const int len)
{
  int i = 0;

#if SRSRAN_SIMD_F_SIZE
  if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(y) && SRSRAN_IS_ALIGNED(z)) {
    for (; i < len - SRSRAN_SIMD_F_SIZE + 1; i += SRSRAN_SIMD_F_SIZE) {
      simd_f_t a = srsran_simd_f_load(&x[i]);
      simd_f_t b = srsran_simd_f_load(&y[i]);

      simd_f_t r = srsran_simd_f_mul(a, b);

      srsran_simd_f_store(&z[i], r);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_F_SIZE + 1; i += SRSRAN_SIMD_F_SIZE) {
      simd_f_t a = srsran_simd_f_loadu(&x[i]);
      simd_f_t b = srsran_simd_f_loadu(&y[i]);

      simd_f_t r = srsran_simd_f_mul(a, b);

      srsran_simd_f_storeu(&z[i], r);
    }
  }
#endif

  for (; i < len; i++) {
    z[i] = x[i] * y[i];
  }
}

void srsran_vec_prod_ccc_simd(const cf_t* x, const cf_t* y, cf_t* z, const int len)
{
  int i = 0;

#if SRSRAN_SIMD_CF_SIZE
  if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(y) && SRSRAN_IS_ALIGNED(z)) {
    for (; i < len - SRSRAN_SIMD_CF_SIZE + 1; i += SRSRAN_SIMD_CF_SIZE) {
      simd_cf_t a = srsran_simd_cfi_load(&x[i]);
      simd_cf_t b = srsran_simd_cfi_load(&y[i]);

      simd_cf_t r = srsran_simd_cf_prod(a, b);

      srsran_simd_cfi_store(&z[i], r);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_CF_SIZE + 1; i += SRSRAN_SIMD_CF_SIZE) {
      simd_cf_t a = srsran_simd_cfi_loadu(&x[i]);
      simd_cf_t b = srsran_simd_cfi_loadu(&y[i]);

      simd_cf_t r = srsran_simd_cf_prod(a, b);

      srsran_simd_cfi_storeu(&z[i], r);
    }
  }
#endif

  for (; i < len; i++) {
    z[i] = x[i] * y[i];
  }
}

void srsran_vec_prod_ccc_split_simd(const float* a_re,
                                    const float* a_im,
                                    const float* b_re,
                                    const float* b_im,
                                    float*       r_re,
                                    float*       r_im,
                                    const int    len)
{
  int i = 0;

#if SRSRAN_SIMD_F_SIZE
  if (SRSRAN_IS_ALIGNED(a_re) && SRSRAN_IS_ALIGNED(a_im) && SRSRAN_IS_ALIGNED(b_re) && SRSRAN_IS_ALIGNED(b_im) &&
      SRSRAN_IS_ALIGNED(r_re) && SRSRAN_IS_ALIGNED(r_im)) {
    for (; i < len - SRSRAN_SIMD_CF_SIZE + 1; i += SRSRAN_SIMD_CF_SIZE) {
      simd_cf_t a = srsran_simd_cf_load(&a_re[i], &a_im[i]);
      simd_cf_t b = srsran_simd_cf_load(&b_re[i], &b_im[i]);

      simd_cf_t r = srsran_simd_cf_prod(a, b);

      srsran_simd_cf_store(&r_re[i], &r_im[i], r);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_CF_SIZE + 1; i += SRSRAN_SIMD_CF_SIZE) {
      simd_cf_t a = srsran_simd_cf_loadu(&a_re[i], &a_im[i]);
      simd_cf_t b = srsran_simd_cf_loadu(&b_re[i], &b_im[i]);

      simd_cf_t r = srsran_simd_cf_prod(a, b);

      srsran_simd_cf_storeu(&r_re[i], &r_im[i], r);
    }
  }
#endif

  for (; i < len; i++) {
    r_re[i] = a_re[i] * b_re[i] - a_im[i] * b_im[i];
    r_im[i] = a_re[i] * b_im[i] + a_im[i] * b_re[i];
  }
}

#ifdef ENABLE_C16
void srsran_vec_prod_ccc_c16_simd(const int16_t* a_re,
                                  const int16_t* a_im,
                                  const int16_t* b_re,
                                  const int16_t* b_im,
                                  int16_t*       r_re,
                                  int16_t*       r_im,
                                  const int      len)
{
  int i = 0;

#if SRSRAN_SIMD_C16_SIZE
  if (SRSRAN_IS_ALIGNED(a_re) && SRSRAN_IS_ALIGNED(a_im) && SRSRAN_IS_ALIGNED(b_re) && SRSRAN_IS_ALIGNED(b_im) &&
      SRSRAN_IS_ALIGNED(r_re) && SRSRAN_IS_ALIGNED(r_im)) {
    for (; i < len - SRSRAN_SIMD_C16_SIZE + 1; i += SRSRAN_SIMD_C16_SIZE) {
      simd_c16_t a = srsran_simd_c16_load(&a_re[i], &a_im[i]);
      simd_c16_t b = srsran_simd_c16_load(&b_re[i], &b_im[i]);

      simd_c16_t r = srsran_simd_c16_prod(a, b);

      srsran_simd_c16_store(&r_re[i], &r_im[i], r);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_C16_SIZE + 1; i += SRSRAN_SIMD_C16_SIZE) {
      simd_c16_t a = srsran_simd_c16_loadu(&a_re[i], &a_im[i]);
      simd_c16_t b = srsran_simd_c16_loadu(&b_re[i], &b_im[i]);

      simd_c16_t r = srsran_simd_c16_prod(a, b);

      srsran_simd_c16_storeu(&r_re[i], &r_im[i], r);
    }
  }
#endif

  for (; i < len; i++) {
    r_re[i] = a_re[i] * b_re[i] - a_im[i] * b_im[i];
    r_im[i] = a_re[i] * b_im[i] + a_im[i] * b_re[i];
  }
}
#endif /* ENABLE_C16 */

void srsran_vec_prod_conj_ccc_simd(const cf_t* x, const cf_t* y, cf_t* z, const int len)
{
  int i = 0;

#if SRSRAN_SIMD_CF_SIZE
  if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(y) && SRSRAN_IS_ALIGNED(z)) {
    for (; i < len - SRSRAN_SIMD_CF_SIZE + 1; i += SRSRAN_SIMD_CF_SIZE) {
      simd_cf_t a = srsran_simd_cfi_load(&x[i]);
      simd_cf_t b = srsran_simd_cfi_load(&y[i]);

      simd_cf_t r = srsran_simd_cf_conjprod(a, b);

      srsran_simd_cfi_store(&z[i], r);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_CF_SIZE + 1; i += SRSRAN_SIMD_CF_SIZE) {
      simd_cf_t a = srsran_simd_cfi_loadu(&x[i]);
      simd_cf_t b = srsran_simd_cfi_loadu(&y[i]);

      simd_cf_t r = srsran_simd_cf_conjprod(a, b);

      srsran_simd_cfi_storeu(&z[i], r);
    }
  }
#endif

  for (; i < len; i++) {
    z[i] = x[i] * conjf(y[i]);
  }
}

void srsran_vec_div_ccc_simd(const cf_t* x, const cf_t* y, cf_t* z, const int len)
{
  int i = 0;

#if SRSRAN_SIMD_CF_SIZE
  if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(y) && SRSRAN_IS_ALIGNED(z)) {
    for (; i < len - SRSRAN_SIMD_CF_SIZE + 1; i += SRSRAN_SIMD_CF_SIZE) {
      simd_cf_t a = srsran_simd_cfi_load(&x[i]);
      simd_cf_t b = srsran_simd_cfi_load(&y[i]);

      simd_cf_t rcpb = srsran_simd_cf_rcp(b);
      simd_cf_t r    = srsran_simd_cf_prod(a, rcpb);

      srsran_simd_cfi_store(&z[i], r);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_CF_SIZE + 1; i += SRSRAN_SIMD_CF_SIZE) {
      simd_cf_t a = srsran_simd_cfi_loadu(&x[i]);
      simd_cf_t b = srsran_simd_cfi_loadu(&y[i]);

      simd_cf_t rcpb = srsran_simd_cf_rcp(b);
      simd_cf_t r    = srsran_simd_cf_prod(a, rcpb);

      srsran_simd_cfi_storeu(&z[i], r);
    }
  }
#endif

  for (; i < len; i++) {
    z[i] = x[i] / y[i];
  }
}

void srsran_vec_div_cfc_simd(const cf_t* x, const float* y, cf_t* z, const int len)
{
  int i = 0;

#if SRSRAN_SIMD_CF_SIZE && SRSRAN_SIMD_CF_SIZE == SRSRAN_SIMD_F_SIZE
  if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(y) && SRSRAN_IS_ALIGNED(z)) {
    for (; i < len - SRSRAN_SIMD_CF_SIZE + 1; i += SRSRAN_SIMD_CF_SIZE) {
      simd_cf_t a = srsran_simd_cfi_load(&x[i]);
      simd_f_t  b = srsran_simd_f_load(&y[i]);

      simd_f_t  rcpb = srsran_simd_f_rcp(b);
      simd_cf_t r    = srsran_simd_cf_mul(a, rcpb);

      srsran_simd_cfi_store(&z[i], r);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_CF_SIZE + 1; i += SRSRAN_SIMD_CF_SIZE) {
      simd_cf_t a = srsran_simd_cfi_loadu(&x[i]);
      simd_f_t  b = srsran_simd_f_loadu(&y[i]);

      simd_f_t  rcpb = srsran_simd_f_rcp(b);
      simd_cf_t r    = srsran_simd_cf_mul(a, rcpb);

      srsran_simd_cfi_storeu(&z[i], r);
    }
  }
#endif

  for (; i < len; i++) {
    z[i] = x[i] / y[i];
  }
}

void srsran_vec_div_fff_simd(const float* x, const float* y, float* z, const int len)
{
  int i = 0;

#if SRSRAN_SIMD_F_SIZE
  if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(y) && SRSRAN_IS_ALIGNED(z)) {
    for (; i < len - SRSRAN_SIMD_F_SIZE + 1; i += SRSRAN_SIMD_F_SIZE) {
      simd_f_t a = srsran_simd_f_load(&x[i]);
      simd_f_t b = srsran_simd_f_load(&y[i]);

      simd_f_t rcpb = srsran_simd_f_rcp(b);
      simd_f_t r    = srsran_simd_f_mul(a, rcpb);

      srsran_simd_f_store(&z[i], r);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_F_SIZE + 1; i += SRSRAN_SIMD_F_SIZE) {
      simd_f_t a = srsran_simd_f_loadu(&x[i]);
      simd_f_t b = srsran_simd_f_loadu(&y[i]);

      simd_f_t rcpb = srsran_simd_f_rcp(b);
      simd_f_t r    = srsran_simd_f_mul(a, rcpb);

      srsran_simd_f_storeu(&z[i], r);
    }
  }
#endif

  for (; i < len; i++) {
    z[i] = x[i] / y[i];
  }
}

int srsran_vec_sc_prod_ccc_simd2(const cf_t* x, const cf_t h, cf_t* z, const int len)
{
  int                i     = 0;
  const unsigned int loops = len / 4;
#ifdef HAVE_NEON
  simd_cf_t h_vec;
  h_vec.val[0] = srsran_simd_f_set1(__real__ h);
  h_vec.val[1] = srsran_simd_f_set1(__imag__ h);
  for (; i < loops; i++) {
    simd_cf_t in   = srsran_simd_cfi_load(&x[i * 4]);
    simd_cf_t temp = srsran_simd_cf_prod(in, h_vec);
    srsran_simd_cfi_store(&z[i * 4], temp);
  }

#endif
  i = loops * 4;
  return i;
}

void srsran_vec_sc_prod_ccc_simd(const cf_t* x, const cf_t h, cf_t* z, const int len)
{
  int i = 0;

#if SRSRAN_SIMD_F_SIZE

#ifdef HAVE_NEON
  i = srsran_vec_sc_prod_ccc_simd2(x, h, z, len);
#else
  const simd_f_t hre = srsran_simd_f_set1(__real__ h);
  const simd_f_t him = srsran_simd_f_set1(__imag__ h);

  if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(z)) {
    for (; i < len - SRSRAN_SIMD_F_SIZE / 2 + 1; i += SRSRAN_SIMD_F_SIZE / 2) {
      simd_f_t temp = srsran_simd_f_load((float*)&x[i]);

      simd_f_t m1 = srsran_simd_f_mul(hre, temp);
      simd_f_t sw = srsran_simd_f_swap(temp);
      simd_f_t m2 = srsran_simd_f_mul(him, sw);
      simd_f_t r  = srsran_simd_f_addsub(m1, m2);
      srsran_simd_f_store((float*)&z[i], r);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_F_SIZE / 2 + 1; i += SRSRAN_SIMD_F_SIZE / 2) {
      simd_f_t temp = srsran_simd_f_loadu((float*)&x[i]);

      simd_f_t m1 = srsran_simd_f_mul(hre, temp);
      simd_f_t sw = srsran_simd_f_swap(temp);
      simd_f_t m2 = srsran_simd_f_mul(him, sw);
      simd_f_t r  = srsran_simd_f_addsub(m1, m2);

      srsran_simd_f_storeu((float*)&z[i], r);
    }
  }
#endif
#endif
  for (; i < len; i++) {
    z[i] = x[i] * h;
  }
}

void srsran_vec_sc_prod_fff_simd(const float* x, const float h, float* z, const int len)
{
  int i = 0;

#if SRSRAN_SIMD_F_SIZE
  const simd_f_t hh = srsran_simd_f_set1(h);
  if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(z)) {
    for (; i < len - SRSRAN_SIMD_F_SIZE + 1; i += SRSRAN_SIMD_F_SIZE) {
      simd_f_t xx = srsran_simd_f_load(&x[i]);

      simd_f_t zz = srsran_simd_f_mul(xx, hh);

      srsran_simd_f_store(&z[i], zz);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_F_SIZE + 1; i += SRSRAN_SIMD_F_SIZE) {
      simd_f_t xx = srsran_simd_f_loadu(&x[i]);

      simd_f_t zz = srsran_simd_f_mul(xx, hh);

      srsran_simd_f_storeu(&z[i], zz);
    }
  }
#endif

  for (; i < len; i++) {
    z[i] = x[i] * h;
  }
}

void srsran_vec_abs_cf_simd(const cf_t* x, float* z, const int len)
{
  int i = 0;

#if SRSRAN_SIMD_F_SIZE
  if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(z)) {
    for (; i < len - SRSRAN_SIMD_F_SIZE + 1; i += SRSRAN_SIMD_F_SIZE) {
      simd_f_t x1 = srsran_simd_f_load((float*)&x[i]);
      simd_f_t x2 = srsran_simd_f_load((float*)&x[i + SRSRAN_SIMD_F_SIZE / 2]);

      simd_f_t mul1 = srsran_simd_f_mul(x1, x1);
      simd_f_t mul2 = srsran_simd_f_mul(x2, x2);

      simd_f_t z1 = srsran_simd_f_hadd(mul1, mul2);
      z1          = srsran_simd_f_sqrt(z1);
      srsran_simd_f_store(&z[i], z1);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_F_SIZE + 1; i += SRSRAN_SIMD_F_SIZE) {
      simd_f_t x1 = srsran_simd_f_loadu((float*)&x[i]);
      simd_f_t x2 = srsran_simd_f_loadu((float*)&x[i + SRSRAN_SIMD_F_SIZE / 2]);

      simd_f_t mul1 = srsran_simd_f_mul(x1, x1);
      simd_f_t mul2 = srsran_simd_f_mul(x2, x2);

      simd_f_t z1 = srsran_simd_f_hadd(mul1, mul2);
      z1          = srsran_simd_f_sqrt(z1);

      srsran_simd_f_storeu(&z[i], z1);
    }
  }
#endif

  for (; i < len; i++) {
    z[i] = sqrtf(__real__(x[i]) * __real__(x[i]) + __imag__(x[i]) * __imag__(x[i]));
  }
}

void srsran_vec_abs_square_cf_simd(const cf_t* x, float* z, const int len)
{
  int i = 0;

#if SRSRAN_SIMD_F_SIZE
  if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(z)) {
    for (; i < len - SRSRAN_SIMD_F_SIZE + 1; i += SRSRAN_SIMD_F_SIZE) {
      simd_f_t x1 = srsran_simd_f_load((float*)&x[i]);
      simd_f_t x2 = srsran_simd_f_load((float*)&x[i + SRSRAN_SIMD_F_SIZE / 2]);

      simd_f_t mul1 = srsran_simd_f_mul(x1, x1);
      simd_f_t mul2 = srsran_simd_f_mul(x2, x2);

      simd_f_t z1 = srsran_simd_f_hadd(mul1, mul2);

      srsran_simd_f_store(&z[i], z1);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_F_SIZE + 1; i += SRSRAN_SIMD_F_SIZE) {
      simd_f_t x1 = srsran_simd_f_loadu((float*)&x[i]);
      simd_f_t x2 = srsran_simd_f_loadu((float*)&x[i + SRSRAN_SIMD_F_SIZE / 2]);

      simd_f_t mul1 = srsran_simd_f_mul(x1, x1);
      simd_f_t mul2 = srsran_simd_f_mul(x2, x2);

      simd_f_t z1 = srsran_simd_f_hadd(mul1, mul2);

      srsran_simd_f_storeu(&z[i], z1);
    }
  }
#endif

  for (; i < len; i++) {
    z[i] = __real__(x[i]) * __real__(x[i]) + __imag__(x[i]) * __imag__(x[i]);
  }
}

void srsran_vec_sc_prod_cfc_simd(const cf_t* x, const float h, cf_t* z, const int len)
{
  int i = 0;

#if SRSRAN_SIMD_F_SIZE
  const simd_f_t tap = srsran_simd_f_set1(h);

  if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(z)) {
    for (; i < len - SRSRAN_SIMD_F_SIZE / 2 + 1; i += SRSRAN_SIMD_F_SIZE / 2) {
      simd_f_t temp = srsran_simd_f_load((float*)&x[i]);

      temp = srsran_simd_f_mul(tap, temp);

      srsran_simd_f_store((float*)&z[i], temp);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_F_SIZE / 2 + 1; i += SRSRAN_SIMD_F_SIZE / 2) {
      simd_f_t temp = srsran_simd_f_loadu((float*)&x[i]);

      temp = srsran_simd_f_mul(tap, temp);

      srsran_simd_f_storeu((float*)&z[i], temp);
    }
  }
#endif

  for (; i < len; i++) {
    z[i] = x[i] * h;
  }
}

void srsran_vec_sc_prod_fcc_simd(const float* x, const cf_t h, cf_t* z, const int len)
{
  int i = 0;

#if SRSRAN_SIMD_F_SIZE
  const simd_cf_t tap = srsran_simd_cf_set1(h);

  if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(z)) {
    for (; i < len - SRSRAN_SIMD_F_SIZE + 1; i += SRSRAN_SIMD_F_SIZE) {
      srsran_simd_cfi_store(&z[i], srsran_simd_cf_mul(tap, srsran_simd_f_load(&x[i])));
    }
  } else {
    for (; i < len - SRSRAN_SIMD_F_SIZE + 1; i += SRSRAN_SIMD_F_SIZE) {
      srsran_simd_cfi_storeu(&z[i], srsran_simd_cf_mul(tap, srsran_simd_f_loadu(&x[i])));
    }
  }
#endif

  for (; i < len; i++) {
    z[i] = x[i] * h;
  }
}

uint32_t srsran_vec_max_fi_simd(const float* x, const int len)
{
  int i = 0;

  float    max_value = -INFINITY;
  uint32_t max_index = 0;

#if SRSRAN_SIMD_I_SIZE
  srsran_simd_aligned int   indexes_buffer[SRSRAN_SIMD_I_SIZE] = {0};
  srsran_simd_aligned float values_buffer[SRSRAN_SIMD_I_SIZE]  = {0};

  for (int k = 0; k < SRSRAN_SIMD_I_SIZE; k++)
    indexes_buffer[k] = k;
  simd_i_t simd_inc         = srsran_simd_i_set1(SRSRAN_SIMD_I_SIZE);
  simd_i_t simd_indexes     = srsran_simd_i_load(indexes_buffer);
  simd_i_t simd_max_indexes = srsran_simd_i_set1(0);

  simd_f_t simd_max_values = srsran_simd_f_set1(-INFINITY);

  if (SRSRAN_IS_ALIGNED(x)) {
    for (; i < len - SRSRAN_SIMD_I_SIZE + 1; i += SRSRAN_SIMD_I_SIZE) {
      simd_f_t   a     = srsran_simd_f_load(&x[i]);
      simd_sel_t res   = srsran_simd_f_max(a, simd_max_values);
      simd_max_indexes = srsran_simd_i_select(simd_max_indexes, simd_indexes, res);
      simd_max_values  = srsran_simd_f_select(simd_max_values, a, res);
      simd_indexes     = srsran_simd_i_add(simd_indexes, simd_inc);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_I_SIZE + 1; i += SRSRAN_SIMD_I_SIZE) {
      simd_f_t   a     = srsran_simd_f_loadu(&x[i]);
      simd_sel_t res   = srsran_simd_f_max(a, simd_max_values);
      simd_max_indexes = srsran_simd_i_select(simd_max_indexes, simd_indexes, res);
      simd_max_values  = srsran_simd_f_select(simd_max_values, a, res);
      simd_indexes     = srsran_simd_i_add(simd_indexes, simd_inc);
    }
  }

  srsran_simd_i_store(indexes_buffer, simd_max_indexes);
  srsran_simd_f_store(values_buffer, simd_max_values);

  for (int k = 0; k < SRSRAN_SIMD_I_SIZE; k++) {
    if (values_buffer[k] > max_value) {
      max_value = values_buffer[k];
      max_index = (uint32_t)indexes_buffer[k];
    }
  }
#endif /* SRSRAN_SIMD_I_SIZE */

  for (; i < len; i++) {
    if (x[i] > max_value) {
      max_value = x[i];
      max_index = (uint32_t)i;
    }
  }

  return max_index;
}

uint32_t srsran_vec_max_abs_fi_simd(const float* x, const int len)
{
  int i = 0;

  float    max_value = -INFINITY;
  uint32_t max_index = 0;

#if SRSRAN_SIMD_I_SIZE
  srsran_simd_aligned int   indexes_buffer[SRSRAN_SIMD_I_SIZE] = {0};
  srsran_simd_aligned float values_buffer[SRSRAN_SIMD_I_SIZE]  = {0};

  for (int k = 0; k < SRSRAN_SIMD_I_SIZE; k++)
    indexes_buffer[k] = k;
  simd_i_t simd_inc         = srsran_simd_i_set1(SRSRAN_SIMD_I_SIZE);
  simd_i_t simd_indexes     = srsran_simd_i_load(indexes_buffer);
  simd_i_t simd_max_indexes = srsran_simd_i_set1(0);

  simd_f_t simd_max_values = srsran_simd_f_set1(-INFINITY);

  if (SRSRAN_IS_ALIGNED(x)) {
    for (; i < len - SRSRAN_SIMD_I_SIZE + 1; i += SRSRAN_SIMD_I_SIZE) {
      simd_f_t   a     = srsran_simd_f_abs(srsran_simd_f_load(&x[i]));
      simd_sel_t res   = srsran_simd_f_max(a, simd_max_values);
      simd_max_indexes = srsran_simd_i_select(simd_max_indexes, simd_indexes, res);
      simd_max_values  = srsran_simd_f_select(simd_max_values, a, res);
      simd_indexes     = srsran_simd_i_add(simd_indexes, simd_inc);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_I_SIZE + 1; i += SRSRAN_SIMD_I_SIZE) {
      simd_f_t   a     = srsran_simd_f_abs(srsran_simd_f_loadu(&x[i]));
      simd_sel_t res   = srsran_simd_f_max(a, simd_max_values);
      simd_max_indexes = srsran_simd_i_select(simd_max_indexes, simd_indexes, res);
      simd_max_values  = srsran_simd_f_select(simd_max_values, a, res);
      simd_indexes     = srsran_simd_i_add(simd_indexes, simd_inc);
    }
  }

  srsran_simd_i_store(indexes_buffer, simd_max_indexes);
  srsran_simd_f_store(values_buffer, simd_max_values);

  for (int k = 0; k < SRSRAN_SIMD_I_SIZE; k++) {
    if (values_buffer[k] > max_value) {
      max_value = values_buffer[k];
      max_index = (uint32_t)indexes_buffer[k];
    }
  }
#endif /* SRSRAN_SIMD_I_SIZE */

  for (; i < len; i++) {
    float a = fabsf(x[i]);
    if (a > max_value) {
      max_value = a;
      max_index = (uint32_t)i;
    }
  }

  return max_index;
}

uint32_t srsran_vec_max_ci_simd(const cf_t* x, const int len)
{
  int i = 0;

  float    max_value = -INFINITY;
  uint32_t max_index = 0;

#if SRSRAN_SIMD_I_SIZE
  srsran_simd_aligned int   indexes_buffer[SRSRAN_SIMD_I_SIZE] = {0};
  srsran_simd_aligned float values_buffer[SRSRAN_SIMD_I_SIZE]  = {0};

  for (int k = 0; k < SRSRAN_SIMD_I_SIZE; k++)
    indexes_buffer[k] = k;
  simd_i_t simd_inc         = srsran_simd_i_set1(SRSRAN_SIMD_I_SIZE);
  simd_i_t simd_indexes     = srsran_simd_i_load(indexes_buffer);
  simd_i_t simd_max_indexes = srsran_simd_i_set1(0);

  simd_f_t simd_max_values = srsran_simd_f_set1(-INFINITY);

  if (SRSRAN_IS_ALIGNED(x)) {
    for (; i < len - SRSRAN_SIMD_I_SIZE + 1; i += SRSRAN_SIMD_I_SIZE) {
      simd_f_t x1 = srsran_simd_f_load((float*)&x[i]);
      simd_f_t x2 = srsran_simd_f_load((float*)&x[i + SRSRAN_SIMD_F_SIZE / 2]);

      simd_f_t mul1 = srsran_simd_f_mul(x1, x1);
      simd_f_t mul2 = srsran_simd_f_mul(x2, x2);

      simd_f_t z1 = srsran_simd_f_hadd(mul1, mul2);

      simd_sel_t res = srsran_simd_f_max(z1, simd_max_values);

      simd_max_indexes = srsran_simd_i_select(simd_max_indexes, simd_indexes, res);
      simd_max_values  = srsran_simd_f_select(simd_max_values, z1, res);
      simd_indexes     = srsran_simd_i_add(simd_indexes, simd_inc);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_I_SIZE + 1; i += SRSRAN_SIMD_I_SIZE) {
      simd_f_t x1 = srsran_simd_f_loadu((float*)&x[i]);
      simd_f_t x2 = srsran_simd_f_loadu((float*)&x[i + SRSRAN_SIMD_F_SIZE / 2]);

      simd_f_t mul1 = srsran_simd_f_mul(x1, x1);
      simd_f_t mul2 = srsran_simd_f_mul(x2, x2);

      simd_f_t z1 = srsran_simd_f_hadd(mul1, mul2);

      simd_sel_t res = srsran_simd_f_max(z1, simd_max_values);

      simd_max_indexes = srsran_simd_i_select(simd_max_indexes, simd_indexes, res);
      simd_max_values  = srsran_simd_f_select(simd_max_values, z1, res);
      simd_indexes     = srsran_simd_i_add(simd_indexes, simd_inc);
    }
  }

  srsran_simd_i_store(indexes_buffer, simd_max_indexes);
  srsran_simd_f_store(values_buffer, simd_max_values);

  for (int k = 0; k < SRSRAN_SIMD_I_SIZE; k++) {
    if (values_buffer[k] > max_value) {
      max_value = values_buffer[k];
      max_index = (uint32_t)indexes_buffer[k];
    }
  }
#endif /* SRSRAN_SIMD_I_SIZE */

  for (; i < len; i++) {
    cf_t  a    = x[i];
    float abs2 = __real__ a * __real__ a + __imag__ a * __imag__ a;
    if (abs2 > max_value) {
      max_value = abs2;
      max_index = (uint32_t)i;
    }
  }

  return max_index;
}

void srsran_vec_interleave_simd(const cf_t* x, const cf_t* y, cf_t* z, const int len)
{
  uint32_t i = 0, k = 0;

#ifdef LV_HAVE_SSE
  if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(y) && SRSRAN_IS_ALIGNED(z)) {
    for (; i < len - 2 + 1; i += 2) {
      __m128i a = _mm_load_si128((__m128i*)&x[i]);
      __m128i b = _mm_load_si128((__m128i*)&y[i]);

      __m128i r1 = _mm_unpacklo_epi64(a, b);
      _mm_store_si128((__m128i*)&z[k], r1);
      k += 2;

      __m128i r2 = _mm_unpackhi_epi64(a, b);
      _mm_store_si128((__m128i*)&z[k], r2);
      k += 2;
    }
  } else {
    for (; i < len - 2 + 1; i += 2) {
      __m128i a = _mm_loadu_si128((__m128i*)&x[i]);
      __m128i b = _mm_loadu_si128((__m128i*)&y[i]);

      __m128i r1 = _mm_unpacklo_epi64(a, b);
      _mm_storeu_si128((__m128i*)&z[k], r1);
      k += 2;

      __m128i r2 = _mm_unpackhi_epi64(a, b);
      _mm_storeu_si128((__m128i*)&z[k], r2);
      k += 2;
    }
  }
#endif /* LV_HAVE_SSE */

  for (; i < len; i++) {
    z[k++] = x[i];
    z[k++] = y[i];
  }
}

void srsran_vec_interleave_add_simd(const cf_t* x, const cf_t* y, cf_t* z, const int len)
{
  uint32_t i = 0, k = 0;

#ifdef LV_HAVE_SSE
  if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(y) && SRSRAN_IS_ALIGNED(z)) {
    for (; i < len - 2 + 1; i += 2) {
      __m128i a = _mm_load_si128((__m128i*)&x[i]);
      __m128i b = _mm_load_si128((__m128i*)&y[i]);

      __m128 r1 = (__m128)_mm_unpacklo_epi64(a, b);
      __m128 z1 = _mm_load_ps((float*)&z[k]);
      r1        = _mm_add_ps((__m128)r1, z1);
      _mm_store_ps((float*)&z[k], r1);
      k += 2;

      __m128 r2 = (__m128)_mm_unpackhi_epi64(a, b);
      __m128 z2 = _mm_load_ps((float*)&z[k]);
      r2        = _mm_add_ps((__m128)r2, z2);
      _mm_store_ps((float*)&z[k], r2);
      k += 2;
    }
  } else {
    for (; i < len - 2 + 1; i += 2) {
      __m128i a = _mm_loadu_si128((__m128i*)&x[i]);
      __m128i b = _mm_loadu_si128((__m128i*)&y[i]);

      __m128 r1 = (__m128)_mm_unpacklo_epi64(a, b);
      __m128 z1 = _mm_loadu_ps((float*)&z[k]);
      r1        = _mm_add_ps((__m128)r1, z1);
      _mm_storeu_ps((float*)&z[k], r1);
      k += 2;

      __m128 r2 = (__m128)_mm_unpackhi_epi64(a, b);
      __m128 z2 = _mm_loadu_ps((float*)&z[k]);
      r2        = _mm_add_ps((__m128)r2, z2);
      _mm_storeu_ps((float*)&z[k], r2);
      k += 2;
    }
  }
#endif /* LV_HAVE_SSE */

  for (; i < len; i++) {
    z[k++] += x[i];
    z[k++] += y[i];
  }
}

cf_t srsran_vec_gen_sine_simd(cf_t amplitude, float freq, cf_t* z, int len)
{
  const float TWOPI = 2.0f * (float)M_PI;
  cf_t        osc   = cexpf(_Complex_I * TWOPI * freq);
  cf_t        phase = 1.0f;
  int         i     = 0;

#if SRSRAN_SIMD_CF_SIZE
  __attribute__((aligned(64))) cf_t _phase[SRSRAN_SIMD_CF_SIZE];
  _phase[0] = phase;

  if (i < len - SRSRAN_SIMD_CF_SIZE + 1) {
    for (int k = 1; k < SRSRAN_SIMD_CF_SIZE; k++) {
      _phase[k] = _phase[k - 1] * osc;
    }
  }
  simd_cf_t _simd_osc   = srsran_simd_cf_set1(cexpf(_Complex_I * TWOPI * freq * SRSRAN_SIMD_CF_SIZE));
  simd_cf_t _simd_phase = srsran_simd_cfi_load(_phase);
  simd_cf_t a           = srsran_simd_cf_set1(amplitude);

  if (SRSRAN_IS_ALIGNED(z)) {
    for (; i < len - SRSRAN_SIMD_CF_SIZE + 1; i += SRSRAN_SIMD_CF_SIZE) {
      simd_cf_t r = srsran_simd_cf_prod(a, _simd_phase);
      srsran_simd_cfi_store(&z[i], r);
      _simd_phase = srsran_simd_cf_prod(_simd_phase, _simd_osc);
    }
  } else {
    for (; i < len - SRSRAN_SIMD_CF_SIZE + 1; i += SRSRAN_SIMD_CF_SIZE) {
      simd_cf_t r = srsran_simd_cf_prod(a, _simd_phase);
      srsran_simd_cfi_storeu(&z[i], r);
      _simd_phase = srsran_simd_cf_prod(_simd_phase, _simd_osc);
    }
  }

  // Store pahse and get last phase
  srsran_simd_cfi_store(_phase, _simd_phase);
  phase = _phase[0];
#endif /* SRSRAN_SIMD_CF_SIZE */

  for (; i < len; i++) {
    z[i] = amplitude * phase;

    phase *= osc;
  }
  return phase;
}

void srsran_vec_apply_cfo_simd(const cf_t* x, float cfo, cf_t* z, int len)
{
  const float TWOPI = 2.0f * (float)M_PI;
  int         i     = 0;
  cf_t        osc   = cexpf(_Complex_I * TWOPI * cfo);
  cf_t        phase = 1.0f;

#if SRSRAN_SIMD_CF_SIZE
  // Load initial phases and oscillator
  srsran_simd_aligned cf_t _phase[SRSRAN_SIMD_CF_SIZE];
  _phase[0] = phase;
  for (int k = 1; k < SRSRAN_SIMD_CF_SIZE; k++) {
    _phase[k] = _phase[k - 1] * osc;
  }
  simd_cf_t _simd_osc   = srsran_simd_cf_set1(_phase[SRSRAN_SIMD_CF_SIZE - 1] * osc);
  simd_cf_t _simd_phase = srsran_simd_cfi_load(_phase);

  if (SRSRAN_IS_ALIGNED(x) && SRSRAN_IS_ALIGNED(z)) {
    // For aligned memory
    for (; i < len - SRSRAN_SIMD_CF_SIZE + 1; i += SRSRAN_SIMD_CF_SIZE) {
      simd_cf_t a = srsran_simd_cfi_load(&x[i]);

      simd_cf_t r = srsran_simd_cf_prod(a, _simd_phase);

      srsran_simd_cfi_store(&z[i], r);

      _simd_phase = srsran_simd_cf_prod(_simd_phase, _simd_osc);
    }
  } else {
    // For unaligned memory
    for (; i < len - SRSRAN_SIMD_F_SIZE + 1; i += SRSRAN_SIMD_F_SIZE) {
      simd_cf_t a = srsran_simd_cfi_loadu(&x[i]);

      simd_cf_t r = srsran_simd_cf_prod(a, _simd_phase);

      srsran_simd_cfi_storeu(&z[i], r);

      _simd_phase = srsran_simd_cf_prod(_simd_phase, _simd_osc);
    }
  }

  // Stores the next phase
  srsran_simd_cfi_store(_phase, _simd_phase);
  phase = _phase[0];
#endif

  for (; i < len; i++) {
    z[i] = x[i] * phase;

    phase *= osc;
  }
}

float srsran_vec_estimate_frequency_simd(const cf_t* x, int len)
{
  cf_t sum = 0.0f;

  /* Asssumes x[n] = cexp(j·2·pi·n·O) = cos(j·2·pi·n·O) + j · sin(j·2·pi·n·O)
   * where O = f / f_s */

  int i = 1;

#if SRSRAN_SIMD_CF_SIZE
  if (len > SRSRAN_SIMD_CF_SIZE) {
    simd_cf_t _sum = srsran_simd_cf_zero();

    for (; i < len - SRSRAN_SIMD_CF_SIZE + 1; i += SRSRAN_SIMD_CF_SIZE) {
      simd_cf_t a1 = srsran_simd_cfi_loadu(&x[i]);
      simd_cf_t a2 = srsran_simd_cfi_loadu(&x[i - 1]);

      // Multiply by conjugate and accumulate
      _sum = srsran_simd_cf_add(srsran_simd_cf_conjprod(a1, a2), _sum);
    }

    // Accumulate using horizontal addition
    simd_f_t _sum_re    = srsran_simd_cf_re(_sum);
    simd_f_t _sum_im    = srsran_simd_cf_im(_sum);
    simd_f_t _sum_re_im = srsran_simd_f_hadd(_sum_re, _sum_im);
    for (int j = 2; j < SRSRAN_SIMD_F_SIZE; j *= 2) {
      _sum_re_im = srsran_simd_f_hadd(_sum_re_im, _sum_re_im);
    }

    // Get accumulator
    srsran_simd_aligned float _sum_v[SRSRAN_SIMD_CF_SIZE];
    srsran_simd_f_store(_sum_v, _sum_re_im);
    __real__ sum = _sum_v[0];
    __imag__ sum = _sum_v[1];
  }
#endif /* SRSRAN_SIMD_CF_SIZE */

  for (; i < len; i++) {
    sum += x[i] * conjf(x[i - 1]);
  }

  // Extract argument and divide by (-2·PI)
  return -cargf(sum) * M_1_PI * 0.5f;
}
