/*
 * Copyright (c) 2023 Rockchip, Inc. All Rights Reserved.
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
#include "ui_page_manager.h"
#include "ui_resource_mgr.h"
#include "ui_ctrl.h"

#include "log.h"

#include <stdio.h>
#include <time.h>

extern lv_img_dsc_t *icon_play;
extern lv_img_dsc_t *icon_yolo;
extern lv_img_dsc_t *icon_world;
extern lv_img_dsc_t *icon_add;

/**********************
 *  STATIC PROTOTYPES
 **********************/
typedef struct {
  lv_obj_t *base_obj;

  lv_group_t *group;

} ui_preview_context_t;

/**********************
 *  STATIC VARIABLES
 **********************/
static ui_preview_context_t g_preview_ctx = {0};

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**********************
 *   STATIC FUNCTIONS
 **********************/
// 播放按钮事件回调
static void btn_play_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = lv_event_get_target(e);

    if (code == LV_EVENT_CLICKED) {
        printf("button play clicked\n");
        ui_ctl(UI_CTRL_SWITCH_PLAY_PAGE);
    }
}

static void btn_add_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = lv_event_get_target(e);

    if (code == LV_EVENT_CLICKED) {
        printf("button add clicked\n");
        ui_ctl(UI_CTRL_SWITCH_DETECT_TYPE_PAGE);
    }
}

// 创建横向控制面板
static void create_control_panel(lv_obj_t *parent) {
  // 1. 设置parent背景为白色不透明
  lv_obj_set_style_bg_color(parent, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

  // 1. 计算整体宽度和起始x坐标
  int logo_gap = 20; // 两个logo之间的间距
  int total_logo_width = 400 + logo_gap + 200;
  int start_x = (1280 - total_logo_width) / 2;
  int logo_y = 20; // 距离顶部20像素

  // 2. yolo logo（400x400）
  lv_obj_t *img_yolo = lv_img_create(parent);
  lv_img_set_src(img_yolo, icon_yolo);
  lv_obj_set_size(img_yolo, 400, 400);
  lv_obj_set_pos(img_yolo, start_x, logo_y);

  // 3. world logo（200x200）
  lv_obj_t *img_world = lv_img_create(parent);
  lv_img_set_src(img_world, icon_world);
  lv_obj_set_size(img_world, 200, 200);
  lv_obj_set_pos(img_world, start_x + 400 + logo_gap, logo_y + 100); // 让world logo垂直居中对齐yolo

  // 3. 中间控件横向排列
  // 计算中间区域起始x坐标，使三个控件整体居中
  int btn_width = 150;
  int btn_height = 150;
  int gap = 150; // 控件间距
  int total_width = btn_width + gap + btn_width; // 两个按钮的总宽度
  start_x = (1280 - total_width) / 2; // 水平居中
  int y = 720 - btn_height - 50; // 距离底部50像素

  // 播放按钮（100x100）- 左边
  lv_obj_t *btn_play = lv_btn_create(parent);
  lv_obj_set_size(btn_play, btn_width, btn_height);
  lv_obj_set_pos(btn_play, start_x, y);
  lv_obj_set_style_bg_opa(btn_play, LV_OPA_TRANSP, 0);
  lv_obj_t *img_play = lv_img_create(btn_play);
  lv_img_set_src(img_play, icon_play);
  lv_obj_center(img_play);
  if (g_preview_ctx.group) {
    lv_group_add_obj(g_preview_ctx.group, btn_play);
  }
  lv_obj_add_event_cb(btn_play, btn_play_event_cb, LV_EVENT_ALL, NULL);

  // 增加按钮（100x100）- 右边
  lv_obj_t *btn_add = lv_btn_create(parent);
  lv_obj_set_size(btn_add, btn_width, btn_height);
  lv_obj_set_pos(btn_add, start_x + btn_width + gap, y);
  lv_obj_set_style_bg_opa(btn_add, LV_OPA_TRANSP, 0);
  lv_obj_t *img_add = lv_img_create(btn_add);
  lv_img_set_src(img_add, icon_add);
  lv_obj_center(img_add);
  if (g_preview_ctx.group) {
    lv_group_add_obj(g_preview_ctx.group, btn_add);
  }
  lv_obj_add_event_cb(btn_add, btn_add_event_cb, LV_EVENT_ALL, NULL);
}

static void preview_create_ctrl(lv_obj_t *page_obj) {
  create_control_panel(page_obj);
  return;
}

static void preview_layout_ctrl(lv_obj_t *page_obj) { return; }

static void preview_destroy_ctrl(void) { return; }

static void preview_add_indev(void) {
  lv_group_t *group = lv_port_indev_group_create();
  if (NULL == group)
    return;

  g_preview_ctx.group = group;
  return;
}

static void preview_delete_indev(void) {
  if (NULL != g_preview_ctx.group) {
    lv_port_indev_group_destroy(g_preview_ctx.group);
    g_preview_ctx.group = NULL;
  }
  return;
}

static void preview_page_create(lv_obj_t *page_obj) {
  preview_create_ctrl(page_obj);
  preview_layout_ctrl(page_obj);
}

static void preview_page_enter(lv_obj_t *page_obj) { preview_add_indev(); }

static void preview_page_exit(lv_obj_t *page_obj) { preview_delete_indev(); }

static void preview_page_destroy(lv_obj_t *page_obj) { preview_destroy_ctrl(); }

static UI_PAGE_HANDLER_T preview_page = {.name = "main",
                                         .init = NULL,
                                         .create = preview_page_create,
                                         .enter = preview_page_enter,
                                         .exit = preview_page_exit,
                                         .destroy = preview_page_destroy};

UI_PAGE_REGISTER(preview_page)