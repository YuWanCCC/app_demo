/*
 * Copyright (c) 2022 Rockchip, Inc. All Rights Reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef __LV_RGA_COPY_H__
#define __LV_RGA_COPY_H__

#include <rga/rga.h>

#ifdef UI_USE_RGA_CPP
#include <rga/RgaApi.h>
#else
#include <im2d.h>
#endif

typedef enum {
    /* Rotation */
    LV_RGA_TRANSFORM_ROT_90     = 1 << 0,
    LV_RGA_TRANSFORM_ROT_180    = 1 << 1,
    LV_RGA_TRANSFORM_ROT_270    = 1 << 2,
    LV_RGA_TRANSFORM_ROT_NONE   = 0xFF
} LV_RGA_ROTATION_E;

typedef struct _lv_rga_rect {
    int32_t xoffset;
    int32_t yoffset;
    int32_t width;
    int32_t height;
    int32_t wstride;
    int32_t hstride;
    int32_t format;
} lv_rga_rect_t;

typedef struct _lv_rga_info {
    int32_t fd;
    void *virAddr;
    int32_t format;
    lv_rga_rect_t rect;
} lv_rga_info_t;

int32_t lv_rga_copy(lv_rga_info_t *src_image, lv_rga_info_t *dst_image, LV_RGA_ROTATION_E rotation);

#endif