// Copyright 2025 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#define _GNU_SOURCE
#include <errno.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>

#include "aoa.h"
#include "asd/rk_aiq_types_asd_algo.h"
#include "rockaa/rockaa_algo_funcs.h"
#include "rockaa/rockaa_symbols.h"

#define SAMPLE_SUSPEND_THRESHOLD_1 300
#define SAMPLE_SUSPEND_THRESHOLD_2 400

#define CMD_FIFO "/tmp/aoa_cmd_fifo"
#define MAX_PATH 256

enum {
	EFFECT_VQE_WAKEUP_IGNORE = 0,
	EFFECT_VQE_WAKEUP_ONLY,
	EFFECT_VQE_WAKEUP_COMMAND,
};

enum {
	WAKEUP_STATUS_NONE = 0,
	WAKEUP_STATUS_KEYWORD,
	WAKEUP_STATUS_COMMAND,
};

enum {
	TEST_MODE_NONE = 0,
	TEST_MODE_DYNAMIC = 1,   /* Test case mode: sleep 1~2000ms, work 0~100ms */
	TEST_MODE_STABILITY = 2, /* sleep and work for a fixed duration */
	TEST_MODE_REINIT = 3     /* Re-initialization aoa handler: sleep 100ms, work 100ms */
};

struct test_config {
	int mode;
	int sleep_duration_ms;
	int work_duration_ms;
	int compile_count;
	int loop_count;
	bool is_pass_test;
};

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

const char *command_words[22] = {
    "No command",
    "Kai Deng",
    "Guan Deng",
    "Da Kai Deng",
    "Guan Bi Deng",
    "Da Kai Tiao Guang Deng",
    "Guan Bi Tiao Guang Deng",
    "Da Kai She Deng",
    "Guan Bi She Deng",
    "Da Kai Tong Deng",
    "Guan Bi Tong Deng",
    "Da Kai Deng Dai",
    "Guan Bi Deng Dai",
    "Da Kai Di Nuan",
    "Guan Bi Di Nuan",
    "Da Kai Xin Feng",
    "Guan Bi Xin Feng",
    "Da Kai Kong Tiao",
    "Guan Bi Kong Tiao",
    "Da Kai Chuang Lian",
    "Guan Bi Chuang Lian",
    "Zan Ting Chuang Lian",
};

static int stop_dump_pcm = 0;
static int g_exit_result = 0;
static struct test_config g_test_config = {
    .mode = TEST_MODE_NONE,
    .sleep_duration_ms = 1000,
    .work_duration_ms = 100,
    .loop_count = 0,
    .compile_count = 0,
    .is_pass_test = true,
};

static void signal_handler(int sig) {
	if (g_test_config.mode != TEST_MODE_NONE)
		g_test_config.loop_count = g_test_config.compile_count; // end the test
	else
		stop_dump_pcm = 1;
	printf("<aoa> %s %d enter sig=%d\n", __func__, __LINE__, sig);
}

static void *cmd_fifo_thread(void *arg) {
	void *hdl_aoa = arg;
	char buf[MAX_PATH] = {0};
	FILE *fp;

	if (mkfifo(CMD_FIFO, 0666) && errno != EEXIST) {
		perror("mkfifo");
		return NULL;
	}

	while (1) {
		int fd = open(CMD_FIFO, O_RDONLY);
		if (fd < 0) {
			perror("open fifo");
			sleep(1);
			continue;
		}
		fp = fdopen(fd, "r");
		if (!fp) {
			close(fd);
			sleep(1);
			continue;
		}

		while (fgets(buf, sizeof(buf), fp)) {
			buf[strcspn(buf, "\r\n")] = '\0';
			if (buf[0] == '\0')
				continue;
			printf("AOA: reload firmware -> %s\n", buf);
			aoa_control(hdl_aoa, AOA_CMD_RELOAD_FIRMWARE, buf);
		}

		fclose(fp);
	}
	return NULL;
}

static void usage(const char *prog) {
	fprintf(
	    stderr,
	    "Usage: %s [OPTION]...\n"
	    "\n"
	    "  -h --help             help\n"
	    "  -a --algo-config      specify the config file of backend VQE algorithm\n"
	    "  -r --rate             sample rate and only support 16000(default) or 8000 "
	    "(Hz)\n"
	    "  -S --suspend-thresold the running frame threshold for automatically "
	    "triggering sleep.\n"
	    "                        if the frame rate is 16ms, this value is set to 450.\n"
	    "                        each time the system wakes up, it will automatically\n"
	    "                        enter sleep after 4.8s.\n"
	    "  -p --period-size      the period size of per-frame (default: 256)\n"
	    "  -i --in-file          specify in file for offline testing\n"
	    "  -o --out-file         specify output file (default: /tmp/dump_sample.pcm)\n"
	    "  -m --mode             0: AOR witout firmware and output pcm is 1ch\n"
	    "                        1: AAD with firmware and output pcm is 2ch with event\n"
	    "  -f --firmware-path    the firmware for AAD mode\n"
	    "  -w --wakeup-mode      need backend VQE wakeup algrithm,\n"
	    "                        0: ignore wakeup, 1: wakeup, 2: wakeup + command\n"
	    "  -z --print-debug      print debug infos\n"
	    "\n"
	    "For example:\n"
	    "  running on AOR mode:\n"
	    "    %s -m 0 -o /tmp/my_aor.pcm\n"
	    "  running on AAD mode and enter suspend after 450 frames:\n"
	    "    %s -m 1 -f /oem/fw_aad_aivad.bin -S 450\n",
	    prog, prog, prog);
}

static int setup_vqe_dump(struct effect_infos *effect) {
	char *uri = NULL;

	if (!effect)
		return 0;

	uri = getenv("sample_ai_vqe_inout");
	if (uri && strcasestr(uri, "true")) {
		char filename[256] = {0};
		char *preDir, *ptr = NULL;
		bool freeStr = false;

		preDir = strtok_r((char *)uri, "@", &ptr);
		if (preDir)
			preDir = strtok_r(NULL, "@", &ptr);

		if (preDir == NULL) {
			preDir = strdup("/tmp");
			freeStr = NULL;
		}

		memset(filename, 0, sizeof(filename));
		sprintf(filename, "%s/sample-ai-vqe-in-%dch-%d.pcm", preDir,
		        effect->ch_near + effect->ch_far, effect->rate);

		effect->fpin_vqe = fopen(filename, "wb");
		if (effect->fpin_vqe == NULL)
			fprintf(stderr, "ERR - Can not open capture effect dump in: %s\n", filename);
		else
			fprintf(stderr, "Open capture effect dump in: %s\n", filename);

		memset(filename, 0, sizeof(filename));
		sprintf(filename, "%s/sample-ai-vqe-out-%dch-%d.pcm", preDir, 1, effect->rate);

		effect->fpout_vqe = fopen(filename, "wb");
		if (effect->fpout_vqe == NULL)
			fprintf(stderr, "ERR - Can not open capture effect dump out: %s\n", filename);
		else
			fprintf(stderr, "Open capture effect dump out: %s\n", filename);

		if (preDir)
			free(preDir);
	}
}

static int try_enter_sleep(void *aoa) {
	uint32_t resume_count, suspend_thresold;

	if (!aoa)
		return 0;

	aoa_control(aoa, AOA_CMD_RESUME_COUNT_GET, &resume_count);
	aoa_control(aoa, AOA_CMD_SUSPEND_THRESOLD_GET, &suspend_thresold);

	if (!suspend_thresold)
		return 0;

	if (resume_count <= suspend_thresold)
		return 0;

	fprintf(stderr, "Not detect voice and enter sleep resume_count:%d\n", resume_count);
	aoa_control(aoa, AOA_CMD_RESUME_COUNT_SET, 0);
	// aoa->suspend_thresold = SAMPLE_SUSPEND_THRESHOLD_1;
	system("echo mem > /sys/power/state");
	return 0;
}

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

	setup_vqe_dump(effect);

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

#if 0
    if (effect->debug_mode & DEBUG_SUMMARY) {
        int period_time = effect->nb_samples * 1000 * 1000 / effect->rate;

        fprintf(stderr, "== frames:%lld, period_time:%ldus, cost_time min:%ldus<%llu> max:%ldus<%llu> avg:%lldus, load min:%.2f%% max:%.2f%% avg:%.2f%% ==",
             effect->frames,
             period_time,
             effect->cost_time_min,
             effect->frame_min,
             effect->cost_time_max,
             effect->frame_max,
             effect->cost_time_total / effect->frames,
             effect->load_min,
             effect->load_max,
             effect->load_total / (float)(effect->frames));
    }
#endif
	return 0;
}

#if 1
static int effect_vqe_process(struct effect_infos *effect, short *pcm_in, short *pcm_out,
                              struct vqe_results *res) {
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

	if (effect->wakeup_mode == EFFECT_VQE_WAKEUP_COMMAND) {
		int32_t tmpstatus = (int32_t)wakeup_status;
		int32_t real_wakeup_status = tmpstatus & 0xfff;
		float cmd_score;
		int cmd_id;
		if (real_wakeup_status >= 1) {
			effect->frame_cot = 180; // init command waiting time to 3s
			effect->num_wakeup++;
			printf("---- [%ld], Hi, I am XiaoRui, wakeup_status: 0x%x ----\n",
			       effect->num_wakeup, wakeup_status);
			wakeup_status = wakeup_status >> 11 << 11;
			if (res)
				res->wakeup_status = WAKEUP_STATUS_KEYWORD;
		}
		if (effect->frame_cot > 0) {
			effect->frame_cot--;
			effect->vqe_func->get_cmd_id(effect->vqe_hdl, &cmd_score, &cmd_id);
			if (cmd_id != 0) {
				printf("==== Command[%d]: (%s), score: %f ====\n", cmd_id,
				       command_words[cmd_id], cmd_score);
				// if(effect->frame_cot < 180)
				//     effect->frame_cot = 180;   // once a command is detected, the
				//     waiting time will be reset ti at least 3s
				effect->frame_cot = 0;
				if (res)
					res->wakeup_status = WAKEUP_STATUS_COMMAND;
			} else if (effect->frame_cot == 0) {
				wakeup_status = 0;
				if (res)
					res->wakeup_status = WAKEUP_STATUS_NONE;
			}

			if (res) {
				res->wakeup_cmd_score = cmd_score;
				res->wakeup_cmd_id = cmd_id;
			}
		}
	} else if (effect->wakeup_mode == EFFECT_VQE_WAKEUP_ONLY) {
		if (wakeup_status > 0) {
			printf("wakeup %ld times, wakeup id = %d\n", ++effect->num_wakeup,
			       wakeup_status);
			wakeup_status = 0;
			// printf("targ_ang is %d,targ_pth is %d\n", targ_ang, targ_pth);
		}
	}
	effect->vqe_status = wakeup_status;

	if (res)
		res->wakeup_mode = effect->wakeup_mode;

	return out_bytes;
}
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#define MAX_PATH_LEN 128

typedef struct {
	char wakeup_path[MAX_PATH_LEN];
	// char cmd_path[MAX_PATH_LEN];
	int valid;
} audio_task_t;

static audio_task_t g_task = {.valid = 0};
static pthread_mutex_t g_task_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_task_cond = PTHREAD_COND_INITIALIZER;
static pid_t g_play_pid = 0;

/* spawn aplay, return child pid (or 0 on fork error) */
static pid_t spawn_aplay(const char *file) {
	pid_t pid = fork();
	if (pid < 0) {
		perror("fork");
		return 0;
	}
	if (pid == 0) {
		// fprintf(stderr, "will aplay file:%s\n", file);
		execl("/oem/aplay", "aplay", "-q", file, NULL);
		perror("execl /oem/aplay");
		_exit(127);
	}
	return pid;
}

/* Play a file, encounter exec failure (exit 127) and try again */
static void play_with_retry(const char *path) {
	while (1) {
		pid_t pid = spawn_aplay(path);
		if (pid <= 0) {
			/* Fork failed, try again later */
			usleep(50000);
			continue;
		}
		int status;
		waitpid(pid, &status, 0);

		/**
		 * If the child process fails to execute and exits directly (code 127),
		 * it is considered busy and retry
		 */
		if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
			usleep(50000);
			continue;
		}
		/* Whether it ends normally or the playback is completed, break */
		break;
	}
}

static void *audio_manager_thread(void *arg) {
	while (1) {
		pthread_mutex_lock(&g_task_mutex);
		while (!g_task.valid) {
			pthread_cond_wait(&g_task_cond, &g_task_mutex);
		}
		audio_task_t task = g_task;
		g_task.valid = 0;
		pthread_mutex_unlock(&g_task_mutex);

		/* Kill the old aplay first */
		if (g_play_pid > 0) {
			kill(g_play_pid, SIGKILL);
			waitpid(g_play_pid, NULL, 0);
			g_play_pid = 0;
			// usleep(10000);
		}

		play_with_retry(task.wakeup_path);

		/* If a new task comes during playback, skip the subsequent command audio. */
		pthread_mutex_lock(&g_task_mutex);
		int new_task = g_task.valid;
		pthread_mutex_unlock(&g_task_mutex);
		if (new_task) {
			continue;
		}

		// play_with_retry(task.cmd_path);
	}
	return NULL;
}

static void init_async_audio_manager() {
	pthread_t tid;
	pthread_create(&tid, NULL, audio_manager_thread, NULL);
	pthread_detach(tid);
}

static void init_cmd_fifo_thread(void *hdl) {
	pthread_t tid;
	pthread_create(&tid, NULL, cmd_fifo_thread, hdl);
	pthread_detach(tid);
}

static void check_and_play_wakeup(void *aoa, struct vqe_results *res) {
	if (res && res->wakeup_status == WAKEUP_STATUS_KEYWORD) {
		audio_task_t task;
		snprintf(task.wakeup_path, sizeof(task.wakeup_path),
		         "/oem/notice_wav/wakeup_00.wav");
		task.valid = 1;

		// fprintf(stderr, "INFO_00: aoa effect vqe results: 0x%x %d %d %f\n",
		//         res->wakeup_status, res->wakeup_mode,
		//         res->wakeup_cmd_id, res->wakeup_cmd_score);

		if (aoa)
			aoa_control(aoa, AOA_CMD_RESUME_COUNT_SET, 0);

		pthread_mutex_lock(&g_task_mutex);
		g_task = task;
		pthread_cond_signal(&g_task_cond);
		pthread_mutex_unlock(&g_task_mutex);
	}

	if (res && (res->wakeup_status == WAKEUP_STATUS_COMMAND) &&
	    (res->wakeup_cmd_id >= 1 && res->wakeup_cmd_id <= 21)) {
		audio_task_t task;
		snprintf(task.wakeup_path, sizeof(task.wakeup_path),
		         "/oem/notice_wav/command_%02d.wav", res->wakeup_cmd_id);
		task.valid = 1;

		// fprintf(stderr, "INFO_01: aoa effect vqe results: 0x%x %d %d %f\n",
		//         res->wakeup_status, res->wakeup_mode,
		//         res->wakeup_cmd_id, res->wakeup_cmd_score);

		res->wakeup_status == WAKEUP_STATUS_NONE;
		pthread_mutex_lock(&g_task_mutex);
		g_task = task;
		pthread_cond_signal(&g_task_cond);
		pthread_mutex_unlock(&g_task_mutex);
	}
}

static void program_handle_error(const char *func, uint32_t line) {
	printf("func: <%s> line: <%d> error exit!", func, line);
	printf("have completed %d times\n", g_test_config.compile_count);
	g_exit_result = -1;
	if (g_test_config.mode != TEST_MODE_NONE)
		g_test_config.loop_count = g_test_config.compile_count; // end the test
	else
		stop_dump_pcm = 1;
}

static int suspend_for_duration(int time) {
	if (time <= 0) {
		fprintf(stderr, "ERR: suspend_for_duration: invalid time %d\n", time);
		return -1;
	}
	char cmd[64];
	sprintf(cmd, "io -4 0x20834310 %u", (uint32_t)(32.768 * time));
	system(cmd);
	fprintf(stdout, "Setting suspend time: %d\n", time);
	system("echo mem > /sys/power/state");
	return 0;
}

static struct aoa_configs config;
static void *hdl_aoa;
static pthread_mutex_t g_aoa_wait_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t g_aoa_read_count = 0;
static uint32_t g_expected_reads = 0;
pthread_cond_t aoa_cond = PTHREAD_COND_INITIALIZER;

static void wait_aoa_read(int16_t read_count) {
	if (read_count <= 0)
		return;
	g_expected_reads = read_count;

	pthread_mutex_lock(&g_aoa_wait_mutex);
	g_aoa_read_count = 0;
	printf("Waiting for AOA read to complete...\n");
	while (g_aoa_read_count < g_expected_reads) {
		pthread_cond_wait(&aoa_cond, &g_aoa_wait_mutex);
	}
	pthread_mutex_unlock(&g_aoa_wait_mutex);
	return;
}

static void check_if_pass_test(const char *func, uint32_t line) {
	if (g_test_config.is_pass_test) {
		printf("Have completed %d test!\n", g_test_config.compile_count);
	} else {
		printf("AOA test failed!\n");
		program_handle_error(func, line);
	}
}

static int Init_Deinit_Test() {
	int ret;

	ret = aoa_stop(hdl_aoa);
	if (ret < 0) {
		program_handle_error(__func__, __LINE__);
		return ret;
	}
	// ret = aoa_destroy(hdl_aoa);
	// if (ret < 0) {
	//	program_handle_error(__func__, __LINE__);
	//	return ret;
	// }
	// hdl_aoa = aoa_create(&config);
	// if (!(hdl_aoa)) {
	//	program_handle_error(__func__, __LINE__);
	//	return -1;
	// }
	ret = aoa_start(hdl_aoa);
	if (ret < 0) {
		program_handle_error(__func__, __LINE__);
		return ret;
	}
}

static void *aoa_suspend_test(void *arg) {
	if (g_test_config.mode == TEST_MODE_NONE) {
		return NULL;
	}
	pthread_setname_np(pthread_self(), "aoa_test");
	srand(time(NULL));
	while (g_test_config.compile_count <= g_test_config.loop_count) {
		switch (g_test_config.mode) {
		case TEST_MODE_DYNAMIC:
			g_test_config.sleep_duration_ms = 49;
			while (g_test_config.sleep_duration_ms < 2000) {
				g_test_config.work_duration_ms = rand() % 100 + 1; // 0~100ms
				g_test_config.sleep_duration_ms += 49;             // 1~2000ms
				usleep(g_test_config.work_duration_ms * 1000);
				printf("work for %d ms\n", g_test_config.work_duration_ms);
				suspend_for_duration(g_test_config.sleep_duration_ms);
			}
			wait_aoa_read(10);
			check_if_pass_test(__func__, __LINE__);
			break;
		case TEST_MODE_STABILITY:
			usleep(g_test_config.work_duration_ms * 1000);
			printf("work for %d ms\n", g_test_config.work_duration_ms);
			suspend_for_duration(g_test_config.sleep_duration_ms);
			wait_aoa_read(10);
			check_if_pass_test(__func__, __LINE__);
			break;
		case TEST_MODE_REINIT:
			// pthread_mutex_lock(&g_aoa_test_mutex);
			Init_Deinit_Test();
			// pthread_mutex_unlock(&g_aoa_test_mutex);
			for (int16_t i = 0; i < 5; i++) {
				usleep(g_test_config.work_duration_ms * 1000);
				printf("work for %d ms\n", g_test_config.work_duration_ms);
				suspend_for_duration(g_test_config.sleep_duration_ms);
			}
			wait_aoa_read(10);
			check_if_pass_test(__func__, __LINE__);
			break;
		default:
			fprintf(stderr, "ERR: unknown test mode %d\n", g_test_config.mode);
		}
		g_test_config.compile_count++;
	}
	stop_dump_pcm = 1;
	return NULL;
}

int main(int argc, char *argv[]) {
	struct effect_infos *effect = NULL;
	pthread_t tid;
	int rate = 16000, mode = AOA_MODE_AOR, wakeup_mode = EFFECT_VQE_WAKEUP_IGNORE;
	int c, ret = 0;
	int period_size = 256, suspend_thresold = 0;
	int debug_mode = 0; /* 0: no print, 1: print info per-frame */
	char *pcm_buf = NULL, *pcm_comb = NULL, *pcm_vqe = NULL;
	char *in_file = NULL, *out_file = NULL, *out_name = NULL;
	char *algo_conf = NULL;
	pthread_t suspend_thread;
	FILE *fp_out = NULL;
	static struct option long_options[] = {
	    {"algo-config", required_argument, 0, 'a'},
	    {"rate", required_argument, 0, 'r'},
	    {"suspend-thresold", required_argument, 0, 'S'},
	    {"period-size", required_argument, 0, 'p'},
	    {"in-file", required_argument, 0, 'i'},
	    {"out-file", required_argument, 0, 'o'},
	    {"mode", required_argument, 0, 'm'},
	    {"firmware-path", required_argument, 0, 'f'},
	    {"wakeup-mode", required_argument, 0, 'w'},
	    {"print-debug", required_argument, 0, 'z'},
	    {"test-mode", required_argument, 0, 't'},
	    {"loop-count", required_argument, 0, 'l'},
	    {"suspend-time", required_argument, 0, 's'},
	    {0, 0, 0, 0}};

	if (argc < 2) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	memset(&config, 0, sizeof(config));
	while ((c = getopt_long(argc, argv, "a:r:S:p:i:o:m:f:w:z:t:l:s:", long_options,
	                        NULL)) != -1) {
		switch (c) {
		case 'a':
			algo_conf = strdup(optarg);
			if (!algo_conf) {
				perror("strdup");
				return EXIT_FAILURE;
			}
			break;
		case 'r':
			if (sscanf(optarg, "%u", &rate) != 1) {
				fprintf(stderr, "failed parsing rate '%s'\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		case 'S':
			if (sscanf(optarg, "%u", &suspend_thresold) != 1) {
				fprintf(stderr, "failed parsing suspend_thresold '%s'\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		case 'p':
			if (sscanf(optarg, "%u", &period_size) != 1) {
				fprintf(stderr, "failed parsing period size '%s'\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		case 'i':
			in_file = strdup(optarg);
			if (!in_file) {
				perror("strdup");
				return EXIT_FAILURE;
			}
			break;
		case 'o':
			out_file = strdup(optarg);
			if (!out_file) {
				perror("strdup");
				return EXIT_FAILURE;
			}
			break;
		case 'm':
			if (sscanf(optarg, "%u", &mode) != 1) {
				fprintf(stderr, "failed parsing mode '%s'\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		case 'f':
			config.fw_path = strdup(optarg);
			if (!config.fw_path) {
				perror("strdup");
				return EXIT_FAILURE;
			}
			break;
		case 'w':
			if (sscanf(optarg, "%u", &wakeup_mode) != 1) {
				fprintf(stderr, "failed parsing wakeup_mode '%s'\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		case 'z':
			if (sscanf(optarg, "%u", &debug_mode) != 1) {
				fprintf(stderr, "failed parsing debug_mode '%s'\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		case 't':
			if (sscanf(optarg, "%u", &g_test_config.mode) != 1) {
				fprintf(stderr, "failed parsing test_mode '%s'\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		case 'l':
			if (sscanf(optarg, "%u", &g_test_config.loop_count) != 1) {
				fprintf(stderr, "failed parsing loop_count '%s'\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		case 's':
			if (sscanf(optarg, "%u", &g_test_config.sleep_duration_ms) != 1) {
				fprintf(stderr, "failed parsing suspend_time '%s'\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		case '?':
		default:
			fprintf(stderr,
			        "Usage: %s [--rate N] [--period-size N] [--mode N] [--firmware-path "
			        "PATH]\n",
			        argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (optind < argc) {
		fprintf(stderr, "ERR: unexpected argument '%s'\n", argv[optind]);
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	if ((mode & AOA_MODE_AAD) && !config.fw_path) {
		fprintf(stderr, "ERR: Please input fw path on AAD mode\n");
		return EXIT_FAILURE;
	}

	config.mode = mode; /* mode - 0: AOR, 1: AAD */
	config.rate = rate;
	config.period_size = period_size;
	config.suspend_thresold = suspend_thresold;
	config.in_file = in_file;

	fprintf(stderr,
	        "INFO: aoa config mode:%d rate:%d period_size:%d fw_path:%s algo_conf:%s\n",
	        config.mode, config.rate, config.period_size, config.fw_path, algo_conf);

	hdl_aoa = aoa_create(&config);
	if (!hdl_aoa) {
		fprintf(stderr, "ERR: create aoa failed: %d\n");
		return -EINVAL;
	}

	pcm_buf = calloc(1, sizeof(int16_t) * period_size);
	if (!pcm_buf) {
		fprintf(stderr, "ERR: %s alloc pcm_buf failed\n", __func__);
		return -EINVAL;
	}

	if (out_file)
		out_name = out_file;
	else
		out_name = "/tmp/dump_sample.pcm";
	fp_out = fopen(out_name, "wb");
	if (!fp_out) {
		fprintf(stderr, "ERR: aoa fopen '%s' failed\n", out_name);
		return -EINVAL;
	}

	if (fp_out) {
		if (!pcm_comb) {
			pcm_comb =
			    calloc(1, sizeof(int16_t) * period_size * 2); /* 2ch: pcm + result */
			if (!pcm_comb) {
				fprintf(stderr, "ERR: %s alloc pcm_comb failed\n", __func__);
				return -EINVAL;
			}
		}
	}

	if (algo_conf) {
		if (!effect) {
			effect = calloc(1, sizeof(*effect));
			if (!effect) {
				fprintf(stderr, "ERR: %s alloc effect failed\n", __func__);
				return -EINVAL;
			}
			effect->rate = rate;
			effect->nb_samples = period_size;
			effect->bits = 16;
			effect->ch_near = 1;
			effect->ch_far = 0;
			effect->vqe_status = 0;
			effect->wakeup_mode = wakeup_mode;
			effect->num_wakeup = 0;
			effect->frame_cot = 0;
			effect->conf_path = algo_conf;
		}
		pcm_vqe = calloc(1, sizeof(int16_t) * period_size);
		if (!pcm_vqe) {
			fprintf(stderr, "ERR: %s alloc pcm_vqe failed\n", __func__);
			return -EINVAL;
		}

		ret = effect_vqe_create(effect);
		if (ret < 0) {
			fprintf(stderr, "ERR: setup vqe effect failed: %d\n", ret);
			return ret;
		}

		init_async_audio_manager();
	}

	if (g_test_config.mode > TEST_MODE_NONE) {
		pthread_create(&suspend_thread, NULL, aoa_suspend_test, NULL);
	}

	/* setup signal for ctrl+c stop capture */
	signal(SIGINT, signal_handler);

	fprintf(stderr, "INFO: %s %d aoa start, %s mode, in_file: %s\n", __func__, __LINE__,
	        config.in_file ? "offline" : "online", config.in_file);
	ret = aoa_start(hdl_aoa);
	if (ret < 0) {
		fprintf(stderr, "ERR: aoa start failed: %d\n", ret);
		return ret;
	}

	/* receive command runtime */
	init_cmd_fifo_thread(hdl_aoa);

	fprintf(stderr, "INFO: %s %d aoa read in loop\n", __func__, __LINE__);

	while (!stop_dump_pcm) {
		if (0 && in_file) {
			pause();
		} else {
			struct aoa_results res;
			struct timespec ts_st, ts_ed;
			uint32_t resume_count;
			static int64_t last_pts;
			int read_size;

			clock_gettime(CLOCK_MONOTONIC, &ts_st);
			read_size = aoa_read(hdl_aoa, pcm_buf, period_size, &res, 1000);
			if (read_size <= 0) {
				fprintf(stderr, "ERR: aoa read failed: %d\n", read_size);
				continue;
			}
			clock_gettime(CLOCK_MONOTONIC, &ts_ed);

			pthread_mutex_lock(&g_aoa_wait_mutex);
			g_aoa_read_count++;
			if (g_aoa_read_count == g_expected_reads)
				pthread_cond_signal(&aoa_cond);
			pthread_mutex_unlock(&g_aoa_wait_mutex);

			if (debug_mode == 1)
				fprintf(stderr,
				        "INFO: aoa read size:%d cost_time: %lluus event: 0x%x pts: "
				        "%lld=(%lld+%lld)\n",
				        read_size, delta_ts_us(&ts_st, &ts_ed), res.event, res.pts,
				        last_pts, res.pts - last_pts);

			if (effect) {
				struct vqe_results vqe_res;

				memset(&vqe_res, 0, sizeof(vqe_res));
				effect_vqe_process(effect, (short *)pcm_buf, (short *)pcm_vqe, &vqe_res);
				// fprintf(stderr, "INFO: aoa effect vqe results: 0x%x %d %d %f\n",
				//     vqe_res.wakeup_status, vqe_res.wakeup_mode,
				//     vqe_res.wakeup_cmd_id, vqe_res.wakeup_cmd_score);
				check_and_play_wakeup(hdl_aoa, &vqe_res);
			}

			if (fp_out) {
				if (mode & AOA_MODE_AAD) {
					int16_t *abuf_comb = (int16_t *)pcm_comb;
					int16_t *buf = (int16_t *)pcm_buf;

					for (int i = 0; i < read_size; i++) {
						abuf_comb[i * 2 + 0] = buf[i];
						abuf_comb[i * 2 + 1] = res.event ? 16384 : 0;
					}
					fwrite(abuf_comb, 1, sizeof(int16_t) * read_size * 2, fp_out);
				} else {
					fwrite(pcm_buf, 1, sizeof(int16_t) * read_size, fp_out);
				}
			}

			if (last_pts != 0 && g_test_config.mode > TEST_MODE_NONE) {
				int64_t delta = res.pts - last_pts;
				if (delta < 0 || delta > 32000) {
					fprintf(stderr, "ERR: aoa read pts is abnormal, delta: %lld\n",
					        delta);
				}
			}
			last_pts = res.pts;

			if (g_test_config.mode == TEST_MODE_REINIT) {
				char *blank_frame = (char *)calloc(1, sizeof(int16_t) * read_size);
				int cmp_result =
				    memcmp(blank_frame, pcm_buf, sizeof(int16_t) * read_size);
				if (cmp_result == 0) {
					fprintf(stderr, "get a blank frame\n");
					g_test_config.is_pass_test = false;
				}
				free(blank_frame);
			}

			if (g_test_config.mode > TEST_MODE_NONE) {
				// 测试模式不执行原有的休眠逻辑
				continue;
			}

			try_enter_sleep(hdl_aoa);
			aoa_control(hdl_aoa, AOA_CMD_RESUME_COUNT_GET, &resume_count);
			aoa_control(hdl_aoa, AOA_CMD_RESUME_COUNT_SET, (void *)(resume_count + 1));
		}
	}

	if (g_test_config.mode > TEST_MODE_NONE) {
		pthread_join(suspend_thread, NULL);
	}

	ret = aoa_stop(hdl_aoa);
	if (ret < 0) {
		fprintf(stderr, "ERR: aoa stop failed: %d\n", ret);
		return ret;
	}

	ret = aoa_destroy(hdl_aoa);
	if (ret < 0) {
		fprintf(stderr, "ERR: aoa deinit failed: %d\n", ret);
		return ret;
	}

	if (effect) {
		effect_vqe_destroy(effect);
		free(effect);
		effect = NULL;
	}

	if (config.fw_path) {
		free(config.fw_path);
		config.fw_path = NULL;
	}

	if (pcm_buf) {
		free(pcm_buf);
		pcm_buf = NULL;
	}

	if (pcm_comb) {
		free(pcm_comb);
		pcm_comb = NULL;
	}

	if (pcm_vqe) {
		free(pcm_vqe);
		pcm_vqe = NULL;
	}

	if (fp_out) {
		fclose(fp_out);
		fp_out = NULL;
	}

	if (out_file) {
		free(out_file);
		out_file = NULL;
	}

	if (in_file) {
		free(in_file);
		in_file = NULL;
	}

	if (effect) {
		free(effect);
		effect = NULL;
	}

	if (algo_conf) {
		free(algo_conf);
		algo_conf = NULL;
	}

	fprintf(stderr, "INFO: %s %d aoa notify stop and finish\n", __func__, __LINE__);
	return g_exit_result;
}
