// Copyright 2020-2021 Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __RKIPC_ROCKIVA_H__
#define __RKIPC_ROCKIVA_H__

#ifdef __cplusplus
extern "C" {
#endif
#include "rockiva/rockiva_det_api.h"

int rkuvc_rockiva_init(int width, int height);
int rkuvc_rockiva_deinit();
int rkuvc_rockiva_write_rgb888_frame(uint16_t width, uint16_t height,
                                     uint32_t frame_id, unsigned char *buffer);

int rkuvc_rockiva_write_rgb888_frame_by_fd(uint16_t width, uint16_t height,
                                           uint32_t frame_id, int32_t fd);

int rkuvc_rockiva_write_nv12_frame_by_fd(uint16_t width, uint16_t height,
                                         uint32_t frame_id, int32_t fd);

RockIvaDetectResult rkuvc_rockiva_get_result();
#ifdef __cplusplus
}
#endif
#endif
