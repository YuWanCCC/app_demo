// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "sample_comm_aov.h"
#include "sample_comm.h"
#include <bits/pthreadtypes.h>
#include <net/if.h>   // must be included later than <net/if.h>
#include <linux/if.h> // must be included later than <net/if.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/prctl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/input.h>

#if defined(RV1126B)
#include "aoa.h"
#include "rockaa/rockaa_algo_funcs.h"
#include "rockaa/rockaa_symbols.h"
#endif

#define MAX_CAMERA_NUM 3
#define SOC_SLEEP_STR "mem"
#define SOC_SLEEP_PATH "/sys/power/state"

#ifdef AOV_FASTBOOT_ENABLE

#include "rk_meta.h"
#include "rk_meta_wakeup_param.h"
#include "sensor_iq_info.h"

// Memory region definitions
#define META_PHY_ADDR 0x800000
// Meta region
#define META_SIZE (384 * 1024)

struct meta_para_vir {
	int mem_fd;
	void *mapped_base;
	void *meta_head;
	int meta_head_size;
	void *param_share2kernel_offset;
	int param_share2kernel_offset_size;
	void *sensor_init_offset;
	int sensor_init_offset_size;
	void *cmdline_offset;
	int cmdline_offset_size;
	void *ae_table_offset;
	int ae_table_offset_size;
	void *app_param_offset;
	int app_param_offset_size;
	void *secondary_sensor_init_offset;
	int secondary_sensor_init_offset_size;
	void *wakeup_param_offset;
	int wakeup_param_offset_size;
	void *reserve;
	int reserve_size;
	void *backup;
	int backup_size;
	void *sensor_iq_bin_offset;
	int sensor_iq_bin_offset_size;
	void *secondary_sensor_iq_bin_offset;
	int secondary_sensor_iq_bin_offset_size;

	void *wakeup_aov_param_offset;
	int wakeup_aov_param_max_size;
};

static struct meta_para_vir *g_meta_vir = NULL;
#endif // #ifdef AOV_FASTBOOT_ENABLE

#if defined(RV1126B)

#define MP3_FRAME_SIZE 144
#define RESERVED_MP3_FRAME 32

static FILE *g_aoa_debug_fp = NULL;
static bool g_aoa_is_init = false;
static pthread_t g_aoa_thread_id;

struct vqe_results {
	int32_t wakeup_status;
	int32_t wakeup_mode;
	int32_t wakeup_cmd_id;
	float wakeup_cmd_score;
};

struct effect_infos {
	/* base members */
	void *vqe_hdl;
	void *vqe_syms;
	funcs_vqe *vqe_func;
	RTSkvParam *mParam;
	int rate;
	int bits;
	int nb_samples;
	int ch_near;
	int ch_far;
	char *conf_path;
	int vqe_status;
	int wakeup_mode; /* 0: ignore wakeup 1: wakeup 2: wakeup + command */
	int debug_mode;
	uint32_t num_wakeup;
	uint32_t frame_cot;
	struct vqe_results res;
	FILE *fpin_vqe;
	FILE *fpout_vqe;
};

#endif

static pthread_mutex_t g_wakeup_run_mutex = PTHREAD_MUTEX_INITIALIZER;
static RK_S32 g_input_device_fd = -1;
static long g_wakeup_period_time = -1;
#ifdef AOV_FASTBOOT_ENABLE
static void SAMPLE_COMM_AOV_MetaDump() {
	if (!g_meta_vir) {
		printf("[%s()] empty meta!\n", __func__);
		return;
	}
	printf("g_meta_vir->mem_fd: %d\n", g_meta_vir->mem_fd);
	printf("g_meta_vir->mapped_base: %p\n", g_meta_vir->mapped_base);
	printf("g_meta_vir->meta_head: %p\n", g_meta_vir->meta_head);
	printf("g_meta_vir->meta_head_size: %d\n", g_meta_vir->meta_head_size);
	printf("g_meta_vir->param_share2kernel_offset: %p\n",
	       g_meta_vir->param_share2kernel_offset);
	printf("g_meta_vir->param_share2kernel_offset_size: %d\n",
	       g_meta_vir->param_share2kernel_offset_size);
	printf("g_meta_vir->sensor_init_offset: %p\n", g_meta_vir->sensor_init_offset);
	printf("g_meta_vir->sensor_init_offset_size: %d\n",
	       g_meta_vir->sensor_init_offset_size);
	printf("g_meta_vir->cmdline_offset: %p\n", g_meta_vir->cmdline_offset);
	printf("g_meta_vir->cmdline_offset_size: %d\n", g_meta_vir->cmdline_offset_size);
	printf("g_meta_vir->ae_table_offset: %p\n", g_meta_vir->ae_table_offset);
	printf("g_meta_vir->ae_table_offset_size: %d\n", g_meta_vir->ae_table_offset_size);
	printf("g_meta_vir->app_param_offset: %p\n", g_meta_vir->app_param_offset);
	printf("g_meta_vir->app_param_offset_size: %d\n", g_meta_vir->app_param_offset_size);
	printf("g_meta_vir->secondary_sensor_init_offset: %p\n",
	       g_meta_vir->secondary_sensor_init_offset);
	printf("g_meta_vir->secondary_sensor_init_offset_size: %d\n",
	       g_meta_vir->wakeup_param_offset_size);
	printf("g_meta_vir->wakeup_param_offset: %p\n", g_meta_vir->wakeup_param_offset);
	printf("g_meta_vir->wakeup_param_offset_size: %d\n",
	       g_meta_vir->secondary_sensor_init_offset_size);
	printf("g_meta_vir->reserve: %p\n", g_meta_vir->reserve);
	printf("g_meta_vir->reserve_size: %d\n", g_meta_vir->reserve_size);
	printf("g_meta_vir->backup: %p\n", g_meta_vir->backup);
	printf("g_meta_vir->backup_size: %d\n", g_meta_vir->backup_size);
	printf("g_meta_vir->sensor_iq_bin_offset: %p\n", g_meta_vir->sensor_iq_bin_offset);
	printf("g_meta_vir->sensor_iq_bin_offset_size: %d\n",
	       g_meta_vir->sensor_iq_bin_offset_size);
	printf("g_meta_vir->secondary_sensor_iq_bin_offset: %p\n",
	       g_meta_vir->secondary_sensor_iq_bin_offset);
	printf("g_meta_vir->secondary_sensor_iq_bin_offset_size: %d\n",
	       g_meta_vir->secondary_sensor_iq_bin_offset_size);
	printf("g_meta_vir->wakeup_aov_param_offset: %p\n",
	       g_meta_vir->wakeup_aov_param_offset);
	printf("g_meta_vir->wakeup_aov_param_max_size: %d\n",
	       g_meta_vir->wakeup_aov_param_max_size);
	// ... Print other fields ...
}

#define se32_to_be32(x)                                                                  \
	((0x000000FF & x) << 24) | ((0x0000FF00 & x) << 8) | ((0x00FF0000 & x) >> 8) |       \
	    ((0xFF000000 & x) >> 24)

static int SAMPLE_COMM_AOV_MetaMmap() {
	int mem_fd = -1;
	void *mapped_base, *virt_addr;
	uint32_t meta_addr, meta_size;
	int dts_fd;

	if (g_meta_vir != NULL) {
		printf("[%s()] g_meta_vir has been mapped!", __func__);
		return RK_FAILURE;
	}
	g_meta_vir = malloc(sizeof(struct meta_para_vir));
	if (g_meta_vir == NULL) {
		printf("[%s()] g_meta_vir malloc failed!", __func__);
		return RK_FAILURE;
	}
	memset(g_meta_vir, 0, sizeof(struct meta_para_vir));

	mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (mem_fd == -1) {
		perror("Error opening /dev/mem");
		return EXIT_FAILURE;
	}

	dts_fd =
	    open("/sys/firmware/devicetree/base/reserved-memory/meta@800000/reg", O_RDONLY);
	if (dts_fd > 0) {
		int read_bytes = read(dts_fd, &meta_addr, sizeof(meta_addr));
		read_bytes = read(dts_fd, &meta_size, sizeof(meta_size));
		if (read_bytes > 0) {
			meta_size = se32_to_be32(meta_size);
			meta_addr = se32_to_be32(meta_addr);
			printf("# read firmware meta addr 0X%08x size 0X%08x!\n", meta_addr,
			       meta_size);
		} else {
			meta_size = META_SIZE;
			meta_addr = META_PHY_ADDR;
			printf("# read dts reg failed!\n");
		}
		close(dts_fd);
	} else {
		meta_size = META_SIZE;
		meta_addr = META_PHY_ADDR;
		printf("# read dts failed!\n");
	}

	mapped_base =
	    mmap(0, meta_size, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, meta_addr);
	if (mapped_base == MAP_FAILED) {
		perror("Error mapping physical address to virtual address");
		close(mem_fd);
		return EXIT_FAILURE;
	}

	g_meta_vir->mem_fd = mem_fd;
	g_meta_vir->mapped_base = mapped_base;
	g_meta_vir->meta_head = mapped_base + META_INFO_HEAD_OFFSET;
	g_meta_vir->meta_head_size = META_INFO_SIZE;
	g_meta_vir->param_share2kernel_offset = mapped_base + PARAM_SHARE2KERNEL_OFFSET;
	g_meta_vir->param_share2kernel_offset_size = PARAM_SHARE2KERNEL_SIZE;
	g_meta_vir->sensor_init_offset = mapped_base + SENSOR_INIT_OFFSET;
	g_meta_vir->sensor_init_offset_size = SENSOR_INIT_MAX_SIZE;
	g_meta_vir->cmdline_offset = mapped_base + CMDLINE_OFFSET;
	g_meta_vir->cmdline_offset_size = CMDLINE_MAX_SIZE;
	g_meta_vir->ae_table_offset = mapped_base + AE_TABLE_OFFSET;
	g_meta_vir->ae_table_offset_size = AE_TABLE_MAX_SIZE;
	g_meta_vir->app_param_offset = mapped_base + APP_PARAM_OFFSET;
	g_meta_vir->app_param_offset_size = APP_PARAM_MAX_SIZE;
	g_meta_vir->secondary_sensor_init_offset = mapped_base + SECONDARY_SENSOR_INIT_OFFSET;
	g_meta_vir->secondary_sensor_init_offset_size = SECONDARY_SENSOR_INIT_MAX_SIZE;
	g_meta_vir->wakeup_param_offset = mapped_base + WAKEUP_PARAM_OFFSET;
	g_meta_vir->wakeup_param_offset_size = WAKEUP_PARAM_MAX_SIZE;
	// g_meta_vir->reserve = mapped_base + RESERVE;
	// g_meta_vir->reserve_size = RESERVE_SIZE;
	// g_meta_vir->backup = mapped_base + BACKUP;
	// g_meta_vir->backup_size = BACKUP_SIZE;
	g_meta_vir->sensor_iq_bin_offset = mapped_base + SENSOR_IQ_BIN_OFFSET;
	g_meta_vir->sensor_iq_bin_offset_size = SENSOR_IQ_BIN_MAX_SIZE;
	g_meta_vir->secondary_sensor_iq_bin_offset =
	    mapped_base + SECONDARY_SENSOR_IQ_BIN_OFFSET;
	g_meta_vir->secondary_sensor_iq_bin_offset_size = SENSOR_IQ_BIN_MAX_SIZE;
	// g_meta_vir->secondary_sensor_iq_bin_offset_size = 0; // not support
	g_meta_vir->wakeup_aov_param_offset = mapped_base + WAKEUP_AOV_PARAM_OFFSET;
	g_meta_vir->wakeup_aov_param_max_size = WAKEUP_AOV_PARAM_MAX_SIZE;

	SAMPLE_COMM_AOV_MetaDump(g_meta_vir);

	// init mcu mode
	struct wakeup_param_info *wakeup_param =
	    (struct wakeup_param_info *)g_meta_vir->wakeup_param_offset;
	wakeup_param->wakeup_mode = 0;
	wakeup_param->ae_wakeup_mode = 0;
	wakeup_param->arm_run_count = 0;
	wakeup_param->arm_max_run_count = -1;
	wakeup_param->mcu_run_count = 0;
	wakeup_param->mcu_max_run_count = -1;

	return RK_SUCCESS;
}

static int SAMPLE_COMM_AOV_MetaMunmap() {
	if (g_meta_vir) {
		if (g_meta_vir->mapped_base)
			munmap(g_meta_vir->mapped_base, META_SIZE);
		if (g_meta_vir->mem_fd >= 0)
			close(g_meta_vir->mem_fd);
		free(g_meta_vir);
		g_meta_vir = NULL;
	} else {
		printf("[%s()] empty g_meta_vir!\n", __func__);
		return RK_FAILURE;
	}
	return RK_SUCCESS;
}

#endif // #ifdef AOV_FASTBOOT_ENABLE

int SAMPLE_COMM_AOV_Init() {
#ifdef AOV_FASTBOOT_ENABLE
	SAMPLE_COMM_AOV_MetaMmap();
#endif
	// INFO:
	// Find event device path in /proc/bus/input/devices is a better way.
	g_input_device_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
	if (g_input_device_fd < 0) {
		printf("Open failed /dev/input/event0: %s, you can ignore this"
		       " msg if you don't need gpio wakeup...\n",
		       strerror(errno));
	}

	pthread_mutex_init(&g_wakeup_run_mutex, NULL);
	return RK_SUCCESS;
}

int SAMPLE_COMM_AOV_Deinit() {
	pthread_mutex_lock(&g_wakeup_run_mutex);
#ifdef AOV_FASTBOOT_ENABLE
	SAMPLE_COMM_AOV_MetaMunmap();
#endif
	pthread_mutex_unlock(&g_wakeup_run_mutex);
	pthread_mutex_destroy(&g_wakeup_run_mutex);

	if (g_input_device_fd >= 0)
		close(g_input_device_fd);
}

void *SAMPLE_COMM_AOV_GetSensorInitParam() {
#ifdef AOV_FASTBOOT_ENABLE
	return g_meta_vir->sensor_init_offset;
#else
	return NULL;
#endif
}

void *SAMPLE_COMM_AOV_GetAppParam() {
#ifdef AOV_FASTBOOT_ENABLE
	return g_meta_vir->app_param_offset;
#else
	return NULL;
#endif
}

static void adjust_suspend_time() {
	if (g_wakeup_period_time != -1) {
		static struct timespec last_time = {0, 0};
		struct timespec current_time;
		clock_gettime(CLOCK_BOOTTIME, &current_time);
		if (last_time.tv_sec != 0 || last_time.tv_nsec != 0) {
			long diff_ms = ((current_time.tv_sec - last_time.tv_sec) * 1000) +
			               ((current_time.tv_nsec - last_time.tv_nsec) / 1000000);
			static int last_suspend_time = 0;
			if (last_suspend_time == 0)
				last_suspend_time = g_wakeup_period_time;
			int current_suspend_time =
			    last_suspend_time - (diff_ms - g_wakeup_period_time);
			if (current_suspend_time > 0) {
				SAMPLE_COMM_AOV_SetSuspendTime(current_suspend_time);
				last_suspend_time = current_suspend_time;
			} else {
				printf("cpu run time > suspend_time, so not set !!!\n");
			}
		}
		last_time = current_time;
	}
}
void SAMPLE_COMM_AOV_EnterSleep() {
	pthread_mutex_lock(&g_wakeup_run_mutex);
	adjust_suspend_time();
	if (SAMPLE_COMM_ECHO(SOC_SLEEP_PATH, SOC_SLEEP_STR, (strlen(SOC_SLEEP_STR))) == -1) {
		pthread_mutex_unlock(&g_wakeup_run_mutex);
		exit(EXIT_FAILURE);
	}
	pthread_mutex_unlock(&g_wakeup_run_mutex);
}

int SAMPLE_COMM_AOV_SetPeriodTime(int wakeup_period_time) {
	g_wakeup_period_time = wakeup_period_time;
	return SAMPLE_COMM_AOV_SetSuspendTime(wakeup_period_time);
}

#define MAX_LINE_SIZE 256
static int SAMPLE_COMM_AOV_CheckSDcardMount(void) {
	int fd = open("/proc/mounts", O_RDONLY);
	int ret = -1;
	if (fd == -1) {
		perror("Error opening /proc/mounts");
		return ret;
	}
	char line[MAX_LINE_SIZE];
	ssize_t bytesRead;
	int pos = 0;
	while ((bytesRead = read(fd, &line[pos], 1)) > 0) {
		if (line[pos] == '\n') {
			line[pos] = '\0'; // Null-terminate the string
			if (strstr(line, "/mnt/sdcard")) {
				printf("Found '/mnt/sdcard' in line: %s", line);
				ret = 0;
				break; // No need to continue searching
			}
			pos = 0; // Reset position for next line
		} else {
			pos++;
			if (pos >= MAX_LINE_SIZE - 1) {
				// Line exceeds buffer size, discard it
				pos = 0;
			}
		}
	}
	close(fd);
	return ret;

} /* -----  end of function SAMPLE_COMM_AOV_CheckSDcardMount  ----- */

#if defined(RV1126)
#define MOUNT_DEV_1 "/dev/mmcblk2p1"
#define MOUNT_DEV_2 "/dev/mmcblk2"
#else
#define MOUNT_DEV_1 "/dev/mmcblk1p1"
#define MOUNT_DEV_2 "/dev/mmcblk1"
#endif

static int copy_file_if_exists(const char *src, const char *dest) {
	FILE *source = fopen(src, "rb");
	if (source == NULL) {
		return -1;
	}

	FILE *destination = fopen(dest, "wb");
	if (destination == NULL) {
		fclose(source);
		return -1;
	}

	char buffer[1024];
	size_t bytes;

	while ((bytes = fread(buffer, 1, sizeof(buffer), source)) > 0) {
		fwrite(buffer, 1, bytes, destination);
	}

	fclose(source);
	fclose(destination);

	return 0;
}

int SAMPLE_COMM_AOV_CopyRawStreamToSdcard(int venc_chn_id, char *data, int data_size,
                                          char *data2, int data2_size) {
	int ret = 0;
	pthread_mutex_lock(&g_wakeup_run_mutex);
	SAMPLE_COMM_AOV_BindSdcard();
	// mount sd
	if (access(MOUNT_DEV_1, F_OK) == 0) {
		// system("mount -t vfat /dev/mmcblk1p1 /mnt/sdcard/");
		ret = mount(MOUNT_DEV_1, "/mnt/sdcard", "vfat", 0, NULL);
		if (ret != 0)
			printf("[%s()] mount failed because %s\n", __func__, strerror(errno));
		else
			printf("[%s()] mount success\n", __func__);
	} else if (access(MOUNT_DEV_2, F_OK) == 0) {
		// system("mount -t vfat /dev/mmcblk1 /mnt/sdcard/");
		ret = mount(MOUNT_DEV_2, "/mnt/sdcard", "vfat", 0, NULL);
		if (ret != 0)
			printf("[%s()] mount failed because %s\n", __func__, strerror(errno));
		else
			printf("[%s()] mount success\n", __func__);
	} else {
		printf("[%s()] bad mount path!\n", __func__);
		// goto SAMPLE_COMM_AOV_CopyRawStreamToSdcard_end;
	}

	if (0 != SAMPLE_COMM_AOV_CheckSDcardMount()) {
		printf("Not found mount sdcard on /mnt/sdcard\n");
		goto SAMPLE_COMM_AOV_CopyRawStreamToSdcard_end;
	}

	static int count_t = 0;
	static char dstPath[256];
	int s32fd = 0;
	char dst[256];
	// 首次进来存储，遍历目录是否可用，最多100个目录
	if (count_t == 0) {
		int i = 0;
		for (i = 0; i < 100; i++) {
			sprintf(dstPath, "/mnt/sdcard/wakeup_frame_%d", i);
			struct stat st;
			if (stat(dstPath, &st) == -1) {
				if (mkdir(dstPath, 0777) == -1)
					printf("mkdir %s failed\n", dstPath);
				break;
			}
		}
		sprintf(dst, "%s/thunderboot_time.txt", dstPath);
		copy_file_if_exists("/tmp/thunderboot_time.txt", dst);
	}
	sprintf(dst, "%s/venc_chn%d_%d.h265", dstPath, venc_chn_id, count_t);
	FILE *fp = fopen(dst, "wb");
	if (fp != NULL) {
		fwrite(data, 1, data_size, fp);
		if (data2)
			fwrite(data2, 1, data2_size, fp);
		s32fd = fileno(fp);
		fsync(s32fd);
		fclose(fp);
		count_t++;
	}
	// unmount sdcard
	umount2("/mnt/sdcard", MNT_DETACH);

SAMPLE_COMM_AOV_CopyRawStreamToSdcard_end:
	SAMPLE_COMM_AOV_UnbindSdcard();
	pthread_mutex_unlock(&g_wakeup_run_mutex);

	return RK_SUCCESS;
}

#if defined(RK_AOV_APP_BUILD_STATIC)
// reduce binary file size
int SAMPLE_COMM_AOV_InitMp4(SAMPLE_VENC_CTX_S *ctx, SAMPLE_MPI_MUXER_S **ppstMuxer) {
	return RK_FAILURE;
}

int SAMPLE_COMM_AOV_DeinitMp4(SAMPLE_MPI_MUXER_S *pstMuxer) { return RK_FAILURE; }

int SAMPLE_COMM_AOV_StartRecordMp4(SAMPLE_MPI_MUXER_S *pstMuxer) { return RK_FAILURE; }

int SAMPLE_COMM_AOV_StopRecordMp4(SAMPLE_MPI_MUXER_S *pstMuxer) { return RK_FAILURE; }

int SAMPLE_COMM_AOV_CopyMp4StreamToSdcard(SAMPLE_MPI_MUXER_S *pstMuxer,
                                          VENC_STREAM_S *frame, void *pData,
                                          bool need_flush) {
	return RK_FAILURE;
}
#else

static pthread_t g_aenc_thread_id;
static bool g_aenc_is_init = false;
static pthread_mutex_t g_list_mutex = PTHREAD_MUTEX_INITIALIZER;
static LIST_HEAD(g_list_head);

#ifndef RV1126B
static void *aenc_get_stream(void *pArgs) {
	SAMPLE_AENC_CTX_S aenc;
	SAMPLE_AI_CTX_S ai;
	RK_S32 s32Ret;
	RK_S32 s32MilliSec = 1000;
	void *pdata = NULL;
	MPP_CHN_S stSrcChn, stDestChn;
	SAMPLE_MPI_MUXER_S *pstMuxer = NULL, *tmp = NULL;

	RK_LOGD("Enter");
	memset(&ai, 0, sizeof(SAMPLE_AI_CTX_S));
	memset(&aenc, 0, sizeof(SAMPLE_AENC_CTX_S));

	SAMPLE_COMM_AI_SetAttr(&ai);
	s32Ret = SAMPLE_COMM_AI_CreateChn(&ai);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("create aenc channel error %#X!", s32Ret);
		return NULL;
	}

	SAMPLE_COMM_AENC_SetAttr(&aenc);
	SAMPLE_COMM_AENC_CreateChn(&aenc);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("create aenc channel error %#X!", s32Ret);
		SAMPLE_COMM_AI_DestroyChn(&ai);
		return NULL;
	}

	stSrcChn.enModId = RK_ID_AI;
	stSrcChn.s32DevId = 0;
	stSrcChn.s32ChnId = ai.s32ChnId;
	stDestChn.enModId = RK_ID_AENC;
	stDestChn.s32DevId = 0;
	stDestChn.s32ChnId = aenc.s32ChnId;
	SAMPLE_COMM_Bind(&stSrcChn, &stDestChn);

	while (g_aenc_is_init) {
		s32Ret = RK_MPI_AENC_GetStream(aenc.s32ChnId, &aenc.stFrame, s32MilliSec);
		if (s32Ret != RK_SUCCESS) {
			RK_LOGE("RK_MPI_AENC_GetStream failed %#X", s32Ret);
			continue;
		}
		pdata = RK_MPI_MB_Handle2VirAddr(aenc.stFrame.pMbBlk);

		pthread_mutex_lock(&g_list_mutex);
		list_for_each_entry_safe(pstMuxer, tmp, &g_list_head, list) {
			if ((pstMuxer->s32VideoDataSize + pstMuxer->s32AudioDataSize +
			     aenc.stFrame.u32Len) > AOV_STREAM_SIZE_WRITE_TO_SDCARD ||
			    pstMuxer->s32AudioFrmCnt >= AUDIOFRAMECNT) {
				RK_LOGE("audio count(%d) or size(%d) is too small",
				        pstMuxer->s32AudioFrmCnt, AOV_STREAM_SIZE_WRITE_TO_SDCARD);
				continue;
			}
			SAMPLE_COMM_MuxerAFrame(pstMuxer, aenc.stFrame.u32Len,
			                        aenc.stFrame.u64TimeStamp, pdata);
		}
		pthread_mutex_unlock(&g_list_mutex);

		RK_LOGD("[MP4] audio frame len %d, pts %lld, seq %u", aenc.stFrame.u32Len,
		        aenc.stFrame.u64TimeStamp, aenc.stFrame.u32Seq);
		RK_MPI_AENC_ReleaseStream(aenc.s32ChnId, &aenc.stFrame);
	}

	SAMPLE_COMM_UnBind(&stSrcChn, &stDestChn);
	SAMPLE_COMM_AENC_DestroyChn(&aenc);
	SAMPLE_COMM_AI_DestroyChn(&ai);
	RK_LOGD("Exit");
	return NULL;
}
#else

static int effect_vqe_create(struct effect_infos *effect) {
	void *func = NULL;
	int ld_mode = RTLD_NOW;
	char *uri = NULL;

	if (!effect) {
		fprintf(stderr, "ERR - effect is NULL\n");
		return -EINVAL;
	}

	if (!effect->conf_path) {
		fprintf(stderr, "ERR - effect conf_path is NULL\n");
		return -EINVAL;
	}

	/* Try to prepare and load rknn symbols */
	uri = getenv("sample_load_vqe_lazy");
	if (uri && !strcasecmp(uri, "true")) {
		ld_mode = RTLD_LAZY;
		fprintf(stderr, "load vqe symbols lazy\n");
	}

	uri = getenv("sample_load_vqe_path");
	if (uri) {
		fprintf(stderr, "rockaa capt func:%p, load vqe path:%s\n", func, uri);
		func =
		    rockaa_symbols_load(uri, rockaa_dlsym_ai_vqe, ld_mode, 0, &effect->vqe_syms);
	}
	if (effect->vqe_func == NULL) {
		func = rockaa_symbols_load(LIB_VQE_NAME, rockaa_dlsym_ai_vqe, ld_mode, 1,
		                           &effect->vqe_syms);
		if (func == NULL) {
			fprintf(stderr, "load legacy library: %s\n", LIB_VQE_NAME_LEGACY);
			func = rockaa_symbols_load(LIB_VQE_NAME_LEGACY, rockaa_dlsym_ai_vqe, ld_mode,
			                           1, &effect->vqe_syms);
			if (func == NULL) {
				fprintf(stderr, "ERR - load capture vqe algo symbols failed\n");
				return -EINVAL;
			}
		}
		effect->vqe_func = func;
	}

	fprintf(stderr, "The nb_samples:%d and rate:%d current algo frame is:%dms\n",
	        effect->nb_samples, effect->rate, effect->nb_samples * 1000 / effect->rate);

	effect->vqe_hdl = effect->vqe_func->init_conf(
	    effect->rate, effect->bits, effect->ch_near, effect->ch_far, effect->conf_path);
	if (!effect->vqe_hdl) {
		fprintf(stderr, "ERR - init conf vqe effect failed\n");
		return -EINVAL;
	}

	effect->mParam = effect->vqe_func->get_param(effect->vqe_hdl);
	if (effect->mParam == NULL) {
		fprintf(stderr, "ERR - get param fail");
		return -EINVAL;
	}

	fprintf(stderr, "%s %d - mParam: %p model: 0x%x ch near:%d far:%d\n", __func__,
	        __LINE__, effect->mParam, effect->mParam->model, effect->ch_near,
	        effect->ch_far);
	return 0;
}

static int effect_vqe_destroy(struct effect_infos *effect) {
	if (effect == NULL) {
		fprintf(stderr, "ERR: %s - Invalid effect!\n", __func__);
		return 0;
	}

	/* destroy effect vqe */
	if (effect->vqe_func) {
		effect->vqe_func->destroy(effect->vqe_hdl);
		rockaa_symbols_close(effect->vqe_syms);
		free(effect->vqe_func);
		effect->vqe_func = NULL;
	}

	return 0;
}

static int effect_vqe_process(struct effect_infos *effect, short *pcm_in,
                              short *pcm_out) {
	int targ_Doa, targ_ang, targ_pth;
	int wakeup_status = effect->vqe_status; /* keeping last status */
	int in_short, out_bytes = 0;
	uint64_t period_time = ((uint64_t)effect->nb_samples) * 1000 * 1000 / effect->rate;
	uint32_t cost_time = 0, cost_vqe = 0, cost_eqdrc = 0;
	float load = 0.0f, load_vqe = 0.0f, load_eqdrc = 0.0f;
	struct timeval tv_start, tv_end;

	if (effect == NULL) {
		fprintf(stderr, "ERR - %s - Invalid effect!\n", __func__);
		return 0;
	}

	in_short = effect->nb_samples * (effect->ch_near + effect->ch_far) * sizeof(int16_t) /
	           sizeof(int16_t);
	if (!effect->vqe_func) {
		fprintf(stderr, "ERR - %s - vqe_func should be vaild\n", __func__);
		return -EINVAL;
	}

	if (effect->fpin_vqe)
		fwrite(pcm_in, 1,
		       effect->nb_samples * sizeof(int16_t) * (effect->ch_near + effect->ch_far),
		       effect->fpin_vqe);

	if (effect->debug_mode)
		gettimeofday(&tv_start, NULL);
	out_bytes = effect->vqe_func->process_tx(effect->vqe_hdl, pcm_in, pcm_out, in_short,
	                                         &wakeup_status);
	if (effect->debug_mode)
		gettimeofday(&tv_end, NULL);

	if (effect->fpout_vqe)
		fwrite(pcm_out, 1, effect->nb_samples * sizeof(int16_t) * 1, effect->fpout_vqe);

	return out_bytes;
}

static void Vqe_SetAttr(struct effect_infos *effect) {
	if (effect == NULL) {
		fprintf(stderr, "ERR: %s - Invalid effect!\n", __func__);
		return;
	}
	effect->rate = 16000;
	effect->nb_samples = 256;
	effect->bits = 16;
	effect->ch_near = 1;
	effect->ch_far = 0;
	effect->vqe_status = 0;
	effect->wakeup_mode = 0;
	effect->num_wakeup = 0;
	effect->frame_cot = 0;
	effect->conf_path = "/oem/usr/share/config_aivqe_aor.json";
}

static void *aoa_aenc_stream(void *pArgs) {
	SAMPLE_AENC_CTX_S *aenc = (SAMPLE_AENC_CTX_S *)pArgs;
	void *pdata = NULL;
	SAMPLE_MPI_MUXER_S *pstMuxer = NULL, *tmp = NULL;
	RK_S32 ret;
	RK_U32 last_aenc_seq = 0;

	prctl(PR_SET_NAME, "aoa_aenc_thread");
	RK_LOGD("Enter");

	while (g_aenc_is_init) {

		ret = RK_MPI_AENC_GetStream(aenc->s32ChnId, &aenc->stFrame, 1000);
		if (ret != RK_SUCCESS) {
			RK_LOGE("RK_MPI_AENC_GetStream failed %#X", ret);
			continue;
		}

		if (g_aoa_debug_fp) {
			if (aenc->stFrame.u32Seq - last_aenc_seq != 1) {
				fprintf(g_aoa_debug_fp, "WARNING: mp3 seq gap detected: %u -> %u\n",
				        last_aenc_seq, aenc->stFrame.u32Seq);
			}
			last_aenc_seq = aenc->stFrame.u32Seq;
		}

		pdata = RK_MPI_MB_Handle2VirAddr(aenc->stFrame.pMbBlk);
		pthread_mutex_lock(&g_list_mutex);
		list_for_each_entry_safe(pstMuxer, tmp, &g_list_head, list) {
			SAMPLE_COMM_MuxerAFrame(pstMuxer, aenc->stFrame.u32Len,
			                        aenc->stFrame.u64TimeStamp, pdata);
		}
		pthread_mutex_unlock(&g_list_mutex);

		RK_MPI_AENC_ReleaseStream(aenc->s32ChnId, &aenc->stFrame);
	}
	return NULL;
}

static RK_S32 cache_audio_frm(SAMPLE_AENC_CTX_S aenc, MB_BLK blk, size_t size, RK_U64 pts,
                              RK_U32 seq) {
	if (blk == NULL) {
		return RK_FAILURE;
	}

	AUDIO_FRAME_S *stAudioFrm = calloc(1, sizeof(AUDIO_FRAME_S));
	if (!stAudioFrm) {
		RK_LOGE("calloc failed");
		return RK_FAILURE;
	}

	stAudioFrm->pMbBlk = blk;
	stAudioFrm->u32Len = size;
	stAudioFrm->u64TimeStamp = pts;
	stAudioFrm->u32Seq = seq;
	stAudioFrm->bBypassMbBlk = RK_TRUE;

	SAMPLE_COMM_AOA_Queue_Enqueue(aenc.wait_queue, stAudioFrm);
	return RK_SUCCESS;
}

static void *aoa_get_stream(void *pArgs) {
	SAMPLE_AOA_CTX_S *ctx = (SAMPLE_AOA_CTX_S *)pArgs;
	void *hdl_aoa;
	struct aoa_configs config;
	struct aoa_results res;
	SAMPLE_AENC_CTX_S aenc;
	struct effect_infos effect;
	RK_U64 last_aoa_pts = 0;
	MB_BLK blk;
	RK_U8 *vqe_buf = NULL, *aoa_buf = NULL;
	RK_U32 pcm_frame_size;
	RK_S32 ret = RK_FALSE;
	RK_U32 seq = 0;
	RK_S32 s32MilliSec = 10;

	prctl(PR_SET_NAME, "aoa_get_stream");
	memset(&config, 0, sizeof(config));
	memset(&aenc, 0, sizeof(SAMPLE_AENC_CTX_S));
	pcm_frame_size = 512;

	config.mode = ctx->u32AoaMode;
	config.rate = 16000;
	config.period_size = pcm_frame_size / sizeof(RK_S16);
	config.fw_path = NULL;

	char *debug_env = getenv("AOA_DEBUG");
	if (debug_env && (strcmp(debug_env, "1") == 0)) {
		g_aoa_debug_fp = fopen("/tmp/aoa_debug.log", "w");
		if (!g_aoa_debug_fp) {
			printf("Failed to open debug log file\n");
		}
	}

	aoa_buf = calloc(1, pcm_frame_size);
	if (!aoa_buf) {
		fprintf(stderr, "ERR: calloc failed\n");
		return NULL;
	}

	hdl_aoa = aoa_create(&config);
	if (!hdl_aoa) {
		fprintf(stderr, "ERR: create aoa failed\n");
		goto __clean_aoabuf;
	}

	ret = aoa_control(hdl_aoa, AOA_CMD_VOLUME_SET, (void *)&ctx->fVolumeGain);
	if (ret < 0) {
		fprintf(stderr, "ERR: aoa set volume failed: %d\n", ret);
		goto __clean_aoabuf;
	}

	Vqe_SetAttr(&effect);
	ret = effect_vqe_create(&effect);
	if (ret < 0) {
		fprintf(stderr, "ERR: setup vqe effect failed: %d\n", ret);
		goto __clean_aoa;
	}

	SAMPLE_COMM_AENC_SetAttr(&aenc);
	ret = SAMPLE_COMM_AENC_CreateChn(&aenc);
	if (ret != RK_SUCCESS) {
		RK_LOGE("create aenc channel error %#X!", ret);
		goto __clean_vqe;
	}

	ret = aoa_start(hdl_aoa);
	if (ret < 0) {
		fprintf(stderr, "ERR: aoa start failed: %d\n", ret);
		goto __clean_aenc;
	}

	g_aenc_is_init = true;
	pthread_t encode_thread_id;
	ret = pthread_create(&encode_thread_id, 0, aoa_aenc_stream, &aenc);
	if (ret != 0) {
		fprintf(stderr, "ERR: create encode thread failed: %d\n", ret);
		goto __stop_aoa;
	}

	while (g_aoa_is_init) {
		seq++;
		int read_size = aoa_read(hdl_aoa, aoa_buf, config.period_size, &res, -1);
		if (read_size <= 0) {
			fprintf(stderr, "ERR: aoa read failed: %d\n", read_size);
			continue;
		}

		if (g_aoa_debug_fp && res.pts != 0) {
			RK_U64 diff = res.pts - last_aoa_pts;
			if (diff < 0 || diff > 32000) {
				fprintf(g_aoa_debug_fp,
				        "WARNING: aoa pts gap detected: %llu -> %llu,diff %llu\n",
				        last_aoa_pts, res.pts, diff);
			}
			last_aoa_pts = res.pts;
		}

	__RE_TRY_GET_MB:
		blk = RK_MPI_MB_GetMB(aenc.pool_id, pcm_frame_size, RK_FALSE);
		if (!blk) {
			fprintf(stderr, "ERR: get mb block failed\n");
			SAMPLE_COMM_AENC_ProcessCachedFrame(&aenc, s32MilliSec);
			goto __RE_TRY_GET_MB;
		}

		vqe_buf = (RK_U8 *)RK_MPI_MB_Handle2VirAddr(blk);
		if (!vqe_buf) {
			fprintf(stderr, "ERR: get vqe buffer failed\n");
			continue;
		}

		ret = effect_vqe_process(&effect, (short *)aoa_buf, (short *)vqe_buf);
		if (ret <= 0) {
			fprintf(stderr, "ERR : effect_vqe_process failed\n");
			continue;
		}

		ret = cache_audio_frm(aenc, blk, pcm_frame_size, res.pts, seq);
		if (ret != RK_SUCCESS) {
			fprintf(stderr, "ERR: cache audio frame failed: %d\n", ret);
			continue;
		}

		ret = SAMPLE_COMM_AENC_ProcessCachedFrame(&aenc, s32MilliSec);
		if (ret != RK_SUCCESS) {
			RK_LOGE("SAMPLE_COMM_AENC_ProcessCachedFrame %#X", ret);
		}
	}

	if (g_aoa_debug_fp) {
		fflush(g_aoa_debug_fp);
		fsync(fileno(g_aoa_debug_fp));
		fclose(g_aoa_debug_fp);
		g_aoa_debug_fp = NULL;
	}

	g_aenc_is_init = false;
	pthread_join(encode_thread_id, NULL);

__stop_aoa:
	ret = aoa_stop(hdl_aoa);
	if (ret < 0) {
		fprintf(stderr, "ERR: aoa stop failed: %d\n", ret);
	}

__clean_aenc:
	SAMPLE_COMM_AENC_DestroyChn(&aenc);

__clean_vqe:
	effect_vqe_destroy(&effect);

__clean_aoa:
	ret = aoa_destroy(hdl_aoa);
	if (ret < 0) {
		fprintf(stderr, "ERR: aoa deinit failed: %d\n", ret);
	}

__clean_aoabuf:
	free(aoa_buf);
	return NULL;
}
#endif // RV1126B

static int save_mp4_to_sdcard(SAMPLE_MPI_MUXER_S *pstMuxer) {
	int ret;

	pthread_mutex_lock(&g_wakeup_run_mutex);
	SAMPLE_COMM_AOV_BindSdcard();
	// mount sd
	if (access(MOUNT_DEV_1, F_OK) == 0) {
		// system("mount -t vfat /dev/mmcblk1p1 /mnt/sdcard/");
		ret = mount(MOUNT_DEV_1, "/mnt/sdcard", "vfat", 0, NULL);
		if (ret != 0)
			printf("[%s()] mount failed because %s\n", __func__, strerror(errno));
		else
			printf("[%s()] mount success\n", __func__);
	} else if (access(MOUNT_DEV_2, F_OK) == 0) {
		// system("mount -t vfat /dev/mmcblk1 /mnt/sdcard/");
		ret = mount(MOUNT_DEV_2, "/mnt/sdcard", "vfat", 0, NULL);
		if (ret != 0)
			printf("[%s()] mount failed because %s\n", __func__, strerror(errno));
		else
			printf("[%s()] mount success\n", __func__);
	} else {
		printf("[%s()] bad mount path!\n", __func__);
		// goto SAMPLE_COMM_AOV_CopyStreamToSdcard_end;
	}

	if (0 != SAMPLE_COMM_AOV_CheckSDcardMount()) {
		printf("Not found mount sdcard on /mnt/sdcard\n");
		// free mp4, then continue
		SAMPLE_COMM_MuxerClean(pstMuxer);
		goto __copy_mp4_stream_end;
	}

	static int count_t = 0;
	static char dstPath[256];
	int s32fd = 0;
	char dstMuxer[256];
	char dst[256];
	char aoa_dst[256];
	FILE *fpMuxer = NULL;
	// 首次进来存储，遍历目录是否可用，最多100个目录
	if (count_t == 0) {
		int i = 0;
		for (i = 0; i < 100; i++) {
			sprintf(dstPath, "/mnt/sdcard/wakeup_video_%d", i);
			struct stat st;
			if (stat(dstPath, &st) == -1) {
				if (mkdir(dstPath, 0777) == -1)
					printf("mkdir %s failed\n", dstPath);
				break;
			}
		}
		sprintf(dst, "%s/thunderboot_time.txt", dstPath);
		copy_file_if_exists("/tmp/thunderboot_time.txt", dst);
		sprintf(aoa_dst, "%s/aoa_debug.log", dstPath);
		copy_file_if_exists("/tmp/aoa_debug.log", aoa_dst);
	}

	if (pstMuxer) {
		sprintf(dstMuxer, "%s/chn_%d_count_%d.mp4", dstPath, pstMuxer->s32VencChnId,
		        count_t);
		fpMuxer = fopen(dstMuxer, "wb");

		if (fpMuxer) {
			if (rkmuxer_init(0, "mp4", dstMuxer, &pstMuxer->stVideoParam,
			                 &pstMuxer->stAudioParam)) {
				RK_LOGE("rkmuxer_init failed");
				fclose(fpMuxer);
				goto __copy_mp4_stream_end;
			}

			SAMPLE_COMM_MuxerWrite(pstMuxer);
			s32fd = fileno(fpMuxer);
			fsync(s32fd);
			rkmuxer_deinit(0);
			fclose(fpMuxer);
			count_t++;
		} else {
			RK_LOGE("fopen rkmuxer failed");
			goto __copy_mp4_stream_end;
		}
	}
	// unmount sdcard
	umount2("/mnt/sdcard", MNT_DETACH);

__copy_mp4_stream_end:
	SAMPLE_COMM_AOV_UnbindSdcard();
	pthread_mutex_unlock(&g_wakeup_run_mutex);

	return RK_SUCCESS;
}

#if defined(RV1126B)

static void start_record_audio(void *pArgs) {
	pthread_mutex_lock(&g_wakeup_run_mutex);
	if (!g_aoa_is_init) {
		g_aoa_is_init = true;
		SAMPLE_COMM_AOV_BindSoundcard();
		pthread_create(&g_aoa_thread_id, 0, aoa_get_stream, pArgs);
		printf("AOA started\n");
	}
	pthread_mutex_unlock(&g_wakeup_run_mutex);
}

static void stop_record_audio(void) {
	// Unlock, wait for aenc thread exit.
	pthread_mutex_lock(&g_wakeup_run_mutex);
	if (g_aoa_is_init) {
		g_aoa_is_init = false;
		pthread_join(g_aoa_thread_id, NULL);
		printf("AOA stopped\n");
	}
	pthread_mutex_unlock(&g_wakeup_run_mutex);
}

#else

static void start_record_audio(void) {
	pthread_mutex_lock(&g_wakeup_run_mutex);
	if (!g_aenc_is_init) {
		g_aenc_is_init = true;
		SAMPLE_COMM_AOV_BindSoundcard();
		pthread_create(&g_aenc_thread_id, 0, aenc_get_stream, NULL);
	}
	pthread_mutex_unlock(&g_wakeup_run_mutex);
}

static void stop_record_audio(void) {
	// Unlock, wait for aenc thread exit.
	pthread_mutex_lock(&g_wakeup_run_mutex);
	if (g_aenc_is_init) {
		g_aenc_is_init = false;
		pthread_join(g_aenc_thread_id, NULL);
		SAMPLE_COMM_AOV_UnbindSoundcard();
	}
	pthread_mutex_unlock(&g_wakeup_run_mutex);
}
#endif // RV1126B

int SAMPLE_COMM_AOV_InitMp4(SAMPLE_VENC_CTX_S *ctx, SAMPLE_MPI_MUXER_S **ppstMuxer) {
	int ret = RK_SUCCESS;
	if (ctx == NULL || ppstMuxer == NULL) {
		RK_LOGE("bad parameter ctx %p, ppstMuxer %p", ctx, ppstMuxer);
		return RK_FAILURE;
	}
	RK_LOGI("[MP4] init muxer %d", ctx->s32ChnId);
	// Allocate cache buffer.
	ret = SAMPLE_COMM_MuxerInit(ppstMuxer, ctx);
	if (ret != RK_SUCCESS) {
		RK_LOGE("SAMPLE_COMM_MuxerInit failed");
		return ret;
	}
	(*ppstMuxer)->s32VencChnId = ctx->s32ChnId;

	pthread_mutex_lock(&g_list_mutex);
	list_add(&(*ppstMuxer)->list, &g_list_head);
	pthread_mutex_unlock(&g_list_mutex);

#if defined(RV1126B)
	start_record_audio(&ctx->stAoaCtx);
#endif
	SAMPLE_COMM_AOV_StartRecordMp4(*ppstMuxer);
	RK_MPI_VENC_RequestIDR((*ppstMuxer)->s32VencChnId, RK_FALSE);
	return ret;
}

int SAMPLE_COMM_AOV_DeinitMp4(SAMPLE_MPI_MUXER_S *pstMuxer) {
	if (pstMuxer == NULL) {
		RK_LOGE("pstMuxer is NULL!");
		return RK_FAILURE;
	}
	RK_LOGI("[MP4] deinit muxer %d", pstMuxer->s32VencChnId);
	pthread_mutex_lock(&g_list_mutex);
	list_del(&pstMuxer->list);
	pthread_mutex_unlock(&g_list_mutex);

#if defined(RV1126B)
	stop_record_audio();
#endif
	SAMPLE_COMM_AOV_StopRecordMp4(pstMuxer);

	// Save stream to sdcard when exit.
	save_mp4_to_sdcard(pstMuxer);

	// Release cache buffer.
	SAMPLE_COMM_MuxerDeinit(&pstMuxer);
}

int SAMPLE_COMM_AOV_StartRecordMp4(SAMPLE_MPI_MUXER_S *pstMuxer) {
#ifndef RV1126B
	if (pstMuxer == NULL) {
		RK_LOGE("pstMuxer is NULL!");
		return RK_FAILURE;
	}
	RK_LOGI("[MP4] start record mp4");
	start_record_audio();
#endif

	return RK_SUCCESS;
}

int SAMPLE_COMM_AOV_StopRecordMp4(SAMPLE_MPI_MUXER_S *pstMuxer) {
#ifndef RV1126B
	if (pstMuxer == NULL) {
		RK_LOGE("pstMuxer is NULL!");
		return RK_FAILURE;
	}

	stop_record_audio();
	RK_LOGI("[MP4] end record mp4");
#endif
	return RK_SUCCESS;
}

int SAMPLE_COMM_AOV_CopyMp4StreamToSdcard(SAMPLE_MPI_MUXER_S *pstMuxer,
                                          VENC_STREAM_S *frame, void *pData,
                                          bool need_flush) {
	int ret = 0;
	bool need_save_sd = false;
	int key_frame = 0;

	if (frame == NULL || pData == NULL || pstMuxer == NULL) {
		RK_LOGE("error parameter frame %p, pData %p, pstMuxer %p", frame, pData,
		        pstMuxer);
		return RK_FAILURE;
	}

	RK_LOGD("[MP4] video frame len:%u, pts:%llu, seq:%u", frame->pstPack->u32Len,
	        frame->pstPack->u64PTS, frame->u32Seq);

#if defined(RV1126B)
	RK_S32 reserve_size = frame->pstPack->u32Len + RESERVED_MP3_FRAME * MP3_FRAME_SIZE;
	if (pstMuxer->s32VideoDataSize + pstMuxer->s32AudioDataSize + reserve_size >
	        AOV_STREAM_SIZE_WRITE_TO_SDCARD ||
	    pstMuxer->s32VideoFrmCnt >= VIDEOFRAMECNT ||
	    pstMuxer->s32AudioFrmCnt + RESERVED_MP3_FRAME >= AUDIOFRAMECNT) {
		RK_LOGE("%d video count and %d audio count is save", pstMuxer->s32VideoFrmCnt,
		        pstMuxer->s32AudioFrmCnt);
		RK_LOGE("There is insufficient remaining resource, save it to the SD card first");
		need_save_sd = true;
	} else if (need_flush) {
		need_save_sd = true;
	}
#else
	if ((pstMuxer->s32VideoDataSize + pstMuxer->s32AudioDataSize +
	     frame->pstPack->u32Len) > AOV_STREAM_SIZE_WRITE_TO_SDCARD ||
	    pstMuxer->s32VideoFrmCnt >= VIDEOFRAMECNT) {
		RK_LOGE("video count(%d) or size(%d) is too small", pstMuxer->s32AudioFrmCnt,
		        AOV_STREAM_SIZE_WRITE_TO_SDCARD);
		need_save_sd = true;
	} else if (need_flush) {
		need_save_sd = true;
	}
#endif

	if (need_save_sd) {
		RK_LOGI("save mp4 stream to sdcard");
		save_mp4_to_sdcard(pstMuxer);
		// A MP4 stream is start with a I frame, so request I frame
		// after a stream end.
		RK_MPI_VENC_RequestIDR(pstMuxer->s32VencChnId, RK_FALSE);
	}

	if (frame->pstPack->DataType.enH264EType == H264E_NALU_PSLICE ||
	    frame->pstPack->DataType.enH265EType == H265E_NALU_PSLICE)
		key_frame = 0;
	else
		key_frame = 1;

	SAMPLE_COMM_MuxerVFrame(pstMuxer, frame->pstPack->u32Len, frame->pstPack->u64PTS,
	                        key_frame, pData);
#ifndef RV1126B
	if (!g_aenc_is_init) {
		RK_LOGD("[MP4] send dummp audio frame");
		SAMPLE_COMM_MuxerMuteAFrame(pstMuxer);
	}
#endif

done:
	return RK_SUCCESS;
}
#endif // RK_AOV_APP_BUILD_STATIC

void SAMPLE_COMM_AOV_DumpPtsToTMP(uint32_t seq, uint64_t pts, int max_dump_pts_count) {
	static int line_count = 0;
	static FILE *file;
	const char *file_path = "/tmp/pts.txt";

	if (line_count >= max_dump_pts_count) {
		return;
	}

	if (line_count == 0) {
		file = fopen(file_path, "w");
		if (file == NULL) {
			perror("Error opening file");
			return;
		}
	}

	if (file != NULL)
		fprintf(file, "seq: %u, pts: %llums\n", seq, (unsigned long long)pts / 1000);

	line_count++;

	if (line_count >= max_dump_pts_count) {
		printf("Closed file after writing %d lines.\n", max_dump_pts_count);
		fclose(file);
		file = NULL;
	}
}

// Return true is there has input event happened.
bool SAMPLE_COMM_AOV_GetGpioIrqStat() {
	bool input_event_happened = false;
	struct input_event event;

	if (g_input_device_fd < 0) {
		// printf("[%s()] unsupport input event detector\n", __func__);
		return input_event_happened;
	}

	// The read() operation is non-block, so the loop would
	// return EGAGIN if there has no any new input event.
	while (read(g_input_device_fd, &event, sizeof(event)) > 0) {
		// printf("[%s()] detect event type %d, code %d, value %d\n"
		// 		, __func__
		// 		, event.type
		// 		, event.code
		// 		, event.value
		// 		);
		// Translate ISP state if gpio-key is pressed between
		// sleeping time.
		if (event.type == EV_KEY && event.code == KEY_POWER && event.value == 1)
			input_event_happened = true;
	}
	return input_event_happened;
}

int SAMPLE_COMM_AOV_PreInitIsp(const char *sensor_name, const char *iq_file_dir,
                               int cam_index) {
	int ret = RK_SUCCESS;
#if defined(RV1126)
	return ret;
#endif
	if (iq_file_dir == NULL) {
		printf("[%s()] thunderboot read iq from meta.img !\n", __func__);
	}
#ifdef AOV_FASTBOOT_ENABLE
	struct sensor_iq_info *sensor_iq = NULL;
	void *main_iq_offset = NULL;
	void *secondary_iq_offset = NULL;

	pthread_mutex_lock(&g_wakeup_run_mutex);
	if (!g_meta_vir) {
		printf("[%s()] empty meta image!\n", __func__);
		pthread_mutex_unlock(&g_wakeup_run_mutex);
		return RK_FAILURE;
	}

	if (cam_index < MAX_CAMERA_NUM) {
		sensor_iq = (struct sensor_iq_info *)g_meta_vir->sensor_iq_bin_offset;
		main_iq_offset = (uint8_t *)g_meta_vir->mapped_base +
		                 (sensor_iq->main_sensor_iq_offset - META_PHY_ADDR);
		secondary_iq_offset = (uint8_t *)g_meta_vir->mapped_base +
		                      (sensor_iq->secondary_sensor_iq_offset - META_PHY_ADDR);
		printf("[%s()] main addr %p size %d, second addr %p size %d\n", __func__,
		       sensor_iq->main_sensor_iq_offset, sensor_iq->main_sensor_iq_size,
		       sensor_iq->secondary_sensor_iq_offset,
		       sensor_iq->secondary_sensor_iq_size);
	} else {
		printf("[%s()] error camera index %d!\n", __func__, cam_index);
		pthread_mutex_unlock(&g_wakeup_run_mutex);
		return RK_FAILURE;
	}

	if (cam_index == 0)
		ret = rk_aiq_uapi2_sysctl_preInit_iq_addr(sensor_name, main_iq_offset,
		                                          sensor_iq->main_sensor_iq_size);
	else if (cam_index == 1)
		ret = rk_aiq_uapi2_sysctl_preInit_iq_addr(sensor_name, secondary_iq_offset,
		                                          sensor_iq->secondary_sensor_iq_size);
	else
		printf("[%s()] not support camera index %d now!\n", cam_index);
	if (ret != RK_SUCCESS)
		printf("[%s()] rk_aiq_uapi2_sysctl_preInit_iq_addr failed %#X!\n", __func__, ret);
	pthread_mutex_unlock(&g_wakeup_run_mutex);
#endif
	return ret;
}
