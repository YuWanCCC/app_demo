/*
 * Copyright (C) 2025 Rockchip Electronics Co., Ltd.
 * Authors:
 *  Hangyu Li <hangyu.li@rock-chips.com>
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
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <memory.h>
#include <unistd.h>
#include <sys/time.h>

#include "lv_rga.h"

#ifdef UI_USE_RGA_CPP
int lv_rga_copy(lv_rga_info_t *src_image, lv_rga_info_t *dst_image, LV_RGA_ROTATION_E rotation) {
    if (src_image == NULL || dst_image == NULL)
        return -1;

    rga_info_t src_info = {0};
    rga_info_t dst_info = {0};
    int32_t hal_rotation = 0;

    switch (rotation) {
        case LV_RGA_TRANSFORM_ROT_90:
            hal_rotation = HAL_TRANSFORM_ROT_90;
        break;

        case LV_RGA_TRANSFORM_ROT_180:
            hal_rotation = HAL_TRANSFORM_ROT_180;
        break;

        case LV_RGA_TRANSFORM_ROT_270:
            hal_rotation = HAL_TRANSFORM_ROT_270;
        break;

        default:
        break;
    }

    src_info.fd = src_image->fd;
    src_info.virAddr = src_image->virAddr;
    src_info.mmuFlag = 1;
    src_info.rotation = hal_rotation;

    dst_info.fd = dst_image->fd;
    dst_info.mmuFlag = 1;
    dst_info.virAddr = dst_image->virAddr;

    rga_set_rect(&src_info.rect, src_image->rect.xoffset, src_image->rect.yoffset,
                 src_image->rect.width, src_image->rect.height,
                 src_image->rect.wstride, src_image->rect.hstride, src_image->rect.format);
    rga_set_rect(&dst_info.rect, dst_image->rect.xoffset, dst_image->rect.yoffset,
                 dst_image->rect.width, dst_image->rect.height,
                 dst_image->rect.wstride, dst_image->rect.hstride, dst_image->rect.format);

    if (c_RkRgaBlit(&src_info, &dst_info, NULL)) {
        return -1;
    }

    return 0;
}
#endif

#ifdef UI_USE_RGA
int lv_rga_copy(lv_rga_info_t *src_image, lv_rga_info_t *dst_image, LV_RGA_ROTATION_E rotation) {
    if (src_image == NULL || dst_image == NULL) {
        return -1;
    }

    int32_t ret = 0;
    int32_t src_dma_fd, dst_dma_fd;
    int usage = 0;

    rga_buffer_t src;
    rga_buffer_t dst;
    im_rect src_rect = {0};
    im_rect dst_rect = {0};
    rga_buffer_t dummy_buf = {};
    im_rect dummy_rect = {};
    rga_buffer_handle_t src_handle;
    rga_buffer_handle_t dst_handle;

    src_dma_fd = src_image->fd;
    dst_dma_fd = dst_image->fd;

    src_handle = importbuffer_virtualaddr(src_image->virAddr,
                     &(im_handle_param_t){src_image->rect.wstride, src_image->rect.hstride, src_image->rect.format});
    if (src_handle == 0) {
        printf("Failed to import CMA buffer fd for src channel! %s\n", imStrError());
        goto release_buffer;
    }

    dst_handle = importbuffer_fd(dst_dma_fd,
                     &(im_handle_param_t){dst_image->rect.wstride, dst_image->rect.hstride, dst_image->rect.format});
    if (dst_handle == 0) {
        printf("Failed to import CMA buffer fd for src channel! %s\n", imStrError());
        goto release_buffer;
    }

    src = wrapbuffer_handle(src_handle, src_image->rect.wstride, src_image->rect.hstride, src_image->rect.format);
    dst = wrapbuffer_handle(dst_handle, dst_image->rect.wstride, dst_image->rect.hstride, dst_image->rect.format);

    src_rect.x = dst_image->rect.xoffset;
    src_rect.y = dst_image->rect.yoffset;
    src_rect.width = dst_image->rect.width;
    src_rect.height = dst_image->rect.height;

    switch (rotation) {
        case LV_RGA_TRANSFORM_ROT_90:
            usage |= IM_HAL_TRANSFORM_ROT_90;
        break;

        case LV_RGA_TRANSFORM_ROT_180:
            usage |= IM_HAL_TRANSFORM_ROT_180;
        break;

        case LV_RGA_TRANSFORM_ROT_270:
            usage |= IM_HAL_TRANSFORM_ROT_270;
        break;

        default:
        break;
    }

    ret = imcheck(src, dst, dummy_rect, src_rect, usage);
    if (IM_STATUS_NOERROR != ret) {
        printf("check error! %s\n", imStrError());
        goto release_buffer;
    }

    usage |= IM_SYNC;
    ret = improcess(src, dst, dummy_buf, dummy_rect, src_rect, dummy_rect, usage);
    if (ret != IM_STATUS_SUCCESS) {
        printf("right running failed, %s\n", imStrError());
        goto release_buffer;
    }

    releasebuffer_handle(src_handle);
    releasebuffer_handle(dst_handle);

    return 0;

release_buffer:
    if (src_handle != 0) {
        releasebuffer_handle(src_handle);
    }
    if (dst_handle != 0) {
        releasebuffer_handle(dst_handle);
    }

    return -1;
}
#endif
