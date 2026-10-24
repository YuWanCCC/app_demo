#include "media_init.h"
#include "isp.h"
#include "rk_rgn.h"
#include "rk_venc.h"
#include "rk_vi.h"
#include "rk_vdec.h"
#include "rk_vo.h"
#include "rk_vpss.h"
#include "rtsp_demo.h"

#include <pthread.h>

#define IQ_FILE_PATH "/oem/usr/share/iqfiles/"

static pthread_t g_vpss_thread;
static pthread_t g_venc_thread;
static void (*g_rgb888_buffer_cb)(const VIDEO_FRAME_INFO_S &frame);
static VENC_STREAM_S g_venc_frame = {0};
rtsp_demo_handle g_rtsplive = NULL;
rtsp_session_handle g_rtsp_session = NULL;
pthread_mutex_t g_rtsp_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool threadExit = false;

static void *vpss_thread_func(void *arg) {
  printf("VPSS Thread Started\n");
  int s32Ret = 0;
  RK_S32 waitTime = 1000;
  VIDEO_FRAME_INFO_S vpssFrame = {0};
  while (!threadExit) {
    {
      s32Ret = RK_MPI_VPSS_GetChnFrame(0, 1, &vpssFrame, waitTime);
      if (s32Ret == RK_SUCCESS) {
        if (g_rgb888_buffer_cb) {
          g_rgb888_buffer_cb(vpssFrame);
        }
      } else {
        printf("RK_MPI_VPSS_GetChnFrame fail %#x\n", s32Ret);
      }
    }
  }
  printf("VPSS Thread Exited\n");
  return NULL;
}

static void *venc_thread_func(void *arg) {
  printf("VENC Thread Started\n");
  int s32Ret = 0;
  RK_S32 waitTime = 1000;
  while (!threadExit) {
    {
      s32Ret = RK_MPI_VENC_GetStream(0, &g_venc_frame, waitTime);
      if (s32Ret == RK_SUCCESS) {
        unsigned char *data = (unsigned char *)RK_MPI_MB_Handle2VirAddr(
            g_venc_frame.pstPack->pMbBlk);
        pthread_mutex_lock(&g_rtsp_mutex);
        if (g_rtsp_session) {
          rtsp_tx_video(g_rtsp_session, data, g_venc_frame.pstPack->u32Len,
                        g_venc_frame.pstPack->u64PTS);
          rtsp_do_event(g_rtsplive);
        }
        pthread_mutex_unlock(&g_rtsp_mutex);
        s32Ret = RK_MPI_VENC_ReleaseStream(0, &g_venc_frame);
        if (s32Ret != RK_SUCCESS) {
          printf("RK_MPI_VENC_ReleaseStream fail %#x\n", s32Ret);
        }
      } else {
        printf("RK_MPI_VENC_GetStream fail %#x\n", s32Ret);
      }
    }
  }
  printf("VENC Thread Exited\n");
  return NULL;
}

int media_release_vpss_frame(VIDEO_FRAME_INFO_S *frame) {
  return RK_MPI_VPSS_ReleaseChnFrame(0, 1, frame);
}

int media_init(int is_usbcam) {
  int ret = RK_SUCCESS;

  pthread_mutex_lock(&g_rtsp_mutex);
  g_rtsplive = create_rtsp_demo(554);
  if (!g_rtsplive) {
    printf("create_rtsp_demo failed\n");
    pthread_mutex_unlock(&g_rtsp_mutex);
    return -1;
  }
  g_rtsp_session = rtsp_new_session(g_rtsplive, "/live/0");
  if (!g_rtsp_session) {
    printf("rtsp_new_session failed\n");
    pthread_mutex_unlock(&g_rtsp_mutex);
    return -1;
  }
  rtsp_set_video(g_rtsp_session, RTSP_CODEC_ID_VIDEO_H264, NULL, 0);
  pthread_mutex_unlock(&g_rtsp_mutex);

  RK_MPI_SYS_Init();

  if (!is_usbcam) {
    ret = rk_isp_init(0, IQ_FILE_PATH);
    if (0 != ret) {
    printf("rk_isp_init failed %d\n", ret);
      return ret;
    }
  }

  ret = rk_vi_init(0, 0, 1920, 1080, is_usbcam);
  if (ret != RK_SUCCESS) {
    printf("rk_vi_init failed %#x\n", ret);
    return ret;
  }

  if (is_usbcam) {
    ret = rk_vdec_init(0, RK_VIDEO_ID_MJPEG, 1920, 1080);
    if (RK_SUCCESS != ret) {
      printf("vdec_init failed: %d", ret);
      return ret;
    }
  }

  ret = rk_vpss_init(1920, 1080, 30, 10);
  if (ret != RK_SUCCESS) {
    printf("rk_vpss_init failed %#x\n", ret);
    return ret;
  }

  ret = rk_rgn_init(1920, 1080);
  if (ret != RK_SUCCESS) {
    printf("rk_rgn_init failed %#x\n", ret);
    return ret;
  }

  ret = rk_vo_init(1920, 1080);
  if (ret != RK_SUCCESS) {
    printf("rk_vo_init failed %#x\n", ret);
    return ret;
  }

  g_venc_frame.pstPack = (VENC_PACK_S *)(malloc(sizeof(VENC_PACK_S)));
  ret = rk_venc_init(0, 1920, 1080, RK_VIDEO_ID_AVC);
  if (ret != RK_SUCCESS) {
    printf("rk_venc_init failed %#x\n", ret);
    return ret;
  }

  MPP_CHN_S stVISrcChn, stVPSSDestChn;
  stVISrcChn.enModId = RK_ID_VI;
  stVISrcChn.s32ChnId = 0;
  stVISrcChn.s32DevId = 0;
  stVPSSDestChn.enModId = RK_ID_VPSS;
  stVPSSDestChn.s32ChnId = 0;
  stVPSSDestChn.s32DevId = 0;
  if (is_usbcam) {
    stVISrcChn.s32DevId = VI_MAX_PIPE_NUM - 1;
    MPP_CHN_S stVDECDestcChn;
    stVDECDestcChn.enModId = RK_ID_VDEC;
    stVDECDestcChn.s32ChnId = 0;
    stVDECDestcChn.s32DevId = 0;
    ret = RK_MPI_SYS_Bind(&stVISrcChn, &stVDECDestcChn);
    if (ret != RK_SUCCESS) {
      printf("RK_MPI_SYS_Bind vi vdec failed %#x\n", ret);
      return ret;
    }
    ret = RK_MPI_SYS_Bind(&stVDECDestcChn, &stVPSSDestChn);
    if (ret != RK_SUCCESS) {
      printf("RK_MPI_SYS_Bind vdec vpss failed %#x\n", ret);
      return ret;
    }
  } else {
    ret = RK_MPI_SYS_Bind(&stVISrcChn, &stVPSSDestChn);
    if (ret != RK_SUCCESS) {
      printf("RK_MPI_SYS_Bind vi vpss failed %#x\n", ret);
      return ret;
    }
  }

  MPP_CHN_S stVencChn;
  stVencChn.enModId = RK_ID_VENC;
  stVencChn.s32DevId = 0;
  stVencChn.s32ChnId = 0;
  ret = RK_MPI_SYS_Bind(&stVPSSDestChn, &stVencChn);
  if (ret != RK_SUCCESS) {
    printf("RK_MPI_SYS_Bind vpss venc failed %#x\n", ret);
    return ret;
  }

  MPP_CHN_S stVOChn;
  stVOChn.enModId = RK_ID_VO;
  stVOChn.s32DevId = 1;
  stVOChn.s32ChnId = 0;
  ret = RK_MPI_SYS_Bind(&stVPSSDestChn, &stVOChn);
  if (ret != RK_SUCCESS) {
    printf("RK_MPI_SYS_Bind vpss vo failed %#x\n", ret);
    return ret;
  }

  pthread_create(&g_vpss_thread, NULL, vpss_thread_func, NULL);
  pthread_create(&g_venc_thread, NULL, venc_thread_func, NULL);

  return ret;
}

int media_deinit() {
  int ret = RK_SUCCESS;
  threadExit = true;
  pthread_join(g_venc_thread, NULL);
  pthread_join(g_vpss_thread, NULL);
  MPP_CHN_S stVISrcChn, stVPSSDestChn, stVencChn, stVOChn;
  stVISrcChn.enModId = RK_ID_VI;
  stVISrcChn.s32ChnId = 0;
  stVISrcChn.s32DevId = 0;
  stVPSSDestChn.enModId = RK_ID_VPSS;
  stVPSSDestChn.s32ChnId = 0;
  stVPSSDestChn.s32DevId = 0;
  stVencChn.enModId = RK_ID_VENC;
  stVencChn.s32DevId = 0;
  stVencChn.s32ChnId = 0;
  stVOChn.enModId = RK_ID_VO;
  stVOChn.s32DevId = 1;
  stVOChn.s32ChnId = 0;
  ret = RK_MPI_SYS_UnBind(&stVPSSDestChn, &stVOChn);
  if (ret != RK_SUCCESS) {
    printf("RK_MPI_SYS_UnBind failed %#x\n", ret);
  }
  ret = RK_MPI_SYS_UnBind(&stVPSSDestChn, &stVencChn);
  if (ret != RK_SUCCESS) {
    printf("RK_MPI_SYS_UnBind failed %#x\n", ret);
  }
  ret = RK_MPI_SYS_UnBind(&stVISrcChn, &stVPSSDestChn);
  if (ret != RK_SUCCESS) {
    printf("RK_MPI_SYS_UnBind failed %#x\n", ret);
  }
  rk_vo_deinit();
  rk_venc_deinit(0);
  if (g_venc_frame.pstPack) {
    free(g_venc_frame.pstPack);
  }
  rk_rgn_deinit();
  rk_vpss_deinit();
  rk_vi_deinit(0, 0);
  rk_isp_deinit(0);
  RK_MPI_SYS_Exit();

  pthread_mutex_lock(&g_rtsp_mutex);
  if (g_rtsp_session) {
    rtsp_del_session(g_rtsp_session);
    g_rtsp_session = NULL;
  }
  if (g_rtsplive) {
    rtsp_del_demo(g_rtsplive);
    g_rtsplive = NULL;
  }
  pthread_mutex_unlock(&g_rtsp_mutex);

  return 0;
}

void media_set_rgb888_buffer_cb(void (*cb)(const VIDEO_FRAME_INFO_S &frame)) {
  g_rgb888_buffer_cb = cb;
}
