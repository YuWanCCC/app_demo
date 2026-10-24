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

#include "lv_port_indev.h"
#include "ui_common.h"
#include "ui_ctrl.h"
#include "ui_page_manager.h"
#include "ui_resource_mgr.h"

#include "log.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

extern lv_img_dsc_t *icon_return;

/**********************
 *  STATIC PROTOTYPES
 **********************/

typedef struct {
  lv_group_t *group;
} ui_video_player_context_t;

/**********************
 *  STATIC VARIABLES
 **********************/
static ui_video_player_context_t g_player_ctx = {0};

// 滑动检测相关变量
static lv_point_t touch_start = {0, 0};
static lv_point_t touch_end = {0, 0};
static bool touch_started = false;

/**********************
 *  GLOBAL VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**********************
 *   STATIC FUNCTIONS
 **********************/

// return按钮事件回调
static void swipe_return_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *obj = lv_event_get_target(e);

  if (code == LV_EVENT_PRESSED) {
    // 触摸开始，记录起始位置
    lv_indev_t *indev = lv_indev_get_act();
    if (indev) {
      lv_indev_get_point(indev, &touch_start);
      touch_started = true;
    }
  } else if (code == LV_EVENT_RELEASED && touch_started) {
    // 触摸结束，记录结束位置并判断滑动
    lv_indev_t *indev = lv_indev_get_act();
    if (indev) {
      lv_indev_get_point(indev, &touch_end);

      // 计算滑动距离
      int delta_x = touch_end.x - touch_start.x;
      int delta_y = touch_end.y - touch_start.y;

      // 判断是否为水平滑动（水平距离大于垂直距离，且水平距离超过阈值）
      if (abs(delta_x) > abs(delta_y) && abs(delta_x) > 200) {
        if (delta_x > 0) {
          // 右滑返回
          printf("return by swipe right\n");
          ui_ctl(UI_CTRL_SWITCH_MAIN_PAGE);
        } else {
          // 左滑返回
          printf("return by swipe left\n");
          ui_ctl(UI_CTRL_SWITCH_MAIN_PAGE);
        }
      }
      touch_started = false;
    }
  }
}

#define DETECT_CLASSES_PATH "/userdata/detect_classes.txt"
#define DROPDOWN_WIDTH 220
#define DROPDOWN_MAX_HEIGHT 400 // 你可以根据实际情况调整

static char dropdown_options[4096]; // 存放所有选项

// 读取 detect_classes.txt 文件内容，拼成 LVGL 下拉框需要的格式
static void load_detect_classes(const char *filename) {
  FILE *fp = fopen(filename, "r");
  if (!fp) {
    printf("Failed to open %s\n", filename);
    strcpy(dropdown_options, "no class");
    return;
  }
  char line[128];
  dropdown_options[0] = '\0';
  while (fgets(line, sizeof(line), fp)) {
    // 去掉换行符
    line[strcspn(line, "\r\n")] = 0;
    if (strlen(dropdown_options) > 0) {
      strcat(dropdown_options, "\n");
    }
    strcat(dropdown_options, line);
  }
  fclose(fp);
}

// 下拉框事件回调
static void dropdown_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *dropdown = lv_event_get_target(e);
  switch (code) {
  case LV_EVENT_READY: {
    lv_obj_t *list = lv_dropdown_get_list(dropdown);
    static bool style_inited = false;
    static lv_style_t list_style;
    if (list) {
      if (!style_inited) {
        lv_style_init(&list_style);
        lv_style_set_bg_opa(&list_style, LV_OPA_COVER);
        lv_style_set_radius(&list_style, 0);
        lv_style_set_border_width(&list_style, 2);
        style_inited = true;
      }
      lv_obj_add_style(list, &list_style, 0);
    }
    break;
  }
  case LV_EVENT_VALUE_CHANGED: {
    char txt[128];
    lv_dropdown_get_selected_str(dropdown, txt, sizeof(txt));
    printf("selected: %s\n", txt);
    break;
  }
  default:
    break;
  }
}

static void video_player_create_ctrl(lv_obj_t *page_obj) {
  static lv_style_t style;
  lv_style_reset(&style);
  lv_style_init(&style);
  lv_style_set_bg_opa(&style, LV_OPA_TRANSP); // 设置样式背景透明度
  lv_obj_add_style(page_obj, &style, LV_STATE_DEFAULT);

  if (g_player_ctx.group) {
    lv_group_add_obj(g_player_ctx.group, page_obj);
  }
  lv_obj_add_event_cb(page_obj, swipe_return_event_cb, LV_EVENT_ALL, NULL);

  // // 下拉框 - 右上角
  // load_detect_classes(DETECT_CLASSES_PATH);
  // lv_obj_t *dropdown = lv_dropdown_create(page_obj);
  // lv_dropdown_set_options(dropdown, dropdown_options);
  // lv_obj_set_size(dropdown, DROPDOWN_WIDTH, 60); // 宽220，高60
  // lv_obj_set_pos(dropdown, 1280 - DROPDOWN_WIDTH - 20, 20); // 右上角位置
  // static lv_style_t dropdown_style;
  // lv_style_init(&dropdown_style);
  // lv_style_set_max_height(&dropdown_style, DROPDOWN_MAX_HEIGHT);
  // lv_style_set_bg_opa(&dropdown_style, LV_OPA_COVER);
  // lv_style_set_radius(&dropdown_style, 0);
  // lv_style_set_border_width(&dropdown_style, 2);
  // lv_obj_add_style(dropdown, &dropdown_style, 0);
  // if (g_player_ctx.group) {
  //     lv_group_add_obj(g_player_ctx.group, dropdown);
  // }
  // lv_obj_add_event_cb(dropdown, dropdown_event_cb, LV_EVENT_ALL, NULL);
}

static int32_t video_player_create(void) { return 0; }

static void video_player_destroy(void) { return; }

static void video_player_layout_ctrl(lv_obj_t *page_obj) {
  // 设置布局
}

static void video_player_destroy_ctrl(void) { return; }

static void video_player_add_indev(void) {
  lv_group_t *group = lv_port_indev_group_create();
  if (NULL == group)
    return;

  g_player_ctx.group = group;
}

static void video_player_delete_indev(void) {
  if (NULL != g_player_ctx.group) {
    lv_port_indev_group_destroy(g_player_ctx.group);
    g_player_ctx.group = NULL;
  }
}

static void video_player_page_create(lv_obj_t *page_obj) {
  video_player_create_ctrl(page_obj);
  video_player_layout_ctrl(page_obj);
}

static void video_player_page_enter(lv_obj_t *page_obj) {
  video_player_add_indev();
}

static void video_player_page_exit(lv_obj_t *page_obj) {
  printf("video_player_page_exit\n");
  video_player_delete_indev();
  video_player_destroy();
}

static void video_player_page_destroy(lv_obj_t *page_obj) {
  video_player_destroy_ctrl();
}

static UI_PAGE_HANDLER_T video_player_page = {
    .name = "video_player",
    .init = NULL,
    .create = video_player_page_create,
    .enter = video_player_page_enter,
    .exit = video_player_page_exit,
    .destroy = video_player_page_destroy};

UI_PAGE_REGISTER(video_player_page)
