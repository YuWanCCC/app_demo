#include "rk_venc.h"

#include <cstdio>
#include <cstring>
#include <unistd.h>

int rk_venc_init(int chnId, int width, int height, RK_CODEC_ID_E enType) {
  VENC_CHN_ATTR_S stAttr;
  memset(&stAttr, 0, sizeof(VENC_CHN_ATTR_S));

  int ret = 0;

  if (enType == RK_VIDEO_ID_AVC) {
    stAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
    stAttr.stRcAttr.stH264Cbr.u32BitRate = 10 * 1024;
    stAttr.stRcAttr.stH264Cbr.u32Gop = 60;
  } else if (enType == RK_VIDEO_ID_HEVC) {
    stAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265CBR;
    stAttr.stRcAttr.stH265Cbr.u32BitRate = 10 * 1024;
    stAttr.stRcAttr.stH265Cbr.u32Gop = 60;
  } else if (enType == RK_VIDEO_ID_MJPEG) {
    stAttr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGCBR;
    stAttr.stRcAttr.stMjpegCbr.u32BitRate = 10 * 1024;
  }

  stAttr.stVencAttr.enType = enType;
  stAttr.stVencAttr.enPixelFormat = RK_FMT_YUV420SP;
  if (enType == RK_VIDEO_ID_AVC) {
    stAttr.stVencAttr.u32Profile = H264E_PROFILE_HIGH;
    stAttr.stVencAttr.stAttrH264e.u32Level = 41;
  }
  stAttr.stVencAttr.u32PicWidth = width;
  stAttr.stVencAttr.u32PicHeight = height;
  stAttr.stVencAttr.u32VirWidth = width;
  stAttr.stVencAttr.u32VirHeight = height;
  stAttr.stVencAttr.u32StreamBufCnt = 2;
  stAttr.stVencAttr.u32BufSize = width * height / 2;
  stAttr.stVencAttr.enMirror = MIRROR_NONE;

  ret = RK_MPI_VENC_CreateChn(chnId, &stAttr);
  if (ret != RK_SUCCESS) {
    printf("RK_MPI_VENC_CreateChn error! ret is %#x\n", ret);
    return ret;
  }

  VENC_CHN_REF_BUF_SHARE_S stVencChnRefBufShare;
  memset(&stVencChnRefBufShare, 0, sizeof(VENC_CHN_REF_BUF_SHARE_S));
  stVencChnRefBufShare.bEnable = RK_TRUE;
  ret = RK_MPI_VENC_SetChnRefBufShareAttr(chnId, &stVencChnRefBufShare);
  if (ret != RK_SUCCESS) {
    printf("RK_MPI_VENC_SetChnRefBufShareAttr error! ret is %#x\n", ret);
  }

  VENC_RECV_PIC_PARAM_S stRecvParam;
  memset(&stRecvParam, 0, sizeof(VENC_RECV_PIC_PARAM_S));
  stRecvParam.s32RecvPicNum = -1;
  ret = RK_MPI_VENC_StartRecvFrame(chnId, &stRecvParam);
  if (ret != RK_SUCCESS) {
    printf("RK_MPI_VENC_StartRecvFrame error! ret is %#x\n", ret);
    return ret;
  }

  printf("venc chn %d init success\n", chnId);

  return ret;
}

int rk_venc_deinit(int chnId) {
  int ret = 0;
  ret = RK_MPI_VENC_StopRecvFrame(chnId);
  if (ret != RK_SUCCESS) {
    printf("RK_MPI_VENC_StopRecvFrame failed: %#x\n", ret);
  }
  ret = RK_MPI_VENC_DestroyChn(chnId);
  if (ret != RK_SUCCESS) {
    printf("RK_MPI_VENC_DestroyChn failed: %#x\n", ret);
  }
  return ret;
}

int media_insert_userdata2venc(unsigned char *userdata, int len) {
  int ret = 0;
  ret = RK_MPI_VENC_InsertUserData(0, userdata, len);
  if (ret != RK_SUCCESS) {
    printf("RK_MPI_VENC_InsertUserData failed: %#x\n", ret);
  }
  return ret;
}
