// Copyright 2025 Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _AOA_H_
#define _AOA_H_

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

#define DEBUG_DUMP_PCM 1
#define DEBUG_FILL_PATTERN_BYTES 0x10
#define DEBUG_OFFLINE_FILE 0
#define DEBUG_OFFLINE_MEM_BASE 0x7c000000

#define NOTIFY_NUM 2

// #define AOA_BUFFER_PERIODS_MAX      64
#define AOA_BUFFER_PERIODS_MAX 65536

#define AOA_RESULT_EVENT_NONE (0 << 0)
#define AOA_RESULT_EVENT_VAD (1 << 0)
#define AOA_RESULT_EVENT_AED (1 << 1)
#define AOA_RESULT_EVENT_BABYCRY (1 << 2)
#define AOA_RESULT_EVENT_DOG (1 << 3)
#define AOA_RESULT_EVENT_GUN (1 << 4)

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof((a)) / sizeof((a)[0]))
#endif

typedef uint32_t audio_pcm_uframes_t;
typedef int32_t audio_pcm_sframes_t;

enum {
	AOA_CMD_RELOAD_FIRMWARE = 0x1000,
	AOA_CMD_RESUME_COUNT_GET,
	AOA_CMD_RESUME_COUNT_SET,
	AOA_CMD_SUSPEND_THRESOLD_GET,
	AOA_CMD_SUSPEND_THRESOLD_SET,
	AOA_CMD_VOLUME_GET,
	AOA_CMD_VOLUME_SET,
};

enum {
	AOA_MODE_AOR = 0,
	AOA_MODE_AAD,
	AOA_MODE_EVENT_FLAG = 0x0100,
	AOA_MODE_AAD_EVENT = (AOA_MODE_AAD | AOA_MODE_EVENT_FLAG),
};

struct aoa_configs {
	char *fw_path;
	char *in_file;
	int mode;
	int period_size;
	int rate; /* TODO: only support 16000Hz now */
	int suspend_thresold;
};

struct aoa_results {
	int32_t event;
	uint32_t seq_hw;
	uint32_t seq_appl;
	uint32_t seq_total;
	int64_t pts;
};

static inline uint64_t delta_ts_us(struct timespec *ts_st, struct timespec *ts_ed) {
	time_t sec_diff;
	long nsec_diff;

	if (!ts_st || !ts_ed)
		return 0;

	if (ts_ed->tv_nsec < ts_st->tv_nsec) {
		sec_diff = ts_ed->tv_sec - ts_st->tv_sec - 1;
		nsec_diff = ts_ed->tv_nsec - ts_st->tv_nsec + 1000000000L;
	} else {
		sec_diff = ts_ed->tv_sec - ts_st->tv_sec;
		nsec_diff = ts_ed->tv_nsec - ts_st->tv_nsec;
	}
	return sec_diff * 1000000ULL + (uint64_t)(nsec_diff / 1000);
}

void *aoa_create(struct aoa_configs *config);
int aoa_destroy(void *hdl);
int aoa_start(void *hdl);
int aoa_stop(void *hdl);
int aoa_control(void *hdl, int cmd, void *arg);
int aoa_read(void *hdl, void *buf, int size, struct aoa_results *res, int timeout_ms);
uint32_t aoa_get_frame_pos(void *hdl);

#endif /* _AOA_H_ */
