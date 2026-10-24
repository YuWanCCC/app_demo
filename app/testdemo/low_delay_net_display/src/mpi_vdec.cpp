/*
 * Copyright 2018 Rockchip Electronics Co. LTD
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

#include <cstdio>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/types.h>
#include "rk_debug.h"
#include "rk_mpi_vdec.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_cal.h"

#include "test_comm_argparse.h"
#include "test_comm_utils.h"
#include "test_comm_ffm.h"
#include "mpi_vdec.h"
#include "mpi_vo.h"

#define   ENABLE_4K   1

extern "C" {
#include "libavformat/avformat.h"
#include "libavformat/version.h"
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
}

#define MAX_STREAM_CNT               128
#define MAX_TIME_OUT_MS              500

#ifndef VDEC_INT64_MIN
#define VDEC_INT64_MIN               (-0x7fffffffffffffffLL-1)
#endif

#ifndef VDEC_INT64_MAX
#define VDEC_INT64_MAX               INT64_C(9223372036854775807)
#endif

typedef struct _rkMpiVDECCtx
{
    const char *srcFileUri;
    const char *dstFilePath;
    RK_U32 u32SrcWidth;
    RK_U32 u32SrcHeight;
    RK_S32 s32LoopCount;
    RK_U32 u32ChnIndex;
    RK_U32 u32ChNum;
    RK_U32 u32InputMode;
    RK_CODEC_ID_E enCodecId;
    RK_U32 u32CompressMode;
    RK_U32 u32FrameBufferCnt;
    pthread_t pVdecThread;
    pthread_t pGetPicThread;
    AVFormatContext *avfc;
    RK_U32 u32StreamIndex;
    void  *pExtraData;
    RK_U32 u32ExtraDataSize;
    RK_U32 u32ReadSize;
    RK_BOOL threadExit;
    RK_S32 s32ChnFd;
    RK_BOOL bEnableMbPool;
    MB_POOL s32Pool;
    RK_S32 s32OutputPixFmt;
    RK_BOOL bEnableDei;
    RK_BOOL bEnableColmv;
    VDECDATA func;
    VDECDATA2 func2;
} TEST_VDEC_CTX_S;

TEST_VDEC_CTX_S *g_vdecctx = NULL;

static void dump_frame_to_file(VIDEO_FRAME_INFO_S *pstFrame, FILE *fp)
{
    RK_U32 i;
    RK_U32 width    = 0;
    RK_U32 height   = 0;
    RK_U32 h_stride = 0;
    RK_U32 v_stride = 0;
    RK_U8 *base_y = RK_NULL;
    RK_U8 *base_c = RK_NULL;
    RK_U8 *base = RK_NULL;

    if (NULL == fp)
        return;

    base = (RK_U8 *)RK_MPI_MB_Handle2VirAddr(pstFrame->stVFrame.pMbBlk);
    RK_MPI_SYS_MmzFlushCache(pstFrame->stVFrame.pMbBlk, RK_TRUE);
    width = pstFrame->stVFrame.u32Width;
    height = pstFrame->stVFrame.u32Height;
    h_stride = pstFrame->stVFrame.u32VirWidth;
    v_stride = pstFrame->stVFrame.u32VirHeight;
    switch (pstFrame->stVFrame.enPixelFormat)
    {
    case RK_FMT_YUV420SP_VU :
    case RK_FMT_YUV420SP :
    {
        base_y = base;
        base_c = base + h_stride * v_stride;

        for (i = 0; i < height; i++, base_y += h_stride)
        {
            fwrite(base_y, 1, width, fp);
        }
        for (i = 0; i < height / 2; i++, base_c += h_stride)
        {
            fwrite(base_c, 1, width, fp);
        }
    }
    break;

    case RK_FMT_YUV420SP_10BIT :
    {
        base_y = base;
        base_c = base + h_stride * v_stride;

        for (i = 0; i < height; i++, base_y += h_stride)
        {
            fwrite(base_y, 1, width, fp);
        }
        for (i = 0; i < height / 2; i++, base_c += h_stride)
        {
            fwrite(base_c, 1, width, fp);
        }
    }
    break;

    case RK_FMT_RGB565:
    case RK_FMT_BGR565:
    {
        base_y = base;

        for (i = 0; i < height; i++, base_y += h_stride)
            fwrite(base_y, 1, width * 2, fp);
    }
    break;
    case RK_FMT_RGB888:
    case RK_FMT_BGR888:
    {
        base_y = base;

        for (i = 0; i < height; i++, base_y += h_stride)
            fwrite(base_y, 1, width * 3, fp);
    }
    break;
    default :
    {
        RK_LOGE("not supported format %d\n", pstFrame->stVFrame.enPixelFormat);
    }
    break;
    }
}

RK_S32 mpi_vdec_poll_event(RK_S32 timeoutMsec, RK_S32 fd)
{
    RK_S32 num_fds = 1;
    struct pollfd pollFds[num_fds];
    RK_S32 ret = 0;

    RK_ASSERT(fd > 0);
    memset(pollFds, 0, sizeof(pollFds));
    pollFds[0].fd = fd;
    pollFds[0].events = (POLLPRI | POLLIN | POLLERR | POLLNVAL | POLLHUP);

    ret = poll(pollFds, num_fds, timeoutMsec);
    if (ret > 0 && (pollFds[0].revents & (POLLERR | POLLNVAL | POLLHUP)))
    {
        RK_LOGE("fd:%d polled error", fd);
        return -1;
    }

    return ret;
}

RK_S32 mpi_create_vdec(TEST_VDEC_CTX_S *ctx, RK_S32 s32Ch, VIDEO_MODE_E enMode, int enSlice)
{
    RK_S32 s32Ret = RK_SUCCESS;
    VDEC_CHN_ATTR_S stAttr;
    VDEC_CHN_PARAM_S stVdecParam;
    MB_POOL_CONFIG_S stMbPoolCfg;
    VDEC_PIC_BUF_ATTR_S stVdecPicBufAttr;
    MB_PIC_CAL_S stMbPicCalResult;
    VDEC_MOD_PARAM_S stModParam;

    memset(&stAttr, 0, sizeof(VDEC_CHN_ATTR_S));
    memset(&stVdecParam, 0, sizeof(VDEC_CHN_PARAM_S));
    memset(&stModParam, 0, sizeof(VDEC_MOD_PARAM_S));
    memset(&stVdecPicBufAttr, 0, sizeof(VDEC_PIC_BUF_ATTR_S));
    memset(&stMbPicCalResult, 0, sizeof(MB_PIC_CAL_S));

    if (ctx->bEnableMbPool)
    {
        stModParam.enVdecMBSource = MB_SOURCE_USER;
        s32Ret = RK_MPI_VDEC_SetModParam(&stModParam);
        if (s32Ret != RK_SUCCESS)
        {
            RK_LOGE("vdec %d SetModParam failed! errcode %x", ctx->u32ChnIndex, s32Ret);
            return s32Ret;
        }
    }
    stVdecPicBufAttr.enCodecType = ctx->enCodecId;
    stVdecPicBufAttr.stPicBufAttr.u32Width = ctx->u32SrcWidth;
    stVdecPicBufAttr.stPicBufAttr.u32Height = ctx->u32SrcHeight;
    stVdecPicBufAttr.stPicBufAttr.enPixelFormat = (PIXEL_FORMAT_E)ctx->s32OutputPixFmt;
    stVdecPicBufAttr.stPicBufAttr.enCompMode = (COMPRESS_MODE_E)ctx->u32CompressMode;

    s32Ret = RK_MPI_CAL_VDEC_GetPicBufferSize(&stVdecPicBufAttr, &stMbPicCalResult);
    ctx->bEnableColmv = RK_FALSE;
    //printf(">>>>>>>enCodecType=%d,w=%d,h=%d,format=(%d,%d),mode=%d\n",\
    //         ctx->enCodecId,ctx->u32SrcWidth,ctx->u32SrcHeight,ctx->s32OutputPixFmt,RK_FMT_YUV420SP,ctx->u32CompressMode);
    if (s32Ret != RK_SUCCESS)
    {
        RK_LOGE(">>>>>>>>get picture buffer size failed. err 0x%x", s32Ret);
        return s32Ret;
    }

    stAttr.enMode = enMode;
    stAttr.enType = ctx->enCodecId;
    stAttr.u32PicWidth = ctx->u32SrcWidth;
    stAttr.u32PicHeight = ctx->u32SrcHeight;
    if (enSlice > 0) 
        stAttr.stVdecVideoAttr.u32LowDelay = enSlice;

    if (ctx->bEnableDei)
    {
        // iep must remain 5 buffers for deinterlace
        stAttr.u32FrameBufCnt = ctx->u32FrameBufferCnt + 5;
    }
    else
    {
        stAttr.u32FrameBufCnt = ctx->u32FrameBufferCnt;
    }
    stAttr.u32StreamBufCnt = MAX_STREAM_CNT;
    /*
     * if decode 10bit stream, need specify the u32FrameBufSize,
     * other conditions can be set to 0, calculated internally.
     */
    stAttr.u32FrameBufSize = stMbPicCalResult.u32MBSize;

    if (!ctx->bEnableColmv)
    {
        stAttr.stVdecVideoAttr.bTemporalMvpEnable = RK_FALSE;
    }

    s32Ret = RK_MPI_VDEC_CreateChn(ctx->u32ChnIndex, &stAttr);
    if (s32Ret != RK_SUCCESS)
    {
        RK_LOGE("create %d vdec failed! ", ctx->u32ChnIndex);
        return s32Ret;
    }

    if (ctx->enCodecId == RK_VIDEO_ID_MJPEG)
    {
        stVdecParam.stVdecPictureParam.enPixelFormat = (PIXEL_FORMAT_E)ctx->s32OutputPixFmt;
    }
    else
    {
        stVdecParam.stVdecVideoParam.enCompressMode = (COMPRESS_MODE_E)ctx->u32CompressMode;
    }
    if (ctx->bEnableDei)
    {
        stVdecParam.stVdecVideoParam.bDeiEn = ctx->bEnableDei;
    }
    // it is only effective to disable MV when decoding sequence output
    if (!ctx->bEnableColmv)
    {
        stVdecParam.stVdecVideoParam.enOutputOrder = VIDEO_OUTPUT_ORDER_DEC;
    }

    RK_MPI_VDEC_SetDisplayMode(0, VIDEO_DISPLAY_MODE_PREVIEW);

    s32Ret = RK_MPI_VDEC_SetChnParam(ctx->u32ChnIndex, &stVdecParam);
    if (s32Ret != RK_SUCCESS)
    {
        RK_LOGE("set chn %d param failed %x! ", ctx->u32ChnIndex, s32Ret);
        return s32Ret;
    }

    ctx->s32ChnFd = RK_MPI_VDEC_GetFd(ctx->u32ChnIndex);
    if (ctx->s32ChnFd <= 0)
    {
        RK_LOGE("get fd chn %d failed %d", ctx->u32ChnIndex, ctx->s32ChnFd);
        return s32Ret;
    }

    if (ctx->bEnableMbPool)
    {
        memset(&stMbPoolCfg, 0, sizeof(MB_POOL_CONFIG_S));
        stMbPoolCfg.u64MBSize = stMbPicCalResult.u32MBSize;
        stMbPoolCfg.u32MBCnt  = 10;
        stMbPoolCfg.enRemapMode = MB_REMAP_MODE_CACHED;
        stMbPoolCfg.bPreAlloc = RK_TRUE;
        ctx->s32Pool = RK_MPI_MB_CreatePool(&stMbPoolCfg);
        if (ctx->s32Pool == MB_INVALID_POOLID)
        {
            RK_LOGE("create pool failed!");
            return s32Ret;
        }
        s32Ret = RK_MPI_VDEC_AttachMbPool(ctx->u32ChnIndex, ctx->s32Pool);
        if (s32Ret != RK_SUCCESS)
        {
            RK_LOGE("attatc vdec mb pool %d failed! ", ctx->u32ChnIndex);
            return s32Ret;
        }
    }

    s32Ret = RK_MPI_VDEC_StartRecvStream(ctx->u32ChnIndex);
    if (s32Ret != RK_SUCCESS)
    {
        RK_LOGE("start recv chn %d failed %x! ", ctx->u32ChnIndex, s32Ret);
        return s32Ret;
    }

    return RK_SUCCESS;
}

RK_S32 mpi_destory_vdec(TEST_VDEC_CTX_S *ctx, RK_S32 s32Ch)
{
    RK_MPI_VDEC_StopRecvStream(s32Ch);

    if (ctx->s32ChnFd > 0)
    {
        RK_MPI_VDEC_CloseFd(s32Ch);
    }

    if (ctx->bEnableMbPool)
    {
        RK_MPI_VDEC_DetachMbPool(s32Ch);
        RK_MPI_VDEC_DestroyChn(s32Ch);
        RK_MPI_MB_DestroyPool(ctx->s32Pool);
    }
    else
    {
        RK_MPI_VDEC_DestroyChn(s32Ch);
    }

    if (ctx->avfc)
        avformat_close_input(&(ctx->avfc));
    return RK_SUCCESS;
}

int init_vdec(VDECDATA func, VDECDATA2 func2, int width, int height, int enSlice)
{

    RK_S32 s32Ret = RK_SUCCESS;
    g_vdecctx = (TEST_VDEC_CTX_S *)malloc(sizeof(TEST_VDEC_CTX_S));
    memset(g_vdecctx, 0, sizeof(TEST_VDEC_CTX_S));

    g_vdecctx->u32InputMode = VIDEO_MODE_FRAME;
    g_vdecctx->s32LoopCount = 1;
    g_vdecctx->u32CompressMode = COMPRESS_MODE_NONE;  // Suggest::COMPRESS_AFBC_16x16;
    g_vdecctx->u32FrameBufferCnt = 8;
    g_vdecctx->u32ReadSize = 1024;
    g_vdecctx->u32ChNum = 1;
    g_vdecctx->bEnableColmv = RK_TRUE;
    g_vdecctx->s32OutputPixFmt = (RK_S32)RK_FMT_YUV420SP;
    g_vdecctx->u32ChnIndex = 0;
    g_vdecctx->func = func;
    g_vdecctx->func2 = func2;
    
    g_vdecctx->u32SrcWidth = width;//3840;
    g_vdecctx->u32SrcHeight = height;//2160;

    g_vdecctx->enCodecId = RK_VIDEO_ID_HEVC;//RK_VIDEO_ID_AVC;
    g_vdecctx->dstFilePath = 0;//"/data/dumpyuv/";
    if (RK_MPI_SYS_Init() != RK_SUCCESS)
    {
        goto __FAILED;
    }

    s32Ret = mpi_create_vdec(g_vdecctx, g_vdecctx->u32ChnIndex, VIDEO_MODE_FRAME, enSlice);
    if (s32Ret != RK_SUCCESS)
    {
        goto __FAILED;
    }

    g_vdecctx->s32ChnFd = RK_MPI_VDEC_GetFd(g_vdecctx->u32ChnIndex);
    printf("%s,%d", __FUNCTION__, __LINE__);
    return 0;

__FAILED:
    RK_LOGE("test running failed! %d count running done not yet.", g_vdecctx->s32LoopCount);
    return RK_FAILURE;
}
static int seqid = 0;
int vdec_send_frame(unsigned char *data, int len, int slice_end)
{
    //printf("%s,%d,%p,len=%d\n", __FUNCTION__, __LINE__,data,len);
    RK_S32 s32Size = 0;
    RK_S32 s32Ret = 0;
    VDEC_STREAM_S stStream;
    MB_BLK buffer = RK_NULL;
    int s32PacketCount = 0;
    MB_EXT_CONFIG_S pstMbExtConfig;
    memset(&pstMbExtConfig, 0, sizeof(MB_EXT_CONFIG_S));
    pstMbExtConfig.pFreeCB = NULL;
    pstMbExtConfig.pOpaque = data;
    pstMbExtConfig.pu8VirAddr = data;
    pstMbExtConfig.u64Size = len;
    RK_MPI_SYS_CreateMB(&buffer, &pstMbExtConfig);
//   printf("%s,%d\n", __FUNCTION__, __LINE__);
    stStream.u64PTS = seqid++;
    stStream.pMbBlk = buffer;
    stStream.u32Len = len;
    stStream.bEndOfStream = RK_FALSE;
    stStream.bEndOfFrame = RK_FALSE;
    stStream.bBypassMbBlk = RK_FALSE;
    stStream.bEndOfFrame = (RK_BOOL)slice_end;
    g_vdecctx->u32ChnIndex = 0;
__RETRY:
    //RK_LOGI("%s,%d,pts=%d\n", __FUNCTION__, __LINE__, stStream.u64PTS);
    // printf("=======slice_end=%d\n",slice_end);
    s32Ret = RK_MPI_VDEC_SendStream(g_vdecctx->u32ChnIndex, &stStream, MAX_TIME_OUT_MS);
    // printf("%s,%d\n", __FUNCTION__, __LINE__);
    if (s32Ret < 0)
    {
        usleep(1000llu);
        goto  __RETRY;
    }
    else
    {
        RK_MPI_MB_ReleaseMB(stStream.pMbBlk);
        s32PacketCount++;
        //RK_LOGI("send chn %d packet %d", g_vdecctx->u32ChnIndex, s32PacketCount);
    }
    //  printf("%s,%d\n", __FUNCTION__, __LINE__);
    return 0;
}
static int frame_cout = 0;
void thread_vdec_get_frame(int flag)
{

    VIDEO_FRAME_INFO_S sFrame;
    RK_S32 s32Ret;
    char name[256] = {0};
    RK_S32 s32FrameCount = 0;
    FILE *fp = NULL;
    memset(&sFrame, 0, sizeof(VIDEO_FRAME_INFO_S));
    if (g_vdecctx->dstFilePath != RK_NULL)
    {
        mkdir(g_vdecctx->dstFilePath, 0777);
        snprintf(name, sizeof(name), "%stest_%d.bin", g_vdecctx->dstFilePath, g_vdecctx->u32ChnIndex);
        fp = fopen(name, "wb");
        if (fp == RK_NULL)
        {
            RK_LOGE("can't open output file %s\n", name);
            return ;
        }
    }
    while (true)
    {
        // printf("%s,%d\n", __FUNCTION__, __LINE__);
        if (g_vdecctx->s32ChnFd > 0)
        {
            s32Ret = mpi_vdec_poll_event(-1, g_vdecctx->s32ChnFd);
            if (s32Ret < 0)
            {
                usleep(1000llu);
                continue;
            }
        }
        s32Ret = RK_MPI_VDEC_GetFrame(g_vdecctx->u32ChnIndex, &sFrame, MAX_TIME_OUT_MS);
        if (s32Ret >= 0)
        {
            s32FrameCount++;
            // RK_LOGI("get chn %d frame %d,pts=%lld", g_vdecctx->u32ChnIndex, s32FrameCount,sFrame.stVFrame.u64PTS);
            if ((sFrame.stVFrame.u32FrameFlag & FRAME_FLAG_SNAP_END) == FRAME_FLAG_SNAP_END)
            {
                RK_MPI_VDEC_ReleaseFrame(g_vdecctx->u32ChnIndex, &sFrame);
                RK_LOGI("chn %d reach eos frame.", g_vdecctx->u32ChnIndex);
                break;
            }
            //printf("%s,%d,width=%d\n", __FUNCTION__, __LINE__,sFrame.stVFrame.u32VirWidth);
            int bufferFd = RK_MPI_MB_Handle2Fd(sFrame.stVFrame.pMbBlk);
            // g_vdecctx->func(bufferFd);
            g_vdecctx->func2(sFrame.stVFrame.u32VirWidth, sFrame.stVFrame.u32VirHeight, \
                             sFrame.stVFrame.u32Width, sFrame.stVFrame.u32Height, \
                             (void *)sFrame.stVFrame.pMbBlk);
            frame_cout++;
            if (frame_cout < 3)
            {
                // dump_frame_to_file(&sFrame, fp);
            }
            RK_MPI_VDEC_ReleaseFrame(g_vdecctx->u32ChnIndex, &sFrame);
        }
        else
        {
            usleep(1000llu);
        }
        //  printf("%s,%d\n", __FUNCTION__, __LINE__);
    }
    if (fp)
        fclose(fp);
    RK_LOGI("%s out", __FUNCTION__);
    return ;
}

int vdec_deinit()
{
    RK_MPI_SYS_Exit();
    return 0;
}

