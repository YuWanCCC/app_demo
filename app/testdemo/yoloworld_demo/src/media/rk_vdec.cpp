#include "rk_vdec.h"

#include <cstdio>
#include <cstring>
#include <unistd.h>

int rk_vdec_init(int chn, RK_CODEC_ID_E enCodecId, int width, int height) {
  int s32Ret;

  // vdec init and create */
  VDEC_MOD_PARAM_S vdec_mod_param;
  memset(&vdec_mod_param, 0, sizeof(VDEC_MOD_PARAM_S));
  vdec_mod_param.enVdecMBSource = MB_SOURCE_MODULE;
  s32Ret = RK_MPI_VDEC_SetModParam(&vdec_mod_param);
  if (RK_SUCCESS != s32Ret) {
    printf("RK_MPI_VDEC_SetModParam fail: 0x%x\n", s32Ret);
  }
  // 创建 VDEC 通道，使用帧模式
  VDEC_CHN_ATTR_S vdec_chn_attr;
  memset(&vdec_chn_attr, 0, sizeof(VDEC_CHN_ATTR_S));

  vdec_chn_attr.enType = enCodecId;
  vdec_chn_attr.enMode = VIDEO_MODE_FRAME; // 使用帧模式
  vdec_chn_attr.u32PicWidth = width;
  vdec_chn_attr.u32PicHeight = height;
  vdec_chn_attr.enPixelFormat = RK_FMT_YUV420SP;
  vdec_chn_attr.u32FrameBufCnt = 6;
  vdec_chn_attr.u32StreamBufCnt = 8;
  vdec_chn_attr.u32FrameBufDepth = 8; // not bind,should set framebufDepth
  s32Ret = RK_MPI_VDEC_CreateChn(chn, &vdec_chn_attr);
  if (RK_SUCCESS != s32Ret) {
    printf("RK_MPI_VDEC_CreateChn failed:%x", s32Ret);
    return -1;
  }

  printf("vdec chn init success\n");
  return 0;
}