/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#include "log.h"
#include "tests.h"

TEST(Cvt, simd_level) {
  enum st_simd_level cpu_level = st_get_simd_level();
  const char* name = st_get_simd_level_name(cpu_level);
  info("simd level by cpu: %d(%s)\n", cpu_level, name);
}

static void test_cvt_rfc4175_422be10_to_yuv422p10le(int w, int h,
                                                    enum st_simd_level cvt_level,
                                                    enum st_simd_level back_level) {
  int ret;
  size_t fb_pg2_size = w * h * 5 / 2;
  struct st20_rfc4175_422_10_pg2_be* pg =
      (struct st20_rfc4175_422_10_pg2_be*)st_test_zmalloc(fb_pg2_size);
  struct st20_rfc4175_422_10_pg2_be* pg_2 =
      (struct st20_rfc4175_422_10_pg2_be*)st_test_zmalloc(fb_pg2_size);
  size_t planar_size = w * h * 2 * sizeof(uint16_t);
  uint16_t* p10_u16 = (uint16_t*)st_test_zmalloc(planar_size);

  if (!pg || !pg_2 || !p10_u16) {
    EXPECT_EQ(0, 1);
    if (pg) st_test_free(pg);
    if (pg_2) st_test_free(pg_2);
    if (p10_u16) st_test_free(p10_u16);
    return;
  }

  st_test_rand_data((uint8_t*)pg, fb_pg2_size, 0);

  ret = st20_rfc4175_422be10_to_yuv422p10le_simd(
      pg, p10_u16, (p10_u16 + w * h), (p10_u16 + w * h * 3 / 2), w, h, cvt_level);
  EXPECT_EQ(0, ret);

  ret = st20_yuv422p10le_to_rfc4175_422be10_simd(
      p10_u16, (p10_u16 + w * h), (p10_u16 + w * h * 3 / 2), pg_2, w, h, back_level);
  EXPECT_EQ(0, ret);

  EXPECT_EQ(0, memcmp(pg, pg_2, fb_pg2_size));

  st_test_free(pg);
  st_test_free(pg_2);
  st_test_free(p10_u16);
}

TEST(Cvt, rfc4175_422be10_to_yuv422p10le) {
  test_cvt_rfc4175_422be10_to_yuv422p10le(1920, 1080, ST_SIMD_LEVEL_MAX,
                                          ST_SIMD_LEVEL_MAX);
}

TEST(Cvt, rfc4175_422be10_to_yuv422p10le_scalar) {
  test_cvt_rfc4175_422be10_to_yuv422p10le(1920, 1080, ST_SIMD_LEVEL_NONE,
                                          ST_SIMD_LEVEL_NONE);
}

TEST(Cvt, rfc4175_422be10_to_yuv422p10le_avx512) {
  test_cvt_rfc4175_422be10_to_yuv422p10le(1920, 1080, ST_SIMD_LEVEL_AVX512,
                                          ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_yuv422p10le(722, 111, ST_SIMD_LEVEL_AVX512,
                                          ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_yuv422p10le(722, 111, ST_SIMD_LEVEL_NONE,
                                          ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_yuv422p10le(722, 111, ST_SIMD_LEVEL_AVX512,
                                          ST_SIMD_LEVEL_NONE);
  int w = 2; /* each pg has two pixels */
  for (int h = 640; h < (640 + 64); h++) {
    test_cvt_rfc4175_422be10_to_yuv422p10le(w, h, ST_SIMD_LEVEL_AVX512,
                                            ST_SIMD_LEVEL_AVX512);
  }
}

TEST(Cvt, rfc4175_422be10_to_yuv422p10le_avx512_vbmi) {
  test_cvt_rfc4175_422be10_to_yuv422p10le(1920, 1080, ST_SIMD_LEVEL_AVX512_VBMI2,
                                          ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422be10_to_yuv422p10le(722, 111, ST_SIMD_LEVEL_AVX512_VBMI2,
                                          ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422be10_to_yuv422p10le(722, 111, ST_SIMD_LEVEL_NONE,
                                          ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422be10_to_yuv422p10le(722, 111, ST_SIMD_LEVEL_AVX512_VBMI2,
                                          ST_SIMD_LEVEL_NONE);
  int w = 2; /* each pg has two pixels */
  for (int h = 640; h < (640 + 64); h++) {
    test_cvt_rfc4175_422be10_to_yuv422p10le(w, h, ST_SIMD_LEVEL_AVX512_VBMI2,
                                            ST_SIMD_LEVEL_AVX512_VBMI2);
  }
}

static void test_cvt_rfc4175_422be10_to_yuv422p10le_dma(st_udma_handle dma, int w, int h,
                                                        enum st_simd_level cvt_level,
                                                        enum st_simd_level back_level) {
  int ret;
  size_t fb_pg2_size = w * h * 5 / 2;
  struct st_tests_context* ctx = st_test_ctx();
  st_handle st = ctx->handle;
  struct st20_rfc4175_422_10_pg2_be* pg =
      (struct st20_rfc4175_422_10_pg2_be*)st_hp_zmalloc(st, fb_pg2_size, ST_PORT_P);
  struct st20_rfc4175_422_10_pg2_be* pg_2 =
      (struct st20_rfc4175_422_10_pg2_be*)st_test_zmalloc(fb_pg2_size);
  size_t planar_size = w * h * 2 * sizeof(uint16_t);
  uint16_t* p10_u16 = (uint16_t*)st_test_zmalloc(planar_size);

  if (!pg || !pg_2 || !p10_u16) {
    EXPECT_EQ(0, 1);
    if (pg) st_hp_free(st, pg);
    if (pg_2) st_test_free(pg_2);
    if (p10_u16) st_test_free(p10_u16);
    return;
  }

  st_test_rand_data((uint8_t*)pg, fb_pg2_size, 0);

  ret = st20_rfc4175_422be10_to_yuv422p10le_simd_dma(
      dma, pg, st_hp_virt2iova(st, pg), p10_u16, (p10_u16 + w * h),
      (p10_u16 + w * h * 3 / 2), w, h, cvt_level);
  EXPECT_EQ(0, ret);

  ret = st20_yuv422p10le_to_rfc4175_422be10_simd(
      p10_u16, (p10_u16 + w * h), (p10_u16 + w * h * 3 / 2), pg_2, w, h, back_level);
  EXPECT_EQ(0, ret);

  EXPECT_EQ(0, memcmp(pg, pg_2, fb_pg2_size));

  st_hp_free(st, pg);
  st_test_free(pg_2);
  st_test_free(p10_u16);
}

TEST(Cvt, rfc4175_422be10_to_yuv422p10le_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_rfc4175_422be10_to_yuv422p10le_dma(dma, 1920, 1080, ST_SIMD_LEVEL_MAX,
                                              ST_SIMD_LEVEL_MAX);

  st_udma_free(dma);
}

TEST(Cvt, rfc4175_422be10_to_yuv422p10le_scalar_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_rfc4175_422be10_to_yuv422p10le_dma(dma, 1920, 1080, ST_SIMD_LEVEL_NONE,
                                              ST_SIMD_LEVEL_NONE);

  st_udma_free(dma);
}

TEST(Cvt, rfc4175_422be10_to_yuv422p10le_avx512_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_rfc4175_422be10_to_yuv422p10le_dma(dma, 1920, 1080, ST_SIMD_LEVEL_AVX512,
                                              ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_yuv422p10le_dma(dma, 722, 111, ST_SIMD_LEVEL_AVX512,
                                              ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_yuv422p10le_dma(dma, 722, 111, ST_SIMD_LEVEL_NONE,
                                              ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_yuv422p10le_dma(dma, 722, 111, ST_SIMD_LEVEL_AVX512,
                                              ST_SIMD_LEVEL_NONE);
  int w = 2; /* each pg has two pixels */
  for (int h = 640; h < (640 + 64); h++) {
    test_cvt_rfc4175_422be10_to_yuv422p10le_dma(dma, w, h, ST_SIMD_LEVEL_AVX512,
                                                ST_SIMD_LEVEL_AVX512);
  }

  st_udma_free(dma);
}

TEST(Cvt, rfc4175_422be10_to_yuv422p10le_avx512_vbmi_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_rfc4175_422be10_to_yuv422p10le_dma(dma, 1920, 1080, ST_SIMD_LEVEL_AVX512_VBMI2,
                                              ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422be10_to_yuv422p10le_dma(dma, 722, 111, ST_SIMD_LEVEL_AVX512_VBMI2,
                                              ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422be10_to_yuv422p10le_dma(dma, 722, 111, ST_SIMD_LEVEL_NONE,
                                              ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422be10_to_yuv422p10le_dma(dma, 722, 111, ST_SIMD_LEVEL_AVX512_VBMI2,
                                              ST_SIMD_LEVEL_NONE);
  int w = 2; /* each pg has two pixels */
  for (int h = 640; h < (640 + 64); h++) {
    test_cvt_rfc4175_422be10_to_yuv422p10le_dma(dma, w, h, ST_SIMD_LEVEL_AVX512_VBMI2,
                                                ST_SIMD_LEVEL_AVX512_VBMI2);
  }

  st_udma_free(dma);
}

static void test_cvt_yuv422p10le_to_rfc4175_422be10(int w, int h,
                                                    enum st_simd_level cvt_level,
                                                    enum st_simd_level back_level) {
  int ret;
  size_t fb_pg2_size = w * h * 5 / 2;
  struct st20_rfc4175_422_10_pg2_be* pg =
      (struct st20_rfc4175_422_10_pg2_be*)st_test_zmalloc(fb_pg2_size);
  size_t planar_size = w * h * 2 * sizeof(uint16_t);
  uint16_t* p10_u16 = (uint16_t*)st_test_zmalloc(planar_size);
  uint16_t* p10_u16_2 = (uint16_t*)st_test_zmalloc(planar_size);

  if (!pg || !p10_u16_2 || !p10_u16) {
    EXPECT_EQ(0, 1);
    if (pg) st_test_free(pg);
    if (p10_u16_2) st_test_free(p10_u16_2);
    if (p10_u16) st_test_free(p10_u16);
    return;
  }

  for (size_t i = 0; i < (planar_size / 2); i++) {
    p10_u16[i] = rand() & 0x3ff; /* only 10 bit */
  }

  ret = st20_yuv422p10le_to_rfc4175_422be10_simd(
      p10_u16, (p10_u16 + w * h), (p10_u16 + w * h * 3 / 2), pg, w, h, cvt_level);
  EXPECT_EQ(0, ret);

  ret = st20_rfc4175_422be10_to_yuv422p10le_simd(
      pg, p10_u16_2, (p10_u16_2 + w * h), (p10_u16_2 + w * h * 3 / 2), w, h, back_level);
  EXPECT_EQ(0, ret);

  EXPECT_EQ(0, memcmp(p10_u16, p10_u16_2, planar_size));

  st_test_free(pg);
  st_test_free(p10_u16);
  st_test_free(p10_u16_2);
}

TEST(Cvt, yuv422p10le_to_rfc4175_422be10) {
  test_cvt_yuv422p10le_to_rfc4175_422be10(1920, 1080, ST_SIMD_LEVEL_MAX,
                                          ST_SIMD_LEVEL_MAX);
}

TEST(Cvt, yuv422p10le_to_rfc4175_422be10_scalar) {
  test_cvt_yuv422p10le_to_rfc4175_422be10(1920, 1080, ST_SIMD_LEVEL_NONE,
                                          ST_SIMD_LEVEL_NONE);
}

TEST(Cvt, yuv422p10le_to_rfc4175_422be10_avx512) {
  test_cvt_yuv422p10le_to_rfc4175_422be10(1920, 1080, ST_SIMD_LEVEL_AVX512,
                                          ST_SIMD_LEVEL_AVX512);
  test_cvt_yuv422p10le_to_rfc4175_422be10(722, 111, ST_SIMD_LEVEL_AVX512,
                                          ST_SIMD_LEVEL_AVX512);
  test_cvt_yuv422p10le_to_rfc4175_422be10(722, 111, ST_SIMD_LEVEL_NONE,
                                          ST_SIMD_LEVEL_AVX512);
  test_cvt_yuv422p10le_to_rfc4175_422be10(722, 111, ST_SIMD_LEVEL_AVX512,
                                          ST_SIMD_LEVEL_NONE);
  int w = 2; /* each pg has two pixels */
  for (int h = 640; h < (640 + 64); h++) {
    test_cvt_yuv422p10le_to_rfc4175_422be10(w, h, ST_SIMD_LEVEL_AVX512,
                                            ST_SIMD_LEVEL_AVX512);
  }
}

TEST(Cvt, yuv422p10le_to_rfc4175_422be10_avx512_vbmi) {
  test_cvt_yuv422p10le_to_rfc4175_422be10(1920, 1080, ST_SIMD_LEVEL_AVX512_VBMI2,
                                          ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_yuv422p10le_to_rfc4175_422be10(722, 111, ST_SIMD_LEVEL_AVX512_VBMI2,
                                          ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_yuv422p10le_to_rfc4175_422be10(722, 111, ST_SIMD_LEVEL_NONE,
                                          ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_yuv422p10le_to_rfc4175_422be10(722, 111, ST_SIMD_LEVEL_AVX512_VBMI2,
                                          ST_SIMD_LEVEL_NONE);
  int w = 2; /* each pg has two pixels */
  for (int h = 640; h < (640 + 64); h++) {
    test_cvt_yuv422p10le_to_rfc4175_422be10(w, h, ST_SIMD_LEVEL_AVX512_VBMI2,
                                            ST_SIMD_LEVEL_AVX512_VBMI2);
  }
}

static void test_cvt_yuv422p10le_to_rfc4175_422be10_dma(st_udma_handle dma, int w, int h,
                                                        enum st_simd_level cvt_level,
                                                        enum st_simd_level back_level) {
  int ret;
  size_t fb_pg2_size = w * h * 5 / 2;
  struct st_tests_context* ctx = st_test_ctx();
  st_handle st = ctx->handle;
  struct st20_rfc4175_422_10_pg2_be* pg =
      (struct st20_rfc4175_422_10_pg2_be*)st_test_zmalloc(fb_pg2_size);
  size_t planar_size = w * h * 2 * sizeof(uint16_t);
  uint16_t* p10_u16 = (uint16_t*)st_hp_zmalloc(st, planar_size, ST_PORT_P);
  st_iova_t p10_u16_iova = st_hp_virt2iova(st, p10_u16);
  uint16_t* p10_u16_2 = (uint16_t*)st_test_zmalloc(planar_size);

  if (!pg || !p10_u16_2 || !p10_u16) {
    EXPECT_EQ(0, 1);
    if (pg) st_test_free(pg);
    if (p10_u16_2) st_test_free(p10_u16_2);
    if (p10_u16) st_test_free(p10_u16);
    return;
  }

  for (size_t i = 0; i < (planar_size / 2); i++) {
    p10_u16[i] = rand() & 0x3ff; /* only 10 bit */
  }

  ret = st20_yuv422p10le_to_rfc4175_422be10_simd_dma(
      dma, p10_u16, p10_u16_iova, (p10_u16 + w * h), (p10_u16_iova + w * h * 2),
      (p10_u16 + w * h * 3 / 2), (p10_u16_iova + w * h * 3), pg, w, h, cvt_level);
  EXPECT_EQ(0, ret);

  ret = st20_rfc4175_422be10_to_yuv422p10le_simd(
      pg, p10_u16_2, (p10_u16_2 + w * h), (p10_u16_2 + w * h * 3 / 2), w, h, back_level);
  EXPECT_EQ(0, ret);

  EXPECT_EQ(0, memcmp(p10_u16, p10_u16_2, planar_size));

  st_test_free(pg);
  st_hp_free(st, p10_u16);
  st_test_free(p10_u16_2);
}

TEST(Cvt, yuv422p10le_to_rfc4175_422be10_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_yuv422p10le_to_rfc4175_422be10_dma(dma, 1920, 1080, ST_SIMD_LEVEL_MAX,
                                              ST_SIMD_LEVEL_MAX);

  st_udma_free(dma);
}

TEST(Cvt, yuv422p10le_to_rfc4175_422be10_scalar_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_yuv422p10le_to_rfc4175_422be10_dma(dma, 1920, 1080, ST_SIMD_LEVEL_NONE,
                                              ST_SIMD_LEVEL_NONE);

  st_udma_free(dma);
}

TEST(Cvt, yuv422p10le_to_rfc4175_422be10_avx512_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_yuv422p10le_to_rfc4175_422be10_dma(dma, 1920, 1080, ST_SIMD_LEVEL_AVX512,
                                              ST_SIMD_LEVEL_AVX512);
  test_cvt_yuv422p10le_to_rfc4175_422be10_dma(dma, 722, 111, ST_SIMD_LEVEL_AVX512,
                                              ST_SIMD_LEVEL_AVX512);
  test_cvt_yuv422p10le_to_rfc4175_422be10_dma(dma, 722, 111, ST_SIMD_LEVEL_NONE,
                                              ST_SIMD_LEVEL_AVX512);
  test_cvt_yuv422p10le_to_rfc4175_422be10_dma(dma, 722, 111, ST_SIMD_LEVEL_AVX512,
                                              ST_SIMD_LEVEL_NONE);
  int w = 2; /* each pg has two pixels */
  for (int h = 640; h < (640 + 64); h++) {
    test_cvt_yuv422p10le_to_rfc4175_422be10_dma(dma, w, h, ST_SIMD_LEVEL_AVX512,
                                                ST_SIMD_LEVEL_AVX512);
  }

  st_udma_free(dma);
}

TEST(Cvt, yuv422p10le_to_rfc4175_422be10_avx512_vbmi_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_yuv422p10le_to_rfc4175_422be10_dma(dma, 1920, 1080, ST_SIMD_LEVEL_AVX512_VBMI2,
                                              ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_yuv422p10le_to_rfc4175_422be10_dma(dma, 722, 111, ST_SIMD_LEVEL_AVX512_VBMI2,
                                              ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_yuv422p10le_to_rfc4175_422be10_dma(dma, 722, 111, ST_SIMD_LEVEL_NONE,
                                              ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_yuv422p10le_to_rfc4175_422be10_dma(dma, 722, 111, ST_SIMD_LEVEL_AVX512_VBMI2,
                                              ST_SIMD_LEVEL_NONE);
  int w = 2; /* each pg has two pixels */
  for (int h = 640; h < (640 + 64); h++) {
    test_cvt_yuv422p10le_to_rfc4175_422be10_dma(dma, w, h, ST_SIMD_LEVEL_AVX512_VBMI2,
                                                ST_SIMD_LEVEL_AVX512_VBMI2);
  }

  st_udma_free(dma);
}

static void test_cvt_rfc4175_422le10_to_yuv422p10le(int w, int h,
                                                    enum st_simd_level cvt_level,
                                                    enum st_simd_level back_level) {
  int ret;
  size_t fb_pg2_size = w * h * 5 / 2;
  struct st20_rfc4175_422_10_pg2_le* pg =
      (struct st20_rfc4175_422_10_pg2_le*)st_test_zmalloc(fb_pg2_size);
  struct st20_rfc4175_422_10_pg2_le* pg_2 =
      (struct st20_rfc4175_422_10_pg2_le*)st_test_zmalloc(fb_pg2_size);
  size_t planar_size = w * h * 2 * sizeof(uint16_t);
  uint16_t* p10_u16 = (uint16_t*)st_test_zmalloc(planar_size);

  if (!pg || !pg_2 || !p10_u16) {
    EXPECT_EQ(0, 1);
    if (pg) st_test_free(pg);
    if (pg_2) st_test_free(pg_2);
    if (p10_u16) st_test_free(p10_u16);
    return;
  }

  st_test_rand_data((uint8_t*)pg, fb_pg2_size, 0);

  ret = st20_rfc4175_422le10_to_yuv422p10le(pg, p10_u16, (p10_u16 + w * h),
                                            (p10_u16 + w * h * 3 / 2), w, h);
  EXPECT_EQ(0, ret);

  ret = st20_yuv422p10le_to_rfc4175_422le10(p10_u16, (p10_u16 + w * h),
                                            (p10_u16 + w * h * 3 / 2), pg_2, w, h);
  EXPECT_EQ(0, ret);

  EXPECT_EQ(0, memcmp(pg, pg_2, fb_pg2_size));

  st_test_free(pg);
  st_test_free(pg_2);
  st_test_free(p10_u16);
}

TEST(Cvt, rfc4175_422le10_to_yuv422p10le) {
  test_cvt_rfc4175_422le10_to_yuv422p10le(1920, 1080, ST_SIMD_LEVEL_MAX,
                                          ST_SIMD_LEVEL_MAX);
}

TEST(Cvt, rfc4175_422le10_to_yuv422p10le_scalar) {
  test_cvt_rfc4175_422le10_to_yuv422p10le(1920, 1080, ST_SIMD_LEVEL_NONE,
                                          ST_SIMD_LEVEL_NONE);
}

static void test_cvt_yuv422p10le_to_rfc4175_422le10(int w, int h,
                                                    enum st_simd_level cvt_level,
                                                    enum st_simd_level back_level) {
  int ret;
  size_t fb_pg2_size = w * h * 5 / 2;
  struct st20_rfc4175_422_10_pg2_le* pg =
      (struct st20_rfc4175_422_10_pg2_le*)st_test_zmalloc(fb_pg2_size);
  size_t planar_size = w * h * 2 * sizeof(uint16_t);
  uint16_t* p10_u16 = (uint16_t*)st_test_zmalloc(planar_size);
  uint16_t* p10_u16_2 = (uint16_t*)st_test_zmalloc(planar_size);

  if (!pg || !p10_u16_2 || !p10_u16) {
    EXPECT_EQ(0, 1);
    if (pg) st_test_free(pg);
    if (p10_u16_2) st_test_free(p10_u16_2);
    if (p10_u16) st_test_free(p10_u16);
    return;
  }

  for (size_t i = 0; i < (planar_size / 2); i++) {
    p10_u16[i] = rand() & 0x3ff; /* only 10 bit */
  }

  ret = st20_yuv422p10le_to_rfc4175_422le10(p10_u16, (p10_u16 + w * h),
                                            (p10_u16 + w * h * 3 / 2), pg, w, h);
  EXPECT_EQ(0, ret);

  ret = st20_rfc4175_422le10_to_yuv422p10le(pg, p10_u16_2, (p10_u16_2 + w * h),
                                            (p10_u16_2 + w * h * 3 / 2), w, h);
  EXPECT_EQ(0, ret);

  EXPECT_EQ(0, memcmp(p10_u16, p10_u16_2, planar_size));

  st_test_free(pg);
  st_test_free(p10_u16);
  st_test_free(p10_u16_2);
}

TEST(Cvt, yuv422p10le_to_rfc4175_422le10) {
  test_cvt_yuv422p10le_to_rfc4175_422le10(1920, 1080, ST_SIMD_LEVEL_MAX,
                                          ST_SIMD_LEVEL_MAX);
}

TEST(Cvt, yuv422p10le_to_rfc4175_422le10_scalar) {
  test_cvt_yuv422p10le_to_rfc4175_422le10(1920, 1080, ST_SIMD_LEVEL_NONE,
                                          ST_SIMD_LEVEL_NONE);
}

static void test_cvt_rfc4175_422be10_to_422le10(int w, int h,
                                                enum st_simd_level cvt_level,
                                                enum st_simd_level back_level) {
  int ret;
  size_t fb_pg2_size = w * h * 5 / 2;
  struct st20_rfc4175_422_10_pg2_be* pg_be =
      (struct st20_rfc4175_422_10_pg2_be*)st_test_zmalloc(fb_pg2_size);
  struct st20_rfc4175_422_10_pg2_le* pg_le =
      (struct st20_rfc4175_422_10_pg2_le*)st_test_zmalloc(fb_pg2_size);
  struct st20_rfc4175_422_10_pg2_be* pg_be_2 =
      (struct st20_rfc4175_422_10_pg2_be*)st_test_zmalloc(fb_pg2_size);

  if (!pg_be || !pg_le || !pg_be_2) {
    EXPECT_EQ(0, 1);
    if (pg_be) st_test_free(pg_be);
    if (pg_le) st_test_free(pg_le);
    if (pg_be_2) st_test_free(pg_be_2);
    return;
  }

  st_test_rand_data((uint8_t*)pg_be, fb_pg2_size, 0);

  ret = st20_rfc4175_422be10_to_422le10_simd(pg_be, pg_le, w, h, cvt_level);
  EXPECT_EQ(0, ret);

  ret = st20_rfc4175_422le10_to_422be10_simd(pg_le, pg_be_2, w, h, back_level);
  EXPECT_EQ(0, ret);

  EXPECT_EQ(0, memcmp(pg_be, pg_be_2, fb_pg2_size));

  st_test_free(pg_be);
  st_test_free(pg_le);
  st_test_free(pg_be_2);
}

TEST(Cvt, rfc4175_422be10_to_422le10) {
  test_cvt_rfc4175_422be10_to_422le10(1920, 1080, ST_SIMD_LEVEL_MAX, ST_SIMD_LEVEL_MAX);
}

TEST(Cvt, rfc4175_422be10_to_422le10_scalar) {
  test_cvt_rfc4175_422be10_to_422le10(1920, 1080, ST_SIMD_LEVEL_NONE, ST_SIMD_LEVEL_NONE);
}

TEST(Cvt, rfc4175_422be10_to_422le10_avx2) {
  test_cvt_rfc4175_422be10_to_422le10(1920, 1080, ST_SIMD_LEVEL_AVX2, ST_SIMD_LEVEL_AVX2);
  test_cvt_rfc4175_422be10_to_422le10(722, 111, ST_SIMD_LEVEL_AVX2, ST_SIMD_LEVEL_AVX2);
  test_cvt_rfc4175_422be10_to_422le10(722, 111, ST_SIMD_LEVEL_NONE, ST_SIMD_LEVEL_AVX2);
  test_cvt_rfc4175_422be10_to_422le10(722, 111, ST_SIMD_LEVEL_AVX2, ST_SIMD_LEVEL_NONE);
  int w = 2; /* each pg has two pixels */
  for (int h = 640; h < (640 + 64); h++) {
    test_cvt_rfc4175_422be10_to_422le10(w, h, ST_SIMD_LEVEL_AVX2, ST_SIMD_LEVEL_AVX2);
  }
}

TEST(Cvt, rfc4175_422be10_to_422le10_avx512) {
  test_cvt_rfc4175_422be10_to_422le10(1920, 1080, ST_SIMD_LEVEL_AVX512,
                                      ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_422le10(722, 111, ST_SIMD_LEVEL_AVX512,
                                      ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_422le10(722, 111, ST_SIMD_LEVEL_NONE, ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_422le10(722, 111, ST_SIMD_LEVEL_AVX512, ST_SIMD_LEVEL_NONE);
  int w = 2; /* each pg has two pixels */
  for (int h = 640; h < (640 + 64); h++) {
    test_cvt_rfc4175_422be10_to_422le10(w, h, ST_SIMD_LEVEL_AVX512, ST_SIMD_LEVEL_AVX512);
  }
}

TEST(Cvt, rfc4175_422be10_to_422le10_avx512_vbmi) {
  test_cvt_rfc4175_422be10_to_422le10(1920, 1080, ST_SIMD_LEVEL_AVX512_VBMI2,
                                      ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422be10_to_422le10(722, 111, ST_SIMD_LEVEL_AVX512_VBMI2,
                                      ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422be10_to_422le10(722, 111, ST_SIMD_LEVEL_NONE,
                                      ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422be10_to_422le10(722, 111, ST_SIMD_LEVEL_AVX512_VBMI2,
                                      ST_SIMD_LEVEL_NONE);
  int w = 2; /* each pg has two pixels */
  for (int h = 640; h < (640 + 64); h++) {
    test_cvt_rfc4175_422be10_to_422le10(w, h, ST_SIMD_LEVEL_AVX512_VBMI2,
                                        ST_SIMD_LEVEL_AVX512_VBMI2);
  }
}

static void test_cvt_rfc4175_422be10_to_422le10_dma(st_udma_handle dma, int w, int h,
                                                    enum st_simd_level cvt_level,
                                                    enum st_simd_level back_level) {
  int ret;
  size_t fb_pg2_size = w * h * 5 / 2;
  struct st_tests_context* ctx = st_test_ctx();
  st_handle st = ctx->handle;
  struct st20_rfc4175_422_10_pg2_be* pg_be =
      (struct st20_rfc4175_422_10_pg2_be*)st_hp_zmalloc(st, fb_pg2_size, ST_PORT_P);
  struct st20_rfc4175_422_10_pg2_le* pg_le =
      (struct st20_rfc4175_422_10_pg2_le*)st_test_zmalloc(fb_pg2_size);
  struct st20_rfc4175_422_10_pg2_be* pg_be_2 =
      (struct st20_rfc4175_422_10_pg2_be*)st_test_zmalloc(fb_pg2_size);

  if (!pg_be || !pg_le || !pg_be_2) {
    EXPECT_EQ(0, 1);
    if (pg_be) st_hp_free(st, pg_be);
    if (pg_le) st_test_free(pg_le);
    if (pg_be_2) st_test_free(pg_be_2);
    return;
  }

  st_test_rand_data((uint8_t*)pg_be, fb_pg2_size, 0);

  ret = st20_rfc4175_422be10_to_422le10_simd_dma(dma, pg_be, st_hp_virt2iova(st, pg_be),
                                                 pg_le, w, h, cvt_level);
  EXPECT_EQ(0, ret);

  ret = st20_rfc4175_422le10_to_422be10_simd(pg_le, pg_be_2, w, h, back_level);
  EXPECT_EQ(0, ret);

  EXPECT_EQ(0, memcmp(pg_be, pg_be_2, fb_pg2_size));

  st_hp_free(st, pg_be);
  st_test_free(pg_le);
  st_test_free(pg_be_2);
}

TEST(Cvt, rfc4175_422be10_to_422le10_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_rfc4175_422be10_to_422le10_dma(dma, 1920, 1080, ST_SIMD_LEVEL_MAX,
                                          ST_SIMD_LEVEL_MAX);
  test_cvt_rfc4175_422be10_to_422le10_dma(dma, 1920 * 4, 1080 * 4, ST_SIMD_LEVEL_MAX,
                                          ST_SIMD_LEVEL_MAX);
  st_udma_free(dma);
}

TEST(Cvt, rfc4175_422be10_to_422le10_scalar_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_rfc4175_422be10_to_422le10_dma(dma, 1920, 1080, ST_SIMD_LEVEL_NONE,
                                          ST_SIMD_LEVEL_NONE);
  st_udma_free(dma);
}

TEST(Cvt, rfc4175_422be10_to_422le10_avx512_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_rfc4175_422be10_to_422le10_dma(dma, 1920, 1080, ST_SIMD_LEVEL_AVX512,
                                          ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_422le10_dma(dma, 722, 111, ST_SIMD_LEVEL_AVX512,
                                          ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_422le10_dma(dma, 722, 111, ST_SIMD_LEVEL_NONE,
                                          ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_422le10_dma(dma, 722, 111, ST_SIMD_LEVEL_AVX512,
                                          ST_SIMD_LEVEL_NONE);
  int w = 2; /* each pg has two pixels */
  for (int h = 640; h < (640 + 64); h++) {
    test_cvt_rfc4175_422be10_to_422le10_dma(dma, w, h, ST_SIMD_LEVEL_AVX512,
                                            ST_SIMD_LEVEL_AVX512);
  }

  st_udma_free(dma);
}

TEST(Cvt, rfc4175_422be10_to_422le10_avx512_vbmi_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_rfc4175_422be10_to_422le10_dma(dma, 1920, 1080, ST_SIMD_LEVEL_AVX512_VBMI2,
                                          ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422be10_to_422le10_dma(dma, 722, 111, ST_SIMD_LEVEL_AVX512_VBMI2,
                                          ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422be10_to_422le10_dma(dma, 722, 111, ST_SIMD_LEVEL_NONE,
                                          ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422be10_to_422le10_dma(dma, 722, 111, ST_SIMD_LEVEL_AVX512_VBMI2,
                                          ST_SIMD_LEVEL_NONE);
  int w = 2; /* each pg has two pixels */
  for (int h = 640; h < (640 + 64); h++) {
    test_cvt_rfc4175_422be10_to_422le10_dma(dma, w, h, ST_SIMD_LEVEL_AVX512_VBMI2,
                                            ST_SIMD_LEVEL_AVX512_VBMI2);
  }

  st_udma_free(dma);
}

static void test_cvt_rfc4175_422le10_to_422be10(int w, int h,
                                                enum st_simd_level cvt_level,
                                                enum st_simd_level back_level) {
  int ret;
  size_t fb_pg2_size = w * h * 5 / 2;
  struct st20_rfc4175_422_10_pg2_le* pg_le =
      (struct st20_rfc4175_422_10_pg2_le*)st_test_zmalloc(fb_pg2_size);
  struct st20_rfc4175_422_10_pg2_be* pg_be =
      (struct st20_rfc4175_422_10_pg2_be*)st_test_zmalloc(fb_pg2_size);
  struct st20_rfc4175_422_10_pg2_le* pg_le_2 =
      (struct st20_rfc4175_422_10_pg2_le*)st_test_zmalloc(fb_pg2_size);

  if (!pg_be || !pg_le || !pg_le_2) {
    EXPECT_EQ(0, 1);
    if (pg_be) st_test_free(pg_be);
    if (pg_le) st_test_free(pg_le);
    if (pg_le_2) st_test_free(pg_le_2);
    return;
  }

  st_test_rand_data((uint8_t*)pg_le, fb_pg2_size, 0);

  ret = st20_rfc4175_422le10_to_422be10_simd(pg_le, pg_be, w, h, cvt_level);
  EXPECT_EQ(0, ret);

  ret = st20_rfc4175_422be10_to_422le10_simd(pg_be, pg_le_2, w, h, back_level);
  EXPECT_EQ(0, ret);

  // st_test_cmp((uint8_t*)pg_le, (uint8_t*)pg_le_2, fb_pg2_size);
  EXPECT_EQ(0, memcmp(pg_le, pg_le_2, fb_pg2_size));

  st_test_free(pg_be);
  st_test_free(pg_le);
  st_test_free(pg_le_2);
}

static void test_cvt_rfc4175_422le10_to_422be10_2(int w, int h,
                                                  enum st_simd_level cvt_level,
                                                  enum st_simd_level back_level) {
  int ret;
  size_t fb_pg2_size = w * h * 5 / 2;
  struct st20_rfc4175_422_10_pg2_le* pg_le =
      (struct st20_rfc4175_422_10_pg2_le*)st_test_zmalloc(fb_pg2_size);
  struct st20_rfc4175_422_10_pg2_be* pg_be =
      (struct st20_rfc4175_422_10_pg2_be*)st_test_zmalloc(fb_pg2_size);
  struct st20_rfc4175_422_10_pg2_be* pg_be_2 =
      (struct st20_rfc4175_422_10_pg2_be*)st_test_zmalloc(fb_pg2_size);

  if (!pg_be || !pg_le || !pg_be_2) {
    EXPECT_EQ(0, 1);
    if (pg_be) st_test_free(pg_be);
    if (pg_le) st_test_free(pg_le);
    if (pg_be_2) st_test_free(pg_be_2);
    return;
  }

  st_test_rand_data((uint8_t*)pg_le, fb_pg2_size, 0);

  ret = st20_rfc4175_422le10_to_422be10_simd(pg_le, pg_be, w, h, ST_SIMD_LEVEL_NONE);
  EXPECT_EQ(0, ret);

  ret = st20_rfc4175_422le10_to_422be10_simd(pg_le, pg_be_2, w, h, cvt_level);
  EXPECT_EQ(0, ret);

  // st_test_cmp((uint8_t*)pg_be, (uint8_t*)pg_be_2, fb_pg2_size);
  EXPECT_EQ(0, memcmp(pg_be, pg_be_2, fb_pg2_size));

  st_test_free(pg_be);
  st_test_free(pg_le);
  st_test_free(pg_be_2);
}

TEST(Cvt, rfc4175_422le10_to_422be10) {
  test_cvt_rfc4175_422le10_to_422be10_2(1920, 1080, ST_SIMD_LEVEL_MAX, ST_SIMD_LEVEL_MAX);
}

TEST(Cvt, rfc4175_422le10_to_422be10_scalar) {
  test_cvt_rfc4175_422le10_to_422be10(1920, 1080, ST_SIMD_LEVEL_NONE, ST_SIMD_LEVEL_NONE);
}

TEST(Cvt, rfc4175_422le10_to_422be10_avx2) {
  test_cvt_rfc4175_422le10_to_422be10(1920, 1080, ST_SIMD_LEVEL_AVX2, ST_SIMD_LEVEL_AVX2);
  test_cvt_rfc4175_422le10_to_422be10(722, 111, ST_SIMD_LEVEL_AVX2, ST_SIMD_LEVEL_AVX2);
  test_cvt_rfc4175_422le10_to_422be10(722, 111, ST_SIMD_LEVEL_NONE, ST_SIMD_LEVEL_AVX2);
  test_cvt_rfc4175_422le10_to_422be10(722, 111, ST_SIMD_LEVEL_AVX2, ST_SIMD_LEVEL_NONE);
  int w = 2; /* each pg has two pixels */
  for (int h = 640; h < (640 + 64); h++) {
    test_cvt_rfc4175_422le10_to_422be10(w, h, ST_SIMD_LEVEL_AVX2, ST_SIMD_LEVEL_AVX2);
  }
}

TEST(Cvt, rfc4175_422le10_to_422be10_avx512) {
  test_cvt_rfc4175_422le10_to_422be10(1920, 1080, ST_SIMD_LEVEL_AVX512,
                                      ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422le10_to_422be10(722, 111, ST_SIMD_LEVEL_AVX512,
                                      ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422le10_to_422be10(722, 111, ST_SIMD_LEVEL_NONE, ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422le10_to_422be10(722, 111, ST_SIMD_LEVEL_AVX512, ST_SIMD_LEVEL_NONE);
  int w = 2; /* each pg has two pixels */
  for (int h = 640; h < (640 + 64); h++) {
    test_cvt_rfc4175_422le10_to_422be10(w, h, ST_SIMD_LEVEL_AVX512, ST_SIMD_LEVEL_AVX512);
  }
}

TEST(Cvt, rfc4175_422le10_to_422be10_vbmi) {
  test_cvt_rfc4175_422le10_to_422be10(1920, 1080, ST_SIMD_LEVEL_AVX512_VBMI2,
                                      ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422le10_to_422be10(722, 111, ST_SIMD_LEVEL_AVX512_VBMI2,
                                      ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422le10_to_422be10(722, 111, ST_SIMD_LEVEL_NONE,
                                      ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422le10_to_422be10(722, 111, ST_SIMD_LEVEL_AVX512_VBMI2,
                                      ST_SIMD_LEVEL_NONE);
  int w = 2; /* each pg has two pixels */
  for (int h = 640; h < (640 + 64); h++) {
    test_cvt_rfc4175_422le10_to_422be10(w, h, ST_SIMD_LEVEL_AVX512_VBMI2,
                                        ST_SIMD_LEVEL_AVX512_VBMI2);
  }
}

static void test_cvt_rfc4175_422le10_to_422be10_dma(st_udma_handle dma, int w, int h,
                                                    enum st_simd_level cvt_level,
                                                    enum st_simd_level back_level) {
  int ret;
  size_t fb_pg2_size = w * h * 5 / 2;
  struct st_tests_context* ctx = st_test_ctx();
  st_handle st = ctx->handle;
  struct st20_rfc4175_422_10_pg2_le* pg_le =
      (struct st20_rfc4175_422_10_pg2_le*)st_hp_zmalloc(st, fb_pg2_size, ST_PORT_P);
  struct st20_rfc4175_422_10_pg2_be* pg_be =
      (struct st20_rfc4175_422_10_pg2_be*)st_test_zmalloc(fb_pg2_size);
  struct st20_rfc4175_422_10_pg2_le* pg_le_2 =
      (struct st20_rfc4175_422_10_pg2_le*)st_test_zmalloc(fb_pg2_size);

  if (!pg_be || !pg_le || !pg_le_2) {
    EXPECT_EQ(0, 1);
    if (pg_be) st_test_free(pg_be);
    if (pg_le) st_hp_free(st, pg_le);
    if (pg_le_2) st_test_free(pg_le_2);
    return;
  }

  st_test_rand_data((uint8_t*)pg_le, fb_pg2_size, 0);

  ret = st20_rfc4175_422le10_to_422be10_simd_dma(dma, pg_le, st_hp_virt2iova(st, pg_le),
                                                 pg_be, w, h, cvt_level);
  EXPECT_EQ(0, ret);

  ret = st20_rfc4175_422be10_to_422le10_simd(pg_be, pg_le_2, w, h, back_level);
  EXPECT_EQ(0, ret);

  EXPECT_EQ(0, memcmp(pg_le, pg_le_2, fb_pg2_size));

  st_test_free(pg_be);
  st_hp_free(st, pg_le);
  st_test_free(pg_le_2);
}

TEST(Cvt, rfc4175_422le10_to_422be10_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_rfc4175_422le10_to_422be10_dma(dma, 1920, 1080, ST_SIMD_LEVEL_MAX,
                                          ST_SIMD_LEVEL_MAX);
  test_cvt_rfc4175_422le10_to_422be10_dma(dma, 1920 * 4, 1080 * 4, ST_SIMD_LEVEL_MAX,
                                          ST_SIMD_LEVEL_MAX);
  st_udma_free(dma);
}

TEST(Cvt, rfc4175_422le10_to_422be10_scalar_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_rfc4175_422le10_to_422be10_dma(dma, 1920, 1080, ST_SIMD_LEVEL_NONE,
                                          ST_SIMD_LEVEL_NONE);
  st_udma_free(dma);
}

TEST(Cvt, rfc4175_422le10_to_422be10_avx512_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_rfc4175_422le10_to_422be10_dma(dma, 1920, 1080, ST_SIMD_LEVEL_AVX512,
                                          ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422le10_to_422be10_dma(dma, 722, 111, ST_SIMD_LEVEL_AVX512,
                                          ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422le10_to_422be10_dma(dma, 722, 111, ST_SIMD_LEVEL_NONE,
                                          ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422le10_to_422be10_dma(dma, 722, 111, ST_SIMD_LEVEL_AVX512,
                                          ST_SIMD_LEVEL_NONE);
  int w = 2; /* each pg has two pixels */
  for (int h = 640; h < (640 + 64); h++) {
    test_cvt_rfc4175_422le10_to_422be10_dma(dma, w, h, ST_SIMD_LEVEL_AVX512,
                                            ST_SIMD_LEVEL_AVX512);
  }

  st_udma_free(dma);
}

TEST(Cvt, rfc4175_422le10_to_422be10_avx512_vbmi_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_rfc4175_422le10_to_422be10_dma(dma, 1920, 1080, ST_SIMD_LEVEL_AVX512_VBMI2,
                                          ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422le10_to_422be10_dma(dma, 722, 111, ST_SIMD_LEVEL_AVX512_VBMI2,
                                          ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422le10_to_422be10_dma(dma, 722, 111, ST_SIMD_LEVEL_NONE,
                                          ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422le10_to_422be10_dma(dma, 722, 111, ST_SIMD_LEVEL_AVX512_VBMI2,
                                          ST_SIMD_LEVEL_NONE);
  int w = 2; /* each pg has two pixels */
  for (int h = 640; h < (640 + 64); h++) {
    test_cvt_rfc4175_422le10_to_422be10_dma(dma, w, h, ST_SIMD_LEVEL_AVX512_VBMI2,
                                            ST_SIMD_LEVEL_AVX512_VBMI2);
  }

  st_udma_free(dma);
}

static int test_cvt_extend_rfc4175_422le8_to_422be10(
    int w, int h, struct st20_rfc4175_422_8_pg2_le* pg_8,
    struct st20_rfc4175_422_10_pg2_be* pg_10) {
  uint32_t cnt = w * h / 2;

  for (uint32_t i = 0; i < cnt; i++) {
    pg_10[i].Cb00 = pg_8[i].Cb00;
    pg_10[i].Y00 = pg_8[i].Y00 >> 2;
    pg_10[i].Cb00_ = 0;
    pg_10[i].Y00_ = (pg_8[i].Y00 & 0x3) << 2;
    pg_10[i].Cr00 = pg_8[i].Cr00 >> 4;
    pg_10[i].Y01 = pg_8[i].Y01 >> 6;
    pg_10[i].Cr00_ = (pg_8[i].Cr00 & 0xF) << 2;
    pg_10[i].Y01_ = pg_8[i].Y01 << 2;
  }

  return 0;
}

static void test_cvt_rfc4175_422be10_to_422le8(int w, int h, enum st_simd_level cvt_level,
                                               enum st_simd_level back_level) {
  int ret;
  size_t fb_pg2_size_10 = w * h * 5 / 2;
  size_t fb_pg2_size_8 = w * h * 2;
  struct st20_rfc4175_422_10_pg2_be* pg_10 =
      (struct st20_rfc4175_422_10_pg2_be*)st_test_zmalloc(fb_pg2_size_10);
  struct st20_rfc4175_422_8_pg2_le* pg_8 =
      (struct st20_rfc4175_422_8_pg2_le*)st_test_zmalloc(fb_pg2_size_8);
  struct st20_rfc4175_422_8_pg2_le* pg_8_2 =
      (struct st20_rfc4175_422_8_pg2_le*)st_test_zmalloc(fb_pg2_size_8);

  if (!pg_10 || !pg_8 || !pg_8_2) {
    EXPECT_EQ(0, 1);
    if (pg_10) st_test_free(pg_10);
    if (pg_8) st_test_free(pg_8);
    if (pg_8_2) st_test_free(pg_8_2);
    return;
  }

  st_test_rand_data((uint8_t*)pg_8, fb_pg2_size_8, 0);
  test_cvt_extend_rfc4175_422le8_to_422be10(w, h, pg_8, pg_10);
  ret = st20_rfc4175_422be10_to_422le8_simd(pg_10, pg_8_2, w, h, cvt_level);
  EXPECT_EQ(0, ret);

  EXPECT_EQ(0, memcmp(pg_8, pg_8_2, fb_pg2_size_8));

  st_test_free(pg_10);
  st_test_free(pg_8);
  st_test_free(pg_8_2);
}

TEST(Cvt, rfc4175_422be10_to_422le8) {
  test_cvt_rfc4175_422be10_to_422le8(1920, 1080, ST_SIMD_LEVEL_MAX, ST_SIMD_LEVEL_MAX);
}

TEST(Cvt, rfc4175_422be10_to_422le8_scalar) {
  test_cvt_rfc4175_422be10_to_422le8(1920, 1080, ST_SIMD_LEVEL_NONE, ST_SIMD_LEVEL_NONE);
}

TEST(Cvt, rfc4175_422be10_to_422le8_avx512) {
  test_cvt_rfc4175_422be10_to_422le8(1920, 1080, ST_SIMD_LEVEL_AVX512,
                                     ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_422le8(722, 111, ST_SIMD_LEVEL_AVX512,
                                     ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_422le8(722, 111, ST_SIMD_LEVEL_NONE, ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_422le8(722, 111, ST_SIMD_LEVEL_AVX512, ST_SIMD_LEVEL_NONE);
  int w = 2; /* each pg has two pixels */
  for (int h = 640; h < (640 + 64); h++) {
    test_cvt_rfc4175_422be10_to_422le8(w, h, ST_SIMD_LEVEL_AVX512, ST_SIMD_LEVEL_AVX512);
  }
}

TEST(Cvt, rfc4175_422be10_to_422le8_avx512_vbmi) {
  test_cvt_rfc4175_422be10_to_422le8(1920, 1080, ST_SIMD_LEVEL_AVX512_VBMI2,
                                     ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422be10_to_422le8(722, 111, ST_SIMD_LEVEL_AVX512_VBMI2,
                                     ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422be10_to_422le8(722, 111, ST_SIMD_LEVEL_NONE,
                                     ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422be10_to_422le8(722, 111, ST_SIMD_LEVEL_AVX512_VBMI2,
                                     ST_SIMD_LEVEL_NONE);
  int w = 2; /* each pg has two pixels */
  for (int h = 640; h < (640 + 64); h++) {
    test_cvt_rfc4175_422be10_to_422le8(w, h, ST_SIMD_LEVEL_AVX512_VBMI2,
                                       ST_SIMD_LEVEL_AVX512_VBMI2);
  }
}

static void test_cvt_rfc4175_422be10_to_422le8_dma(st_udma_handle dma, int w, int h,
                                                   enum st_simd_level cvt_level,
                                                   enum st_simd_level back_level) {
  int ret;
  size_t fb_pg2_size_10 = w * h * 5 / 2;
  size_t fb_pg2_size_8 = w * h * 2;
  struct st_tests_context* ctx = st_test_ctx();
  st_handle st = ctx->handle;
  struct st20_rfc4175_422_10_pg2_be* pg_10 =
      (struct st20_rfc4175_422_10_pg2_be*)st_hp_zmalloc(st, fb_pg2_size_10, ST_PORT_P);
  struct st20_rfc4175_422_8_pg2_le* pg_8 =
      (struct st20_rfc4175_422_8_pg2_le*)st_test_zmalloc(fb_pg2_size_8);
  struct st20_rfc4175_422_8_pg2_le* pg_8_2 =
      (struct st20_rfc4175_422_8_pg2_le*)st_test_zmalloc(fb_pg2_size_8);

  if (!pg_10 || !pg_8 || !pg_8_2) {
    EXPECT_EQ(0, 1);
    if (pg_10) st_hp_free(st, pg_10);
    if (pg_8) st_test_free(pg_8);
    if (pg_8_2) st_test_free(pg_8_2);
    return;
  }

  st_test_rand_data((uint8_t*)pg_8, fb_pg2_size_8, 0);
  test_cvt_extend_rfc4175_422le8_to_422be10(w, h, pg_8, pg_10);
  ret = st20_rfc4175_422be10_to_422le8_simd_dma(dma, pg_10, st_hp_virt2iova(st, pg_10),
                                                pg_8_2, w, h, cvt_level);
  EXPECT_EQ(0, ret);

  EXPECT_EQ(0, memcmp(pg_8, pg_8_2, fb_pg2_size_8));

  st_hp_free(st, pg_10);
  st_test_free(pg_8);
  st_test_free(pg_8_2);
}

TEST(Cvt, rfc4175_422be10_to_422le8_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_rfc4175_422be10_to_422le8_dma(dma, 1920, 1080, ST_SIMD_LEVEL_MAX,
                                         ST_SIMD_LEVEL_MAX);
  test_cvt_rfc4175_422be10_to_422le8_dma(dma, 1920 * 4, 1080 * 4, ST_SIMD_LEVEL_MAX,
                                         ST_SIMD_LEVEL_MAX);
  st_udma_free(dma);
}

TEST(Cvt, rfc4175_422be10_to_422le8_scalar_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_rfc4175_422be10_to_422le8_dma(dma, 1920, 1080, ST_SIMD_LEVEL_NONE,
                                         ST_SIMD_LEVEL_NONE);
  st_udma_free(dma);
}

TEST(Cvt, rfc4175_422be10_to_422le8_avx512_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_rfc4175_422be10_to_422le8_dma(dma, 1920, 1080, ST_SIMD_LEVEL_AVX512,
                                         ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_422le8_dma(dma, 722, 111, ST_SIMD_LEVEL_AVX512,
                                         ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_422le8_dma(dma, 722, 111, ST_SIMD_LEVEL_NONE,
                                         ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_422le8_dma(dma, 722, 111, ST_SIMD_LEVEL_AVX512,
                                         ST_SIMD_LEVEL_NONE);
  int w = 2; /* each pg has two pixels */
  for (int h = 640; h < (640 + 64); h++) {
    test_cvt_rfc4175_422be10_to_422le8_dma(dma, w, h, ST_SIMD_LEVEL_AVX512,
                                           ST_SIMD_LEVEL_AVX512);
  }

  st_udma_free(dma);
}

TEST(Cvt, rfc4175_422be10_to_422le8_avx512_vbmi_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_rfc4175_422be10_to_422le8_dma(dma, 1920, 1080, ST_SIMD_LEVEL_AVX512_VBMI2,
                                         ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422be10_to_422le8_dma(dma, 722, 111, ST_SIMD_LEVEL_AVX512_VBMI2,
                                         ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422be10_to_422le8_dma(dma, 722, 111, ST_SIMD_LEVEL_NONE,
                                         ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422be10_to_422le8_dma(dma, 722, 111, ST_SIMD_LEVEL_AVX512_VBMI2,
                                         ST_SIMD_LEVEL_NONE);
  int w = 2; /* each pg has two pixels */
  for (int h = 640; h < (640 + 64); h++) {
    test_cvt_rfc4175_422be10_to_422le8_dma(dma, w, h, ST_SIMD_LEVEL_AVX512_VBMI2,
                                           ST_SIMD_LEVEL_AVX512_VBMI2);
  }

  st_udma_free(dma);
}

static void test_cvt_rfc4175_422le10_to_v210(int w, int h, enum st_simd_level cvt_level,
                                             enum st_simd_level back_level) {
  int ret;
  bool fail_case = (w * h % 6); /* do not convert when pg_num is not multiple of 3 */
  size_t fb_pg2_size = w * h * 5 / 2;
  size_t fb_pg2_size_v210 = w * h * 8 / 3;
  struct st20_rfc4175_422_10_pg2_le* pg_le =
      (struct st20_rfc4175_422_10_pg2_le*)st_test_zmalloc(fb_pg2_size);
  struct st20_rfc4175_422_10_pg2_le* pg_le_2 =
      (struct st20_rfc4175_422_10_pg2_le*)st_test_zmalloc(fb_pg2_size);
  uint8_t* pg_v210 = (uint8_t*)st_test_zmalloc(fb_pg2_size_v210);

  if (!pg_le || !pg_le_2 || !pg_v210) {
    EXPECT_EQ(0, 1);
    if (pg_le) st_test_free(pg_le);
    if (pg_le_2) st_test_free(pg_le_2);
    if (pg_v210) st_test_free(pg_v210);
    return;
  }

  st_test_rand_data((uint8_t*)pg_le, fb_pg2_size, 0);
  ret = st20_rfc4175_422le10_to_v210_simd((uint8_t*)pg_le, pg_v210, w, h, cvt_level);
  if (fail_case)
    EXPECT_NE(0, ret);
  else
    EXPECT_EQ(0, ret);

  ret = st20_v210_to_rfc4175_422le10(pg_v210, (uint8_t*)pg_le_2, w, h);
  if (fail_case)
    EXPECT_NE(0, ret);
  else
    EXPECT_EQ(0, ret);

  if (fail_case)
    EXPECT_NE(0, memcmp(pg_le, pg_le_2, fb_pg2_size));
  else
    EXPECT_EQ(0, memcmp(pg_le, pg_le_2, fb_pg2_size));

  st_test_free(pg_v210);
  st_test_free(pg_le);
  st_test_free(pg_le_2);
}

TEST(Cvt, rfc4175_422le10_to_v210) {
  test_cvt_rfc4175_422le10_to_v210(1920, 1080, ST_SIMD_LEVEL_MAX, ST_SIMD_LEVEL_MAX);
}

TEST(Cvt, rfc4175_422le10_to_v210_scalar) {
  test_cvt_rfc4175_422le10_to_v210(1920, 1080, ST_SIMD_LEVEL_NONE, ST_SIMD_LEVEL_NONE);
}

TEST(Cvt, rfc4175_422le10_to_v210_avx512) {
  test_cvt_rfc4175_422le10_to_v210(1920, 1080, ST_SIMD_LEVEL_AVX512,
                                   ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422le10_to_v210(1920, 1080, ST_SIMD_LEVEL_AVX512,
                                   ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422le10_to_v210(1920, 1080, ST_SIMD_LEVEL_NONE, ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422le10_to_v210(1920, 1080, ST_SIMD_LEVEL_AVX512, ST_SIMD_LEVEL_NONE);
  test_cvt_rfc4175_422le10_to_v210(722, 111, ST_SIMD_LEVEL_AVX512, ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422le10_to_v210(1921, 1079, ST_SIMD_LEVEL_AVX512,
                                   ST_SIMD_LEVEL_AVX512);
}

TEST(Cvt, rfc4175_422le10_to_v210_avx512_vbmi) {
  test_cvt_rfc4175_422le10_to_v210(1920, 1080, ST_SIMD_LEVEL_AVX512_VBMI2,
                                   ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422le10_to_v210(1920, 1080, ST_SIMD_LEVEL_AVX512_VBMI2,
                                   ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422le10_to_v210(1920, 1080, ST_SIMD_LEVEL_NONE,
                                   ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422le10_to_v210(1920, 1080, ST_SIMD_LEVEL_AVX512_VBMI2,
                                   ST_SIMD_LEVEL_NONE);
  test_cvt_rfc4175_422le10_to_v210(722, 111, ST_SIMD_LEVEL_AVX512_VBMI2,
                                   ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422le10_to_v210(1921, 1079, ST_SIMD_LEVEL_AVX512_VBMI2,
                                   ST_SIMD_LEVEL_AVX512_VBMI2);
}

static void test_cvt_rfc4175_422be10_to_v210(int w, int h, enum st_simd_level cvt_level,
                                             enum st_simd_level back_level) {
  int ret;
  bool fail_case = (w * h % 6); /* do not convert when pg_num is not multiple of 3 */
  size_t fb_pg2_size = w * h * 5 / 2;
  size_t fb_pg2_size_v210 = w * h * 8 / 3;
  struct st20_rfc4175_422_10_pg2_be* pg_be =
      (struct st20_rfc4175_422_10_pg2_be*)st_test_zmalloc(fb_pg2_size);
  struct st20_rfc4175_422_10_pg2_be* pg_be_2 =
      (struct st20_rfc4175_422_10_pg2_be*)st_test_zmalloc(fb_pg2_size);
  struct st20_rfc4175_422_10_pg2_le* pg_le =
      (struct st20_rfc4175_422_10_pg2_le*)st_test_zmalloc(fb_pg2_size);
  uint8_t* pg_v210 = (uint8_t*)st_test_zmalloc(fb_pg2_size_v210);

  if (!pg_le || !pg_be || !pg_v210 || !pg_be_2) {
    EXPECT_EQ(0, 1);
    if (pg_le) st_test_free(pg_le);
    if (pg_be) st_test_free(pg_be);
    if (pg_v210) st_test_free(pg_v210);
    if (pg_be_2) st_test_free(pg_be_2);
    return;
  }

  st_test_rand_data((uint8_t*)pg_be, fb_pg2_size, 0);
  ret = st20_rfc4175_422be10_to_v210_simd(pg_be, pg_v210, w, h, cvt_level);
  if (fail_case)
    EXPECT_NE(0, ret);
  else
    EXPECT_EQ(0, ret);

  ret = st20_v210_to_rfc4175_422le10(pg_v210, (uint8_t*)pg_le, w, h);
  if (fail_case)
    EXPECT_NE(0, ret);
  else
    EXPECT_EQ(0, ret);

  st20_rfc4175_422le10_to_422be10(pg_le, pg_be_2, w, h);

  if (fail_case)
    EXPECT_NE(0, memcmp(pg_be, pg_be_2, fb_pg2_size));
  else
    EXPECT_EQ(0, memcmp(pg_be, pg_be_2, fb_pg2_size));

  st_test_free(pg_v210);
  st_test_free(pg_be);
  st_test_free(pg_be_2);
  st_test_free(pg_le);
}

TEST(Cvt, rfc4175_422be10_to_v210) {
  test_cvt_rfc4175_422be10_to_v210(1920, 1080, ST_SIMD_LEVEL_MAX, ST_SIMD_LEVEL_MAX);
}

TEST(Cvt, rfc4175_422be10_to_v210_scalar) {
  test_cvt_rfc4175_422be10_to_v210(1920, 1080, ST_SIMD_LEVEL_NONE, ST_SIMD_LEVEL_NONE);
}

TEST(Cvt, rfc4175_422be10_to_v210_avx512) {
  test_cvt_rfc4175_422be10_to_v210(1920, 1080, ST_SIMD_LEVEL_AVX512,
                                   ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_v210(1920, 1080, ST_SIMD_LEVEL_AVX512,
                                   ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_v210(1920, 1080, ST_SIMD_LEVEL_NONE, ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_v210(1920, 1080, ST_SIMD_LEVEL_AVX512, ST_SIMD_LEVEL_NONE);
  test_cvt_rfc4175_422be10_to_v210(722, 111, ST_SIMD_LEVEL_AVX512, ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_v210(1921, 1079, ST_SIMD_LEVEL_AVX512,
                                   ST_SIMD_LEVEL_AVX512);
}

TEST(Cvt, rfc4175_422be10_to_v210_avx512_vbmi) {
  test_cvt_rfc4175_422be10_to_v210(1920, 1080, ST_SIMD_LEVEL_AVX512_VBMI2,
                                   ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422be10_to_v210(1920, 1080, ST_SIMD_LEVEL_AVX512_VBMI2,
                                   ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422be10_to_v210(1920, 1080, ST_SIMD_LEVEL_NONE,
                                   ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422be10_to_v210(1920, 1080, ST_SIMD_LEVEL_AVX512_VBMI2,
                                   ST_SIMD_LEVEL_NONE);
  test_cvt_rfc4175_422be10_to_v210(722, 111, ST_SIMD_LEVEL_AVX512_VBMI2,
                                   ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422be10_to_v210(1921, 1079, ST_SIMD_LEVEL_AVX512_VBMI2,
                                   ST_SIMD_LEVEL_AVX512_VBMI2);
}

static void test_cvt_rfc4175_422be10_to_v210_dma(st_udma_handle dma, int w, int h,
                                                 enum st_simd_level cvt_level,
                                                 enum st_simd_level back_level) {
  int ret;
  bool fail_case = (w * h % 6); /* do not convert when pg_num is not multiple of 3 */
  size_t fb_pg2_size = w * h * 5 / 2;
  size_t fb_pg2_size_v210 = w * h * 8 / 3;
  struct st_tests_context* ctx = st_test_ctx();
  st_handle st = ctx->handle;
  struct st20_rfc4175_422_10_pg2_be* pg_be =
      (struct st20_rfc4175_422_10_pg2_be*)st_hp_zmalloc(st, fb_pg2_size, ST_PORT_P);
  struct st20_rfc4175_422_10_pg2_be* pg_be_2 =
      (struct st20_rfc4175_422_10_pg2_be*)st_test_zmalloc(fb_pg2_size);
  struct st20_rfc4175_422_10_pg2_le* pg_le =
      (struct st20_rfc4175_422_10_pg2_le*)st_test_zmalloc(fb_pg2_size);
  uint8_t* pg_v210 = (uint8_t*)st_test_zmalloc(fb_pg2_size_v210);

  if (!pg_le || !pg_be || !pg_v210 || !pg_be_2) {
    EXPECT_EQ(0, 1);
    if (pg_be) st_hp_free(st, pg_be);
    if (pg_be_2) st_test_free(pg_be_2);
    if (pg_v210) st_test_free(pg_v210);
    if (pg_le) st_test_free(pg_le);
    return;
  }

  st_test_rand_data((uint8_t*)pg_be, fb_pg2_size, 0);
  ret = st20_rfc4175_422be10_to_v210_simd_dma(dma, pg_be, st_hp_virt2iova(st, pg_be),
                                              pg_v210, w, h, cvt_level);
  if (fail_case)
    EXPECT_NE(0, ret);
  else
    EXPECT_EQ(0, ret);

  ret = st20_v210_to_rfc4175_422le10(pg_v210, (uint8_t*)pg_le, w, h);
  if (fail_case)
    EXPECT_NE(0, ret);
  else
    EXPECT_EQ(0, ret);

  st20_rfc4175_422le10_to_422be10(pg_le, pg_be_2, w, h);

  if (fail_case)
    EXPECT_NE(0, memcmp(pg_be, pg_be_2, fb_pg2_size));
  else
    EXPECT_EQ(0, memcmp(pg_be, pg_be_2, fb_pg2_size));

  st_hp_free(st, pg_be);
  st_test_free(pg_be_2);
  st_test_free(pg_le);
  st_test_free(pg_v210);
}

TEST(Cvt, rfc4175_422be10_to_v210_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_rfc4175_422be10_to_v210_dma(dma, 1920, 1080, ST_SIMD_LEVEL_MAX,
                                       ST_SIMD_LEVEL_MAX);

  st_udma_free(dma);
}

TEST(Cvt, rfc4175_422be10_to_v210_scalar_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_rfc4175_422be10_to_v210_dma(dma, 1920, 1080, ST_SIMD_LEVEL_NONE,
                                       ST_SIMD_LEVEL_NONE);

  st_udma_free(dma);
}

TEST(Cvt, rfc4175_422be10_to_v210_avx512_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_rfc4175_422be10_to_v210_dma(dma, 1920, 1080, ST_SIMD_LEVEL_AVX512,
                                       ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_v210_dma(dma, 1920, 1080, ST_SIMD_LEVEL_AVX512,
                                       ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_v210_dma(dma, 1920, 1080, ST_SIMD_LEVEL_NONE,
                                       ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_v210_dma(dma, 1920, 1080, ST_SIMD_LEVEL_AVX512,
                                       ST_SIMD_LEVEL_NONE);
  test_cvt_rfc4175_422be10_to_v210_dma(dma, 722, 111, ST_SIMD_LEVEL_AVX512,
                                       ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_v210_dma(dma, 1921, 1079, ST_SIMD_LEVEL_AVX512,
                                       ST_SIMD_LEVEL_AVX512);

  st_udma_free(dma);
}

TEST(Cvt, rfc4175_422be10_to_v210_avx512_vbmi_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_rfc4175_422be10_to_v210_dma(dma, 1920, 1080, ST_SIMD_LEVEL_AVX512_VBMI2,
                                       ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422be10_to_v210_dma(dma, 1920, 1080, ST_SIMD_LEVEL_AVX512_VBMI2,
                                       ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422be10_to_v210_dma(dma, 1920, 1080, ST_SIMD_LEVEL_NONE,
                                       ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422be10_to_v210_dma(dma, 1920, 1080, ST_SIMD_LEVEL_AVX512_VBMI2,
                                       ST_SIMD_LEVEL_NONE);
  test_cvt_rfc4175_422be10_to_v210_dma(dma, 722, 111, ST_SIMD_LEVEL_AVX512_VBMI2,
                                       ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_rfc4175_422be10_to_v210_dma(dma, 1921, 1079, ST_SIMD_LEVEL_AVX512_VBMI2,
                                       ST_SIMD_LEVEL_AVX512_VBMI2);

  st_udma_free(dma);
}

static void test_cvt_v210_to_rfc4175_422be10(int w, int h, enum st_simd_level cvt_level,
                                             enum st_simd_level back_level) {
  int ret;
  bool fail_case = (w * h % 6); /* do not convert when pg_num is not multiple of 3 */
  size_t fb_pg2_size = w * h * 5 / 2;
  size_t fb_pg2_size_v210 = w * h * 8 / 3;
  struct st20_rfc4175_422_10_pg2_be* pg_be =
      (struct st20_rfc4175_422_10_pg2_be*)st_test_zmalloc(fb_pg2_size);
  struct st20_rfc4175_422_10_pg2_be* pg_be_2 =
      (struct st20_rfc4175_422_10_pg2_be*)st_test_zmalloc(fb_pg2_size);
  uint8_t* pg_v210 = (uint8_t*)st_test_zmalloc(fb_pg2_size_v210);

  if (!pg_be || !pg_v210 || !pg_be_2) {
    EXPECT_EQ(0, 1);
    if (pg_be) st_test_free(pg_be);
    if (pg_v210) st_test_free(pg_v210);
    if (pg_be_2) st_test_free(pg_be_2);
    return;
  }

  st_test_rand_data((uint8_t*)pg_be, fb_pg2_size, 0);
  ret = st20_rfc4175_422be10_to_v210_simd(pg_be, pg_v210, w, h, cvt_level);
  if (fail_case)
    EXPECT_NE(0, ret);
  else
    EXPECT_EQ(0, ret);

  ret = st20_v210_to_rfc4175_422be10_simd(pg_v210, pg_be_2, w, h, back_level);
  if (fail_case)
    EXPECT_NE(0, ret);
  else
    EXPECT_EQ(0, ret);

  // st_test_cmp((uint8_t*)pg_be, (uint8_t*)pg_be_2, fb_pg2_size);
  if (fail_case)
    EXPECT_NE(0, memcmp(pg_be, pg_be_2, fb_pg2_size));
  else
    EXPECT_EQ(0, memcmp(pg_be, pg_be_2, fb_pg2_size));

  st_test_free(pg_v210);
  st_test_free(pg_be);
  st_test_free(pg_be_2);
}

TEST(Cvt, v210_to_rfc4175_422be10) {
  test_cvt_v210_to_rfc4175_422be10(1920, 1080, ST_SIMD_LEVEL_MAX, ST_SIMD_LEVEL_MAX);
}

TEST(Cvt, v210_to_rfc4175_422be10_scalar) {
  test_cvt_v210_to_rfc4175_422be10(1920, 1080, ST_SIMD_LEVEL_NONE, ST_SIMD_LEVEL_NONE);
}

TEST(Cvt, v210_to_rfc4175_422be10_avx512) {
  test_cvt_v210_to_rfc4175_422be10(1920, 1080, ST_SIMD_LEVEL_AVX512,
                                   ST_SIMD_LEVEL_AVX512);
  test_cvt_v210_to_rfc4175_422be10(1920, 1080, ST_SIMD_LEVEL_AVX512,
                                   ST_SIMD_LEVEL_AVX512);
  test_cvt_v210_to_rfc4175_422be10(1920, 1080, ST_SIMD_LEVEL_NONE, ST_SIMD_LEVEL_AVX512);
  test_cvt_v210_to_rfc4175_422be10(1920, 1080, ST_SIMD_LEVEL_AVX512, ST_SIMD_LEVEL_NONE);
  test_cvt_v210_to_rfc4175_422be10(722, 111, ST_SIMD_LEVEL_AVX512, ST_SIMD_LEVEL_AVX512);
  test_cvt_v210_to_rfc4175_422be10(1921, 1079, ST_SIMD_LEVEL_AVX512,
                                   ST_SIMD_LEVEL_AVX512);
}

TEST(Cvt, v210_to_rfc4175_422be10_vbmi) {
  test_cvt_v210_to_rfc4175_422be10(1920, 1080, ST_SIMD_LEVEL_AVX512_VBMI2,
                                   ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_v210_to_rfc4175_422be10(1920, 1080, ST_SIMD_LEVEL_AVX512_VBMI2,
                                   ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_v210_to_rfc4175_422be10(1920, 1080, ST_SIMD_LEVEL_NONE,
                                   ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_v210_to_rfc4175_422be10(1920, 1080, ST_SIMD_LEVEL_AVX512_VBMI2,
                                   ST_SIMD_LEVEL_NONE);
  test_cvt_v210_to_rfc4175_422be10(722, 111, ST_SIMD_LEVEL_AVX512_VBMI2,
                                   ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_v210_to_rfc4175_422be10(1921, 1079, ST_SIMD_LEVEL_AVX512_VBMI2,
                                   ST_SIMD_LEVEL_AVX512_VBMI2);
}

static void test_cvt_v210_to_rfc4175_422be10_2(int w, int h, enum st_simd_level cvt_level,
                                               enum st_simd_level back_level) {
  int ret;
  bool fail_case = (w * h % 6); /* do not convert when pg_num is not multiple of 3 */
  size_t fb_pg2_size = w * h * 5 / 2;
  size_t fb_pg2_size_v210 = w * h * 8 / 3;
  struct st20_rfc4175_422_10_pg2_be* pg_be =
      (struct st20_rfc4175_422_10_pg2_be*)st_test_zmalloc(fb_pg2_size);
  uint8_t* pg_v210 = (uint8_t*)st_test_zmalloc(fb_pg2_size_v210);
  uint8_t* pg_v210_2 = (uint8_t*)st_test_zmalloc(fb_pg2_size_v210);

  if (!pg_be || !pg_v210 || !pg_v210_2) {
    EXPECT_EQ(0, 1);
    if (pg_be) st_test_free(pg_be);
    if (pg_v210) st_test_free(pg_v210);
    if (pg_v210_2) st_test_free(pg_v210_2);
    return;
  }

  st_test_rand_v210(pg_v210, fb_pg2_size_v210, 0);
  ret = st20_v210_to_rfc4175_422be10_simd(pg_v210, pg_be, w, h, cvt_level);
  if (fail_case)
    EXPECT_NE(0, ret);
  else
    EXPECT_EQ(0, ret);

  ret = st20_rfc4175_422be10_to_v210_simd(pg_be, pg_v210_2, w, h, back_level);
  if (fail_case)
    EXPECT_NE(0, ret);
  else
    EXPECT_EQ(0, ret);

  // st_test_cmp(pg_v210, pg_v210_2, fb_pg2_size_v210);
  if (fail_case)
    EXPECT_NE(0, memcmp(pg_v210, pg_v210_2, fb_pg2_size_v210));
  else
    EXPECT_EQ(0, memcmp(pg_v210, pg_v210_2, fb_pg2_size_v210));

  st_test_free(pg_v210);
  st_test_free(pg_be);
  st_test_free(pg_v210_2);
}

TEST(Cvt, v210_to_rfc4175_422be10_2) {
  test_cvt_v210_to_rfc4175_422be10_2(1920, 1080, ST_SIMD_LEVEL_MAX, ST_SIMD_LEVEL_MAX);
}

TEST(Cvt, v210_to_rfc4175_422be10_2_scalar) {
  test_cvt_v210_to_rfc4175_422be10_2(1920, 1080, ST_SIMD_LEVEL_NONE, ST_SIMD_LEVEL_NONE);
}

TEST(Cvt, v210_to_rfc4175_422be10_2_avx512) {
  test_cvt_v210_to_rfc4175_422be10_2(1920, 1080, ST_SIMD_LEVEL_AVX512,
                                     ST_SIMD_LEVEL_AVX512);
  test_cvt_v210_to_rfc4175_422be10_2(1920, 1080, ST_SIMD_LEVEL_AVX512,
                                     ST_SIMD_LEVEL_AVX512);
  test_cvt_v210_to_rfc4175_422be10_2(1920, 1080, ST_SIMD_LEVEL_NONE,
                                     ST_SIMD_LEVEL_AVX512);
  test_cvt_v210_to_rfc4175_422be10_2(1920, 1080, ST_SIMD_LEVEL_AVX512,
                                     ST_SIMD_LEVEL_NONE);
  test_cvt_v210_to_rfc4175_422be10_2(722, 111, ST_SIMD_LEVEL_AVX512,
                                     ST_SIMD_LEVEL_AVX512);
  test_cvt_v210_to_rfc4175_422be10_2(1921, 1079, ST_SIMD_LEVEL_AVX512,
                                     ST_SIMD_LEVEL_AVX512);
}

TEST(Cvt, v210_to_rfc4175_422be10_2_vbmi) {
  test_cvt_v210_to_rfc4175_422be10_2(1920, 1080, ST_SIMD_LEVEL_AVX512_VBMI2,
                                     ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_v210_to_rfc4175_422be10_2(1920, 1080, ST_SIMD_LEVEL_AVX512_VBMI2,
                                     ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_v210_to_rfc4175_422be10_2(1920, 1080, ST_SIMD_LEVEL_NONE,
                                     ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_v210_to_rfc4175_422be10_2(1920, 1080, ST_SIMD_LEVEL_AVX512_VBMI2,
                                     ST_SIMD_LEVEL_NONE);
  test_cvt_v210_to_rfc4175_422be10_2(722, 111, ST_SIMD_LEVEL_AVX512_VBMI2,
                                     ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_v210_to_rfc4175_422be10_2(1921, 1079, ST_SIMD_LEVEL_AVX512_VBMI2,
                                     ST_SIMD_LEVEL_AVX512_VBMI2);
}

static void test_cvt_v210_to_rfc4175_422be10_dma(st_udma_handle dma, int w, int h,
                                                 enum st_simd_level cvt_level,
                                                 enum st_simd_level back_level) {
  int ret;
  bool fail_case = (w * h % 6); /* do not convert when pg_num is not multiple of 3 */
  size_t fb_pg2_size = w * h * 5 / 2;
  size_t fb_pg2_size_v210 = w * h * 8 / 3;
  struct st20_rfc4175_422_10_pg2_be* pg_be =
      (struct st20_rfc4175_422_10_pg2_be*)st_test_zmalloc(fb_pg2_size);
  struct st20_rfc4175_422_10_pg2_be* pg_be_2 =
      (struct st20_rfc4175_422_10_pg2_be*)st_test_zmalloc(fb_pg2_size);
  struct st_tests_context* ctx = st_test_ctx();
  st_handle st = ctx->handle;
  uint8_t* pg_v210 = (uint8_t*)st_hp_zmalloc(st, fb_pg2_size_v210, ST_PORT_P);

  if (!pg_be || !pg_v210 || !pg_be_2) {
    EXPECT_EQ(0, 1);
    if (pg_be) st_test_free(pg_be);
    if (pg_v210) st_hp_free(st, pg_v210);
    if (pg_be_2) st_test_free(pg_be_2);
    return;
  }

  st_test_rand_data((uint8_t*)pg_be, fb_pg2_size, 0);
  ret = st20_rfc4175_422be10_to_v210_simd(pg_be, pg_v210, w, h, cvt_level);
  if (fail_case)
    EXPECT_NE(0, ret);
  else
    EXPECT_EQ(0, ret);

  ret = st20_v210_to_rfc4175_422be10_simd_dma(dma, pg_v210, st_hp_virt2iova(st, pg_v210),
                                              pg_be_2, w, h, back_level);
  if (fail_case)
    EXPECT_NE(0, ret);
  else
    EXPECT_EQ(0, ret);

  st_test_cmp((uint8_t*)pg_be, (uint8_t*)pg_be_2, fb_pg2_size);
  if (fail_case)
    EXPECT_NE(0, memcmp(pg_be, pg_be_2, fb_pg2_size));
  else
    EXPECT_EQ(0, memcmp(pg_be, pg_be_2, fb_pg2_size));

  st_hp_free(st, pg_v210);
  st_test_free(pg_be);
  st_test_free(pg_be_2);
}

TEST(Cvt, v210_to_rfc4175_422be10_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_v210_to_rfc4175_422be10_dma(dma, 1920, 1080, ST_SIMD_LEVEL_MAX,
                                       ST_SIMD_LEVEL_MAX);

  st_udma_free(dma);
}

TEST(Cvt, v210_to_rfc4175_422be10_scalar_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_v210_to_rfc4175_422be10_dma(dma, 1920, 1080, ST_SIMD_LEVEL_NONE,
                                       ST_SIMD_LEVEL_NONE);

  st_udma_free(dma);
}

TEST(Cvt, v210_to_rfc4175_422be10_avx512_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_v210_to_rfc4175_422be10_dma(dma, 1920, 1080, ST_SIMD_LEVEL_AVX512,
                                       ST_SIMD_LEVEL_AVX512);
  test_cvt_v210_to_rfc4175_422be10_dma(dma, 1920, 1080, ST_SIMD_LEVEL_AVX512,
                                       ST_SIMD_LEVEL_AVX512);
  test_cvt_v210_to_rfc4175_422be10_dma(dma, 1920, 1080, ST_SIMD_LEVEL_NONE,
                                       ST_SIMD_LEVEL_AVX512);
  test_cvt_v210_to_rfc4175_422be10_dma(dma, 1920, 1080, ST_SIMD_LEVEL_AVX512,
                                       ST_SIMD_LEVEL_NONE);
  test_cvt_v210_to_rfc4175_422be10_dma(dma, 722, 111, ST_SIMD_LEVEL_AVX512,
                                       ST_SIMD_LEVEL_AVX512);
  test_cvt_v210_to_rfc4175_422be10_dma(dma, 1921, 1079, ST_SIMD_LEVEL_AVX512,
                                       ST_SIMD_LEVEL_AVX512);

  st_udma_free(dma);
}

TEST(Cvt, v210_to_rfc4175_422be10_vbmi_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_v210_to_rfc4175_422be10_dma(dma, 1920, 1080, ST_SIMD_LEVEL_AVX512_VBMI2,
                                       ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_v210_to_rfc4175_422be10_dma(dma, 1920, 1080, ST_SIMD_LEVEL_AVX512_VBMI2,
                                       ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_v210_to_rfc4175_422be10_dma(dma, 1920, 1080, ST_SIMD_LEVEL_NONE,
                                       ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_v210_to_rfc4175_422be10_dma(dma, 1920, 1080, ST_SIMD_LEVEL_AVX512_VBMI2,
                                       ST_SIMD_LEVEL_NONE);
  test_cvt_v210_to_rfc4175_422be10_dma(dma, 722, 111, ST_SIMD_LEVEL_AVX512_VBMI2,
                                       ST_SIMD_LEVEL_AVX512_VBMI2);
  test_cvt_v210_to_rfc4175_422be10_dma(dma, 1921, 1079, ST_SIMD_LEVEL_AVX512_VBMI2,
                                       ST_SIMD_LEVEL_AVX512_VBMI2);

  st_udma_free(dma);
}

static void test_cvt_rfc4175_422be10_to_y210(int w, int h, enum st_simd_level cvt_level,
                                             enum st_simd_level back_level) {
  int ret;
  size_t fb_pg2_size = w * h * 5 / 2;
  struct st20_rfc4175_422_10_pg2_be* pg =
      (struct st20_rfc4175_422_10_pg2_be*)st_test_zmalloc(fb_pg2_size);
  struct st20_rfc4175_422_10_pg2_be* pg_2 =
      (struct st20_rfc4175_422_10_pg2_be*)st_test_zmalloc(fb_pg2_size);
  size_t fb_pg_y210_size = w * h * 2 * sizeof(uint16_t);
  uint16_t* pg_y210 = (uint16_t*)st_test_zmalloc(fb_pg_y210_size);

  if (!pg || !pg_2 || !pg_y210) {
    EXPECT_EQ(0, 1);
    if (pg) st_test_free(pg);
    if (pg_2) st_test_free(pg_2);
    if (pg_y210) st_test_free(pg_y210);
    return;
  }

  st_test_rand_data((uint8_t*)pg, fb_pg2_size, 0);

  ret = st20_rfc4175_422be10_to_y210_simd(pg, pg_y210, w, h, cvt_level);
  EXPECT_EQ(0, ret);

  ret = st20_y210_to_rfc4175_422be10_simd(pg_y210, pg_2, w, h, back_level);
  EXPECT_EQ(0, ret);

  EXPECT_EQ(0, memcmp(pg, pg_2, fb_pg2_size));

  st_test_free(pg);
  st_test_free(pg_2);
  st_test_free(pg_y210);
}

TEST(Cvt, rfc4175_422be10_to_y210) {
  test_cvt_rfc4175_422be10_to_y210(1920, 1080, ST_SIMD_LEVEL_MAX, ST_SIMD_LEVEL_MAX);
}

TEST(Cvt, rfc4175_422be10_to_y210_scalar) {
  test_cvt_rfc4175_422be10_to_y210(1920, 1080, ST_SIMD_LEVEL_NONE, ST_SIMD_LEVEL_NONE);
}

TEST(Cvt, rfc4175_422be10_to_y210_avx512) {
  test_cvt_rfc4175_422be10_to_y210(1920, 1080, ST_SIMD_LEVEL_AVX512,
                                   ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_y210(722, 111, ST_SIMD_LEVEL_AVX512, ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_y210(722, 111, ST_SIMD_LEVEL_NONE, ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_y210(722, 111, ST_SIMD_LEVEL_AVX512, ST_SIMD_LEVEL_NONE);
  int w = 2; /* each pg has two pixels */
  for (int h = 640; h < (640 + 64); h++) {
    test_cvt_rfc4175_422be10_to_y210(w, h, ST_SIMD_LEVEL_AVX512, ST_SIMD_LEVEL_AVX512);
  }
}

static void test_cvt_rfc4175_422be10_to_y210_dma(st_udma_handle dma, int w, int h,
                                                 enum st_simd_level cvt_level,
                                                 enum st_simd_level back_level) {
  int ret;
  size_t fb_pg2_size = w * h * 5 / 2;
  size_t fb_pg2_size_y210 = w * h * 4;
  struct st_tests_context* ctx = st_test_ctx();
  st_handle st = ctx->handle;
  struct st20_rfc4175_422_10_pg2_be* pg_be =
      (struct st20_rfc4175_422_10_pg2_be*)st_hp_zmalloc(st, fb_pg2_size, ST_PORT_P);
  struct st20_rfc4175_422_10_pg2_be* pg_be_2 =
      (struct st20_rfc4175_422_10_pg2_be*)st_test_zmalloc(fb_pg2_size);
  uint16_t* pg_y210 = (uint16_t*)st_test_zmalloc(fb_pg2_size_y210);

  if (!pg_be || !pg_y210 || !pg_be_2) {
    EXPECT_EQ(0, 1);
    if (pg_be) st_hp_free(st, pg_be);
    if (pg_be_2) st_test_free(pg_be_2);
    if (pg_y210) st_test_free(pg_y210);
    return;
  }

  st_test_rand_data((uint8_t*)pg_be, fb_pg2_size, 0);

  ret = st20_rfc4175_422be10_to_y210_simd_dma(dma, pg_be, st_hp_virt2iova(st, pg_be),
                                              pg_y210, w, h, cvt_level);
  EXPECT_EQ(0, ret);

  ret = st20_y210_to_rfc4175_422be10(pg_y210, pg_be_2, w, h);
  EXPECT_EQ(0, ret);
  EXPECT_EQ(0, memcmp(pg_be, pg_be_2, fb_pg2_size));

  st_hp_free(st, pg_be);
  st_test_free(pg_be_2);
  st_test_free(pg_y210);
}

TEST(Cvt, rfc4175_422be10_to_y210_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_rfc4175_422be10_to_y210_dma(dma, 1920, 1080, ST_SIMD_LEVEL_MAX,
                                       ST_SIMD_LEVEL_MAX);

  st_udma_free(dma);
}

TEST(Cvt, rfc4175_422be10_to_y210_scalar_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_rfc4175_422be10_to_y210_dma(dma, 1920, 1080, ST_SIMD_LEVEL_NONE,
                                       ST_SIMD_LEVEL_NONE);

  st_udma_free(dma);
}

TEST(Cvt, rfc4175_422be10_to_y210_avx512_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_rfc4175_422be10_to_y210_dma(dma, 1920, 1080, ST_SIMD_LEVEL_AVX512,
                                       ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_y210_dma(dma, 722, 111, ST_SIMD_LEVEL_AVX512,
                                       ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_y210_dma(dma, 722, 111, ST_SIMD_LEVEL_NONE,
                                       ST_SIMD_LEVEL_AVX512);
  test_cvt_rfc4175_422be10_to_y210_dma(dma, 722, 111, ST_SIMD_LEVEL_AVX512,
                                       ST_SIMD_LEVEL_NONE);
  int w = 2; /* each pg has two pixels */
  for (int h = 640; h < (640 + 64); h++) {
    test_cvt_rfc4175_422be10_to_y210_dma(dma, w, h, ST_SIMD_LEVEL_AVX512,
                                         ST_SIMD_LEVEL_AVX512);
  }

  st_udma_free(dma);
}

static void test_cvt_y210_to_rfc4175_422be10(int w, int h, enum st_simd_level cvt_level,
                                             enum st_simd_level back_level) {
  int ret;
  size_t fb_pg2_size = w * h * 5 / 2;
  struct st20_rfc4175_422_10_pg2_be* pg =
      (struct st20_rfc4175_422_10_pg2_be*)st_test_zmalloc(fb_pg2_size);
  size_t fb_pg_y210_size = w * h * 2 * sizeof(uint16_t);
  uint16_t* pg_y210 = (uint16_t*)st_test_zmalloc(fb_pg_y210_size);
  uint16_t* pg_y210_2 = (uint16_t*)st_test_zmalloc(fb_pg_y210_size);

  if (!pg || !pg_y210_2 || !pg_y210) {
    EXPECT_EQ(0, 1);
    if (pg) st_test_free(pg);
    if (pg_y210_2) st_test_free(pg_y210_2);
    if (pg_y210) st_test_free(pg_y210);
    return;
  }

  for (size_t i = 0; i < (fb_pg_y210_size / 2); i++) {
    pg_y210[i] = rand() & 0xFFC0; /* only 10 bit */
  }

  ret = st20_y210_to_rfc4175_422be10_simd(pg_y210, pg, w, h, cvt_level);
  EXPECT_EQ(0, ret);

  ret = st20_rfc4175_422be10_to_y210_simd(pg, pg_y210_2, w, h, back_level);
  EXPECT_EQ(0, ret);

  EXPECT_EQ(0, memcmp(pg_y210, pg_y210_2, fb_pg_y210_size));

  st_test_free(pg);
  st_test_free(pg_y210);
  st_test_free(pg_y210_2);
}

TEST(Cvt, y210_to_rfc4175_422be10) {
  test_cvt_y210_to_rfc4175_422be10(1920, 1080, ST_SIMD_LEVEL_MAX, ST_SIMD_LEVEL_MAX);
}

TEST(Cvt, y210_to_rfc4175_422be10_scalar) {
  test_cvt_y210_to_rfc4175_422be10(1920, 1080, ST_SIMD_LEVEL_NONE, ST_SIMD_LEVEL_NONE);
}

TEST(Cvt, y210_to_rfc4175_422be10_avx512) {
  test_cvt_y210_to_rfc4175_422be10(1920, 1080, ST_SIMD_LEVEL_AVX512,
                                   ST_SIMD_LEVEL_AVX512);
  test_cvt_y210_to_rfc4175_422be10(722, 111, ST_SIMD_LEVEL_AVX512, ST_SIMD_LEVEL_AVX512);
  test_cvt_y210_to_rfc4175_422be10(722, 111, ST_SIMD_LEVEL_NONE, ST_SIMD_LEVEL_AVX512);
  test_cvt_y210_to_rfc4175_422be10(722, 111, ST_SIMD_LEVEL_AVX512, ST_SIMD_LEVEL_NONE);
  int w = 2; /* each pg has two pixels */
  for (int h = 640; h < (640 + 64); h++) {
    test_cvt_y210_to_rfc4175_422be10(w, h, ST_SIMD_LEVEL_AVX512, ST_SIMD_LEVEL_AVX512);
  }
}

static void test_cvt_y210_to_rfc4175_422be10_dma(st_udma_handle dma, int w, int h,
                                                 enum st_simd_level cvt_level,
                                                 enum st_simd_level back_level) {
  int ret;
  size_t fb_pg2_size = w * h * 5 / 2;
  struct st_tests_context* ctx = st_test_ctx();
  st_handle st = ctx->handle;
  struct st20_rfc4175_422_10_pg2_be* pg =
      (struct st20_rfc4175_422_10_pg2_be*)st_test_zmalloc(fb_pg2_size);
  size_t fb_pg_y210_size = w * h * 2 * sizeof(uint16_t);
  uint16_t* pg_y210 = (uint16_t*)st_hp_zmalloc(st, fb_pg_y210_size, ST_PORT_P);
  st_iova_t pg_y210_iova = st_hp_virt2iova(st, pg_y210);
  uint16_t* pg_y210_2 = (uint16_t*)st_test_zmalloc(fb_pg_y210_size);

  if (!pg || !pg_y210_2 || !pg_y210) {
    EXPECT_EQ(0, 1);
    if (pg) st_test_free(pg);
    if (pg_y210_2) st_test_free(pg_y210_2);
    if (pg_y210) st_test_free(pg_y210);
    return;
  }

  for (size_t i = 0; i < (fb_pg_y210_size / 2); i++) {
    pg_y210[i] = rand() & 0xFFC0; /* only 10 bit */
  }

  ret = st20_y210_to_rfc4175_422be10_simd_dma(dma, pg_y210, pg_y210_iova, pg, w, h,
                                              cvt_level);
  EXPECT_EQ(0, ret);

  ret = st20_rfc4175_422be10_to_y210_simd(pg, pg_y210_2, w, h, back_level);
  EXPECT_EQ(0, ret);

  EXPECT_EQ(0, memcmp(pg_y210, pg_y210_2, fb_pg_y210_size));

  st_test_free(pg);
  st_hp_free(st, pg_y210);
  st_test_free(pg_y210_2);
}

TEST(Cvt, y210_to_rfc4175_422be10_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_y210_to_rfc4175_422be10_dma(dma, 1920, 1080, ST_SIMD_LEVEL_MAX,
                                       ST_SIMD_LEVEL_MAX);

  st_udma_free(dma);
}

TEST(Cvt, y210_to_rfc4175_422be10_scalar_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_y210_to_rfc4175_422be10_dma(dma, 1920, 1080, ST_SIMD_LEVEL_NONE,
                                       ST_SIMD_LEVEL_NONE);

  st_udma_free(dma);
}

TEST(Cvt, y210_to_rfc4175_422be10_avx512_dma) {
  struct st_tests_context* ctx = st_test_ctx();
  st_handle handle = ctx->handle;
  st_udma_handle dma = st_udma_create(handle, 128, ST_PORT_P);
  if (!dma) return;

  test_cvt_y210_to_rfc4175_422be10_dma(dma, 1920, 1080, ST_SIMD_LEVEL_AVX512,
                                       ST_SIMD_LEVEL_AVX512);
  test_cvt_y210_to_rfc4175_422be10_dma(dma, 722, 111, ST_SIMD_LEVEL_AVX512,
                                       ST_SIMD_LEVEL_AVX512);
  test_cvt_y210_to_rfc4175_422be10_dma(dma, 722, 111, ST_SIMD_LEVEL_NONE,
                                       ST_SIMD_LEVEL_AVX512);
  test_cvt_y210_to_rfc4175_422be10_dma(dma, 722, 111, ST_SIMD_LEVEL_AVX512,
                                       ST_SIMD_LEVEL_NONE);
  int w = 2; /* each pg has two pixels */
  for (int h = 640; h < (640 + 64); h++) {
    test_cvt_y210_to_rfc4175_422be10_dma(dma, w, h, ST_SIMD_LEVEL_AVX512,
                                         ST_SIMD_LEVEL_AVX512);
  }

  st_udma_free(dma);
}

static void test_rotate_rfc4175_422be10_422le10_yuv422p10le(
    int w, int h, enum st_simd_level cvt1_level, enum st_simd_level cvt2_level,
    enum st_simd_level cvt3_level) {
  int ret;
  size_t fb_pg2_size = w * h * 5 / 2;
  struct st20_rfc4175_422_10_pg2_be* pg_be =
      (struct st20_rfc4175_422_10_pg2_be*)st_test_zmalloc(fb_pg2_size);
  struct st20_rfc4175_422_10_pg2_le* pg_le =
      (struct st20_rfc4175_422_10_pg2_le*)st_test_zmalloc(fb_pg2_size);
  size_t planar_size = w * h * 2 * sizeof(uint16_t);
  uint16_t* p10_u16 = (uint16_t*)st_test_zmalloc(planar_size);
  struct st20_rfc4175_422_10_pg2_be* pg_be_2 =
      (struct st20_rfc4175_422_10_pg2_be*)st_test_zmalloc(fb_pg2_size);

  if (!pg_le || !pg_be || !p10_u16 || !pg_be_2) {
    EXPECT_EQ(0, 1);
    if (pg_le) st_test_free(pg_le);
    if (pg_be) st_test_free(pg_be);
    if (p10_u16) st_test_free(p10_u16);
    if (pg_be_2) st_test_free(pg_be_2);
    return;
  }

  st_test_rand_data((uint8_t*)pg_be, fb_pg2_size, 0);

  ret = st20_rfc4175_422be10_to_422le10_simd(pg_be, pg_le, w, h, cvt1_level);
  EXPECT_EQ(0, ret);

  ret = st20_rfc4175_422le10_to_yuv422p10le(pg_le, p10_u16, (p10_u16 + w * h),
                                            (p10_u16 + w * h * 3 / 2), w, h);
  EXPECT_EQ(0, ret);

  ret = st20_yuv422p10le_to_rfc4175_422be10_simd(
      p10_u16, (p10_u16 + w * h), (p10_u16 + w * h * 3 / 2), pg_be_2, w, h, cvt3_level);
  EXPECT_EQ(0, ret);

  EXPECT_EQ(0, memcmp(pg_be, pg_be_2, fb_pg2_size));

  st_test_free(pg_be);
  st_test_free(pg_le);
  st_test_free(pg_be_2);
  st_test_free(p10_u16);
}

TEST(Cvt, rotate_rfc4175_422be10_422le10_yuv422p10le_avx512) {
  test_rotate_rfc4175_422be10_422le10_yuv422p10le(
      1920, 1080, ST_SIMD_LEVEL_MAX, ST_SIMD_LEVEL_AVX512, ST_SIMD_LEVEL_AVX512);
  test_rotate_rfc4175_422be10_422le10_yuv422p10le(
      1920, 1080, ST_SIMD_LEVEL_MAX, ST_SIMD_LEVEL_NONE, ST_SIMD_LEVEL_AVX512);
  test_rotate_rfc4175_422be10_422le10_yuv422p10le(
      1920, 1080, ST_SIMD_LEVEL_MAX, ST_SIMD_LEVEL_AVX512, ST_SIMD_LEVEL_NONE);
}

TEST(Cvt, rotate_rfc4175_422be10_422le10_yuv422p10le_vbmi) {
  test_rotate_rfc4175_422be10_422le10_yuv422p10le(1920, 1080, ST_SIMD_LEVEL_MAX,
                                                  ST_SIMD_LEVEL_AVX512_VBMI2,
                                                  ST_SIMD_LEVEL_AVX512_VBMI2);
  test_rotate_rfc4175_422be10_422le10_yuv422p10le(
      1920, 1080, ST_SIMD_LEVEL_MAX, ST_SIMD_LEVEL_NONE, ST_SIMD_LEVEL_AVX512_VBMI2);
  test_rotate_rfc4175_422be10_422le10_yuv422p10le(
      1920, 1080, ST_SIMD_LEVEL_MAX, ST_SIMD_LEVEL_AVX512_VBMI2, ST_SIMD_LEVEL_NONE);
}

TEST(Cvt, rotate_rfc4175_422be10_422le10_yuv422p10le_scalar) {
  test_rotate_rfc4175_422be10_422le10_yuv422p10le(1920, 1080, ST_SIMD_LEVEL_NONE,
                                                  ST_SIMD_LEVEL_NONE, ST_SIMD_LEVEL_NONE);
}

static void test_rotate_rfc4175_422be10_yuv422p10le_422le10(
    int w, int h, enum st_simd_level cvt1_level, enum st_simd_level cvt2_level,
    enum st_simd_level cvt3_level) {
  int ret;
  size_t fb_pg2_size = w * h * 5 / 2;
  struct st20_rfc4175_422_10_pg2_be* pg_be =
      (struct st20_rfc4175_422_10_pg2_be*)st_test_zmalloc(fb_pg2_size);
  struct st20_rfc4175_422_10_pg2_le* pg_le =
      (struct st20_rfc4175_422_10_pg2_le*)st_test_zmalloc(fb_pg2_size);
  size_t planar_size = w * h * 2 * sizeof(uint16_t);
  uint16_t* p10_u16 = (uint16_t*)st_test_zmalloc(planar_size);
  struct st20_rfc4175_422_10_pg2_be* pg_be_2 =
      (struct st20_rfc4175_422_10_pg2_be*)st_test_zmalloc(fb_pg2_size);

  if (!pg_le || !pg_be || !p10_u16 || !pg_be_2) {
    EXPECT_EQ(0, 1);
    if (pg_le) st_test_free(pg_le);
    if (pg_be) st_test_free(pg_be);
    if (p10_u16) st_test_free(p10_u16);
    if (pg_be_2) st_test_free(pg_be_2);
    return;
  }

  st_test_rand_data((uint8_t*)pg_be, fb_pg2_size, 0);

  ret = st20_rfc4175_422be10_to_yuv422p10le_simd(
      pg_be, p10_u16, (p10_u16 + w * h), (p10_u16 + w * h * 3 / 2), w, h, cvt1_level);
  EXPECT_EQ(0, ret);

  ret = st20_yuv422p10le_to_rfc4175_422le10(p10_u16, (p10_u16 + w * h),
                                            (p10_u16 + w * h * 3 / 2), pg_le, w, h);
  EXPECT_EQ(0, ret);

  ret = st20_rfc4175_422le10_to_422be10(pg_le, pg_be_2, w, h);
  EXPECT_EQ(0, ret);

  EXPECT_EQ(0, memcmp(pg_be, pg_be_2, fb_pg2_size));

  st_test_free(pg_be);
  st_test_free(pg_le);
  st_test_free(pg_be_2);
  st_test_free(p10_u16);
}

TEST(Cvt, rotate_rfc4175_422be10_yuv422p10le_422le10_avx512) {
  test_rotate_rfc4175_422be10_yuv422p10le_422le10(
      1920, 1080, ST_SIMD_LEVEL_MAX, ST_SIMD_LEVEL_AVX512, ST_SIMD_LEVEL_AVX512);
  test_rotate_rfc4175_422be10_yuv422p10le_422le10(
      1920, 1080, ST_SIMD_LEVEL_MAX, ST_SIMD_LEVEL_AVX512, ST_SIMD_LEVEL_NONE);
  test_rotate_rfc4175_422be10_yuv422p10le_422le10(
      1920, 1080, ST_SIMD_LEVEL_MAX, ST_SIMD_LEVEL_NONE, ST_SIMD_LEVEL_AVX512);
}

TEST(Cvt, rotate_rfc4175_422be10_yuv422p10le_422le10_vbmi) {
  test_rotate_rfc4175_422be10_yuv422p10le_422le10(1920, 1080, ST_SIMD_LEVEL_MAX,
                                                  ST_SIMD_LEVEL_AVX512_VBMI2,
                                                  ST_SIMD_LEVEL_AVX512_VBMI2);
  test_rotate_rfc4175_422be10_yuv422p10le_422le10(
      1920, 1080, ST_SIMD_LEVEL_MAX, ST_SIMD_LEVEL_AVX512_VBMI2, ST_SIMD_LEVEL_NONE);
  test_rotate_rfc4175_422be10_yuv422p10le_422le10(
      1920, 1080, ST_SIMD_LEVEL_MAX, ST_SIMD_LEVEL_NONE, ST_SIMD_LEVEL_AVX512_VBMI2);
}

TEST(Cvt, rotate_rfc4175_422be10_yuv422p10le_422le10_scalar) {
  test_rotate_rfc4175_422be10_yuv422p10le_422le10(1920, 1080, ST_SIMD_LEVEL_NONE,
                                                  ST_SIMD_LEVEL_NONE, ST_SIMD_LEVEL_NONE);
}

static void test_am824_to_aes3(int blocks) {
  int ret;
  int subframes = blocks * 2 * 192;
  size_t blocks_size = subframes * 4;
  struct st31_aes3* b_aes3 = (struct st31_aes3*)st_test_zmalloc(blocks_size);
  struct st31_am824* b_am824 = (struct st31_am824*)st_test_zmalloc(blocks_size);
  struct st31_am824* b_am824_2 = (struct st31_am824*)st_test_zmalloc(blocks_size);
  if (!b_aes3 || !b_am824 || !b_am824_2) {
    EXPECT_EQ(0, 1);
    if (b_aes3) st_test_free(b_aes3);
    if (b_am824) st_test_free(b_am824);
    if (b_am824_2) st_test_free(b_am824_2);
    return;
  }

  st_test_rand_data((uint8_t*)b_am824, blocks_size, 0);
  /* set 'b' and 'f' for subframes */
  struct st31_am824* sf_am824 = b_am824;
  for (int i = 0; i < subframes; i++) {
    sf_am824->unused = 0;
    if (i % (192 * 2) == 0) {
      sf_am824->b = 1;
      sf_am824->f = 1;
    } else if (i % 2 == 0) {
      sf_am824->b = 0;
      sf_am824->f = 1;
    } else {
      sf_am824->b = 0;
      sf_am824->f = 0;
    }
    sf_am824++;
  }

  ret = st31_am824_to_aes3(b_am824, b_aes3, subframes);
  EXPECT_EQ(0, ret);

  ret = st31_aes3_to_am824(b_aes3, b_am824_2, subframes);
  EXPECT_EQ(0, ret);

  EXPECT_EQ(0, memcmp(b_am824, b_am824_2, blocks_size));

  st_test_free(b_aes3);
  st_test_free(b_am824);
  st_test_free(b_am824_2);
}

TEST(Cvt, st31_am824_to_aes3) {
  test_am824_to_aes3(1);
  test_am824_to_aes3(10);
  test_am824_to_aes3(100);
}

static void test_aes3_to_am824(int blocks) {
  int ret;
  int subframes = blocks * 2 * 192;
  size_t blocks_size = subframes * 4;
  struct st31_aes3* b_aes3 = (struct st31_aes3*)st_test_zmalloc(blocks_size);
  struct st31_am824* b_am824 = (struct st31_am824*)st_test_zmalloc(blocks_size);
  struct st31_aes3* b_aes3_2 = (struct st31_aes3*)st_test_zmalloc(blocks_size);
  if (!b_aes3 || !b_am824 || !b_aes3_2) {
    EXPECT_EQ(0, 1);
    if (b_aes3) st_test_free(b_aes3);
    if (b_am824) st_test_free(b_am824);
    if (b_aes3_2) st_test_free(b_aes3_2);
    return;
  }

  st_test_rand_data((uint8_t*)b_am824, blocks_size, 0);
  /* set 'b' and 'f' for subframes */
  struct st31_aes3* sf_aes3 = b_aes3;
  for (int i = 0; i < subframes; i++) {
    if (i % (192 * 2) == 0) {
      sf_aes3->preamble = 0x2;
    } else if (i % 2 == 0) {
      sf_aes3->preamble = 0x0;
    } else {
      sf_aes3->preamble = 0x1;
    }
    sf_aes3++;
  }

  ret = st31_aes3_to_am824(b_aes3, b_am824, subframes);
  EXPECT_EQ(0, ret);

  ret = st31_am824_to_aes3(b_am824, b_aes3_2, subframes);
  EXPECT_EQ(0, ret);

  EXPECT_EQ(0, memcmp(b_aes3, b_aes3_2, blocks_size));

  st_test_free(b_aes3);
  st_test_free(b_am824);
  st_test_free(b_aes3_2);
}

TEST(Cvt, st31_aes3_to_am824) {
  test_aes3_to_am824(1);
  test_aes3_to_am824(10);
  test_aes3_to_am824(100);
}