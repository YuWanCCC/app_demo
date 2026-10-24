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
#include <sys/types.h>

#include "rk_debug.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_venc.h"
#include "rk_mpi_cal.h"
#include "rt_trace.h"
#include "test_comm_utils.h"
#include "mpi_venc.h"
#define ALIGN(value, align) ((value + align -1) & ~(align-1))
#define MAX_TIME_OUT_MS          20
#define TEST_RC_MODE             0
#define TEST_CROP_PARAM_CHANGE   0
#define DUMP_H264_PATH  "/data/h265_slice.h265"
FILE  *_fp = NULL;
typedef struct _rkMpiVENCCtx
{
    const char     *srcFileUri;
    const char     *dstFilePath;
    RK_U32          u32SrcWidth;
    RK_U32          u32SrcHeight;
    RK_U32          u32srcVirWidth;
    RK_U32          u32srcVirHeight;
    RK_S32          s32LoopCount;
    RK_U32          u32ChnIndex;
    RK_U32          u32ChNum;
    RK_U32          u32SrcPixFormat;
    RK_U32          u32DstCodec;
    RK_U32          u32BufferSize;
    RK_U32          u32StreamSize;
    RK_U32          u32StreamBufCnt;
    RK_U32          u32BitRateKb;
    RK_U32          u32GopSize;
    RK_U32          u32RoiTestCount;
    RK_U32          u32FixQp;
    RK_U32          u32DeBreath;
    RK_S32          s32SnapPicCount;
    RK_BOOL         threadExit;
    RK_BOOL         bFrameRate;
    RK_BOOL         bInsertUserData;
    RK_U32          u32OneStreamBuffer;
    COMPRESS_MODE_E enCompressMode;
    VENC_RC_MODE_E  enRcMode;
    VENC_GOP_MODE_E enGopMode;
    VENC_CROP_TYPE_E enCropType;
    ROTATION_E enRotation;
    MIRROR_E   enMirror;
    RK_BOOL    bSuperFrm;
    RK_BOOL    bFrmLost;
    RK_BOOL    bIntraRefresh;
    RK_BOOL    bHierarchicalQp;
    RK_BOOL    bMjpegParam;
    RK_BOOL    bForceIdr;
    RK_BOOL    bFullRange;
    RK_BOOL    bSendFrameEx;
    RK_BOOL    bQpmap;
    MB_POOL    vencPoolInput;
    MB_POOL    vencPoolOutput[VENC_MAX_CHN_NUM];
    RK_BOOL    bAttachPool;
    bool       enSlice;
    RK_S32     sliceMode;
    RK_S32     sliceSize;
    VENCDATA   func;
} TEST_VENC_CTX_S;

TEST_VENC_CTX_S *g_pstCtx = NULL;

void h265_set_qpmap_64x64(RK_S16 *dst, RK_S16 *src, RK_S32 w, RK_S32 h)
{
    RK_S32 mb_w = RK_ALIGN(w, 64) / 64;
    RK_S32 mb_h = RK_ALIGN(h, 64) / 64;
    RK_S32 ctu_line = mb_w;
    RK_S32 i, j, cu16cnt;

    for (j = 0; j < mb_h; j++)
    {
        for (i = 0; i < mb_w; i++)
        {
            RK_S32 ctu_addr = j * ctu_line + i;
            RK_S32 cu16_num_line = ctu_line * 4;
            for (cu16cnt = 0; cu16cnt < 16; cu16cnt++)
            {
                RK_S32 cu16_x;
                RK_S32 cu16_y;
                RK_S32 cu16_addr_in_frame;
                cu16_x = cu16cnt & 3;
                cu16_y = cu16cnt / 4;
                cu16_x += i * 4;
                cu16_y += j * 4;
                cu16_addr_in_frame = cu16_x + cu16_y * cu16_num_line;
                dst[ctu_addr * 16 + cu16cnt] = src[cu16_addr_in_frame];
            }
        }
    }
}

RK_S32 TEST_VENC_SetBitRate(VENC_RC_ATTR_S *pRcAttr, RK_U32 u32BitRate, RK_U32 u32BitRateMax, RK_U32 u32BitRateMin)
{
    switch (pRcAttr->enRcMode)
    {
    case VENC_RC_MODE_MJPEGCBR:
        pRcAttr->stMjpegCbr.u32BitRate = u32BitRate;
        break;
    case VENC_RC_MODE_H264CBR:
        pRcAttr->stH264Cbr.u32BitRate = u32BitRate;
        break;
    case VENC_RC_MODE_H265CBR:
        pRcAttr->stH265Cbr.u32BitRate = u32BitRate;
        break;
    case VENC_RC_MODE_MJPEGVBR:
        pRcAttr->stMjpegVbr.u32BitRate = u32BitRate;
        pRcAttr->stMjpegVbr.u32MaxBitRate = u32BitRateMax;
        pRcAttr->stMjpegVbr.u32MinBitRate = u32BitRateMin;
        break;
    case VENC_RC_MODE_H264VBR:
        pRcAttr->stH264Vbr.u32BitRate = u32BitRate;
        pRcAttr->stH264Vbr.u32MaxBitRate = u32BitRateMax;
        pRcAttr->stH264Vbr.u32MinBitRate = u32BitRateMin;
        break;
    case VENC_RC_MODE_H265VBR:
        pRcAttr->stH265Vbr.u32BitRate = u32BitRate;
        pRcAttr->stH265Vbr.u32MaxBitRate = u32BitRateMax;
        pRcAttr->stH265Vbr.u32MinBitRate = u32BitRateMin;
        break;
    case VENC_RC_MODE_H264AVBR:
        pRcAttr->stH264Avbr.u32BitRate = u32BitRate;
        pRcAttr->stH264Avbr.u32MaxBitRate = u32BitRateMax;
        pRcAttr->stH264Avbr.u32MinBitRate = u32BitRateMin;
        break;
    case VENC_RC_MODE_H265AVBR:
        pRcAttr->stH265Avbr.u32BitRate = u32BitRate;
        pRcAttr->stH265Avbr.u32MaxBitRate = u32BitRateMax;
        pRcAttr->stH265Avbr.u32MinBitRate = u32BitRateMin;
        break;
    default:
        return RK_ERR_VENC_NOT_SUPPORT;
    }

    return RK_SUCCESS;
}

RK_S32 TEST_VENC_SetFixQp(VENC_RC_ATTR_S *pRcAttr, RK_U32 u32FixIQp,
                          RK_U32 u32FixPQp, RK_U32 u32FixBQp)
{
    switch (pRcAttr->enRcMode)
    {
    case VENC_RC_MODE_MJPEGFIXQP:
        pRcAttr->stMjpegFixQp.u32Qfactor = u32FixIQp;
        break;
    case VENC_RC_MODE_H264FIXQP:
        pRcAttr->stH264FixQp.u32IQp = u32FixIQp;
        pRcAttr->stH264FixQp.u32PQp = u32FixPQp;
        pRcAttr->stH264FixQp.u32BQp = u32FixBQp;
        break;
    case VENC_RC_MODE_H265FIXQP:
        pRcAttr->stH265FixQp.u32IQp = u32FixIQp;
        pRcAttr->stH265FixQp.u32PQp = u32FixPQp;
        pRcAttr->stH265FixQp.u32BQp = u32FixBQp;
        break;
    default:
        return RK_ERR_VENC_NOT_SUPPORT;
    }

    return RK_SUCCESS;
}


RK_S32 TEST_VENC_SetFrameRate(VENC_RC_ATTR_S *pRcAttr, RK_U32 u32Fps) {
    switch (pRcAttr->enRcMode) {
      case VENC_RC_MODE_MJPEGCBR:
        pRcAttr->stMjpegCbr.u32SrcFrameRateNum = u32Fps;
        pRcAttr->stMjpegCbr.u32SrcFrameRateDen = 1;
        pRcAttr->stMjpegCbr.fr32DstFrameRateNum = u32Fps;
        pRcAttr->stMjpegCbr.fr32DstFrameRateDen = 1;
        break;
      case VENC_RC_MODE_H264CBR:
        pRcAttr->stH264Cbr.u32SrcFrameRateNum = u32Fps;
        pRcAttr->stH264Cbr.u32SrcFrameRateDen = 1;
        pRcAttr->stH264Cbr.fr32DstFrameRateNum = u32Fps;
        pRcAttr->stH264Cbr.fr32DstFrameRateDen = 1;
        break;
      case VENC_RC_MODE_H265CBR:
        pRcAttr->stH265Cbr.u32SrcFrameRateNum = u32Fps;
        pRcAttr->stH265Cbr.u32SrcFrameRateDen = 1;
        pRcAttr->stH265Cbr.fr32DstFrameRateNum = u32Fps;
        pRcAttr->stH265Cbr.fr32DstFrameRateDen = 1;
        break;
      case VENC_RC_MODE_MJPEGVBR:
        pRcAttr->stMjpegVbr.u32SrcFrameRateNum = u32Fps;
        pRcAttr->stMjpegVbr.u32SrcFrameRateDen = 1;
        pRcAttr->stMjpegVbr.fr32DstFrameRateNum = u32Fps;
        pRcAttr->stMjpegVbr.fr32DstFrameRateDen = 1;
        break;
      case VENC_RC_MODE_H264VBR:
        pRcAttr->stH264Vbr.u32SrcFrameRateNum = u32Fps;
        pRcAttr->stH264Vbr.u32SrcFrameRateDen = 1;
        pRcAttr->stH264Vbr.fr32DstFrameRateNum = u32Fps;
        pRcAttr->stH264Vbr.fr32DstFrameRateDen = 1;
        break;
      case VENC_RC_MODE_H265VBR:
        pRcAttr->stH265Vbr.u32SrcFrameRateNum = u32Fps;
        pRcAttr->stH265Vbr.u32SrcFrameRateDen = 1;
        pRcAttr->stH265Vbr.fr32DstFrameRateNum = u32Fps;
        pRcAttr->stH265Vbr.fr32DstFrameRateDen = 1;
        break;
      case VENC_RC_MODE_H264AVBR:
        pRcAttr->stH264Avbr.u32SrcFrameRateNum = u32Fps;
        pRcAttr->stH264Avbr.u32SrcFrameRateDen = 1;
        pRcAttr->stH264Avbr.fr32DstFrameRateNum = u32Fps;
        pRcAttr->stH264Avbr.fr32DstFrameRateDen = 1;
        break;
      case VENC_RC_MODE_H265AVBR:
        pRcAttr->stH265Avbr.u32SrcFrameRateNum = u32Fps;
        pRcAttr->stH265Avbr.u32SrcFrameRateDen = 1;
        pRcAttr->stH265Avbr.fr32DstFrameRateNum = u32Fps;
        pRcAttr->stH265Avbr.fr32DstFrameRateDen = 1;
        break;
      default:
        return RK_ERR_VENC_NOT_SUPPORT;
    }

    return RK_SUCCESS;
}

RK_S32 TEST_VENC_SetRcFrameRate(VENC_RC_ATTR_S *pRcAttr, RK_U32 u32Fps) {
    switch (pRcAttr->enRcMode) {
      case VENC_RC_MODE_MJPEGCBR:
        pRcAttr->stMjpegCbr.u32SrcFrameRateNum = 0;
        pRcAttr->stMjpegCbr.u32SrcFrameRateDen = 0;
        pRcAttr->stMjpegCbr.fr32DstFrameRateNum = u32Fps;
        pRcAttr->stMjpegCbr.fr32DstFrameRateDen = 1;
        break;
      case VENC_RC_MODE_H264CBR:
        pRcAttr->stH264Cbr.u32SrcFrameRateNum = 0;
        pRcAttr->stH264Cbr.u32SrcFrameRateDen = 0;
        pRcAttr->stH264Cbr.fr32DstFrameRateNum = u32Fps;
        pRcAttr->stH264Cbr.fr32DstFrameRateDen = 1;
        break;
      case VENC_RC_MODE_H265CBR:
        pRcAttr->stH265Cbr.u32SrcFrameRateNum = 0;
        pRcAttr->stH265Cbr.u32SrcFrameRateDen = 0;
        pRcAttr->stH265Cbr.fr32DstFrameRateNum = u32Fps;
        pRcAttr->stH265Cbr.fr32DstFrameRateDen = 1;
        break;
      case VENC_RC_MODE_MJPEGVBR:
        pRcAttr->stMjpegVbr.u32SrcFrameRateNum = 0;
        pRcAttr->stMjpegVbr.u32SrcFrameRateDen = 0;
        pRcAttr->stMjpegVbr.fr32DstFrameRateNum = u32Fps;
        pRcAttr->stMjpegVbr.fr32DstFrameRateDen = 1;
        break;
      case VENC_RC_MODE_H264VBR:
        pRcAttr->stH264Vbr.u32SrcFrameRateNum = 0;
        pRcAttr->stH264Vbr.u32SrcFrameRateDen = 0;
        pRcAttr->stH264Vbr.fr32DstFrameRateNum = u32Fps;
        pRcAttr->stH264Vbr.fr32DstFrameRateDen = 1;
        break;
      case VENC_RC_MODE_H265VBR:
        pRcAttr->stH265Vbr.u32SrcFrameRateNum = 0;
        pRcAttr->stH265Vbr.u32SrcFrameRateDen = 0;
        pRcAttr->stH265Vbr.fr32DstFrameRateNum = u32Fps;
        pRcAttr->stH265Vbr.fr32DstFrameRateDen = 1;
        break;
      case VENC_RC_MODE_H264AVBR:
        pRcAttr->stH264Avbr.u32SrcFrameRateNum = 0;
        pRcAttr->stH264Avbr.u32SrcFrameRateDen = 0;
        pRcAttr->stH264Avbr.fr32DstFrameRateNum = u32Fps;
        pRcAttr->stH264Avbr.fr32DstFrameRateDen = 1;
        break;
      case VENC_RC_MODE_H265AVBR:
        pRcAttr->stH265Avbr.u32SrcFrameRateNum = 0;
        pRcAttr->stH265Avbr.u32SrcFrameRateDen = 0;
        pRcAttr->stH265Avbr.fr32DstFrameRateNum = u32Fps;
        pRcAttr->stH265Avbr.fr32DstFrameRateDen = 1;
        break;
      default:
        return RK_ERR_VENC_NOT_SUPPORT;
    }

    return RK_SUCCESS;
}

int init_venc(int width, int height, int vir_width, int vir_height, int codecType, \
              int bitrate, int format, VENCDATA func, bool enSlice, int sliceMode, int sliceSize)
{
    g_pstCtx = (TEST_VENC_CTX_S *)malloc(sizeof(TEST_VENC_CTX_S));
    memset(g_pstCtx, 0, sizeof(TEST_VENC_CTX_S));
    g_pstCtx->s32LoopCount    = 1;
    g_pstCtx->u32StreamBufCnt = 8;
    g_pstCtx->u32ChNum        = 1;

    g_pstCtx->u32SrcPixFormat = (PIXEL_FORMAT_E)format;
    g_pstCtx->u32DstCodec     = RK_VIDEO_ID_HEVC;  //RK_VIDEO_ID_AVC
    g_pstCtx->enCropType      = VENC_CROP_NONE;
    g_pstCtx->bFrameRate      = RK_FALSE;
    g_pstCtx->s32SnapPicCount = -1;
    g_pstCtx->u32GopSize      = 360;
    g_pstCtx->enRcMode        = VENC_RC_MODE_H265CBR;//VENC_RC_MODE_H264CBR;
    g_pstCtx->u32BitRateKb    = bitrate * 1024;
    g_pstCtx->bFullRange      = RK_TRUE;
    g_pstCtx->u32SrcWidth = width;
    g_pstCtx->u32SrcHeight = height;
    g_pstCtx->u32srcVirWidth = vir_width;
    g_pstCtx->u32srcVirHeight = vir_height;
    g_pstCtx->enSlice = enSlice;
    g_pstCtx->sliceMode = sliceMode;
    g_pstCtx->sliceSize = sliceSize;
    printf("Width: %d, Height: %d, Virtual Width: %d, Virtual Height: %d\n", width, height, vir_width, vir_height);
    printf("Codec Type: %d, Bitrate: %d, Format: %d\n", codecType, bitrate, format);
    printf("Slice Enabled: %d, Slice Mode: %d, Slice Size: %d\n", enSlice, sliceMode, sliceSize);
    RK_U32 s32Ret = RK_MPI_SYS_Init();
    if (s32Ret != RK_SUCCESS)
    {
        return s32Ret;
    }
    RK_U32                  u32Ch = 0;
    VENC_CHN_ATTR_S         stAttr;
    VENC_CHN_PARAM_S        stParam;
    VENC_RECV_PIC_PARAM_S   stRecvParam;
    VENC_RC_PARAM_S         stRcParam;
    MB_POOL_CONFIG_S        stMbPoolCfg;
    TEST_VENC_CTX_S         stVencCtx[VENC_MAX_CHN_NUM];
    g_pstCtx->u32ChNum = 1;
    memset(&stAttr, 0, sizeof(VENC_CHN_ATTR_S));
    memset(&stRecvParam, 0, sizeof(VENC_RECV_PIC_PARAM_S));
    memset(&stRcParam, 0, sizeof(VENC_RC_PARAM_S));
    memset(&stParam, 0, sizeof(VENC_CHN_PARAM_S));
    if (g_pstCtx->u32BufferSize <= 0)
    {
        PIC_BUF_ATTR_S stPicBufAttr;
        MB_PIC_CAL_S stMbPicCalResult;

        stPicBufAttr.u32Width = g_pstCtx->u32SrcWidth;
        stPicBufAttr.u32Height = g_pstCtx->u32SrcHeight;
        stPicBufAttr.enPixelFormat = (PIXEL_FORMAT_E)g_pstCtx->u32SrcPixFormat;
        stPicBufAttr.enCompMode = g_pstCtx->enCompressMode;
        s32Ret = RK_MPI_CAL_COMM_GetPicBufferSize(&stPicBufAttr, &stMbPicCalResult);
        if (s32Ret != RK_SUCCESS)
        {
            RK_LOGE("get picture buffer size failed. err 0x%x", s32Ret);
            return s32Ret;
        }
        g_pstCtx->u32BufferSize = stMbPicCalResult.u32MBSize;
        RK_LOGD("calc picture size: %d", g_pstCtx->u32BufferSize);
    }
    if (g_pstCtx->u32BufferSize > 8192 * 8192 * 4)
    {
        RK_LOGE("too large picture size: %d", g_pstCtx->u32BufferSize);
        return RK_FAILURE;
    }
    if (g_pstCtx->u32StreamSize == 0)
    {
        g_pstCtx->u32StreamSize = g_pstCtx->u32BufferSize;
    }
    // for input image pool
    /* memset(&stMbPoolCfg, 0, sizeof(MB_POOL_CONFIG_S));
     stMbPoolCfg.u64MBSize = ctx->u32BufferSize;
     stMbPoolCfg.u32MBCnt  = 3;
     stMbPoolCfg.enAllocType = MB_ALLOC_TYPE_DMA;
     stMbPoolCfg.bPreAlloc = RK_TRUE;
     ctx->vencPoolInput = RK_MPI_MB_CreatePool(&stMbPoolCfg);
     if (ctx->vencPoolInput == MB_INVALID_POOLID) {
         RK_LOGE("create vencPoolInput failed!");
         return RK_FAILURE;
     }*/
    // for (u32Ch = 0; u32Ch < g_pstCtx->u32ChNum; u32Ch++) {
    memset(&stAttr, 0, sizeof(VENC_CHN_ATTR_S));
    memset(&stRecvParam, 0, sizeof(VENC_RECV_PIC_PARAM_S));
    memset(&stRcParam, 0, sizeof(VENC_RC_PARAM_S));
    memset(&stParam, 0, sizeof(VENC_CHN_PARAM_S));
    if (g_pstCtx->u32ChNum >= 1)
    {
        g_pstCtx->u32ChnIndex = u32Ch;
    }

    stAttr.stRcAttr.enRcMode = g_pstCtx->enRcMode;
    stAttr.stRcAttr.stH265Cbr.u32Gop = g_pstCtx->u32GopSize;
    stAttr.stRcAttr.stH265Cbr.u32BitRate = g_pstCtx->u32BitRateKb;
    printf("============================u32BitRateKb=%d============================\n", g_pstCtx->u32BitRateKb);
    TEST_VENC_SetBitRate(&stAttr.stRcAttr, g_pstCtx->u32BitRateKb, bitrate * 1024, 5 * 1024);
    TEST_VENC_SetFixQp(&stAttr.stRcAttr, g_pstCtx->u32FixQp, g_pstCtx->u32FixQp + 2, g_pstCtx->u32FixQp);
    TEST_VENC_SetRcFrameRate(&stAttr.stRcAttr, 60);
    stAttr.stVencAttr.enType = (RK_CODEC_ID_E)g_pstCtx->u32DstCodec;
    stAttr.stVencAttr.u32Profile = H264E_PROFILE_HIGH;
    stAttr.stVencAttr.enPixelFormat = (PIXEL_FORMAT_E)g_pstCtx->u32SrcPixFormat;
    stAttr.stVencAttr.u32PicWidth = g_pstCtx->u32SrcWidth;
    stAttr.stVencAttr.u32PicHeight = g_pstCtx->u32SrcHeight;
    stAttr.stVencAttr.enMirror = g_pstCtx->enMirror;

    if (g_pstCtx->u32srcVirWidth <= 0)
    {
        g_pstCtx->u32srcVirWidth = g_pstCtx->u32SrcWidth;
    }
    stAttr.stVencAttr.u32VirWidth = g_pstCtx->u32srcVirWidth;

    if (g_pstCtx->u32srcVirHeight <= 0)
    {
        g_pstCtx->u32srcVirHeight = g_pstCtx->u32SrcHeight;
    }
    stAttr.stVencAttr.u32VirHeight = g_pstCtx->u32srcVirHeight;
    stAttr.stVencAttr.u32StreamBufCnt = g_pstCtx->u32StreamBufCnt;
    stAttr.stVencAttr.u32BufSize = g_pstCtx->u32StreamSize;

    if (stAttr.stVencAttr.enType == RK_VIDEO_ID_JPEG)
    {
        stAttr.stVencAttr.stAttrJpege.bSupportDCF = RK_FALSE;
        stAttr.stVencAttr.stAttrJpege.stMPFCfg.u8LargeThumbNailNum = 0;
        stAttr.stVencAttr.stAttrJpege.enReceiveMode = VENC_PIC_RECEIVE_SINGLE;
    }
    VENC_PARAM_MOD_S stModParam;
    stModParam.enVencModType = MODTYPE_H265E;
    RK_MPI_VENC_GetModParam(&stModParam);
    stModParam.stH265eModParam.u32OneStreamBuffer = 1;
    //stModParam.stH265eModParam.u32H265eTileSize = 3840 * 2999;
    g_pstCtx->u32OneStreamBuffer = 1;
    s32Ret = RK_MPI_VENC_SetModParam(&stModParam);
    if (s32Ret != RK_SUCCESS)
    {
        RK_LOGE("close double vpu failed. err 0x%x", s32Ret);
        return s32Ret;
    }

    RK_MPI_VENC_CreateChn(u32Ch, &stAttr);

    stParam.stCropCfg.enCropType = g_pstCtx->enCropType;
    // for crop test
    if (stParam.stCropCfg.enCropType == VENC_CROP_ONLY)
    {
        stParam.stCropCfg.stCropRect.s32X = 10;
        stParam.stCropCfg.stCropRect.s32Y = 10;
        stParam.stCropCfg.stCropRect.u32Height = 100;
        stParam.stCropCfg.stCropRect.u32Width = 100;
    }
    else if (stParam.stCropCfg.enCropType == VENC_CROP_SCALE)
    {
        // for crop/scale test
        stParam.stCropCfg.stScaleRect.stSrc.s32X = 100;
        stParam.stCropCfg.stScaleRect.stSrc.s32Y = 200;
        stParam.stCropCfg.stScaleRect.stSrc.u32Width = g_pstCtx->u32SrcWidth / 2;
        stParam.stCropCfg.stScaleRect.stSrc.u32Height = g_pstCtx->u32SrcHeight / 2;
        stParam.stCropCfg.stScaleRect.stDst.s32X = 0;
        stParam.stCropCfg.stScaleRect.stDst.s32Y = 0;
        stParam.stCropCfg.stScaleRect.stDst.u32Width = 200;
        stParam.stCropCfg.stScaleRect.stDst.u32Height = 200;
    }
    // for framerate test
    {
        stParam.stFrameRate.bEnable = g_pstCtx->bFrameRate;
        stParam.stFrameRate.s32SrcFrmRateNum = 60;
        stParam.stFrameRate.s32SrcFrmRateDen = 1;
        stParam.stFrameRate.s32DstFrmRateNum = 10;
        stParam.stFrameRate.s32DstFrmRateDen = 1;
    }
    RK_MPI_VENC_SetChnParam(u32Ch, &stParam);
#if 0
    stAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265CBR;
    stAttr.stRcAttr.stH265Cbr.u32Gop = 99;
    stAttr.stRcAttr.stH265Cbr.u32SrcFrameRateNum = 0;
    stAttr.stRcAttr.stH265Cbr.u32SrcFrameRateDen = 0;
    stAttr.stRcAttr.stH265Cbr.fr32DstFrameRateNum = 60;
    stAttr.stRcAttr.stH265Cbr.fr32DstFrameRateDen = 1;
    stAttr.stRcAttr.stH265Cbr.u32BitRate = bitrate*1024;
    RK_MPI_VENC_SetChnAttr(u32Ch, &stAttr);
#endif
    if (g_pstCtx->bAttachPool)
    {
        // for output stream pool  fr32DstframeRateNum及fr32DstFrameRateDen
        /*memset(&stMbPoolCfg, 0, sizeof(MB_POOL_CONFIG_S));
        stMbPoolCfg.u64MBSize = ctx->u32StreamSize;
        stMbPoolCfg.u32MBCnt  = 3;
        stMbPoolCfg.enAllocType = MB_ALLOC_TYPE_DMA;
        stMbPoolCfg.bPreAlloc = RK_TRUE;  //  this must prealloc if attach enc output
        ctx->vencPoolOutput[u32Ch] = RK_MPI_MB_CreatePool(&stMbPoolCfg);
        if (ctx->vencPoolOutput[u32Ch] == MB_INVALID_POOLID) {
            RK_LOGE("create vencPoolOutput[%d] failed!", u32Ch);
            goto __FAILED;
        }

        s32Ret = RK_MPI_VENC_AttachMbPool(u32Ch, ctx->vencPoolOutput[u32Ch]);
        if (s32Ret != RK_SUCCESS) {
            RK_LOGE("RK_MPI_VENC_AttachMbPool fail:0x%x!", s32Ret);
            goto __FAILED;
        }*/
    }
    if (g_pstCtx->enGopMode)
    {
        stAttr.stGopAttr.enGopMode = g_pstCtx->enGopMode;
        if (!stAttr.stRcAttr.stH264Cbr.u32Gop)
            stAttr.stRcAttr.stH264Cbr.u32Gop = 60;
        stAttr.stGopAttr.s32VirIdrLen = stAttr.stRcAttr.stH264Cbr.u32Gop / 2;
        RK_MPI_VENC_SetChnAttr(u32Ch, &stAttr);
    }

    if (g_pstCtx->enRotation)
    {
        RK_MPI_VENC_SetChnRotation(u32Ch, g_pstCtx->enRotation);
    }
    stRecvParam.s32RecvPicNum = g_pstCtx->s32SnapPicCount;
    RK_MPI_VENC_StartRecvFrame(u32Ch, &stRecvParam);

    if (stAttr.stVencAttr.enType == RK_VIDEO_ID_JPEG)
    {
        VENC_JPEG_PARAM_S stJpegParam;
        memset(&stJpegParam, 0, sizeof(stJpegParam));
        stJpegParam.u32Qfactor = 77;
        RK_MPI_VENC_SetJpegParam(u32Ch, &stJpegParam);
    }

    for (RK_U32 i = 0; i < g_pstCtx->u32RoiTestCount; i++)
    {
        VENC_ROI_ATTR_S stRoiAttr;
        stRoiAttr.u32Index = i;
        stRoiAttr.bEnable = RK_TRUE;
        stRoiAttr.bAbsQp = i % 2 ? RK_FALSE : RK_TRUE;
        stRoiAttr.s32Qp = i + 42;
        stRoiAttr.bIntra = i % 2 ? RK_TRUE : RK_FALSE;
        stRoiAttr.stRect.s32X = RK_ALIGN_16((i + 1) * 16);
        stRoiAttr.stRect.s32Y = RK_ALIGN_16((i + 1) * 16);
        stRoiAttr.stRect.u32Width = RK_ALIGN_16(g_pstCtx->u32SrcWidth / (i + 2));
        stRoiAttr.stRect.u32Height = RK_ALIGN_16(g_pstCtx->u32SrcHeight / (i + 2));
        RK_MPI_VENC_SetRoiAttr(u32Ch, &stRoiAttr);
    }

#if 1
    stRcParam.s32FirstFrameStartQp = 25;
    stRcParam.stParamH265.u32StepQp = 5;
    stRcParam.stParamH265.u32MinQp = 1;
    stRcParam.stParamH265.u32MaxQp = 40;
    stRcParam.stParamH265.u32MinIQp = 20;
    stRcParam.stParamH265.u32MaxIQp = 30;
    stRcParam.stParamH265.s32DeltIpQp = -2;
    stRcParam.stParamH265.s32MaxReEncodeTimes = 2;
    RK_MPI_VENC_SetRcParam(u32Ch, &stRcParam);
#endif

    if (!g_pstCtx->bFullRange)
    {
        if (stAttr.stVencAttr.enType == RK_VIDEO_ID_AVC)
        {
            VENC_H264_VUI_S stH264Vui;
            RK_MPI_VENC_GetH264Vui(u32Ch, &stH264Vui);
            stH264Vui.stVuiVideoSignal.video_full_range_flag = 0;
            RK_MPI_VENC_SetH264Vui(u32Ch, &stH264Vui);
        }
        else if (stAttr.stVencAttr.enType == RK_VIDEO_ID_HEVC)
        {
            VENC_H265_VUI_S stH265Vui;
            RK_MPI_VENC_GetH265Vui(u32Ch, &stH265Vui);
            stH265Vui.stVuiVideoSignal.video_full_range_flag = 0;
            RK_MPI_VENC_SetH265Vui(u32Ch, &stH265Vui);
        }
    }

    if (g_pstCtx->bSuperFrm)
    {
        VENC_SUPERFRAME_CFG_S stSuperFrameCfg;
        memset(&stSuperFrameCfg, 0, sizeof(stSuperFrameCfg));
        stSuperFrameCfg.enSuperFrmMode = SUPERFRM_DISCARD;
        stSuperFrameCfg.u32SuperIFrmBitsThr = 100 * 1024 * 8;  // 100KByte
        stSuperFrameCfg.u32SuperPFrmBitsThr = 20 * 1024 * 8;  // 20KByte
        stSuperFrameCfg.enRcPriority = VENC_RC_PRIORITY_FRAMEBITS_FIRST;
        RK_MPI_VENC_SetSuperFrameStrategy(u32Ch, &stSuperFrameCfg);
    }

    if (g_pstCtx->bIntraRefresh)
    {
        VENC_INTRA_REFRESH_S stIntraRefresh;
        memset(&stIntraRefresh, 0, sizeof(stIntraRefresh));
        stIntraRefresh.bRefreshEnable = RK_TRUE;
        stIntraRefresh.enIntraRefreshMode = INTRA_REFRESH_ROW;
        stIntraRefresh.u32RefreshNum = 5;
        RK_MPI_VENC_SetIntraRefresh(u32Ch, &stIntraRefresh);
    }

    if (g_pstCtx->bHierarchicalQp)
    {
        VENC_HIERARCHICAL_QP_S stHierarchicalQp;
        memset(&stHierarchicalQp, 0, sizeof(stHierarchicalQp));
        stHierarchicalQp.bHierarchicalQpEn = RK_TRUE;
        stHierarchicalQp.s32HierarchicalQpDelta[0] = -10;
        stHierarchicalQp.s32HierarchicalQpDelta[1] = -5;
        stHierarchicalQp.s32HierarchicalFrameNum[0] = 2;
        stHierarchicalQp.s32HierarchicalFrameNum[1] = 3;
        RK_MPI_VENC_SetHierarchicalQp(u32Ch, &stHierarchicalQp);
    }

    if (g_pstCtx->bMjpegParam && stAttr.stVencAttr.enType == RK_VIDEO_ID_MJPEG)
    {
        VENC_MJPEG_PARAM_S stMjpegParam;
        memset(&stMjpegParam, 0, sizeof(stMjpegParam));
        /* set qtable for test only, users need set actual qtable */
        memset(&stMjpegParam.u8YQt, 1, sizeof(stMjpegParam.u8YQt));
        memset(&stMjpegParam.u8CbQt, 1, sizeof(stMjpegParam.u8CbQt));
        memset(&stMjpegParam.u8CrQt, 1, sizeof(stMjpegParam.u8CrQt));
        stMjpegParam.u32MCUPerECS = 100;
        RK_MPI_VENC_SetMjpegParam(u32Ch, &stMjpegParam);
    }

    if (g_pstCtx->bFrmLost)
    {
        VENC_FRAMELOST_S stFrmLost;
        memset(&stFrmLost, 0, sizeof(stFrmLost));
        stFrmLost.bFrmLostOpen = RK_TRUE;
        stFrmLost.u32FrmLostBpsThr = 1;
        stFrmLost.enFrmLostMode = FRMLOST_NORMAL;
        stFrmLost.u32EncFrmGaps = 0;
        RK_MPI_VENC_SetFrameLostStrategy(u32Ch, &stFrmLost);
    }

    if (g_pstCtx->u32DeBreath > 0)
    {
        VENC_DEBREATHEFFECT_S stDeBreathEffect;
        memset(&stDeBreathEffect, 0, sizeof(stDeBreathEffect));
        stDeBreathEffect.bEnable = RK_TRUE;
        stDeBreathEffect.s32Strength1 = g_pstCtx->u32DeBreath;
        RK_MPI_VENC_SetDeBreathEffect(u32Ch, &stDeBreathEffect);
    }

    if (g_pstCtx->enSlice)
    {
        VENC_SLICE_SPLIT_S stSliceSplit;
        RK_MPI_VENC_GetSliceSplit(0, &stSliceSplit);
        stSliceSplit.bSplitEnable = RK_TRUE;
        stSliceSplit.u32SplitMode = g_pstCtx->sliceMode;
        int s_w = RK_ALIGN(g_pstCtx->u32SrcWidth, 32) / 32;
        int s_h = RK_ALIGN(g_pstCtx->u32SrcHeight, 32) / 32;
        int slice_size = s_w * s_h / g_pstCtx->sliceSize;
        stSliceSplit.u32SplitSize = slice_size;//(RK_ALIGN_64(g_pstCtx->u32SrcWidth)*RK_ALIGN_64(g_pstCtx->u32SrcHeight))/(g_pstCtx->sliceSize*64*64);
        RK_MPI_VENC_SetSliceSplit(0, &stSliceSplit);
    }
    VENC_H265_VUI_S pstH265Vui;
    RK_MPI_VENC_GetH265Vui(0, &pstH265Vui);
    pstH265Vui.stVuiVideoSignal.video_full_range_flag = 0;
    RK_MPI_VENC_SetH265Vui(0, &pstH265Vui);


    g_pstCtx->func = func;
    // }
    return 0;
}

int set_bitrate(int bitrate, int maxBitrate, int minBitrate)
{
    VENC_CHN_ATTR_S stAttr;
    memset(&stAttr, 0, sizeof(VENC_CHN_ATTR_S));
    RK_MPI_VENC_GetChnAttr(0, &stAttr);
    TEST_VENC_SetBitRate(&stAttr.stRcAttr, bitrate , maxBitrate, minBitrate);
    TEST_VENC_SetRcFrameRate(&stAttr.stRcAttr, 60);
    RK_MPI_VENC_SetChnAttr(0, &stAttr);
    return 0;
}

int set_frame(int bufferFd, int size)
{

    TEST_VENC_CTX_S     *pstCtx         = g_pstCtx;
    RK_S32               s32Ret         = RK_SUCCESS;
    RK_U8               *pVirAddr       = RK_NULL;
    MB_BLK               blk            = RK_NULL;
    RK_S32               s32LoopCount   = pstCtx->s32LoopCount;
    RK_U32               u32Ch          = pstCtx->u32ChnIndex;
    RK_S32               s32FrameCount  = 0;
    RK_S32               s32ReachEOS    = 0;
    VIDEO_FRAME_INFO_S   stFrame;
    MB_BLK               qpmapBlk = RK_NULL;
    RK_S16              *qpmapData = RK_NULL;
    RK_S16              *qpmapTmp = RK_NULL;
    MB_BLK               buffer = RK_NULL;
    MB_EXT_CONFIG_S      stMbExtConfig;

    if (pstCtx->bSendFrameEx || pstCtx->bQpmap)
    {
        if (pstCtx->u32DstCodec == RK_VIDEO_ID_AVC || pstCtx->u32DstCodec == RK_VIDEO_ID_HEVC)
        {
            RK_U8 align = (pstCtx->u32DstCodec == RK_VIDEO_ID_AVC ? 16 : 64);
            RK_U32 qpmapWidth = RK_ALIGN(pstCtx->u32SrcWidth, align) / 16;
            RK_U32 qpmapHeight = RK_ALIGN(pstCtx->u32SrcHeight, align) / 16;
            RK_U32 qpmapSize = qpmapWidth * qpmapHeight * 2;
            RK_S16 *qpmap = RK_NULL;
            if (!qpmapBlk)
            {
                s32Ret = RK_MPI_SYS_MmzAlloc(&qpmapBlk, RK_NULL, RK_NULL, qpmapSize);
                if (s32Ret != RK_SUCCESS)
                    goto __EXIT;
                qpmapData = (RK_S16 *)RK_MPI_MB_Handle2VirAddr(qpmapBlk);
            }
            if (pstCtx->u32DstCodec == RK_VIDEO_ID_HEVC && !qpmapTmp)
            {
                qpmapTmp = (RK_S16 *)malloc(qpmapSize);
                if (!qpmapTmp)
                    goto __EXIT;
            }
            qpmap = (pstCtx->u32DstCodec == RK_VIDEO_ID_AVC ? qpmapData : qpmapTmp);
            for (RK_U32 j = 0; j < qpmapHeight; j++)
            {
                for (RK_U32 i = 0; i < qpmapWidth; i++)
                {
                    qpmap[j * qpmapHeight + i] = 0x8F80;  // 0x8F: abs qp, qp 15, 0x80: must
                }
            }
            if (pstCtx->u32DstCodec == RK_VIDEO_ID_HEVC)
            {
                h265_set_qpmap_64x64(qpmapData, qpmapTmp, pstCtx->u32SrcWidth, pstCtx->u32SrcHeight);
            }
        }
    }

    if (pstCtx->bQpmap)
    {
        RK_MPI_VENC_SetQpmap(pstCtx->u32ChnIndex, qpmapBlk);
    }

    memset(&stFrame, 0, sizeof(VIDEO_FRAME_INFO_S));

    //while (!pstCtx->threadExit) {
    /*    blk = RK_MPI_MB_GetMB(pool, pstCtx->u32BufferSize, RK_TRUE);

        if (RK_NULL == blk) {
            RK_LOGE("RK_MPI_MB_GetMB fail:%d", u32Ch);
            usleep(2000llu);
            continue;
        }
        pVirAddr = reinterpret_cast<RK_U8 *>(RK_MPI_MB_Handle2VirAddr(blk));
    */
    memset(&stMbExtConfig, 0, sizeof(MB_EXT_CONFIG_S));
    stMbExtConfig.u64Size = size;
    stMbExtConfig.s32Fd = bufferFd;
    RK_MPI_SYS_CreateMB(&buffer, &stMbExtConfig);
    //RK_MPI_MB_ReleaseMB(blk);
#if TEST_CROP_PARAM_CHANGE
    if (s32FrameCount == 50)
    {
        VENC_CHN_PARAM_S stParam;
        RK_MPI_VENC_GetChnParam(u32Ch, &stParam);
        // for crop test
        if (stParam.stCropCfg.enCropType == VENC_CROP_ONLY)
        {
            stParam.stCropCfg.stCropRect.s32X = 100;
            stParam.stCropCfg.stCropRect.s32Y = 100;
            stParam.stCropCfg.stCropRect.u32Height = 400;
            stParam.stCropCfg.stCropRect.u32Width = 300;
        }
        else if (stParam.stCropCfg.enCropType == VENC_CROP_SCALE)
        {
            // for crop/scale test
            stParam.stCropCfg.stScaleRect.stSrc.s32X = 300;
            stParam.stCropCfg.stScaleRect.stSrc.s32Y = 500;
            stParam.stCropCfg.stScaleRect.stSrc.u32Width = stParam.stCropCfg.stScaleRect.stSrc.u32Width / 4;
            stParam.stCropCfg.stScaleRect.stSrc.u32Height = stParam.stCropCfg.stScaleRect.stSrc.u32Height;
            stParam.stCropCfg.stScaleRect.stDst.s32X = 0;
            stParam.stCropCfg.stScaleRect.stDst.s32Y = 0;
            stParam.stCropCfg.stScaleRect.stDst.u32Width = 400;
            stParam.stCropCfg.stScaleRect.stDst.u32Height = 400;
        }
        RK_MPI_VENC_SetChnParam(u32Ch, &stParam);
    }
#endif

    RK_MPI_SYS_MmzFlushCache(buffer, RK_FALSE);
    stFrame.stVFrame.pMbBlk = buffer;
    stFrame.stVFrame.u32Width = pstCtx->u32SrcWidth;
    stFrame.stVFrame.u32Height = pstCtx->u32SrcHeight;
    stFrame.stVFrame.u32VirWidth = pstCtx->u32srcVirWidth;
    stFrame.stVFrame.u32VirHeight = pstCtx->u32srcVirHeight;
    stFrame.stVFrame.enPixelFormat = (PIXEL_FORMAT_E)pstCtx->u32SrcPixFormat;
    stFrame.stVFrame.u32FrameFlag |= s32ReachEOS ? FRAME_FLAG_SNAP_END : 0;
    stFrame.stVFrame.enCompressMode = pstCtx->enCompressMode;

    if (pstCtx->bInsertUserData)
    {
        RK_U32 user_len;
        RK_U8 user_data[] = "this is rockchip user data";
        user_len = sizeof(user_data);
        RK_MPI_VENC_InsertUserData(u32Ch, user_data, user_len);
        RK_CHAR user_data2[1024];
        snprintf(user_data2, sizeof(user_data2), "rockchip continuous user data:%d", s32FrameCount);
        user_len = strlen(user_data2);
        RK_MPI_VENC_InsertUserData(u32Ch, reinterpret_cast<RK_U8 *>(user_data2), user_len);
    }
__RETRY:
    rt_atrace_begin_body("RK_MPI_VENC_SendFrame");
    if (pstCtx->bSendFrameEx)
    {
        if (pstCtx->u32DstCodec == RK_VIDEO_ID_AVC || pstCtx->u32DstCodec == RK_VIDEO_ID_HEVC)
        {
            USER_FRAME_INFO_S stUser;
            memset(&stUser, 0, sizeof(stUser));
            memcpy(&stUser.stUserFrame, &stFrame, sizeof(stFrame));
            stUser.stUserRcInfo.bQpMapValid = RK_TRUE;
            stUser.stUserRcInfo.pMbBlkQpMap = qpmapBlk;
            s32Ret = RK_MPI_VENC_SendFrameEx(u32Ch, &stUser, -1);
        }
        else
        {
            s32Ret = RK_MPI_VENC_SendFrame(u32Ch, &stFrame, -1);
        }
    }
    else
    {
        s32Ret = RK_MPI_VENC_SendFrame(u32Ch, &stFrame, -1);
    }
    if (s32Ret != RK_SUCCESS)
    {
        printf("===========rockit sendframe fail.\n");
    }
    RK_MPI_MB_ReleaseMB(buffer);
    rt_atrace_end_body();
__EXIT:
    if (pstCtx->bSendFrameEx || pstCtx->bQpmap)
    {
        if (qpmapBlk)
            RK_MPI_SYS_MmzFree(qpmapBlk);
        if (qpmapTmp)
            free(qpmapTmp);
    }

    return 0;
}

void keyframe_req()
{
    RK_MPI_VENC_RequestIDR(0, RK_TRUE);
}

// FILE *fp;
void thread_get_stream(int flag)
{
    TEST_VENC_CTX_S *pstCtx     = g_pstCtx;
    void            *pData      = RK_NULL;
    RK_S32           s32Ret     = RK_SUCCESS;
    FILE            *fp         = RK_NULL;
    char             name[256]  = {0};
    RK_U32           u32Ch      = pstCtx->u32ChnIndex;
    RK_S32           s32StreamCnt = 0;
    VENC_STREAM_S    stFrame;
    int              len = 0;
    RK_BOOL          bKeyFrame = RK_FALSE;
    int isfirst = 1;
    int ifrequestIDR = 0;
    while (true)
    {
        stFrame.pstPack = reinterpret_cast<VENC_PACK_S *>(malloc(sizeof(VENC_PACK_S) * 8));
        stFrame.u32PackCount = 8;
        stFrame.pstPack->bFrameEnd = RK_FALSE;
        s32Ret = RK_MPI_VENC_GetStream(u32Ch, &stFrame, -1);
        if (isfirst == 1 && !(stFrame.pstPack->DataType.enH265EType == H265E_NALU_ISLICE
                                            || stFrame.pstPack->DataType.enH265EType == H265E_NALU_IDRSLICE) ) {
            RK_MPI_VENC_ReleaseStream(u32Ch, &stFrame);
            if (ifrequestIDR == 0) {
                RK_MPI_VENC_RequestIDR(u32Ch, RK_FALSE);
                ifrequestIDR = 1;
            }
            continue;
        }
        if (isfirst == 1) {
            isfirst = 0;
            RK_LOGI("now begin to send first I frame! eq is %llu\n", stFrame.u32Seq);
        }

        if (s32Ret >= 0)
        {

            if (pstCtx->u32OneStreamBuffer)
            {
                s32StreamCnt++;
                pData = RK_MPI_MB_Handle2VirAddr(stFrame.pstPack->pMbBlk);
                //RK_LOGD("get chn %d stream %d, pData=%p, endframe=%d", u32Ch, s32StreamCnt, pData,stFrame.pstPack->bFrameEnd);
                len = stFrame.pstPack->u32Len;
                if (s32StreamCnt % 4 == 0)
                {
                    if (stFrame.pstPack->bFrameEnd != RK_TRUE)
                    {
                        //printf("***************************************************eoi error.\n");
                    }
                }
#if 0
                if (stFrame.pstPack->bFrameEnd == RK_TRUE /*|| s32StreamCnt%4==0*/)
                {
                    char *tmp = (char *)pData;
                    if (tmp[4] == 0x26)
                    {
                        tmp[4] = 0x36;
                    }
                    if (tmp[4] == 0x02)
                    {
                        tmp[4] = 0x12;
                    }
                }
#endif

                if (stFrame.pstPack->DataType.enH265EType == H265E_NALU_ISLICE ||
                    stFrame.pstPack->DataType.enH265EType == H265E_NALU_IDRSLICE)
                {
                    bKeyFrame = RK_TRUE;
                }
                g_pstCtx->func(bKeyFrame, (unsigned char *)pData, len);
                RK_MPI_VENC_ReleaseStream(u32Ch, &stFrame);
                if (stFrame.pstPack->bStreamEnd == RK_TRUE)
                {
                    RK_LOGI("chn %d reach EOS stream", u32Ch);
                }
            }
            else
            {
                for (RK_U32 i = 0; i < stFrame.u32PackCount; i++)
                {
                    RK_LOGD("get chn(%d) stream(%d) packet(%d) eoi(%d) type(%d) offset(%d) lenth(%d)",
                            u32Ch, s32StreamCnt, i,
                            stFrame.pstPack[i].bFrameEnd,
                            stFrame.pstPack[i].DataType,
                            stFrame.pstPack[i].u32Offset,
                            stFrame.pstPack[i].u32Len);
                    pData = (char *)RK_MPI_MB_Handle2VirAddr(stFrame.pstPack[i].pMbBlk);
                    if (_fp != NULL)
                    {
                        //fwrite(pData+stFrame.pstPack[i].u32Offset, stFrame.pstPack[i].u32Len, 1, _fp);
                        //fflush(_fp);
                    }
                    if (stFrame.pstPack[i].DataType.enH265EType == H265E_NALU_ISLICE ||
                        stFrame.pstPack[i].DataType.enH265EType == H265E_NALU_IDRSLICE)
                    {
                        bKeyFrame = RK_TRUE;
                    }
                    g_pstCtx->func(bKeyFrame, (unsigned char *)pData + stFrame.pstPack[i].u32Offset, stFrame.pstPack[i].u32Len);
                }

                RK_MPI_VENC_ReleaseStream(u32Ch, &stFrame);
                if (stFrame.pstPack->bStreamEnd == RK_TRUE)
                {
                    RK_LOGI("chn %d reach EOS stream", u32Ch);
                    break;
                }

            }
        }
        else
        {
            RK_LOGD("get encode frame fail.\n");
        }
        if (stFrame.pstPack)
            free(stFrame.pstPack);
    }
}

int free_stream(void *p)
{
    VENC_PACK_S *pstPack = reinterpret_cast<VENC_PACK_S *>(p);
    free(pstPack);
    pstPack = NULL;
    return 0;
}

int deinit_venc()
{
    RK_MPI_VENC_StopRecvFrame(0);
    RK_MPI_VENC_DestroyChn(0);
    fclose(_fp);
    return 0;
}



