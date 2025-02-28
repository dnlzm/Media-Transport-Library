/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#include "st_audio_transmitter.h"

#include "../mt_log.h"
#include "../mt_queue.h"
#include "st_err.h"
#include "st_tx_audio_session.h"

static int st_audio_trs_tasklet_start(void* priv) {
  struct st_audio_transmitter_impl* trs = priv;
  int idx = trs->idx;
  struct st_tx_audio_sessions_mgr* mgr = trs->mgr;

  rte_atomic32_set(&mgr->transmitter_started, 1);

  info("%s(%d), succ\n", __func__, idx);
  return 0;
}

static int st_audio_trs_tasklet_stop(void* priv) {
  struct st_audio_transmitter_impl* trs = priv;
  struct mtl_main_impl* impl = trs->parent;
  struct st_tx_audio_sessions_mgr* mgr = trs->mgr;
  int idx = trs->idx, port;

  rte_atomic32_set(&mgr->transmitter_started, 0);

  for (port = 0; port < mt_num_ports(impl); port++) {
    /* flush all the pkts in the tx ring desc */
    mt_txq_flush(mgr->queue[port], mt_get_pad(impl, port));
    mt_ring_dequeue_clean(mgr->ring[port]);
    info("%s(%d), port %d, remaining entries %d\n", __func__, idx, port,
         rte_ring_count(mgr->ring[port]));

    if (trs->inflight[port]) {
      rte_pktmbuf_free(trs->inflight[port]);
      trs->inflight[port] = NULL;
    }
  }
  mgr->st30_stat_pkts_burst = 0;

  return 0;
}

/* pacing handled by session itself */
static int st_audio_trs_session_tasklet(struct mtl_main_impl* impl,
                                        struct st_audio_transmitter_impl* trs,
                                        struct st_tx_audio_sessions_mgr* mgr,
                                        enum mtl_port port) {
  struct rte_ring* ring = mgr->ring[port];
  int ret;
  uint16_t n;
  struct rte_mbuf* pkt;

  /* check if any inflight pkts in transmitter */
  pkt = trs->inflight[port];
  if (pkt) {
    n = mt_txq_burst(mgr->queue[port], &pkt, 1);
    if (n >= 1) {
      trs->inflight[port] = NULL;
    } else {
      mgr->stat_trs_ret_code[port] = -STI_TSCTRS_BURST_INFLIGHT_FAIL;
      return MT_TASKLET_HAS_PENDING;
    }
    mgr->st30_stat_pkts_burst += n;
  }

  for (int i = 0; i < mgr->max_idx; i++) {
    /* try to dequeue */
    ret = rte_ring_sc_dequeue(ring, (void**)&pkt);
    if (ret < 0) {
      mgr->stat_trs_ret_code[port] = -STI_TSCTRS_DEQUEUE_FAIL;
      return MT_TASKLET_ALL_DONE; /* all done */
    }

    n = mt_txq_burst(mgr->queue[port], &pkt, 1);
    mgr->st30_stat_pkts_burst += n;
    if (n < 1) {
      trs->inflight[port] = pkt;
      trs->inflight_cnt[port]++;
      mgr->stat_trs_ret_code[port] = -STI_TSCTRS_BURST_INFLIGHT_FAIL;
      return MT_TASKLET_HAS_PENDING;
    }
  }

  mgr->stat_trs_ret_code[port] = 0;
  return MT_TASKLET_HAS_PENDING; /* may has pending pkt in the ring */
}

static int st_audio_trs_tasklet_handler(void* priv) {
  struct st_audio_transmitter_impl* trs = priv;
  struct mtl_main_impl* impl = trs->parent;
  struct st_tx_audio_sessions_mgr* mgr = trs->mgr;
  int port;
  int pending = MT_TASKLET_ALL_DONE;

  for (port = 0; port < mt_num_ports(impl); port++) {
    pending += st_audio_trs_session_tasklet(impl, trs, mgr, port);
  }

  return pending;
}

int st_audio_transmitter_init(struct mtl_main_impl* impl, struct mt_sch_impl* sch,
                              struct st_tx_audio_sessions_mgr* mgr,
                              struct st_audio_transmitter_impl* trs) {
  int idx = sch->idx;
  struct mt_sch_tasklet_ops ops;

  trs->parent = impl;
  trs->idx = idx;
  trs->mgr = mgr;

  rte_atomic32_set(&mgr->transmitter_started, 0);

  memset(&ops, 0x0, sizeof(ops));
  ops.priv = trs;
  ops.name = "audio_transmitter";
  ops.start = st_audio_trs_tasklet_start;
  ops.stop = st_audio_trs_tasklet_stop;
  ops.handler = st_audio_trs_tasklet_handler;

  trs->tasklet = mt_sch_register_tasklet(sch, &ops);
  if (!trs->tasklet) {
    err("%s(%d), mt_sch_register_tasklet fail\n", __func__, idx);
    return -EIO;
  }

  info("%s(%d), succ\n", __func__, idx);
  return 0;
}

int st_audio_transmitter_uinit(struct st_audio_transmitter_impl* trs) {
  int idx = trs->idx;

  if (trs->tasklet) {
    mt_sch_unregister_tasklet(trs->tasklet);
    trs->tasklet = NULL;
  }

  for (int i = 0; i < mt_num_ports(trs->parent); i++) {
    info("%s(%d), succ, inflight %d:%d\n", __func__, idx, i, trs->inflight_cnt[i]);
  }
  return 0;
}
