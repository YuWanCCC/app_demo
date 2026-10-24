/*
 * Copyright 2024 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * author: xing.zheng@rock-chips.com
 *   date: 2024-12-04
 * module: rockaa algorithm functions header
 */

#ifndef __ROCKAA_ALGO_FUNCS_H__
#define __ROCKAA_ALGO_FUNCS_H__

#include "skv.h"

#define LIB_RKNN_NAME "librknnmrt.so"
#define LIB_VQE_NAME "librkaudio_vqe.so"
#define LIB_VQE_NAME_LEGACY "libaec_bf_process.so"

#define CONF_EFFECT_SIZE 544
#define LIB_RKAUDIO_EFFECT_NAME "librkaudio_effect_eqdrc.so"

typedef struct _RTSkvParam {
	int model;
	SkvAecParam *aec;
	SkvBeamFormParam *bf;
	SkvRxParam *rx;
} RTSkvParam;

// external function symbols:
#if 0
__attribute__((weak)) void *AudioPost_Init(float* para, int frameLen) {
    LOGE("%s - failed to link to EQDRC Library. Solution:-Wl,--whole-archive librkaudio_effect_eqdrc.a", __func__);
    return NULL;
}

__attribute__((weak)) void *AudioPost_InitByPath(char* paraPath, int frameLen) {
    LOGE("%s - failed to link to EQDRC Library. Solution:-Wl,--whole-archive librkaudio_effect_eqdrc.a", __func__);
    return NULL;
}

__attribute__((weak)) int AudioPost_Destroy(void* context) {
    LOGE("%s - failed to link to EQDRC Library", __func__);
    if (context)
        free(context);
    return 0;
}

__attribute__((weak)) int AudioPost_Process(void* context, short int* pcmIn, short int* pcmOut, short int channels, int frameLen) {
    LOGE("%s - failed to link to EQDRC Library", __func__);
    return 0;
}

__attribute__((weak)) int AudioPost_GetPara(void* context, float *pfPara) {
    LOGE("%s - failed to link to EQDRC Library", __func__);
    return 0;
}

__attribute__((weak)) int AudioPost_SetPara(void* context, float *pfPara,int frameLen) {
    LOGE("%s - failed to link to EQDRC Library", __func__);
    return 0;
}
#endif

typedef void *(*EQDRC_Init)(float *para, int frameLen);
typedef void *(*EQDRC_InitByPath)(char *paraPath, int frameLen);
typedef int (*EQDRC_Destroy)(void *context);
typedef int (*EQDRC_Process)(void *context, short int *pcmIn, short int *pcmOut,
                             short int channels, int frameLen);
typedef int (*EQDRC_GetPara)(void *context, float *pfPara);
typedef int (*EQDRC_SetPara)(void *context, float *pfPara, int frameLen);

typedef struct _funcs_eqdrc {
	EQDRC_Init init;
	EQDRC_InitByPath initByPath;
	EQDRC_Destroy destroy;
	EQDRC_Process process;
	EQDRC_GetPara getPara;
	EQDRC_SetPara setPara;
} funcs_eqdrc;

typedef void *(*vqe_init)(int sampling_rate, int bits_persample, int num_src_channel,
                          int num_ref_channel, void *param);
typedef void *(*vqe_init_conf)(int sampling_rate, int bits_persample, int num_src_channel,
                               int num_ref_channel, char *conf_path);
typedef void (*vqe_destroy)(void *handle);
typedef int (*vqe_process_tx)(void *st_ptr, short *in, short *out, int in_size,
                              int *is_wakeup);
typedef int (*vqe_process_rx)(void *st_ptr, short *in, short *out);
typedef int (*vqe_doa_invoke)(void *st_ptr);
typedef int (*vqe_set_modelpath)(char *model_word1, char *model_word2, char *model_ainr);
typedef int (*vqe_cir_doa_invoke)(void *st_ptr, int *ang_doa, int *pth_doa);
typedef void *(*vqe_get_param)(void *st_ptr);
typedef int (*vqe_set_param)(void *st_ptr, void *param);
typedef int (*vqe_get_asr_id)(void *st_ptr, float *asr_score, int *asr_id);
typedef int (*vqe_get_cmd_id)(void *st_ptr, float *cmd_score, int *cmd_id);

typedef struct _funcs_vqe {
	vqe_init init;
	vqe_init_conf init_conf;
	vqe_destroy destroy;
	vqe_process_tx process_tx;
	vqe_process_rx process_rx;
	vqe_doa_invoke doa_invoke;
	vqe_set_modelpath set_modelpath;
	vqe_cir_doa_invoke cir_doa_invoke;
	vqe_get_param get_param;
	vqe_set_param set_param;
	vqe_get_asr_id get_asr_id;
	vqe_get_cmd_id get_cmd_id;
} funcs_vqe;

void *rockaa_dlsym_ai_vqe(void *handler);
void *rockaa_dlsym_ao_vqe(void *handler);
void *rockaa_dlsym_eqdrc(void *handler);
void *rockaa_static_symbols_eqdrc(void);

#endif /* __ROCKAA_ALGO_FUNCS_H__ */
