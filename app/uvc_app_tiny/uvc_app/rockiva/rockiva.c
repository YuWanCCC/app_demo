// Copyright 2021 Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "rockiva.h"
#include "iniparser.h"
#include "param.h"
#include "uvc_log.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "rockiva.c"

RockIvaHandle rkba_handle;
RockIvaDetTaskParams initParams;
RockIvaInitParam globalParams;
RockIvaDetectResult gRockivaResult;
int rockit_run_flag = 0;

RockIvaDetectResult rkuvc_rockiva_get_result() { return gRockivaResult; }

void detect_result_callback(const RockIvaDetectResult *result,
                            const RockIvaExecuteStatus status, void *userData) {
  if (status == ROCKIVA_SUCCESS) {
    memcpy(&gRockivaResult, result, sizeof(RockIvaDetectResult));
  }
  LOG_DEBUG("status is %d, frame %d, result->objNum is %d\n", status,
            result->frameId, result->objNum);
  for (int i = 0; i < result->objNum; i++) {
    LOG_DEBUG("topLeft:[%d,%d], bottomRight:[%d,%d],",
              //"objId is %d, frameId is %d, score is %d, type is %d\n",
              result->objInfo[i].rect.topLeft.x,
              result->objInfo[i].rect.topLeft.y,
              result->objInfo[i].rect.bottomRight.x,
              result->objInfo[i].rect.bottomRight.y);

    LOG_DEBUG("objId is %d, frameId is %d, score is %d, type is %d\n",
              result->objInfo[i].objId, result->objInfo[i].frameId,
              result->objInfo[i].score, result->objInfo[i].type);
    // LOG_INFO("triggerRules is %d, ruleID is %d, triggerType is %d\n",
    //          result->triggerObjects[i].triggerRules,
    //          result->triggerObjects[i].firstTrigger.ruleID,
    //          result->triggerObjects[i].firstTrigger.triggerType);
  }
  nn_process_release_frame_by_id(result->frameId);
}

int rkuvc_rockiva_init(int width, int height) {
  RockIvaRetCode ret;
  const char *model_type;
  // char *license_path = NULL;
  // char *license_key;
  // int license_size;

  memset(&initParams, 0, sizeof(initParams));
  memset(&globalParams, 0, sizeof(globalParams));
  memset(&gRockivaResult, 0, sizeof(RockIvaDetectResult));

  // if (license_path != NULL) {
  //     license_size = read_data_file(license_path, &license_key);
  //     if (license_key != NULL && license_size > 0) {
  //         globalParams.license.memAddr = license_key;
  //         globalParams.license.memSize = license_size;
  //     }
  // }

  snprintf(globalParams.modelPath, ROCKIVA_PATH_LENGTH, "/oem/usr/lib");
  globalParams.coreMask = 0x04;
  globalParams.logLevel = ROCKIVA_LOG_ERROR;
  globalParams.detModel |= ROCKIVA_DET_MODEL_PFP;

  globalParams.imageInfo.width = width;
  globalParams.imageInfo.height = height;
  LOG_INFO("iva init width %d, height %d\n", width, height);
  ROCKIVA_Init(&rkba_handle, ROCKIVA_MODE_VIDEO, &globalParams, NULL);

  initParams.detObjectType |=
      ROCKIVA_OBJECT_TYPE_BITMASK(ROCKIVA_OBJECT_TYPE_FACE);
  ret = ROCKIVA_DETECT_Init(rkba_handle, &initParams, detect_result_callback);
  if (ret != ROCKIVA_RET_SUCCESS) {
    LOG_ERROR("ROCKIVA_DETECT_Init error %d\n", ret);
    return -1;
  }
  rockit_run_flag = 1;

  return ret;
}

int rkuvc_rockiva_deinit() {
  rockit_run_flag = 0;
  ROCKIVA_DETECT_Release(rkba_handle);
  ROCKIVA_Release(rkba_handle);

  return 0;
}

int rkuvc_rockiva_write_rgb888_frame(uint16_t width, uint16_t height,
                                     uint32_t frame_id, unsigned char *buffer) {
  int ret;
  RockIvaImage image;
  if (!rockit_run_flag)
    return 0;

  memset(&image, 0, sizeof(RockIvaImage));
  image.info.width = width;
  image.info.height = height;
  image.info.format = ROCKIVA_IMAGE_FORMAT_BGR888;
  image.dataAddr = buffer;
  image.frameId = frame_id;
  ret = ROCKIVA_PushFrame(rkba_handle, &image, NULL);

  return ret;
}

int rkuvc_rockiva_write_rgb888_frame_by_fd(uint16_t width, uint16_t height,
                                           uint32_t frame_id, int32_t fd) {
  int ret;
  RockIvaImage image;

  if (!rockit_run_flag)
    return 0;

  memset(&image, 0, sizeof(RockIvaImage));
  image.info.width = width;
  image.info.height = height;
  image.info.format = ROCKIVA_IMAGE_FORMAT_BGR888;
  image.frameId = frame_id;
  image.dataAddr = NULL;
  image.dataPhyAddr = NULL;
  image.dataFd = fd;
  ret = ROCKIVA_PushFrame(rkba_handle, &image, NULL);

  return ret;
}

int rkuvc_rockiva_write_nv12_frame_by_fd(uint16_t width, uint16_t height,
                                         uint32_t frame_id, int32_t fd) {
  int ret;
  RockIvaImage image;

  if (!rockit_run_flag)
    return 0;

  memset(&image, 0, sizeof(RockIvaImage));
  image.info.width = width;
  image.info.height = height;
  image.info.format = ROCKIVA_IMAGE_FORMAT_YUV420SP_NV12;
  image.frameId = frame_id;
  image.dataAddr = NULL;
  image.dataPhyAddr = NULL;
  image.dataFd = fd;
  ret = ROCKIVA_PushFrame(rkba_handle, &image, NULL);

  return ret;
}
