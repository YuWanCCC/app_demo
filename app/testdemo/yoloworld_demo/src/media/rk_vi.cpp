#include "rk_vi.h"

#include <cstdio>
#include <cstring>
#include <unistd.h>

static int vi_dev_init() {
  printf("%s\n", __func__);
  int ret = 0;
  int devId = 0;
  int pipeId = devId;

  VI_DEV_ATTR_S stDevAttr;
  VI_DEV_BIND_PIPE_S stBindPipe;
  memset(&stDevAttr, 0, sizeof(stDevAttr));
  memset(&stBindPipe, 0, sizeof(stBindPipe));
  // 0. get dev config status
  ret = RK_MPI_VI_GetDevAttr(devId, &stDevAttr);
  if (ret == RK_ERR_VI_NOT_CONFIG) {
    // 0-1.config dev
    ret = RK_MPI_VI_SetDevAttr(devId, &stDevAttr);
    if (RK_SUCCESS != ret) {
      printf("RK_MPI_VI_SetDevAttr %x\n", ret);
      return -1;
    }
  } else {
    printf("RK_MPI_VI_SetDevAttr already\n");
  }
  // 1.get dev enable status
  ret = RK_MPI_VI_GetDevIsEnable(devId);
  if (RK_SUCCESS != ret) {
    // 1-2.enable dev
    ret = RK_MPI_VI_EnableDev(devId);
    if (RK_SUCCESS != ret) {
      printf("RK_MPI_VI_EnableDev %x\n", ret);
      return -1;
    }
    // 1-3.bind dev/pipe
    stBindPipe.u32Num = 1;
    stBindPipe.PipeId[0] = pipeId;
    ret = RK_MPI_VI_SetDevBindPipe(devId, &stBindPipe);
    if (RK_SUCCESS != ret) {
      printf("RK_MPI_VI_SetDevBindPipe %x\n", ret);
      return -1;
    }
  } else {
    printf("RK_MPI_VI_EnableDev already\n");
  }

#if 0
    // not use vi extchn
    VI_PARAM_MOD_S stModParam;
    memset(&stModParam, 0, sizeof(VI_PARAM_MOD_S));
    stModParam.enViModType = VI_EXT_CHN_MODE;
    stModParam.stExtChnParam.mirrorCmsc = 1;
    if (ret = RK_MPI_VI_SetModParam(&stModParam)) {
        printf("RK_MPI_VI_SetModParam fail:%#X", ret);
    }
    memset(&stModParam, 0, sizeof(stModParam));
    stModParam.enViModType = VI_EXT_CHN_MODE;
    ret = RK_MPI_VI_GetModParam(&stModParam);
    if (ret)
        printf("RK_MPI_VI_GetModParam fail:%#X", ret);

    printf("vi mod:%d mirrorCmsc:%d\n",
    stModParam.enViModType, stModParam.stExtChnParam.mirrorCmsc);
#endif

  printf("vi dev init success\n");
  return 0;
}

static int vi_chn_init(int channelId, int width, int height, int bufCnt,
                       const char *video_path,
                       RK_CODEC_ID_E enCodecId, int is_usbcam) {
  int ret;
  // virtual pipeId
  int pipeId = is_usbcam ? VI_MAX_PIPE_NUM - 1 : 0;
  // VI init
  VI_CHN_ATTR_S vi_chn_attr;
  memset(&vi_chn_attr, 0, sizeof(vi_chn_attr));
  vi_chn_attr.stIspOpt.u32BufCount = bufCnt;
  if (enCodecId != RK_VIDEO_ID_Unused) {
    vi_chn_attr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_MMAP;
  } else {
    vi_chn_attr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
  }
  if (pipeId == VI_MAX_PIPE_NUM - 1 && video_path)
    memcpy(vi_chn_attr.stIspOpt.aEntityName, video_path, strlen(video_path));
  vi_chn_attr.stSize.u32Width = width;
  vi_chn_attr.stSize.u32Height = height;
  vi_chn_attr.enPixelFormat = RK_FMT_YUV420SP;
  vi_chn_attr.enCompressMode = COMPRESS_MODE_NONE; // COMPRESS_AFBC_16x16;
  ret = RK_MPI_VI_SetChnAttr(pipeId, channelId, &vi_chn_attr);

  if (enCodecId != RK_VIDEO_ID_Unused) {
    ret = RK_MPI_VI_SetChnStreamCodec(pipeId, channelId, enCodecId);
    if (ret != RK_SUCCESS) {
      printf("RK_MPI_VI_SetChnStreamCodec %x", ret);
      return ret;
    }
  }

  ret |= RK_MPI_VI_EnableChn(pipeId, channelId);
  if (ret) {
    printf("ERROR: create VI error! ret=%#x\n", ret);
    return ret;
  }

  printf("vi chn init success\n");
  return ret;
}

int rk_vi_init(int dev_id, int chn_id, int width, int height, int is_usbcam) {
  int ret;
  if (!is_usbcam) {
    ret = vi_dev_init();
    if (ret != 0) {
      printf("vi dev init failed\n");
      return ret;
    }
  }
  if (is_usbcam) {
    ret = vi_chn_init(chn_id, width, height, 2, "/dev/video1", RK_VIDEO_ID_MJPEG, is_usbcam);
    if (ret != 0) {
      printf("vi chn init failed\n");
      return ret;
    }
  } else {
    ret = vi_chn_init(chn_id, width, height, 2, NULL, RK_VIDEO_ID_Unused, is_usbcam);
    if (ret != 0) {
      printf("vi chn init failed\n");
      return ret;
    }
  }
  printf("vi init success\n");
  return 0;
}

int rk_vi_deinit(int dev_id, int chn_id) {
  int ret;
  ret = RK_MPI_VI_DisableChn(dev_id, chn_id);
  if (ret != RK_SUCCESS) {
    printf("vi disable chn failed\n");
    return ret;
  }
  ret = RK_MPI_VI_DisableDev(dev_id);
  if (ret != RK_SUCCESS) {
    printf("vi disable dev failed\n");
    return ret;
  }
  printf("vi deinit success\n");
  return 0;
}
