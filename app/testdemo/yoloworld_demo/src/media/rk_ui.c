/*
 * Copyright (c) 2023 Rockchip, Inc. All Rights Reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
#include "disp.h"
#include <fcntl.h>
#include <lvgl.h>
#include <rga/im2d.h>
#include <rga/rga.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctime>

#include <rk_mpi_mb.h>
#include <rk_mpi_mmz.h>
#include <rk_mpi_rgn.h>
#include <rk_mpi_sys.h>
#include <rk_mpi_tde.h>
#include <rk_mpi_venc.h>
#include <rk_mpi_vi.h>
#include <rk_mpi_vo.h>
#include <rk_mpi_vpss.h>

#define DIV_ROUND_UP(n, d) (((n) + (d)-1) / (d))

#define UPALIGNED_16(x) (((x) + 15) & ~15)
#define DOWNALIGNED_16(x) ((x) & ~15)
#define UPALIGNED_64(x) (((x) + 63) & ~63)

struct disp_dev {
  lv_disp_rot_t rot;
  VIDEO_FRAME_INFO_S display_bufs[2];
  VIDEO_FRAME_INFO_S *active_buf;
  VIDEO_FRAME_INFO_S *free_buf;
  uint32_t width, height;
  uint32_t mm_width, mm_height;
} disp_dev;

static int32_t rk_disp_setup(void) {
  int32_t ret;
  VO_CHN_ATTR_S vo_chn_attr;
  int display_chn_id = 1;
  int display_layer = 1;
  uint32_t fps = 30;

  memset(&vo_chn_attr, 0, sizeof(vo_chn_attr));
  vo_chn_attr.bDeflicker = RK_FALSE;
  vo_chn_attr.u32Priority = 1;
  vo_chn_attr.stRect.s32X = 0;
  vo_chn_attr.stRect.s32Y = 0;
  vo_chn_attr.stRect.u32Width = 720;
  vo_chn_attr.stRect.u32Height = 1280;
  vo_chn_attr.enRotation = ROTATION_90;
  ret = RK_MPI_VO_SetChnAttr(display_layer, display_chn_id, &vo_chn_attr);
  ret = RK_MPI_VO_EnableChn(display_layer, display_chn_id);
  if (ret != RK_SUCCESS) {
    printf("create %d layer %d ch vo failed %#X!\n", display_layer,
           display_chn_id, ret);
    return ret;
  }
  printf("rk_disp: ui created successfully.\n");

  return 0;
}

static void rk_disp_teardown(void) {
  int ret = 0;
  uint32_t display_layer = 1;
  int display_chn_id = 1;
  ret = RK_MPI_VO_DisableChn(display_layer, display_chn_id);
  if (ret) {
    printf("RK_MPI_VO_DisableChn failed, ret is %#x\n", ret);
    return;
  }
  printf("rk_disp: ui destroyed successfully.\n");
}

static int32_t rk_disp_setup_buffers(void) {
  int32_t ret;
  uint32_t i, size;
  void *blk = NULL, *viraddr = NULL;
  uint32_t format = 0;

  if (LV_COLOR_DEPTH == 32) {
    format = RK_FMT_BGRA8888;
  } else {
    format = 0;
    printf("drm_flush rga not supported format\n");
    return -1;
  }

  size = disp_dev.width * disp_dev.height * (LV_COLOR_SIZE / 8);

  for (int i = 0; i < 2; ++i) {
    ret = RK_MPI_MMZ_Alloc(&blk, size, RK_MMZ_ALLOC_CACHEABLE);
    if (0 != ret) {
      printf("alloc buf size %d failed %#X!\n", size, ret);
      return ret;
    }
    disp_dev.display_bufs[i].stVFrame.enPixelFormat = (rkPIXEL_FORMAT_E)format;
    disp_dev.display_bufs[i].stVFrame.pMbBlk = blk;
    disp_dev.display_bufs[i].stVFrame.u32Width = disp_dev.width;
    disp_dev.display_bufs[i].stVFrame.u32VirWidth = disp_dev.width;
    disp_dev.display_bufs[i].stVFrame.u32Height = disp_dev.height;
    disp_dev.display_bufs[i].stVFrame.u32VirHeight = disp_dev.height;
    viraddr = RK_MPI_MB_Handle2VirAddr(blk);
    memset(viraddr, 0x00, size);
    printf("mb blk %p, width %d, height %d, viraddr %p\n",
           disp_dev.display_bufs[i].stVFrame.pMbBlk,
           disp_dev.display_bufs[i].stVFrame.u32Width,
           disp_dev.display_bufs[i].stVFrame.u32Height, viraddr);
  }
  disp_dev.active_buf = &disp_dev.display_bufs[0];
  disp_dev.free_buf = &disp_dev.display_bufs[1];
  return 0;
}

static void rk_disp_teardown_buffers(void) {
  for (int i = 0; i < 2; ++i) {
    printf("free mb blk %p\n", disp_dev.display_bufs[i].stVFrame.pMbBlk);
    RK_MPI_MMZ_Free(disp_dev.display_bufs[i].stVFrame.pMbBlk);
    memset(&disp_dev.display_bufs[i], 0, sizeof(VIDEO_FRAME_INFO_S));
  }
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
int32_t rk_disp_init(lv_disp_rot_t rotate_disp) {
  int32_t ret;

  memset(&disp_dev, 0, sizeof(disp_dev));
  disp_dev.width = 1280;
  disp_dev.height = 720;
  ret = rk_disp_setup();
  if (0 != ret) {
    printf("rk_disp_setup failed");
    return -1;
  }

  ret = rk_disp_setup_buffers();
  if (0 != ret) {
    printf("Allocating display buffer failed\n");
    goto err;
  }

  disp_dev.rot = rotate_disp;

  return 0;
err:
  rk_disp_teardown();

  return -1;
}

void rk_disp_exit(void) {
  rk_disp_teardown_buffers();
  rk_disp_teardown();
}

static void draw_buf_rotate_90(lv_color_t *color_p, const lv_area_t *area,
                               lv_color_t *dst_buf, lv_coord_t canvas_w,
                               lv_coord_t canvas_h) {
  lv_coord_t area_w = (area->x2 - area->x1 + 1);
  lv_coord_t area_h = (area->y2 - area->y1 + 1);
  uint32_t initial_i = area->x1 * canvas_w + (canvas_w - area->y1 - 1);
  for (lv_coord_t y = 0; y < area_h; y++) {
    uint32_t i = initial_i - y;
    for (lv_coord_t x = 0; x < area_w; x++) {
      dst_buf[i] = *(color_p++);
      i += canvas_w;
    }
  }
}

static void rga_copy(VIDEO_FRAME_INFO_S *src_frame,
                     VIDEO_FRAME_INFO_S *dst_frame) {
  int ret = 0;
  int src_width = src_frame->stVFrame.u32Width;
  int src_height = src_frame->stVFrame.u32Height;
  int src_format = RK_FORMAT_BGRA_8888;
  int dst_width = dst_frame->stVFrame.u32Width;
  int dst_height = dst_frame->stVFrame.u32Height;
  int dst_format = RK_FORMAT_BGRA_8888;
  int src_fd = RK_MPI_MB_Handle2Fd(src_frame->stVFrame.pMbBlk);
  int dst_fd = RK_MPI_MB_Handle2Fd(dst_frame->stVFrame.pMbBlk);
  rga_buffer_t src_img = {}, dst_img = {};
  rga_buffer_handle_t src_handle = 0, dst_handle = 0;
  im_handle_param_t src_handle_param = {
      .width = (uint32_t)src_width,
      .height = (uint32_t)src_height,
      .format = (uint32_t)RK_FORMAT_BGRA_8888,
  };
  im_handle_param_t dst_handle_param = {
      .width = (uint32_t)dst_width,
      .height = (uint32_t)dst_height,
      .format = RK_FORMAT_BGRA_8888,
  };
  src_handle = importbuffer_fd(src_fd, &src_handle_param);
  dst_handle = importbuffer_fd(dst_fd, &dst_handle_param);
  if (src_handle == 0 || dst_handle == 0) {
    printf("importbuffer failed!\n");
    return;
  }

  src_img = wrapbuffer_handle_t(src_handle, src_width, src_height, src_width,
                                src_height, src_format);
  dst_img = wrapbuffer_handle_t(dst_handle, dst_width, dst_height, dst_width,
                                dst_height, dst_format);

  ret = imcopy_t(src_img, dst_img, 1);
  if (ret != IM_STATUS_SUCCESS) {
    printf("imcopy failed! %s\n", imStrError(ret));
  }

  if (src_handle)
    releasebuffer_handle(src_handle);
  if (dst_handle)
    releasebuffer_handle(dst_handle);
}

static void rga_copy_area(VIDEO_FRAME_INFO_S *dst_frame, const lv_area_t *area,
                          lv_color_t *color_p, int rotation) {
  int ret = 0;
  int fg_width, fg_height, fg_format;
  int bg_width, bg_height, bg_format;
  int bg_fd = RK_MPI_MB_Handle2Fd(dst_frame->stVFrame.pMbBlk);
  void *fg_viraddr = color_p;
  int usage = IM_SYNC;
  im_rect bg_rect = {}, fg_rect = {}, dummy_rect = {};
  rga_buffer_t fg_img = {}, bg_img = {}, dummy_img = {};
  rga_buffer_handle_t fg_handle, bg_handle;
  im_handle_param_t fg_handle_param, bg_handle_param;

  fg_width = area->x2 - area->x1 + 1;
  if (fg_width == 1) {
    fg_width = 2;
  }
  fg_height = area->y2 - area->y1 + 1;
  if (fg_height == 1) {
    fg_height = 2;
  }
  fg_format = RK_FORMAT_BGRA_8888;

  bg_width = dst_frame->stVFrame.u32Width;
  bg_height = dst_frame->stVFrame.u32Height;
  bg_format = RK_FORMAT_BGRA_8888;

  fg_handle_param.width = fg_width;
  fg_handle_param.height = fg_height;
  fg_handle_param.format = fg_format;
  fg_handle = importbuffer_virtualaddr(fg_viraddr, &fg_handle_param);

  bg_handle_param.width = bg_width;
  bg_handle_param.height = bg_height;
  bg_handle_param.format = bg_format;
  bg_handle = importbuffer_fd(bg_fd, &bg_handle_param);
  if (fg_handle == 0 || bg_handle == 0) {
    printf("importbuffer failed!\n");
    return;
  }

  fg_img = wrapbuffer_handle_t(fg_handle, fg_width, fg_height, fg_width,
                               fg_height, fg_format);
  bg_img = wrapbuffer_handle_t(bg_handle, bg_width, bg_height, bg_width,
                               bg_height, bg_format);

  if (rotation == ROTATION_90 || rotation == ROTATION_270) {
    bg_rect.x = (bg_width - area->y1 - fg_height);
    bg_rect.y = area->x1;
    bg_rect.width = fg_height;
    bg_rect.height = fg_width;
    if (rotation == ROTATION_90) {
      usage |= IM_HAL_TRANSFORM_ROT_90;
    } else {
      usage |= IM_HAL_TRANSFORM_ROT_270;
    }
  } else {
    bg_rect.x = area->x1;
    bg_rect.y = area->y1;
    bg_rect.width = fg_width;
    bg_rect.height = fg_height;
  }
  ret = imcheck(fg_img, bg_img, dummy_rect, bg_rect, usage);
  if (ret != IM_STATUS_NOERROR) {
    printf("imcheck failed! %s\n", imStrError(ret));
    releasebuffer_handle(fg_handle);
    releasebuffer_handle(bg_handle);
    return;
  }

  ret = improcess(fg_img, bg_img, dummy_img, dummy_rect, bg_rect, dummy_rect,
                  usage);
  if (ret != IM_STATUS_SUCCESS) {
    printf("improcess failed! %s\n", imStrError(ret));
  }

  if (fg_handle)
    releasebuffer_handle(fg_handle);
  if (bg_handle)
    releasebuffer_handle(bg_handle);
}

static void print_timestamp(const char *function_name) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[26];
    strftime(time_str, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    printf("[%s] %s called\n", time_str, function_name);
}

void rk_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area,
                   lv_color_t *color_p) {
  // print_timestamp("rk_disp_flush");
  int ret = 0;
  lv_coord_t area_width = (area->x2 - area->x1 + 1);
  lv_coord_t area_height = (area->y2 - area->y1 + 1);
  int rotation = ROTATION_0;
  uint32_t display_layer = 1;
  int display_chn_id = 1;
  VIDEO_FRAME_INFO_S *dst_frame = disp_dev.free_buf;

  // printf("x %d:%d y %d:%d area_width %d area_height %d\n", area->x1,
  //       area->x2, area->y1, area->y2, area_width, area_height);

  rga_copy(disp_dev.active_buf, disp_dev.free_buf);
  rga_copy_area(disp_dev.free_buf, area, color_p, rotation);
  ret = RK_MPI_VO_SendFrame(display_layer, display_chn_id, dst_frame, 1000);
  if (ret != 0) {
    printf("RK_MPI_VO_SendFrame [%d,%d] failed %#X\n", display_layer,
           display_chn_id, ret);
  }
  // swap buffer
  void *tmp = disp_dev.free_buf;
  disp_dev.free_buf = disp_dev.active_buf;
  disp_dev.active_buf = (VIDEO_FRAME_INFO_S *)tmp;
  lv_disp_flush_ready(disp_drv);
  // INFO: Flush layer to make sure the display is updated.
  if (rotation == ROTATION_0 || rotation == ROTATION_180) {
    if (disp_dev.width == area_width && disp_dev.height == area_height) {
      RK_MPI_VO_SetLayerFlush(display_layer);
    }
  } else {
    if (disp_dev.height == area_width && disp_dev.width == area_height) {
      RK_MPI_VO_SetLayerFlush(display_layer);
    }
  }
}

void rk_disp_get_sizes(lv_coord_t *width, lv_coord_t *height, uint32_t *dpi) {
  if (width)
    *width = disp_dev.width;

  if (height)
    *height = disp_dev.height;

  if (dpi && disp_dev.mm_width)
    *dpi = DIV_ROUND_UP(disp_dev.width * 25400, disp_dev.mm_width * 1000);

  return;
}

disp_ops_t rk_ui_ops = {
    rk_disp_init,
    rk_disp_exit,
    rk_disp_get_sizes,
    rk_disp_flush,
};
