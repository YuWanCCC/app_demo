#include <stdio.h>
#include <errno.h>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <pthread.h>
#include <sys/poll.h>
#include "rk_defines.h"
#include "rk_debug.h"
#include "mpi_ai.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_mb.h"
#include "rk_comm_aio.h"
#include "rk_mpi_ai.h"
#define DUMP_PCM_PATH  "/data/dump.pcm"

static RK_BOOL gAiExit = RK_FALSE;
#define TEST_AI_WITH_FD 0

typedef struct _rkMpiAICtx {
    const char *srcFilePath;
    const char *dstFilePath;
    RK_S32      s32LoopCount;
    RK_S32      s32ChnNum;
    RK_S32      s32DeviceSampleRate;
    RK_S32      s32SampleRate;
    RK_S32      s32DeviceChannel;
    RK_S32      s32Channel;
    RK_S32      s32BitWidth;
    RK_S32      s32DevId;
    RK_S32      s32FrameNumber;
    RK_S32      s32FrameLength;
    char       *chCardName;
    RK_S32      s32ChnIndex;
    RK_S32      s32DevFd;
    RK_U32      u32PeriodCount;
    RK_U32      u32PeriodSize;
    RK_U32      u32StartDelay;
    RK_U32      u32StopDelay;
    AIDATA      func;
    FILE        *fp;
} TEST_AI_CTX_S;

static TEST_AI_CTX_S  *ctx = RK_NULL;
static AUDIO_SOUND_MODE_E ai_find_sound_mode(RK_S32 ch) {
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

static AUDIO_BIT_WIDTH_E ai_find_bit_width(RK_S32 bit) {
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

static RK_S32 ai_poll_event(RK_S32 timeoutMsec, RK_S32 fd) {
    RK_S32 num_fds = 1;
    struct pollfd pollFds[num_fds];
    RK_S32 ret = 0;

    RK_ASSERT(fd > 0);
    memset(pollFds, 0, sizeof(pollFds));
    pollFds[0].fd = fd;
    pollFds[0].events = (POLLPRI | POLLIN | POLLERR | POLLNVAL | POLLHUP);

    ret = poll(pollFds, num_fds, timeoutMsec);

    if (ret > 0 && (pollFds[0].revents & (POLLERR | POLLNVAL | POLLHUP))) {
        RK_LOGE("fd:%d polled error", fd);
        return -1;
    }

    return ret;
}

static RK_S32 open_device_ai() {
    AUDIO_DEV aiDevId = ctx->s32DevId;
    AUDIO_SOUND_MODE_E soundMode;

    AIO_ATTR_S aiAttr;
    AIO_ADVPARAM_S aiAdvParam;
    RK_S32 result;
    memset(&aiAttr, 0, sizeof(AIO_ATTR_S));
    memset(&aiAdvParam, 0, sizeof(AIO_ADVPARAM_S));

    if (ctx->chCardName) {
        snprintf(reinterpret_cast<char *>(aiAttr.u8CardName),
                 sizeof(aiAttr.u8CardName), "%s", ctx->chCardName);
    }

    aiAttr.soundCard.channels = ctx->s32DeviceChannel;
    aiAttr.soundCard.sampleRate = ctx->s32DeviceSampleRate;
    aiAttr.soundCard.bitWidth = AUDIO_BIT_WIDTH_16;

    AUDIO_BIT_WIDTH_E bitWidth = ai_find_bit_width(ctx->s32BitWidth);
    if (bitWidth == AUDIO_BIT_WIDTH_BUTT) {
        goto __FAILED;
    }
    aiAttr.enBitwidth = bitWidth;
    aiAttr.enSamplerate = (AUDIO_SAMPLE_RATE_E)ctx->s32SampleRate;
    soundMode = ai_find_sound_mode(ctx->s32Channel);
    if (soundMode == AUDIO_SOUND_MODE_BUTT) {
        goto __FAILED;
    }
    aiAttr.enSoundmode = soundMode;
    aiAttr.u32FrmNum = ctx->s32FrameNumber;
    aiAttr.u32PtNumPerFrm = ctx->s32FrameLength;
    aiAttr.u32EXFlag = 0;
    aiAttr.u32ChnCnt = 2;

    result = RK_MPI_AI_SetPubAttr(aiDevId, &aiAttr);
    if (result != 0) {
        RK_LOGE("ai set attr fail, reason = %d", result);
        goto __FAILED;
    }
    aiAdvParam.u32PeriodSize = ctx->u32PeriodSize;
    aiAdvParam.u32PeriodCount = ctx->u32PeriodCount;
    aiAdvParam.u32StartDelay = ctx->u32StartDelay;
    aiAdvParam.u32StopDelay = ctx->u32StopDelay;
    RK_MPI_AI_SetAdvParam(aiDevId, &aiAdvParam);

    result = RK_MPI_AI_Enable(aiDevId);
    if (result != 0) {
        RK_LOGE("ai enable fail, reason = %d", result);
        goto __FAILED;
    }

    return RK_SUCCESS;
__FAILED:
    return RK_FAILURE;
}

static RK_S32 init_mpi_ai(TEST_AI_CTX_S *params) {
    RK_S32 result;

    result =  RK_MPI_AI_EnableChn(params->s32DevId, params->s32ChnIndex);
    if (result != 0) {
        RK_LOGE("ai enable channel fail, aoChn = %d, reason = %x", params->s32ChnIndex, result);
        return RK_FAILURE;
    }

#if TEST_AI_WITH_FD
    // open fd immediate after enable chn will be better.
    params->s32DevFd = RK_MPI_AI_GetFd(params->s32DevId, params->s32ChnIndex);
    RK_LOGI("ai (devId: %d, chnId: %d), selectFd:%d", params->s32DevId, params->s32ChnIndex, params->s32DevFd);
#endif

    result = RK_MPI_AI_EnableReSmp(params->s32DevId, params->s32ChnIndex,
                                  (AUDIO_SAMPLE_RATE_E)params->s32SampleRate);
    if (result != 0) {
        RK_LOGE("ai enable channel fail, reason = %x, aoChn = %d", result, params->s32ChnIndex);
        return RK_FAILURE;
    }
#ifdef DUMP_PCM_PATH
    params->fp = fopen(DUMP_PCM_PATH, "wb+");
    if (params->fp == RK_NULL) {
        RK_LOGE("open dump file %s failed", DUMP_PCM_PATH);
        return RK_FAILURE;
    }
#endif

    return RK_SUCCESS;
}

static RK_S32 deinit_mpi_ai(TEST_AI_CTX_S *params) {
    RK_MPI_AI_DisableReSmp(params->s32DevId, params->s32ChnIndex);
    RK_S32 result = RK_MPI_AI_DisableChn(params->s32DevId, params->s32ChnIndex);
    if (result != 0) {
        RK_LOGE("ai disable channel fail, reason = %d", result);
        return RK_FAILURE;
    }

    result =  RK_MPI_AI_Disable(params->s32DevId);
    if (result != 0) {
        RK_LOGE("ai disable fail, reason = %d", result);
        return RK_FAILURE;
    }

    return RK_SUCCESS;
}

void getDataThread() {
    printf(">>>enter getDataThread,%s,%d\n", __FUNCTION__, __LINE__);
    RK_S32 result = 0;
    RK_S32 s32MilliSec = -1;
    AUDIO_FRAME_S frame;
    TEST_AI_CTX_S  *ctx_ = ctx;
    if (ctx_ == RK_NULL) {
        RK_LOGE("ctx is null, please init first\n");
        return;
    }
    while (!gAiExit) {
#if TEST_AI_WITH_FD
        ai_poll_event(-1, ctx_->s32DevFd);
#endif
        result = RK_MPI_AI_GetFrame(ctx_->s32DevId, ctx_->s32ChnIndex, &frame, RK_NULL, s32MilliSec);
        if (result == 0) {
            void* data = RK_MPI_MB_Handle2VirAddr(frame.pMbBlk);
            // get the length of valid data in this frame
            RK_U64 len = RK_MPI_MB_GetLength(frame.pMbBlk);
            /*if (ctx_->fp != NULL)
               fwrite(data, len, 1, ctx_->fp);*/
            ctx_->func(reinterpret_cast<unsigned char *>(data), len);
            RK_LOGV("AI GetFrame: %d, %d, %d", ctx_->s32DevId, ctx_->s32ChnIndex, frame.u32Len);
            RK_MPI_AI_ReleaseFrame(ctx_->s32DevId, ctx_->s32ChnIndex, &frame, RK_NULL);
        }
    }

    return RK_NULL;
}

int init_ai(int channel, int sampleRate, int bitWidth, int devId, int frame_time_ms, AIDATA func) {
    
    RK_S32          i;
    RK_S32          s32Ret;
    ctx = reinterpret_cast<TEST_AI_CTX_S *>(malloc(sizeof(TEST_AI_CTX_S)));
    memset(ctx, 0, sizeof(TEST_AI_CTX_S));
    ctx->s32DeviceSampleRate = sampleRate;
    ctx->s32DeviceChannel   = channel;
    ctx->s32SampleRate      = sampleRate;
    ctx->s32Channel         = channel;
    ctx->s32BitWidth       = bitWidth;
    ctx->s32DevId           = devId;
    ctx->chCardName = "hw:1,0";
    ctx->func = func;
    ctx->s32FrameLength = sampleRate*(frame_time_ms/1000.0f)*channel*(bitWidth/8); // 16ms
    RK_MPI_SYS_Init();
    
    s32Ret = open_device_ai();
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("test_open_device_ai failed, ret = %d", s32Ret);
        goto __FAILED;
    }

    s32Ret = init_mpi_ai(ctx);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("init_mpi_ai failed, ret = %d", s32Ret);
        goto __FAILED;
    }
    printf("init_mpi_ai success\n");
    return s32Ret;
__FAILED:
    deinit_mpi_ai(ctx);
    RK_MPI_SYS_Exit();
    if (ctx) {
        free(ctx);
        ctx = RK_NULL;
    }
    return s32Ret;
}

int deinit_ai() {
    printf("deinit_ai\n");
    deinit_mpi_ai(ctx);
    RK_MPI_SYS_Exit();
    if (ctx) {
        free(ctx);
        ctx = RK_NULL;
    }
    gAiExit = RK_TRUE;
    return 0;
}


