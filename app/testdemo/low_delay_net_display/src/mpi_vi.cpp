#include <cstdio>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <linux/input.h>
#include <fcntl.h>
#include <sys/time.h>
#include <map>
#include "rk_debug.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_vo.h"
#include "rk_mpi_mb.h"
#include "rk_common.h"
#include "pthread.h"
#include "mpi_vi.h"
#include "rk_comm_venc.h"
#include "rk_comm_vi.h"
#include "rk_mpi_vi.h"
#define TEST_VENC_MAX 2
typedef struct _rkTestVencCfg
{
    RK_BOOL bOutDebugCfg;
    VENC_CHN_ATTR_S stAttr;
    RK_CHAR dstFilePath[128];
    RK_CHAR dstFileName[128];
    RK_S32 s32ChnId;
    FILE *fp;
    RK_S32 selectFd;
} TEST_VENC_CFG;
/*
typedef struct rkVPSS_CFG_S {
    const char *dstFilePath;
    RK_S32 s32DevId;
    RK_S32 s32ChnId;
    RK_U32 u32VpssChnCnt;
    VPSS_GRP_ATTR_S stGrpVpssAttr;
    VPSS_CHN_ATTR_S stVpssChnAttr[VPSS_MAX_CHN_NUM];
} VPSS_CFG_S;

typedef struct rkRGN_CFG_S {
    RGN_ATTR_S stRgnAttr;
    RGN_CHN_ATTR_S stRgnChnAttr;
} RGN_CFG_S;
*/
typedef enum rkTestVIMODE_E
{
    TEST_VI_MODE_VI_FRAME_ONLY = 0,
    TEST_VI_MODE_BIND_VENC = 1,
    TEST_VI_MODE_BIND_VENC_MULTI = 2,
    TEST_VI_MODE_BIND_VPSS_BIND_VENC = 3,
    TEST_VI_MODE_BIND_VO = 4,
    TEST_VI_MODE_MUTI_VI = 5,
    TEST_VI_MODE_VI_STREAM_ONLY = 6,
    TEST_VI_MODE_BIND_VDEC_BIND_VO = 7,
} TEST_VI_MODE_E;

typedef struct _rkMpiVICtx
{
    RK_S32 width;
    RK_S32 height;
    RK_S32 devId;
    RK_S32 pipeId;
    RK_S32 channelId;
    RK_S32 loopCountSet;
    RK_S32 selectFd;
    RK_BOOL bFreeze;
    RK_BOOL bEnRgn;
    RK_S32 s32RgnCnt;
    RK_S32 rgnType;
    RK_BOOL bUserPicEnabled;
    RK_BOOL bGetConnecInfo;
    RK_BOOL bGetEdid;
    RK_BOOL bSrcChange;
    RK_BOOL bSetEdid;
    COMPRESS_MODE_E enCompressMode;
    VI_DEV_ATTR_S stDevAttr;
    VI_DEV_BIND_PIPE_S stBindPipe;
    VI_CHN_ATTR_S stChnAttr;
    VI_SAVE_FILE_INFO_S stDebugFile;
    VIDEO_FRAME_INFO_S stViFrame;
    VI_CHN_STATUS_S stChnStatus;
    VI_USERPIC_ATTR_S stUsrPic;
    TEST_VI_MODE_E enMode;
    const char *aEntityName;
    // for vi
    //RGN_CFG_S stViRgn;
    // for venc
    TEST_VENC_CFG stVencCfg[TEST_VENC_MAX];
    VENC_STREAM_S stFrame[TEST_VENC_MAX];
    // VPSS_CFG_S stVpssCfg;
    // for vo
    VO_LAYER s32VoLayer;
    VO_DEV s32VoDev;
    // for stream
    RK_CODEC_ID_E enCodecId;
} TEST_VI_CTX_S;

static TEST_VI_CTX_S *ctx = NULL;


RK_U64 TEST_COMM_GetNowUs()
{
    struct timespec time = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (RK_U64)time.tv_sec * 1000000 + (RK_U64)time.tv_nsec / 1000; /* microseconds */
}


RK_S32 vi_init(int width, int height, int devId, const char *entityName, int format)
{

    RK_S32 i;
    RK_S32 s32Ret = RK_FAILURE;
    ctx = reinterpret_cast<TEST_VI_CTX_S *>(malloc(sizeof(TEST_VI_CTX_S)));
    memset(ctx, 0, sizeof(TEST_VI_CTX_S));

    ctx->width = width;
    ctx->height = height;
    ctx->devId = devId;
    ctx->pipeId = ctx->devId;
    ctx->enCodecId = RK_VIDEO_ID_HEVC;
    ctx->channelId = 0;
    ctx->enMode = TEST_VI_MODE_BIND_VENC;
    ctx->stChnAttr.stIspOpt.u32BufCount = 3;
    ctx->stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_MMAP;
    ctx->stChnAttr.stIspOpt.enCaptureType = VI_V4L2_CAPTURE_TYPE_VIDEO_CAPTURE;
    ctx->stChnAttr.u32Depth = 3;
    ctx->aEntityName = entityName;
    ctx->stChnAttr.enPixelFormat = (PIXEL_FORMAT_E)format;
    ctx->stChnAttr.stFrameRate.s32SrcFrameRate = -1;
    ctx->stChnAttr.stFrameRate.s32DstFrameRate = -1;
    ctx->bEnRgn = RK_FALSE;
    if (ctx->aEntityName != RK_NULL)
        memcpy(ctx->stChnAttr.stIspOpt.aEntityName, ctx->aEntityName, strlen(ctx->aEntityName));
    if (RK_MPI_SYS_Init() != RK_SUCCESS)
    {
        RK_LOGE("rk mpi sys init fail!");
        goto __FAILED;
    }
    // 0. get dev config status
    if (ctx->devId < VI_MAX_PHY_PIPE_NUM) {
        s32Ret = RK_MPI_VI_GetDevAttr(ctx->devId, &ctx->stDevAttr);
        if (s32Ret == RK_ERR_VI_NOT_CONFIG)
        {
            // 0-1.config dev
            s32Ret = RK_MPI_VI_SetDevAttr(ctx->devId, &ctx->stDevAttr);
            if (s32Ret != RK_SUCCESS)
            {
                RK_LOGE("RK_MPI_VI_SetDevAttr %x", s32Ret);
                goto __FAILED;
            }
        }
        else
        {
            RK_LOGE("RK_MPI_VI_SetDevAttr already");
        }
        // 1.get  dev enable status
        s32Ret = RK_MPI_VI_GetDevIsEnable(ctx->devId);
        if (s32Ret != RK_SUCCESS)
        {
            // 1-2.enable dev
            s32Ret = RK_MPI_VI_EnableDev(ctx->devId);
            if (s32Ret != RK_SUCCESS)
            {
                RK_LOGE("RK_MPI_VI_EnableDev %x", s32Ret);
                goto __FAILED;
            }
            // 1-3.bind dev/pipe
            ctx->stBindPipe.u32Num = ctx->pipeId;
            ctx->stBindPipe.PipeId[0] = ctx->pipeId;
            s32Ret = RK_MPI_VI_SetDevBindPipe(ctx->devId, &ctx->stBindPipe);
            if (s32Ret != RK_SUCCESS)
            {
                RK_LOGE("RK_MPI_VI_SetDevBindPipe %x", s32Ret);
                goto __FAILED;
            }
        }
        else
        {
            RK_LOGE("RK_MPI_VI_EnableDev already");
        }
    } else {
        RK_LOGI("use vi virtual dev !\n");
    }
    // 2.config channel
    ctx->stChnAttr.stSize.u32Width = ctx->width;
    ctx->stChnAttr.stSize.u32Height = ctx->height;
    ctx->stChnAttr.enCompressMode = ctx->enCompressMode;
    s32Ret = RK_MPI_VI_SetChnAttr(ctx->pipeId, ctx->channelId, &ctx->stChnAttr);
    if (s32Ret != RK_SUCCESS)
    {
        RK_LOGE("RK_MPI_VI_SetChnAttr %x", s32Ret);
        goto __FAILED;
    }

    // 3.enable channel
    s32Ret = RK_MPI_VI_EnableChn(ctx->pipeId, ctx->channelId);
    if (s32Ret != RK_SUCCESS)
    {
        RK_LOGE("RK_MPI_VI_EnableChn %x", s32Ret);
        goto __FAILED;
    }

    return s32Ret;

__FAILED:
    return s32Ret;
}


RK_S32 get_frame()
{

    RK_S32 s32Ret = 0;
    RK_S32 loopCount = 0;
    RK_S32 waitTime = 33;
    RK_S64 s64FirstStreamTime = 0;
    RK_S64 s64StartTime;
    RK_S32  buffer_fd;
    if (ctx->enCodecId == RK_VIDEO_ID_Unused)
    {
        RK_LOGE("this mode need set codec id", ctx->enCodecId);
        return RK_FAILURE;
    }
    s64StartTime = TEST_COMM_GetNowUs();

    s32Ret = RK_MPI_VI_GetChnFrame(ctx->pipeId, ctx->channelId, &ctx->stViFrame, 1000);
    if (s32Ret == RK_SUCCESS)
    {
        RK_U64 nowUs = TEST_COMM_GetNowUs();
        if (!s64FirstStreamTime)
        {
            s64FirstStreamTime = nowUs - s64StartTime;
            // RK_LOGE("get first stream use time:%d ms", s64FirstStreamTime / 1000);
        }

        buffer_fd = RK_MPI_MB_Handle2Fd(ctx->stViFrame.stVFrame.pMbBlk);
        //RK_LOGD("++++++++++buffer_fd=%d\n",buffer_fd);
        // RK_LOGI("get okay!\n");
        // 6.get the channel status
        s32Ret = RK_MPI_VI_QueryChnStatus(ctx->pipeId, ctx->channelId, &ctx->stChnStatus);
        /*    RK_LOGD("RK_MPI_VI_QueryChnStatus ret %x, w:%d,h:%d,enable:%d," \
                    "current frame id:%d,input lost:%d,output lost:%d," \
                    "framerate:%d,vbfail:%d delay=%lldus",
                        s32Ret,
                        ctx->stChnStatus.stSize.u32Width,
                        ctx->stChnStatus.stSize.u32Height,
                        ctx->stChnStatus.bEnable,
                        ctx->stChnStatus.u32CurFrameID,
                        ctx->stChnStatus.u32InputLostFrame,
                        ctx->stChnStatus.u32OutputLostFrame,
                        ctx->stChnStatus.u32FrameRate,
                        ctx->stChnStatus.u32VbFail,
                        nowUs - ctx->stViFrame.stVFrame.u64PTS);
         */
        // 7.release the stream
        s32Ret = RK_MPI_VI_ReleaseChnFrame(ctx->pipeId, ctx->channelId, &ctx->stViFrame);
        if (s32Ret != RK_SUCCESS)
        {
            RK_LOGE("RK_MPI_VI_ReleaseChnFrame fail %x", s32Ret);
        }
        return buffer_fd;
    }
    else
    {
        RK_LOGE("RK_MPI_VI_GetChnFrame timeout %x", s32Ret);
    }
    //}

    return s32Ret;
__FAILED:
    // 9. disable one chn
    s32Ret = RK_MPI_VI_DisableChn(ctx->pipeId, ctx->channelId);
    // 10.disable dev(will diabled all chn)
    s32Ret = RK_MPI_VI_DisableDev(ctx->devId);

    return s32Ret;
}


RK_S32 deinit_vi()
{
    RK_S32 s32Ret;
    // 9. disable one chn
    s32Ret = RK_MPI_VI_DisableChn(ctx->pipeId, ctx->channelId);
    // 10.disable dev(will diabled all chn)
    s32Ret = RK_MPI_VI_DisableDev(ctx->devId);
    RK_MPI_SYS_Exit();
    free(ctx);
    return s32Ret;
}


















