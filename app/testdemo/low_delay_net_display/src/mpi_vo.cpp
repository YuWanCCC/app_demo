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
#include "mpi_vo.h"
#include "pthread.h"
#include "rt_trace.h"


typedef struct rkVO_VIDEO_S
{
    RK_U32  u32VoDev;
    RK_U32  u32VoLayer;
    RK_U16  u16VirWidth;
    RK_U16  u16VirHeight;
    RK_U16  u16Width;
    RK_U16  u16Height;
    RK_S16  s16Xpos;
    RK_S16  s16Ypos;
    PIXEL_FORMAT_E  format;
} VO_VIDEO_S;

VO_VIDEO_S stVoVideoCfg;
VIDEO_FRAME_INFO_S stVFrame, uiVframe, uiVframe2;
VO_VIDEO_LAYER_ATTR_S stMouseLayerAttr, stUiLayerAttr, stSecondUiLayerAttr;
RK_VO_VSYNC_CALLBACK_FUNC_S  mVsyncCallback;
std::map<int, void *> mVideoDataMap;
std::map<int, void *> mMBDataMap;
pthread_mutex_t mutex;
RK_U32  g_vcnt_time = 0;
void vsync_callback(RK_VOID *pPrivateData, VO_CB_INFO_S *info)
{

    rt_atrace_begin_body("vcnt vsync_callback");
    rt_atrace_end_body();
//   g_vcnt_time =  (info->u32Sec*1000000+info->u32Usec)/1000;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    g_vcnt_time = (ts.tv_sec * 1000000000 + ts.tv_nsec) / 1000000;
}

RK_U32  get_vcnt_time()
{
    return g_vcnt_time;
}


RK_S32 init_vo(int vir_w, int vir_h, int w, int h, int format, int intf_type, int vo_dev, int vcnt_line)
{

    RK_U32 s32Ret;

    if (RK_MPI_SYS_Init() != RK_SUCCESS)
    {
        goto __FAILED;
    }
    VO_PUB_ATTR_S VoPubAttr;

    memset(&VoPubAttr, 0, sizeof(VO_PUB_ATTR_S));
    VoPubAttr.u32BgColor = 0xf;
    //VoPubAttr.enIntfType = VO_INTF_MIPI;//VO_INTF_HDMI;
    //VoPubAttr.enIntfType = VO_INTF_MIPI;
    VoPubAttr.enIntfType = intf_type;
    switch (h)
    {
    case 1080:
        VoPubAttr.enIntfSync = VO_OUTPUT_1080P60;
        break;
    case 2160:
        VoPubAttr.enIntfSync = VO_OUTPUT_3840x2160_60;
        break;
    default:
        // RK_LOGE("resolution[%d,%d] is not support!", w, h);
        // return RK_FAILURE;
        VoPubAttr.enIntfSync = VO_OUTPUT_DEFAULT;
        break;
    }

    VO_HDMI_PARAM_S stHDMIParam;
    RK_MPI_VO_GetHdmiParam(intf_type, 0, &stHDMIParam);
    RK_LOGE("RK_MPI_VO_GetHdmiParam enColorFmt=%d,enQuantRange=%d,enHdmiMode=%d", stHDMIParam.enColorFmt, stHDMIParam.enQuantRange, stHDMIParam.enHdmiMode);
    stHDMIParam.enColorFmt = VO_HDMI_COLOR_FORMAT_YCBCR420;
    stHDMIParam.enQuantRange = VO_HDMI_QUANT_RANGE_FULL;
    stHDMIParam.enHdmiMode = VO_HDMI_MODE_HDMI;
    s32Ret = RK_MPI_VO_SetHdmiParam(intf_type, 0, &stHDMIParam);
    RK_LOGE("RK_MPI_VO_SetHdmiParam ret=%x", s32Ret);
    RK_MPI_VO_GetHdmiParam(VO_INTF_HDMI, 0, &stHDMIParam);
    RK_LOGE("RK_MPI_VO_GetHdmiParam2 enColorFmt=%d,enQuantRange=%d,enHdmiMode=%d", stHDMIParam.enColorFmt, stHDMIParam.enQuantRange, stHDMIParam.enHdmiMode);

    // stVoVideoCfg.u32VoDev = RK3588_VO_DEV_HD3;
    stVoVideoCfg.u32VoDev = vo_dev;
    stVoVideoCfg.u32VoLayer = VO_LAYER_ESMART0;
    stVoVideoCfg.u16Width = w;
    stVoVideoCfg.u16Height = h;
    stVoVideoCfg.u16VirWidth = vir_w;
    stVoVideoCfg.u16VirHeight = vir_h;
    stVoVideoCfg.format = (PIXEL_FORMAT_E)format;

    s32Ret = RK_MPI_VO_SetPubAttr(stVoVideoCfg.u32VoDev, &VoPubAttr);
    if (s32Ret != RK_SUCCESS)
    {
        printf("==========RK_MPI_VO_SetPubAttr error\n");
        return s32Ret;
    }

    s32Ret = RK_MPI_VO_SetVcntTiming(stVoVideoCfg.u32VoDev, vcnt_line);
    if (s32Ret != RK_SUCCESS)
    {
        printf("==========RK_MPI_VO_SetVcntTiming error\n");
        return s32Ret;
    }
    s32Ret = RK_MPI_VO_Enable(stVoVideoCfg.u32VoDev);
    if (s32Ret != RK_SUCCESS)
    {
        printf("==========RK_MPI_VO_Enable error\n");
        return s32Ret;
    }

    RK_MPI_VO_BindLayer(stVoVideoCfg.u32VoLayer, stVoVideoCfg.u32VoDev, VO_LAYER_MODE_GRAPHIC);
    memset(&stUiLayerAttr, 0, sizeof(VO_VIDEO_LAYER_ATTR_S));
    stUiLayerAttr.enPixFormat = (PIXEL_FORMAT_E)format;
    stUiLayerAttr.stDispRect.s32X = 0;
    stUiLayerAttr.stDispRect.s32Y = 0;
    stUiLayerAttr.stDispRect.u32Width = w;
    stUiLayerAttr.stDispRect.u32Height = h;
    stUiLayerAttr.stImageSize.u32Width = vir_w;
    stUiLayerAttr.stImageSize.u32Height = vir_h;
    stUiLayerAttr.enCompressMode = (COMPRESS_MODE_E)0;

    s32Ret = RK_MPI_VO_SetLayerAttr(stVoVideoCfg.u32VoLayer, &stUiLayerAttr);
    s32Ret = RK_MPI_VO_EnableLayer(stVoVideoCfg.u32VoLayer);
    if (s32Ret != RK_SUCCESS)
    {
        printf("==========RK_MPI_VO_EnableLayer error\n");
        return s32Ret;
    }

    pthread_mutex_init(&mutex, NULL);
    mVsyncCallback.pfnEventCallback = vsync_callback;
    RK_MPI_VO_RegVsyncCallbackFunc(stVoVideoCfg.u32VoDev, &mVsyncCallback);

    return RK_SUCCESS;

__FAILED:
    RK_LOGE("test running failed!");
    return RK_FAILURE;
}

int deinit_vo()
{
    printf(">>>>>>>>>deinit_vo\n");
    RK_MPI_VO_DisableLayer(stVoVideoCfg.u32VoLayer);
    RK_MPI_VO_UnBindLayer(stVoVideoCfg.u32VoLayer, stVoVideoCfg.u32VoDev);
    RK_MPI_VO_Disable(stVoVideoCfg.u32VoDev);
    return 0;
}
static int skip_count = 0;
static int frame_count = 0;
void pushFrame(void *obj)
{
//  rt_atrace_begin_body(tmp);
    pthread_mutex_lock(&mutex);
    mVideoDataMap.clear();
    mVideoDataMap.insert(std::pair<int, void *>(0, obj));
    pthread_mutex_unlock(&mutex);
    // rt_atrace_end_body();
}

int sendframe_vo(void *obj)
{
    frame_count++;
    char tmp[50];
    memset(tmp, 0, sizeof(tmp));
    sprintf(tmp, "sendframe_vo=%d", frame_count);
    rt_atrace_begin_body(tmp);
    MB_BLK buffer = RK_NULL;
    MB_EXT_CONFIG_S stMbExtConfig;
    VIDEO_FRAME_INFO_S frame;
    frame.stVFrame.u32Width = stVoVideoCfg.u16Width;
    frame.stVFrame.u32Height = stVoVideoCfg.u16Height;
    frame.stVFrame.u32VirWidth = stVoVideoCfg.u16VirWidth;
    frame.stVFrame.u32VirHeight = stVoVideoCfg.u16VirHeight;
    frame.stVFrame.enPixelFormat = stVoVideoCfg.format;
    frame.stVFrame.pMbBlk = (MB_BLK *)obj;
    frame.stVFrame.u64PrivateData = 0;
    frame.stVFrame.u64PTS = frame_count;
    int ret = RK_MPI_VO_SendLayerFrame(stVoVideoCfg.u32VoLayer, &frame);
    rt_atrace_end_body();
    // printf("%s:%d, RK_MPI_VO_SendLayerFrame ret=0x%x\n", __FUNCTION__, __LINE__,ret);
    return 0;
}

int init_MB(int fd, int len)
{
    MB_EXT_CONFIG_S pstMbExtConfig;
    MB_BLK buffer = RK_NULL;
    memset(&pstMbExtConfig, 0, sizeof(MB_EXT_CONFIG_S));
    pstMbExtConfig.pFreeCB = NULL;
    //pstMbExtConfig.pOpaque = data;
    //pstMbExtConfig.pu8VirAddr = data;
    pstMbExtConfig.u64Size = len;
    pstMbExtConfig.s32Fd = fd;
    RK_MPI_SYS_CreateMB(&buffer, &pstMbExtConfig);
    mMBDataMap.insert(std::pair<int, void *>(fd, buffer));
    return 0;
}

int sendframe_vo_for_fd(int fd, int len)
{
    /*MB_EXT_CONFIG_S pstMbExtConfig;
    MB_BLK buffer = RK_NULL;
    memset(&pstMbExtConfig, 0, sizeof(MB_EXT_CONFIG_S));
    pstMbExtConfig.pFreeCB = NULL;
    //pstMbExtConfig.pOpaque = data;
    //pstMbExtConfig.pu8VirAddr = data;
    pstMbExtConfig.u64Size = len;
    pstMbExtConfig.s32Fd = fd;
    RK_MPI_SYS_CreateMB(&buffer, &pstMbExtConfig);*/
    std::map<int, void *>::iterator it = mMBDataMap.find(fd);
    if (it != mMBDataMap.end())
    {
        MB_BLK buffer = it->second;
        sendframe_vo((void *)buffer);
    }
    else
    {
        printf("=========mMBDataMap fd %d fail.\n", fd);
    }
    //RK_MPI_MB_ReleaseMB(buffer);
    return 0;
}

void switch_vsync(int status)
{
    FILE *fp;
    fp = fopen("/sys/kernel/debug/dri/0/video_port0/enable", "wb");
    if (fp == NULL)
    {
        printf("open vsync enable fail.\n");
        return;
    }
    fwrite(&status, sizeof(status), 1, fp);
    fclose(fp);
}




