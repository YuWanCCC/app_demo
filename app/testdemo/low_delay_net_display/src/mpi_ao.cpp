/*
 * Copyright 2020 Rockchip Electronics Co. LTD
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
 */

#include <stdio.h>
#include <errno.h>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <pthread.h>
#include "rk_defines.h"
#include "rk_debug.h"
#include "rk_mpi_adec.h"
#include "rk_mpi_ao.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_sys.h"


#define USE_AO_MIXER 0

typedef struct _rkMpiAOCtx {
    const char *srcFilePath;
    const char *dstFilePath;
    RK_S32      s32LoopCount;
    RK_S32      s32ChnNum;
    RK_S32      s32SampleRate;
    RK_S32      s32ReSmpSampleRate;
    RK_S32      s32Channel;
    RK_S32      s32DeviceChannel;
    RK_S32      s32BitWidth;
    RK_S32      s32DevId;
    RK_U32      u32FrmCount;
    RK_U32      u32FrmSize;
    RK_U32      u32PeriodCount;
    RK_U32      u32PeriodSize;
    RK_U32      u32StartDelay;
    RK_U32      u32StopDelay;
    char       *chCardName;
    RK_S32      s32ChnIndex;
    RK_S32      s32SetVolume;
    RK_S32      s32SetMute;
    RK_S32      s32SetFadeRate;
    RK_S32      s32SetTrackMode;
    RK_S32      s32GetVolume;
    RK_S32      s32GetMute;
    RK_S32      s32GetTrackMode;
    RK_S32      s32QueryChnStat;
    RK_S32      s32PauseResumeChn;
    RK_S32      s32SaveFile;
    RK_S32      s32QueryFileStat;
    RK_S32      s32ClrChnBuf;
    RK_S32      s32ClrPubAttr;
    RK_S32      s32GetPubAttr;
} TEST_AO_CTX_S;

static TEST_AO_CTX_S  *ctx = RK_NULL;


static AUDIO_SOUND_MODE_E find_sound_mode(RK_S32 ch) {
    AUDIO_SOUND_MODE_E channel = AUDIO_SOUND_MODE_BUTT;
    switch (ch) {
      case 1:
        channel = AUDIO_SOUND_MODE_MONO;
        break;
      case 2:
        channel = AUDIO_SOUND_MODE_STEREO;
        break;
      default:
        RK_LOGE("channel = %d not support", ch);
        return AUDIO_SOUND_MODE_BUTT;
    }

    return channel;
}

static AUDIO_BIT_WIDTH_E find_bit_width(RK_S32 bit) {
    AUDIO_BIT_WIDTH_E bitWidth = AUDIO_BIT_WIDTH_BUTT;
    switch (bit) {
      case 8:
        bitWidth = AUDIO_BIT_WIDTH_8;
        break;
      case 16:
        bitWidth = AUDIO_BIT_WIDTH_16;
        break;
      case 24:
        bitWidth = AUDIO_BIT_WIDTH_24;
        break;
      default:
        RK_LOGE("bitwidth(%d) not support", bit);
        return AUDIO_BIT_WIDTH_BUTT;
    }

    return bitWidth;
}

static RK_S32 open_device_ao(TEST_AO_CTX_S *ctx) {
    AUDIO_DEV aoDevId = ctx->s32DevId;
    AUDIO_SOUND_MODE_E soundMode;

    AIO_ATTR_S aoAttr;
    AIO_ADVPARAM_S aoAdvParam;
    memset(&aoAttr, 0, sizeof(AIO_ATTR_S));
    memset(&aoAdvParam, 0, sizeof(AIO_ADVPARAM_S));

    if (ctx->chCardName) {
        snprintf(reinterpret_cast<char *>(aoAttr.u8CardName),
                 sizeof(aoAttr.u8CardName), "%s", ctx->chCardName);
    }

    aoAttr.soundCard.channels = ctx->s32DeviceChannel;
    aoAttr.soundCard.sampleRate = ctx->s32SampleRate;
    aoAttr.soundCard.bitWidth = AUDIO_BIT_WIDTH_16;

    AUDIO_BIT_WIDTH_E bitWidth = find_bit_width(ctx->s32BitWidth);
    if (bitWidth == AUDIO_BIT_WIDTH_BUTT) {
        printf("bitwidth(%d) not support", ctx->s32BitWidth);
        goto __FAILED;
    }
    aoAttr.enBitwidth = bitWidth;
    aoAttr.enSamplerate = (AUDIO_SAMPLE_RATE_E)ctx->s32ReSmpSampleRate;
    soundMode = find_sound_mode(ctx->s32Channel);
    if (soundMode == AUDIO_SOUND_MODE_BUTT) {
        printf("channel = %d not support", ctx->s32Channel);
        goto __FAILED;
    }
    aoAttr.enSoundmode = soundMode;
    aoAttr.u32FrmNum = ctx->u32FrmCount;
    aoAttr.u32PtNumPerFrm = ctx->u32FrmSize;

    aoAttr.u32EXFlag = 0;
    aoAttr.u32ChnCnt = 2;

    RK_MPI_AO_SetPubAttr(aoDevId, &aoAttr);

    aoAdvParam.u32PeriodSize = ctx->u32PeriodSize;
    aoAdvParam.u32PeriodCount = ctx->u32PeriodCount;
    aoAdvParam.u32StartDelay = ctx->u32StartDelay;
    aoAdvParam.u32StopDelay = ctx->u32StopDelay;
    RK_MPI_AO_SetAdvParam(aoDevId, &aoAdvParam);

    RK_MPI_AO_Enable(aoDevId);

    return RK_SUCCESS;
__FAILED:
    return RK_FAILURE;
}

static RK_S32 init_mpi_ao_(TEST_AO_CTX_S *params) {
    RK_S32 result;

    result =  RK_MPI_AO_EnableChn(params->s32DevId, params->s32ChnIndex);
    if (result != 0) {
        printf("ao enable channel fail, aoChn = %d, reason = %x,s32DevId=%d\n", params->s32ChnIndex, result, params->s32DevId);
        return RK_FAILURE;
    }

    // set sample rate of input data
    /*result = RK_MPI_AO_EnableReSmp(params->s32DevId, params->s32ChnIndex,
                                  (AUDIO_SAMPLE_RATE_E)params->s32ReSmpSampleRate);
    if (result != 0) {
        printf("ao enable channel fail, reason = %x, aoChn = %d", result, params->s32ChnIndex);
        return RK_FAILURE;
    }*/

    return RK_SUCCESS;
}


static RK_S32 deinit_mpi_ao(AUDIO_DEV aoDevId, AO_CHN aoChn) {
    RK_S32 result = RK_MPI_AO_DisableReSmp(aoDevId, aoChn);
    if (result != 0) {
        RK_LOGE("ao disable resample fail, reason = %d", result);
        return RK_FAILURE;
    }

    result = RK_MPI_AO_DisableChn(aoDevId, aoChn);
    if (result != 0) {
        RK_LOGE("ao disable channel fail, reason = %d", result);
        return RK_FAILURE;
    }

    return RK_SUCCESS;
}

static RK_S32 close_device_ao(TEST_AO_CTX_S *ctx) {
    AUDIO_DEV aoDevId = ctx->s32DevId;
    RK_S32 result =  RK_MPI_AO_Disable(aoDevId);
    if (result != 0) {
        RK_LOGE("ao disable fail, reason = %d", result);
        return RK_FAILURE;
    }
    return RK_SUCCESS;
}

static RK_S32 set_ao_channel_mode(AUDIO_DEV aoDevId, AO_CHN aoChn) {
    RK_S32 result = 0;
    AO_CHN_PARAM_S pstParams;
    // 1. always get the AO_CHN_PARAM_S from AO
    // 2. set the parameter which want to change.
    // 3. set the new parameter to AO
    RK_MPI_AO_GetChnParams(aoDevId, aoChn, &pstParams);

    // for test : aoChn0 output left channel,  aoChn1 output right channel,
    if (aoChn == 0) {
        pstParams.enMode = AUDIO_CHN_MODE_LEFT;
    } else if (aoChn == 1) {
        pstParams.enMode = AUDIO_CHN_MODE_RIGHT;
    }

    result = RK_MPI_AO_SetChnParams(aoDevId, aoChn, &pstParams);
    if (result != RK_SUCCESS) {
        RK_LOGE("ao set channel params, aoChn = %d", aoChn);
        return RK_FAILURE;
    }

    return RK_SUCCESS;
}

int sendData(void* data, int len) {

    MB_POOL_CONFIG_S pool_config;
    // set default value for struct
    RK_U8 *srcData = data;
    AUDIO_FRAME_S frame;
    RK_U64 timeStamp = 0;
    RK_S32 s32MilliSec = -1;
    RK_S32 size = len;
    RK_S32 result = 0;

    frame.u32Len = size;
    frame.u64TimeStamp = timeStamp++;
    frame.enBitWidth = find_bit_width(ctx->s32BitWidth);
    frame.enSoundMode = find_sound_mode(ctx->s32Channel);
    frame.bBypassMbBlk = RK_FALSE;

    MB_EXT_CONFIG_S extConfig;
    memset(&extConfig, 0, sizeof(extConfig));
    extConfig.pOpaque = srcData;
    extConfig.pu8VirAddr = srcData;
    extConfig.u64Size = size;
    RK_MPI_SYS_CreateMB(&(frame.pMbBlk), &extConfig);
    #if 1
    result = RK_MPI_AO_SendFrame(ctx->s32DevId, ctx->s32ChnIndex, &frame, s32MilliSec);
    if (result < 0) {
        RK_LOGE("send frame fail, result = %d, TimeStamp = %lld, s32MilliSec = %d",
            result, frame.u64TimeStamp, s32MilliSec);
        goto __EXIT;
    } else {
       // printf("send frame success, result = %d, TimeStamp = %lld, s32MilliSec = %d\n",
        //    result, frame.u64TimeStamp, s32MilliSec);
    }
    #endif
    RK_MPI_MB_ReleaseMB(frame.pMbBlk);
    return 0;

__EXIT:
   // RK_MPI_AO_WaitEos(ctx->s32DevId, ctx->s32ChnIndex, s32MilliSec);
   RK_MPI_MB_ReleaseMB(frame.pMbBlk);
    return RK_NULL;
}


static RK_S32 init_mpi_ao() {
    RK_S32 i = 0;
    RK_S32 iRet = 0;
    TEST_AO_CTX_S params[AO_MAX_CHN_NUM];
    if (ctx == RK_NULL) {
        RK_LOGE("ctx is null, please init first\n");
        return RK_FAILURE;
    }
    if (ctx->s32ChnNum > AO_MAX_CHN_NUM) {
        RK_LOGE("ao chn(%d) > max_chn(%d)", ctx->s32ChnNum, AO_MAX_CHN_NUM);
        goto __FAILED;
    }

    if (open_device_ao(ctx) != RK_SUCCESS) {
        goto __FAILED;
    }
    RK_MPI_AO_SetVolume(ctx->s32DevId, 100);

    for (i = 0; i < ctx->s32ChnNum; i++) {
        memcpy(&(params[i]), ctx, sizeof(TEST_AO_CTX_S));
        params[i].s32ChnIndex = i;

        iRet = init_mpi_ao_(&params[i]);

        if (iRet != RK_SUCCESS) {
            RK_LOGE("init_mpi_ao_ failed, ret = %d", iRet);
            goto __FAILED;
        }
    }

    return RK_SUCCESS;
__FAILED:
    for (i = 0; i < ctx->s32ChnNum; i++) {
        deinit_mpi_ao(ctx->s32DevId, i);
    }
    close_device_ao(ctx);
    RK_MPI_SYS_Exit();
    if (ctx) {
        free(ctx);
        ctx = RK_NULL;
    }
    RK_LOGE("init_mpi_ao failed, ret = %d", iRet);
    return RK_FAILURE;
}


int init_ao(int s32DevId, int s32ChnNum, int s32SampleRate, int s32BitWidth,int s32ReSmpSampleRate) {
    RK_S32          s32Ret;
    ctx = reinterpret_cast<TEST_AO_CTX_S *>(malloc(sizeof(TEST_AO_CTX_S)));
    memset(ctx, 0, sizeof(TEST_AO_CTX_S));
    ctx->srcFilePath        = NULL;
    ctx->dstFilePath        = NULL;
    ctx->s32LoopCount       = 1;
    ctx->s32ChnNum          = s32ChnNum; //2
    ctx->s32SampleRate      = s32SampleRate; //48k
    ctx->s32ReSmpSampleRate = s32ReSmpSampleRate;//48k
    ctx->s32DeviceChannel   = 2;
    ctx->s32Channel         = 2;
    ctx->s32BitWidth        = s32BitWidth; //16
    ctx->u32FrmCount        = 4;
    //ctx->u32FrmSize         = 1024;
    ctx->u32PeriodCount     = 4;
    ctx->u32PeriodSize      = 1024;
    ctx->chCardName         = "hw:3,0";
    ctx->s32DevId           = s32DevId;  //3
    RK_MPI_SYS_Init();
    s32Ret = init_mpi_ao();
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("init_mpi_ao failed, ret = %d", s32Ret);
        goto __FAILED;
    }
    RK_LOGI("init_mpi_ao success\n");
    return s32Ret;
__FAILED:
    if (ctx) {
        free(ctx);
        ctx = RK_NULL;
    }
    RK_MPI_SYS_Exit();
    RK_LOGE("init_mpi_ao failed, ret = %d", s32Ret);
    return s32Ret;
}

int deinit_ao() {

    for (int i = 0; i < ctx->s32ChnNum; i++) {
        deinit_mpi_ao(ctx->s32DevId, ctx->s32ChnIndex);
    }
    close_device_ao(ctx);
    RK_MPI_SYS_Exit();
    if (ctx) {
        free(ctx);
        ctx = RK_NULL;
    }
    return 0;
}

