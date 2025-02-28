/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#include "../sample/sample_util.h"

static int perf_cvt_422_10_pg2_be_to_le8(mtl_handle st, int w, int h, int frames,
                                         int fb_cnt) {
  size_t fb_pg10_size = (size_t)w * h * 5 / 2;
  mtl_udma_handle dma = mtl_udma_create(st, 128, MTL_PORT_P);
  struct st20_rfc4175_422_10_pg2_be* pg_10 =
      (struct st20_rfc4175_422_10_pg2_be*)mtl_hp_malloc(st, fb_pg10_size * fb_cnt,
                                                        MTL_PORT_P);
  mtl_iova_t pg_10_iova = mtl_hp_virt2iova(st, pg_10);
  mtl_iova_t pg_10_in_iova;
  size_t fb_pg8_size = (size_t)w * h * 2;
  float fb_pg8_size_m = (float)fb_pg8_size / 1024 / 1024;
  struct st20_rfc4175_422_8_pg2_le* pg_8 =
      (struct st20_rfc4175_422_8_pg2_le*)malloc(fb_pg8_size * fb_cnt);
  enum mtl_simd_level cpu_level = mtl_get_simd_level();

  struct st20_rfc4175_422_10_pg2_be* pg_10_in;
  struct st20_rfc4175_422_8_pg2_le* pg_8_out;

  for (int i = 0; i < fb_cnt; i++) {
    pg_10_in = pg_10 + (i % fb_cnt) * (fb_pg10_size / sizeof(*pg_10));
    fill_rfc4175_422_10_pg2_data(pg_10_in, w, h);
  }

  clock_t start, end;
  float duration;

  start = clock();
  for (int i = 0; i < frames; i++) {
    pg_10_in = pg_10 + (i % fb_cnt) * (fb_pg10_size / sizeof(*pg_10));
    pg_8_out = pg_8 + (i % fb_cnt) * (fb_pg8_size / sizeof(*pg_8));
    st20_rfc4175_422be10_to_422le8_simd(pg_10_in, pg_8_out, w, h, MTL_SIMD_LEVEL_NONE);
  }
  end = clock();
  duration = (float)(end - start) / CLOCKS_PER_SEC;
  info("scalar, time: %f secs with %d frames(%dx%d,%fm@%d buffers)\n", duration, frames,
       w, h, fb_pg8_size_m, fb_cnt);

  if (cpu_level >= MTL_SIMD_LEVEL_AVX512) {
    start = clock();
    for (int i = 0; i < frames; i++) {
      pg_10_in = pg_10 + (i % fb_cnt) * (fb_pg10_size / sizeof(*pg_10));
      pg_8_out = pg_8 + (i % fb_cnt) * (fb_pg8_size / sizeof(*pg_8));
      st20_rfc4175_422be10_to_422le8_simd(pg_10_in, pg_8_out, w, h,
                                          MTL_SIMD_LEVEL_AVX512);
    }
    end = clock();
    float duration_simd = (float)(end - start) / CLOCKS_PER_SEC;
    info("avx512, time: %f secs with %d frames(%dx%d@%d buffers)\n", duration_simd,
         frames, w, h, fb_cnt);
    info("avx512, %fx performance to scalar\n", duration / duration_simd);

    if (dma) {
      start = clock();
      for (int i = 0; i < frames; i++) {
        pg_10_in = pg_10 + (i % fb_cnt) * (fb_pg10_size / sizeof(*pg_10));
        pg_10_in_iova = pg_10_iova + (i % fb_cnt) * (fb_pg10_size);
        pg_8_out = pg_8 + (i % fb_cnt) * (fb_pg8_size / sizeof(*pg_8));
        st20_rfc4175_422be10_to_422le8_simd_dma(dma, pg_10_in, pg_10_in_iova, pg_8_out, w,
                                                h, MTL_SIMD_LEVEL_AVX512);
      }
      end = clock();
      float duration_simd = (float)(end - start) / CLOCKS_PER_SEC;
      info("dma+avx512, time: %f secs with %d frames(%dx%d@%d buffers)\n", duration_simd,
           frames, w, h, fb_cnt);
      info("dma+avx512, %fx performance to scalar\n", duration / duration_simd);
    }
  }

  if (cpu_level >= MTL_SIMD_LEVEL_AVX512_VBMI2) {
    start = clock();
    for (int i = 0; i < frames; i++) {
      pg_10_in = pg_10 + (i % fb_cnt) * (fb_pg10_size / sizeof(*pg_10));
      pg_8_out = pg_8 + (i % fb_cnt) * (fb_pg8_size / sizeof(*pg_8));
      st20_rfc4175_422be10_to_422le8_simd(pg_10_in, pg_8_out, w, h,
                                          MTL_SIMD_LEVEL_AVX512_VBMI2);
    }
    end = clock();
    float duration_vbmi = (float)(end - start) / CLOCKS_PER_SEC;
    info("avx512_vbmi, time: %f secs with %d frames(%dx%d@%d buffers)\n", duration_vbmi,
         frames, w, h, fb_cnt);
    info("avx512_vbmi, %fx performance to scalar\n", duration / duration_vbmi);

    if (dma) {
      start = clock();
      for (int i = 0; i < frames; i++) {
        pg_10_in = pg_10 + (i % fb_cnt) * (fb_pg10_size / sizeof(*pg_10));
        pg_10_in_iova = pg_10_iova + (i % fb_cnt) * (fb_pg10_size);
        pg_8_out = pg_8 + (i % fb_cnt) * (fb_pg8_size / sizeof(*pg_8));
        st20_rfc4175_422be10_to_422le8_simd_dma(dma, pg_10_in, pg_10_in_iova, pg_8_out, w,
                                                h, MTL_SIMD_LEVEL_AVX512_VBMI2);
      }
      end = clock();
      float duration_vbmi = (float)(end - start) / CLOCKS_PER_SEC;
      info("dma+avx512_vbmi, time: %f secs with %d frames(%dx%d@%d buffers)\n",
           duration_vbmi, frames, w, h, fb_cnt);
      info("dma+avx512_vbmi, %fx performance to scalar\n", duration / duration_vbmi);
    }
  }

  mtl_hp_free(st, pg_10);
  free(pg_8);
  if (dma) mtl_udma_free(dma);

  return 0;
}

static void* perf_thread(void* arg) {
  mtl_handle dev_handle = arg;
  int frames = 60;
  int fb_cnt = 3;

  unsigned int lcore = 0;
  int ret = mtl_get_lcore(dev_handle, &lcore);
  if (ret < 0) {
    return NULL;
  }
  mtl_bind_to_lcore(dev_handle, pthread_self(), lcore);
  info("%s, run in lcore %u\n", __func__, lcore);

  perf_cvt_422_10_pg2_be_to_le8(dev_handle, 640, 480, frames, fb_cnt);
  perf_cvt_422_10_pg2_be_to_le8(dev_handle, 1280, 720, frames, fb_cnt);
  perf_cvt_422_10_pg2_be_to_le8(dev_handle, 1920, 1080, frames, fb_cnt);
  perf_cvt_422_10_pg2_be_to_le8(dev_handle, 1920 * 2, 1080 * 2, frames, fb_cnt);
  perf_cvt_422_10_pg2_be_to_le8(dev_handle, 1920 * 4, 1080 * 4, frames, fb_cnt);

  mtl_put_lcore(dev_handle, lcore);

  return NULL;
}

int main(int argc, char** argv) {
  struct st_sample_context ctx;
  int ret;

  memset(&ctx, 0, sizeof(ctx));
  ret = tx_sample_parse_args(&ctx, argc, argv);
  if (ret < 0) return ret;

  ctx.st = mtl_init(&ctx.param);
  if (!ctx.st) {
    err("%s: mtl_init fail\n", __func__);
    return -EIO;
  }

  pthread_t thread;
  pthread_create(&thread, NULL, perf_thread, ctx.st);
  pthread_join(thread, NULL);

  /* release sample(st) dev */
  if (ctx.st) {
    mtl_uninit(ctx.st);
    ctx.st = NULL;
  }
  return ret;
}