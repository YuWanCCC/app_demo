#include "isp.h"

#include <rk_aiq_user_api2_acsm.h>
#include <rk_aiq_user_api2_camgroup.h>
#include <rk_aiq_user_api2_imgproc.h>
#include <rk_aiq_user_api2_isp.h>
#include <rk_aiq_user_api2_sysctl.h>

#define MAX_AIQ_CTX 8
char g_iq_file_dir_[256];
static int light_level = -1;
static int light_state = -1;
static int rkipc_aiq_use_group = 0;
static rk_aiq_sys_ctx_t *g_aiq_ctx[MAX_AIQ_CTX];
static rk_aiq_camgroup_ctx_t *g_camera_group_ctx[MAX_AIQ_CTX];
rk_aiq_working_mode_t g_WDRMode[MAX_AIQ_CTX];
rk_aiq_wb_gain_t gs_wb_gain = {2.083900, 1.000000, 1.000000, 2.018500};

#define RK_ISP_CHECK_CAMERA_ID(CAMERA_ID)                                      \
  do {                                                                         \
    if (rkipc_aiq_use_group) {                                                 \
      if (CAMERA_ID >= MAX_AIQ_CTX || !g_camera_group_ctx[CAMERA_ID]) {        \
        printf("camera_group_id is over 3 or not init\n");                     \
        return -1;                                                             \
      }                                                                        \
    } else {                                                                   \
      if (CAMERA_ID >= MAX_AIQ_CTX || !g_aiq_ctx[CAMERA_ID]) {                 \
        printf("camera_id is over 3 or not init\n");                           \
        return -1;                                                             \
      }                                                                        \
    }                                                                          \
  } while (0)

#define RK_ISP_CHECK_NORMAL_MODE(CAMERA_ID)                                    \
  do {                                                                         \
    if (g_WDRMode[cam_id] != RK_AIQ_WORKING_MODE_NORMAL) {                     \
      printf("Not support in HDR mode\n");                                     \
      return 0;                                                                \
    }                                                                          \
  } while (0)

rk_aiq_sys_ctx_t *rkipc_aiq_get_ctx(int cam_id) {
  if (rkipc_aiq_use_group)
    return (rk_aiq_sys_ctx_t *)g_camera_group_ctx[cam_id];

  return g_aiq_ctx[cam_id];
}

int sample_common_isp_init(int cam_id, rk_aiq_working_mode_t WDRMode,
                           bool MultiCam, const char *iq_file_dir) {
  if (cam_id >= MAX_AIQ_CTX) {
    printf("%s : cam_id is over 3\n");
    return -1;
  }
  setlinebuf(stdout);
  if (iq_file_dir == NULL) {
    printf("rk_isp_init : not start.\n");
    g_aiq_ctx[cam_id] = NULL;
    return 0;
  }

  // must set HDR_MODE, before init
  g_WDRMode[cam_id] = WDRMode;
  char hdr_str[16];
  snprintf(hdr_str, sizeof(hdr_str), "%d", (int)WDRMode);
  setenv("HDR_MODE", hdr_str, 1);

  rk_aiq_sys_ctx_t *aiq_ctx;
  rk_aiq_static_info_t aiq_static_info;
  rk_aiq_uapi2_sysctl_enumStaticMetasByPhyId(cam_id, &aiq_static_info);
  if (aiq_static_info.sensor_info.phyId == -1) {
    printf("WARN: aiq_static_info.sensor_info.phyId is %d\n",
           aiq_static_info.sensor_info.phyId);
  }

  printf("ID: %d, sensor_name is %s, iqfiles is %s\n", cam_id,
         aiq_static_info.sensor_info.sensor_name, iq_file_dir);
  int ret;
  if (WDRMode == RK_AIQ_WORKING_MODE_NORMAL)
    ret = rk_aiq_uapi2_sysctl_preInit_scene(
        aiq_static_info.sensor_info.sensor_name, "normal", "day");
  else
    ret = rk_aiq_uapi2_sysctl_preInit_scene(
        aiq_static_info.sensor_info.sensor_name, "hdr", "day");
  if (ret < 0)
    printf("%s: failed to set scene\n",
           aiq_static_info.sensor_info.sensor_name);

#if 1
  aiq_ctx = rk_aiq_uapi2_sysctl_init(aiq_static_info.sensor_info.sensor_name,
                                     iq_file_dir, NULL, NULL);
// printf("tmp force use m00_b_imx464 3-001a\n");
#else
  if (cam_id == 0)
    aiq_ctx = rk_aiq_uapi2_sysctl_init("m02_b_imx464 3-001a", iq_file_dir, NULL,
                                       NULL);
  else if (cam_id == 1)
    aiq_ctx = rk_aiq_uapi2_sysctl_init("m03_b_imx464 3-0036", iq_file_dir, NULL,
                                       NULL);
  else if (cam_id == 2)
    aiq_ctx = rk_aiq_uapi2_sysctl_init("m00_b_imx464 4-001a", iq_file_dir, NULL,
                                       NULL);
  else if (cam_id == 3)
    aiq_ctx = rk_aiq_uapi2_sysctl_init("m01_b_imx464 4-0036", iq_file_dir, NULL,
                                       NULL);
  else if (cam_id == 4)
    aiq_ctx = rk_aiq_uapi2_sysctl_init("m04_b_imx464 5-001a", iq_file_dir, NULL,
                                       NULL);
  else if (cam_id == 5)
    aiq_ctx = rk_aiq_uapi2_sysctl_init("m05_b_imx464 5-0036", iq_file_dir, NULL,
                                       NULL);
#endif
  // if (MultiCam)
  // 	rk_aiq_uapi2_sysctl_setMulCamConc(aiq_ctx, true);

  aibnr_api_attrib_t aibnr_attr;
  rk_aiq_user_api2_aibnr_GetAttrib(aiq_ctx, &aibnr_attr);
  aibnr_attr.en = 0;
  rk_aiq_user_api2_aibnr_SetAttrib(aiq_ctx, &aibnr_attr);
  g_aiq_ctx[cam_id] = aiq_ctx;

  return 0;
}

int sample_common_isp_run(int cam_id) {
  RK_ISP_CHECK_CAMERA_ID(cam_id);
  if (rk_aiq_uapi2_sysctl_prepare(g_aiq_ctx[cam_id], 0, 0, g_WDRMode[cam_id])) {
    printf("rkaiq engine prepare failed !\n");
    g_aiq_ctx[cam_id] = NULL;
    return -1;
  }
  printf("rk_aiq_uapi2_sysctl_init/prepare succeed\n");
  if (rk_aiq_uapi2_sysctl_start(g_aiq_ctx[cam_id])) {
    printf("rk_aiq_uapi2_sysctl_start  failed\n");
    return -1;
  }
  printf("rk_aiq_uapi2_sysctl_start succeed\n");
  return 0;
}

int rk_isp_init(int cam_id, const char *iqfile_path) {
  printf("cam_id is %d\n", cam_id);
  int ret;
  rkipc_aiq_use_group = 0;
  if (iqfile_path)
    memcpy(g_iq_file_dir_, iqfile_path, strlen(iqfile_path));
  else
    memcpy(g_iq_file_dir_, "/etc/iqfiles", strlen("/etc/iqfiles"));
  printf("g_iq_file_dir_ is %s\n", g_iq_file_dir_);

  ret = sample_common_isp_init(cam_id, RK_AIQ_WORKING_MODE_NORMAL, true,
                               g_iq_file_dir_);

  ret |= sample_common_isp_run(cam_id);

  return ret;
}

int rk_isp_deinit(int cam_id) {
  printf("cam_id is %d\n", cam_id);
  RK_ISP_CHECK_CAMERA_ID(cam_id);
  printf("rk_aiq_uapi2_sysctl_stop enter\n");
  rk_aiq_uapi2_sysctl_stop(g_aiq_ctx[cam_id], false);
  printf("rk_aiq_uapi2_sysctl_deinit enter\n");
  rk_aiq_uapi2_sysctl_deinit(g_aiq_ctx[cam_id]);
  printf("rk_aiq_uapi2_sysctl_deinit exit\n");
  g_aiq_ctx[cam_id] = NULL;

  return 0;
}

void *rk_isp_get_ctx(int cam_id) { return g_aiq_ctx[cam_id]; }
