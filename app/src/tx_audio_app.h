/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "app_base.h"
#include "log.h"

#ifndef _TX_APP_AUDIO_HEAD_H_
#define _TX_APP_AUDIO_HEAD_H_
int st_app_tx_audio_sessions_init(struct st_app_context* ctx);

int st_app_tx_audio_sessions_uinit(struct st_app_context* ctx);

int st_app_tx_audio_sessions_stop(struct st_app_context* ctx);
#endif