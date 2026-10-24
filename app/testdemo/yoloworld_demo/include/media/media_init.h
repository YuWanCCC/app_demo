#pragma once

#include "rk_comm_mb.h"
#include "rk_common.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_sys.h"

typedef enum rkCOLOR_INDEX_E {
  RGN_COLOR_LUT_INDEX_0 = 0,
  RGN_COLOR_LUT_INDEX_1 = 1,
} COLOR_INDEX_E;
typedef struct rkRGN_RECT_S {
  int x;
  int y;
  int w;
  int h;
  char text[32];
  COLOR_INDEX_E color_index;
} RGN_RECT_S;

int media_init(int is_usbcam);
int media_deinit();
void media_set_rgb888_buffer_cb(void (*cb)(const VIDEO_FRAME_INFO_S &frame));
void media_add_rgn_rect(RGN_RECT_S rect);
void media_add_update_rgn_task(void *arg);
void media_add_clear_rgn_task(void *arg);
void media_insert_userdata2venc(unsigned char *userdata, int len);
int media_release_vpss_frame(VIDEO_FRAME_INFO_S *frame);
