#include "rk_vpss.h"

#include <cstdio>
#include <cstring>
#include <unistd.h>

int rk_vpss_init(int width, int height, int src_fps, int dst_fps) {
  int ret;

  // VPSS
  VPSS_CHN VpssChn[2] = {0, 1};
  VPSS_GRP VpssGrp = 0;
  VPSS_GRP_ATTR_S stVpssGrpAttr;
  VPSS_CHN_ATTR_S stVpssChnAttr[2];
  memset(&stVpssGrpAttr, 0, sizeof(stVpssGrpAttr));
  memset(&stVpssChnAttr, 0, sizeof(stVpssChnAttr));
  stVpssGrpAttr.u32MaxW = width;
  stVpssGrpAttr.u32MaxH = height;
  stVpssGrpAttr.enPixelFormat = RK_FMT_YUV420SP;
  stVpssGrpAttr.stFrameRate.s32SrcFrameRate = -1;
  stVpssGrpAttr.stFrameRate.s32DstFrameRate = -1;
  stVpssGrpAttr.enCompressMode = COMPRESS_MODE_NONE;
  stVpssGrpAttr.enVProcDev = VIDEO_PROC_DEV_VPSS;
  ret = RK_MPI_VPSS_CreateGrp(VpssGrp, &stVpssGrpAttr);
  if (ret != RK_SUCCESS) {
    printf("RK_MPI_VPSS_CreateGrp error! ret is %#x\n", ret);
    return ret;
  }
  stVpssChnAttr[0].enChnMode = VPSS_CHN_MODE_USER;
  stVpssChnAttr[0].enDynamicRange = DYNAMIC_RANGE_SDR8;
  stVpssChnAttr[0].enPixelFormat = RK_FMT_YUV420SP;
  stVpssChnAttr[0].stFrameRate.s32SrcFrameRate = -1;
  stVpssChnAttr[0].stFrameRate.s32DstFrameRate = -1;
  stVpssChnAttr[0].u32Width = width;
  stVpssChnAttr[0].u32Height = height;
  stVpssChnAttr[0].enCompressMode = COMPRESS_MODE_NONE;
  stVpssChnAttr[0].u32FrameBufCnt = 2;
  ret = RK_MPI_VPSS_SetChnAttr(VpssGrp, VpssChn[0], &stVpssChnAttr[0]);
  if (ret != RK_SUCCESS)
    printf("0: RK_MPI_VPSS_SetChnAttr error! ret is %#x\n", ret);
  ret = RK_MPI_VPSS_EnableChn(VpssGrp, VpssChn[0]);
  if (ret != RK_SUCCESS)
    printf("0: RK_MPI_VPSS_EnableChn error! ret is %#x\n", ret);
  stVpssChnAttr[1].enChnMode = VPSS_CHN_MODE_USER;
  stVpssChnAttr[1].enDynamicRange = DYNAMIC_RANGE_SDR8;
  stVpssChnAttr[1].enPixelFormat = RK_FMT_YUV420SP;
  stVpssChnAttr[1].stFrameRate.s32SrcFrameRate = -1;
  stVpssChnAttr[1].stFrameRate.s32DstFrameRate = -1;
  stVpssChnAttr[1].u32Width = 640;
  stVpssChnAttr[1].u32Height = 640;
  stVpssChnAttr[1].enCompressMode = COMPRESS_MODE_NONE;
  stVpssChnAttr[1].u32Depth = 3;
  stVpssChnAttr[1].u32FrameBufCnt = 3;
  ret = RK_MPI_VPSS_SetChnAttr(VpssGrp, VpssChn[1], &stVpssChnAttr[1]);
  if (ret != RK_SUCCESS)
    printf("1: RK_MPI_VPSS_SetChnAttr error! ret is %#x\n", ret);
  ret = RK_MPI_VPSS_EnableChn(VpssGrp, VpssChn[1]);
  if (ret != RK_SUCCESS)
    printf("1: RK_MPI_VPSS_EnableChn error! ret is %#x\n", ret);
  ret = RK_MPI_VPSS_EnableBackupFrame(VpssGrp);
  if (ret != RK_SUCCESS) {
    printf("RK_MPI_VPSS_EnableBackupFrame error! ret is %#x\n", ret);
    return ret;
  }
  RK_MPI_VPSS_SetVProcDev(VpssGrp, VIDEO_PROC_DEV_VPSS);
  ret = RK_MPI_VPSS_StartGrp(VpssGrp);
  if (ret != RK_SUCCESS) {
    printf("RK_MPI_VPSS_StartGrp error! ret is %#x\n", ret);
    return ret;
  }

  printf("vpss init success\n");
  return 0;
}

int rk_vpss_deinit() {
  int ret;
  int grp_id = 0;
  ret = RK_MPI_VPSS_DisableChn(grp_id, 1);
  if (ret != RK_SUCCESS) {
    printf("RK_MPI_VPSS_DisableChn error! ret is %#x\n", ret);
  }
  ret = RK_MPI_VPSS_DisableChn(grp_id, 0);
  if (ret != RK_SUCCESS) {
    printf("RK_MPI_VPSS_DisableChn error! ret is %#x\n", ret);
  }
  ret = RK_MPI_VPSS_StopGrp(grp_id);
  if (RK_SUCCESS != ret) {
    printf("RK_MPI_VPSS_StopGrp failed: %d\n", ret);
  }
  ret = RK_MPI_VPSS_DestroyGrp(grp_id);
  if (ret != RK_SUCCESS) {
    printf("RK_MPI_VPSS_DestroyGrp failed: %d\n", ret);
  }
  printf("vpss deinit success\n");
  return 0;
}
