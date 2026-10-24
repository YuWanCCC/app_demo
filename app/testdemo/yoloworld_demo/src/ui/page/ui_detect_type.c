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

#include "ui_detect_type.h"
#include "lv_port_indev.h"
#include "ui_common.h"
#include "ui_ctrl.h"
#include "ui_page_manager.h"
#include "ui_resource_mgr.h"

#include "log.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h> // 为了使用 abs 函数
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define DETECT_PATH "/userdata/detect_classes.txt"
#define NO_DETECT_PATH "/userdata/no_detect_classes.txt"

extern lv_img_dsc_t *icon_return;

#define MAX_DETECT_CLASSES 80      // 检测类别最大数量
#define MAX_NO_DETECT_CLASSES 1024 // 未检测类别最大数量
#define MAX_TOTAL_CLASSES                                                      \
  (MAX_DETECT_CLASSES + MAX_NO_DETECT_CLASSES) // 总最大数量
#define BUTTON_WIDTH 180
#define BUTTON_HEIGHT 60
#define BUTTON_MARGIN 25

static char g_class_names[MAX_TOTAL_CLASSES][64];    // 存储类名
static int g_detect_class_count = 0;                 // 检测类别数量
static int g_no_detect_class_count = 0;              // 未检测类别数量
static lv_obj_t *g_class_buttons[MAX_TOTAL_CLASSES]; // 按钮指针数组
static int g_selected_class = -1;                    // 当前选中的类
static detect_type_operation_callback g_detect_type_add_callback = NULL;
static detect_type_operation_callback g_detect_type_delete_callback = NULL;

// 输入框相关变量
static lv_obj_t *g_input_textarea = NULL; // 输入框控件
static lv_obj_t *g_add_button = NULL;     // 添加按钮
static lv_obj_t *g_keyboard = NULL;       // 虚拟键盘
static lv_obj_t *g_keyboard_mask = NULL;  // 键盘背景遮罩

// 提示框相关变量
static lv_obj_t *g_message_box = NULL;   // 提示框
static lv_obj_t *g_message_label = NULL; // 提示框文本
static lv_obj_t *g_message_btn = NULL;   // 提示框按钮

/**********************
 *  STATIC PROTOTYPES
 **********************/

typedef struct {
  lv_group_t *group;
} ui_detect_type_context_t;

/**********************
 *  STATIC VARIABLES
 **********************/
static ui_detect_type_context_t g_detect_type_ctx = {0};

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

// 读取文件到数组
static int load_class_list(const char *filename, char arr[][64],
                           int max_count) {
  FILE *fp = fopen(filename, "r");
  if (!fp)
    return 0;
  int count = 0;
  char line[128];
  while (fgets(line, sizeof(line), fp) && count < max_count) {
    line[strcspn(line, "\r\n")] = 0;
    strncpy(arr[count], line, sizeof(arr[count]) - 1);
    arr[count][sizeof(arr[count]) - 1] = '\0';
    count++;
  }
  fclose(fp);
  return count;
}

// 从src删除，添加到dst末尾
static void move_class_between_files(const char *src, const char *dst,
                                     const char *classname) {
  // 1. 读取src到内存，去掉classname
  char lines[MAX_TOTAL_CLASSES][64];
  int count = load_class_list(src, lines, MAX_TOTAL_CLASSES);
  FILE *fp = fopen(src, "w");
  if (!fp)
    return;
  for (int i = 0; i < count; i++) {
    if (strcmp(lines[i], classname) != 0) {
      fprintf(fp, "%s\n", lines[i]);
    }
  }
  fclose(fp);
  // 2. 添加到dst末尾
  fp = fopen(dst, "a");
  if (fp) {
    fprintf(fp, "%s\n", classname);
    fclose(fp);
  }
}

// 隐藏提示框
static void hide_message_box(void) {
  if (g_message_box != NULL) {
    lv_obj_del(g_message_box);
    g_message_box = NULL;
    g_message_label = NULL;
    g_message_btn = NULL;
    printf("Message box hidden\n");
  }
}

// 提示框按钮事件回调
static void message_box_btn_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *btn = lv_event_get_target(e);

  if (code == LV_EVENT_CLICKED) {
    hide_message_box();
  }
}

// 显示提示框
static void show_message_box(const char *message) {
  // 如果提示框已存在，先隐藏
  if (g_message_box != NULL) {
    hide_message_box();
  }

  // 创建提示框背景遮罩
  g_message_box = lv_obj_create(lv_scr_act());
  lv_obj_set_size(g_message_box, 400, 200);
  lv_obj_set_pos(g_message_box, (1280 - 400) / 2, (720 - 200) / 2);
  lv_obj_set_style_bg_color(g_message_box, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(g_message_box, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_message_box, 2, 0);
  lv_obj_set_style_border_color(g_message_box, lv_color_hex(0xCCCCCC), 0);
  lv_obj_set_style_radius(g_message_box, 10, 0);
  lv_obj_set_style_shadow_width(g_message_box, 10, 0);
  lv_obj_set_style_shadow_color(g_message_box, lv_color_black(), 0);
  lv_obj_set_style_shadow_opa(g_message_box, LV_OPA_30, 0);

  // 禁用滚动条 - 使用兼容旧版本LVGL的方法
  lv_obj_set_scrollbar_mode(g_message_box, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_scroll_dir(g_message_box, LV_DIR_NONE);

  // 创建提示文本
  g_message_label = lv_label_create(g_message_box);
  lv_label_set_text(g_message_label, message);
  lv_obj_set_style_text_font(g_message_label, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(g_message_label, lv_color_black(), 0);
  lv_obj_set_width(g_message_label, 360);
  lv_obj_set_pos(g_message_label, 0, 40);
  lv_label_set_long_mode(g_message_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(g_message_label, LV_TEXT_ALIGN_CENTER, 0);

  // 创建确定按钮
  g_message_btn = lv_btn_create(g_message_box);
  lv_obj_set_size(g_message_btn, 80, 40);
  lv_obj_set_pos(g_message_btn, (400 - 80) / 2, 140); // 底部位置
  lv_obj_set_style_bg_color(g_message_btn, lv_color_hex(0x4CAF50), 0);
  lv_obj_set_style_text_color(g_message_btn, lv_color_white(), 0);
  lv_obj_set_style_radius(g_message_btn, 8, 0);
  lv_obj_set_style_border_width(g_message_btn, 0, 0);

  // 添加按钮文字
  lv_obj_t *btn_label = lv_label_create(g_message_btn);
  lv_label_set_text(btn_label, "OK");
  lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_24, 0);
  lv_obj_center(btn_label);

  // 添加按钮事件
  lv_obj_add_event_cb(g_message_btn, message_box_btn_event_cb, LV_EVENT_CLICKED,
                      NULL);

  // 确保提示框在最顶层
  lv_obj_move_foreground(g_message_box);

  printf("Message box shown: %s\n", message);
}

// 按钮点击事件回调
static void class_button_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *btn = lv_event_get_target(e);

  if (code == LV_EVENT_CLICKED) {
    // 找到按钮索引
    int btn_index = -1;
    for (int i = 0; i < g_detect_class_count + g_no_detect_class_count; i++) {
      if (g_class_buttons[i] == btn) {
        btn_index = i;
        break;
      }
    }
    if (btn_index < 0)
      return;

    if (strcmp(g_class_names[btn_index], "person") == 0) {
      printf("person class cannot be modified\n");
      return;
    }

    // 判断当前按钮是灰色还是白色
    lv_color_t bg = lv_obj_get_style_bg_color(btn, LV_PART_MAIN);
    int is_white = (bg.full == lv_color_hex(0xFFFFFF).full);

    if (is_white) {
      // 从未检测移动到检测类别
      if (g_detect_class_count >= MAX_DETECT_CLASSES) {
        show_message_box("detect class count exceeds the limit(80)");
        return;
      }

      if (g_detect_type_add_callback) {
        g_detect_type_add_callback(NULL, g_class_names[btn_index]);
      }
      // 变成蓝色
      lv_obj_set_style_bg_color(btn, lv_color_hex(0x1E3A8A), 0);
      lv_obj_set_style_text_color(btn, lv_color_white(), 0);
      // 文件操作
      move_class_between_files(NO_DETECT_PATH, DETECT_PATH,
                               g_class_names[btn_index]);
      // 更新计数
      g_detect_class_count++;
      g_no_detect_class_count--;
      printf("Moved '%s' to detect classes. Count: detect=%d, no_detect=%d\n",
             g_class_names[btn_index], g_detect_class_count,
             g_no_detect_class_count);
    } else {
      // 从检测类别移动到未检测类别
      if (g_no_detect_class_count >= MAX_NO_DETECT_CLASSES) {
        show_message_box("no detect class count exceeds the limit(1024)");
        return;
      }

      if (g_detect_type_delete_callback) {
        g_detect_type_delete_callback(NULL, g_class_names[btn_index]);
      }
      // 变成白色
      lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
      lv_obj_set_style_text_color(btn, lv_color_black(), 0);
      // 文件操作
      move_class_between_files(DETECT_PATH, NO_DETECT_PATH,
                               g_class_names[btn_index]);
      // 更新计数
      g_detect_class_count--;
      g_no_detect_class_count++;
      printf(
          "Moved '%s' to no_detect classes. Count: detect=%d, no_detect=%d\n",
          g_class_names[btn_index], g_detect_class_count,
          g_no_detect_class_count);
    }
  }
}

// 重新创建按钮网格的函数
static void refresh_class_buttons(lv_obj_t *page_obj) {
  // 读取detect和no_detect列表
  char detect_list[MAX_TOTAL_CLASSES][64];
  char no_detect_list[MAX_TOTAL_CLASSES][64];
  int detect_count =
      load_class_list(DETECT_PATH, detect_list, MAX_TOTAL_CLASSES);
  int no_detect_count =
      load_class_list(NO_DETECT_PATH, no_detect_list, MAX_TOTAL_CLASSES);

  // 找到滚动容器 - 使用更可靠的方法
  lv_obj_t *scroll_container = NULL;

  // 方法1：通过输入框的父对象找到页面，然后查找滚动容器
  lv_obj_t *parent = lv_obj_get_parent(g_input_textarea);
  if (parent == page_obj) {
    // 遍历页面子对象找到滚动容器
    uint32_t child_cnt = lv_obj_get_child_cnt(page_obj);
    for (uint32_t i = 0; i < child_cnt; i++) {
      lv_obj_t *child = lv_obj_get_child(page_obj, i);
      if (child != NULL) {
        // 检查是否是滚动容器（通过大小判断）
        if (lv_obj_get_width(child) == 1280 - 40 &&
            lv_obj_get_height(child) == 720 - 120 - 40) {
          scroll_container = child;
          printf("Found scroll container at index %d\n", i);
          break;
        }
      }
    }
  }

  if (scroll_container == NULL) {
    printf("ERROR: Could not find scroll container for refresh\n");
    return;
  }

  // 清除滚动容器内的所有现有按钮
  uint32_t container_child_cnt = lv_obj_get_child_cnt(scroll_container);
  printf("Clearing %d existing buttons from scroll container\n",
         container_child_cnt);

  // 从后往前删除，避免索引问题
  for (int i = container_child_cnt - 1; i >= 0; i--) {
    lv_obj_t *child = lv_obj_get_child(scroll_container, i);
    if (child != NULL) {
      lv_obj_del(child);
    }
  }

  // 重置按钮数组
  for (int i = 0; i < MAX_TOTAL_CLASSES; i++) {
    g_class_buttons[i] = NULL;
  }

  // 计算网格布局
  int buttons_per_row = 5;
  int start_x = 120; // 距离左边120像素
  int start_y = 120; // 距离顶部120像素

  // 在滚动容器内重新创建按钮网格
  int current_row = 0;
  int current_col = 0;

  for (int i = 0; i < g_detect_class_count + g_no_detect_class_count; i++) {
    // 创建按钮
    g_class_buttons[i] = lv_btn_create(scroll_container);
    lv_obj_set_size(g_class_buttons[i], BUTTON_WIDTH, BUTTON_HEIGHT);

    // 计算位置 - 相对于滚动容器
    int x = start_x - 20 + current_col * (BUTTON_WIDTH + BUTTON_MARGIN);
    int y = current_row * (BUTTON_HEIGHT + BUTTON_MARGIN);
    lv_obj_set_pos(g_class_buttons[i], x, y);

    // 判断是否在detect_list中
    int is_in_detect = 0;
    for (int j = 0; j < detect_count; j++) {
      if (strcmp(g_class_names[i], detect_list[j]) == 0) {
        is_in_detect = 1;
        break;
      }
    }

    // 特殊处理person类
    if (strcmp(g_class_names[i], "person") == 0) {
      lv_obj_set_style_bg_color(g_class_buttons[i], lv_color_hex(0x808080), 0);
      lv_obj_set_style_text_color(g_class_buttons[i], lv_color_white(), 0);
      lv_obj_add_state(g_class_buttons[i], LV_STATE_DISABLED);
    } else if (is_in_detect) {
      // 白色
      lv_obj_set_style_bg_color(g_class_buttons[i], lv_color_hex(0x1E3A8A), 0);
      lv_obj_set_style_text_color(g_class_buttons[i], lv_color_white(), 0);
    } else {
      // 灰色
      lv_obj_set_style_bg_color(g_class_buttons[i], lv_color_white(), 0);
      lv_obj_set_style_text_color(g_class_buttons[i], lv_color_black(), 0);
    }

    // 设置按钮样式
    lv_obj_set_style_radius(g_class_buttons[i], 5, 0);
    lv_obj_set_style_border_width(g_class_buttons[i], 1, 0);
    lv_obj_set_style_border_color(g_class_buttons[i], lv_color_hex(0xCCCCCC),
                                  0);

    // 创建标签显示类名
    lv_obj_t *label = lv_label_create(g_class_buttons[i]);
    lv_label_set_text(label, g_class_names[i]);
    lv_obj_center(label);

    // 添加事件回调
    lv_obj_add_event_cb(g_class_buttons[i], class_button_event_cb,
                        LV_EVENT_CLICKED, NULL);

    // 添加到焦点组
    if (g_detect_type_ctx.group) {
      lv_group_add_obj(g_detect_type_ctx.group, g_class_buttons[i]);
    }

    // 更新行列位置
    current_col++;
    if (current_col >= buttons_per_row) {
      current_col = 0;
      current_row++;
    }
  }

  printf("Successfully refreshed %d class buttons\n",
         g_detect_class_count + g_no_detect_class_count);
}

// 读取类名文件 - 从两个文件中加载并合并
static void load_class_names(void) {
  g_detect_class_count = 0;
  g_no_detect_class_count = 0;

  // 从 detect_class.txt 加载
  FILE *fp = fopen(DETECT_PATH, "r");
  if (fp) {
    char line[128];
    while (fgets(line, sizeof(line), fp) &&
           g_detect_class_count < MAX_DETECT_CLASSES) {
      line[strcspn(line, "\r\n")] = 0;
      strncpy(g_class_names[g_detect_class_count], line,
              sizeof(g_class_names[g_detect_class_count]) - 1);
      g_class_names[g_detect_class_count]
                   [sizeof(g_class_names[g_detect_class_count]) - 1] = '\0';
      g_detect_class_count++;
    }
    fclose(fp);
    printf("Loaded %d classes from detect file\n", g_detect_class_count);
  }

  // 从 no_detect_class.txt 加载
  fp = fopen(NO_DETECT_PATH, "r");
  if (fp) {
    char line[128];
    while (fgets(line, sizeof(line), fp) &&
           g_no_detect_class_count < MAX_NO_DETECT_CLASSES) {
      line[strcspn(line, "\r\n")] = 0;
      strncpy(
          g_class_names[g_detect_class_count + g_no_detect_class_count], line,
          sizeof(
              g_class_names[g_detect_class_count + g_no_detect_class_count]) -
              1);
      g_class_names[g_detect_class_count + g_no_detect_class_count]
                   [sizeof(g_class_names[g_detect_class_count +
                                         g_no_detect_class_count]) -
                    1] = '\0';
      g_no_detect_class_count++;
    }
    fclose(fp);
    printf("Loaded %d classes from no_detect file\n", g_no_detect_class_count);
  }

  printf("Total loaded %d classes (detect: %d, no_detect: %d)\n",
         g_detect_class_count + g_no_detect_class_count, g_detect_class_count,
         g_no_detect_class_count);
}

// return按钮事件回调
static void btn_return_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *btn = lv_event_get_target(e);

  if (code == LV_EVENT_CLICKED) {
    printf("Return button clicked, switching to main page\n");
    ui_ctl(UI_CTRL_SWITCH_MAIN_PAGE);
  }
}

// 添加按钮事件回调
static void add_button_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *btn = lv_event_get_target(e);

  if (code == LV_EVENT_CLICKED) {
    // 获取输入框的文本
    char input_text[64] = {0};
    lv_textarea_get_text(g_input_textarea);
    strncpy(input_text, lv_textarea_get_text(g_input_textarea),
            sizeof(input_text) - 1);

    // 去除首尾空格
    char *start = input_text;
    while (*start == ' ' || *start == '\t')
      start++;
    char *end = start + strlen(start) - 1;
    while (end > start &&
           (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r'))
      end--;
    *(end + 1) = '\0';

    // 检查输入是否为空
    if (strlen(start) == 0) {
      show_message_box("please input class name");
      lv_textarea_set_text(g_input_textarea, "");
      return;
    }

    // 检查是否已存在该类
    for (int i = 0; i < g_detect_class_count + g_no_detect_class_count; i++) {
      if (strcmp(g_class_names[i], start) == 0) {
        show_message_box("class already exists");
        lv_textarea_set_text(g_input_textarea, "");
        return;
      }
    }

    // 检查检测类别是否超过最大数量
    if (g_detect_class_count >= MAX_DETECT_CLASSES) {
      show_message_box("detect class count exceeds the limit(80)");
      lv_textarea_set_text(g_input_textarea, "");
      return;
    }

    // 添加新类到detect_classes.txt文件（直接添加到检测类别）
    FILE *fp = fopen(DETECT_PATH, "a");
    if (fp) {
      fprintf(fp, "%s\n", start);
      fclose(fp);
      printf("Added new class '%s' to detect classes\n", start);
      if (g_detect_type_add_callback) {
        g_detect_type_add_callback(NULL, start);
      }

      // 清空输入框
      lv_textarea_set_text(g_input_textarea, "");

      // 重新加载类名
      load_class_names();

      // 自动刷新页面 - 重新创建所有按钮
      printf("Refreshing page to show new class...\n");

      // 获取当前页面对象
      lv_obj_t *page_obj = lv_obj_get_parent(g_input_textarea);

      // 清除现有的类按钮
      for (int i = 0; i < g_detect_class_count + g_no_detect_class_count; i++) {
        if (g_class_buttons[i] != NULL) {
          lv_obj_del(g_class_buttons[i]);
          g_class_buttons[i] = NULL;
        }
      }

      // 重新创建按钮网格
      refresh_class_buttons(page_obj);

      printf("Page refreshed successfully, new class '%s' is now visible in "
             "detect classes\n",
             start);
    } else {
      show_message_box("file write failed");
      lv_textarea_set_text(g_input_textarea, "");
    }
  }
}

// 键盘隐藏事件回调
static void keyboard_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *keyboard = lv_event_get_target(e);

  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    // 隐藏键盘和背景遮罩
    lv_obj_add_flag(g_keyboard, LV_OBJ_FLAG_HIDDEN);
    if (g_keyboard_mask) {
      lv_obj_add_flag(g_keyboard_mask, LV_OBJ_FLAG_HIDDEN);
    }
    printf("Virtual keyboard hidden\n");
  }
}

// 输入框点击事件回调 - 显示虚拟键盘
static void input_textarea_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *textarea = lv_event_get_target(e);

  if (code == LV_EVENT_CLICKED || code == LV_EVENT_FOCUSED) {
    printf("Input textarea clicked/focused\n");

    // 如果键盘不存在，创建键盘
    if (g_keyboard == NULL) {
      printf("Creating virtual keyboard...\n");

      // 创建背景遮罩
      g_keyboard_mask = lv_obj_create(lv_scr_act());
      lv_obj_set_size(g_keyboard_mask, 1280, 720);
      lv_obj_set_pos(g_keyboard_mask, 0, 0);
      lv_obj_set_style_bg_color(g_keyboard_mask, lv_color_black(), 0);
      lv_obj_set_style_bg_opa(g_keyboard_mask, LV_OPA_50, 0); // 半透明黑色
      lv_obj_set_style_border_width(g_keyboard_mask, 0, 0);
      lv_obj_set_style_radius(g_keyboard_mask, 0, 0);
      lv_obj_clear_flag(g_keyboard_mask, LV_OBJ_FLAG_CLICKABLE);

      // 尝试使用不同的父对象创建键盘
      lv_obj_t *parent = lv_obj_get_parent(textarea);
      printf("Textarea parent: %p\n", parent);

      // 尝试使用输入框的父对象创建键盘，这样可以避免层级问题
      g_keyboard = lv_keyboard_create(parent);
      if (g_keyboard == NULL) {
        printf("Failed with textarea parent, trying screen parent\n");
        g_keyboard = lv_keyboard_create(lv_scr_act());
        if (g_keyboard == NULL) {
          printf("ERROR: Failed to create keyboard!\n");
          return;
        }
      }

      printf("Keyboard created, setting properties...\n");

      // 设置键盘尺寸和位置 - 固定在屏幕中间偏下位置
      lv_obj_set_size(g_keyboard, 1280, 360);
      lv_obj_set_pos(g_keyboard, 0, 0); // 修改为指定位置

      // 立即验证位置设置
      lv_coord_t test_x = lv_obj_get_x(g_keyboard);
      lv_coord_t test_y = lv_obj_get_y(g_keyboard);
      printf("Initial keyboard position: (%d, %d)\n", test_x, test_y);

      // 如果位置不正确，强制重新设置
      if (test_y != 0) {
        printf("Position incorrect, forcing to (0, 0)\n");
        lv_obj_set_pos(g_keyboard, 0, 0);
      }

      // 设置明显的样式
      lv_obj_set_style_bg_color(g_keyboard, lv_color_hex(0xE0E0E0), 0);
      lv_obj_set_style_bg_opa(g_keyboard, LV_OPA_COVER, 0);
      lv_obj_set_style_border_width(g_keyboard, 2, 0);
      lv_obj_set_style_border_color(g_keyboard, lv_color_hex(0x999999), 0);
      lv_obj_set_style_radius(g_keyboard, 0, 0);

      // 确保键盘可见和可交互
      lv_obj_clear_flag(g_keyboard, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(g_keyboard, LV_OBJ_FLAG_CLICKABLE);

      // 设置键盘为最顶层显示，浮于所有控件上方
      lv_obj_move_foreground(g_keyboard);

      // 添加键盘事件回调
      lv_obj_add_event_cb(g_keyboard, keyboard_event_cb, LV_EVENT_READY, NULL);
      lv_obj_add_event_cb(g_keyboard, keyboard_event_cb, LV_EVENT_CANCEL, NULL);

      printf("Keyboard properties set successfully\n");
    } else {
      printf("Keyboard already exists\n");
    }

    // 将键盘与输入框关联
    lv_keyboard_set_textarea(g_keyboard, textarea);
    printf("Keyboard linked to textarea\n");

    // 显示背景遮罩
    lv_obj_clear_flag(g_keyboard_mask, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_keyboard_mask);

    // 强制显示键盘并确保在最顶层
    lv_obj_clear_flag(g_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_keyboard); // 再次确保在最顶层

    // 强制重新设置位置，确保位置正确
    lv_obj_set_pos(g_keyboard, 0, 0);

    // 如果键盘位置仍然不对，尝试调整到屏幕中间
    lv_coord_t final_y = lv_obj_get_y(g_keyboard);
    if (final_y >= 720 || final_y < 0) {
      printf("Keyboard position still wrong, trying center position\n");
      lv_obj_set_pos(g_keyboard, 0, 180); // 屏幕中间位置
    }

    lv_obj_invalidate(g_keyboard);

    // 检查键盘状态
    if (lv_obj_has_flag(g_keyboard, LV_OBJ_FLAG_HIDDEN)) {
      printf("WARNING: Keyboard still has HIDDEN flag!\n");
    } else {
      printf("Keyboard should be visible now\n");
    }

    // 获取键盘位置和大小进行验证
    lv_coord_t x = lv_obj_get_x(g_keyboard);
    lv_coord_t y = lv_obj_get_y(g_keyboard);
    lv_coord_t w = lv_obj_get_width(g_keyboard);
    lv_coord_t h = lv_obj_get_height(g_keyboard);
    printf("Final keyboard position: (%d, %d), size: %dx%d\n", x, y, w, h);

    // 检查键盘是否在屏幕范围内
    if (y >= 720) {
      printf("WARNING: Keyboard is below screen! Screen height: 720, Keyboard "
             "Y: %d\n",
             y);
    } else if (y < 0) {
      printf("WARNING: Keyboard is above screen! Keyboard Y: %d\n", y);
    } else {
      printf("Keyboard is within screen bounds\n");
    }

    printf("Virtual keyboard shown at position (0, 0)\n");
  }
}

static void detect_type_create_ctrl(lv_obj_t *page_obj) {
  lv_obj_set_style_bg_color(page_obj, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(page_obj, LV_OPA_COVER, 0);

  // 加载类名
  load_class_names();

  // 读取detect和no_detect列表
  char detect_list[MAX_TOTAL_CLASSES][64];
  char no_detect_list[MAX_TOTAL_CLASSES][64];
  int detect_count =
      load_class_list(DETECT_PATH, detect_list, MAX_TOTAL_CLASSES);
  int no_detect_count =
      load_class_list(NO_DETECT_PATH, no_detect_list, MAX_TOTAL_CLASSES);

  // 添加 return 按钮 - 左上角
  lv_obj_t *btn_return = lv_btn_create(page_obj);
  lv_obj_set_size(btn_return, 80, 80);
  lv_obj_set_pos(btn_return, 20, 20);                    // 左上角位置
  lv_obj_set_style_bg_opa(btn_return, LV_OPA_TRANSP, 0); // 黑色背景
  // 添加 return 图标
  lv_obj_t *img_return = lv_img_create(btn_return);
  lv_img_set_src(img_return, icon_return);
  lv_obj_center(img_return);

  // 添加点击事件
  lv_obj_add_event_cb(btn_return, btn_return_event_cb, LV_EVENT_CLICKED, NULL);

  // 添加到焦点组
  if (g_detect_type_ctx.group) {
    lv_group_add_obj(g_detect_type_ctx.group, btn_return);
  }

  // 添加输入框和添加按钮 - 右上角区域
  // 输入框
  g_input_textarea = lv_textarea_create(page_obj);
  lv_obj_set_size(g_input_textarea, 200, 40);
  lv_obj_set_pos(g_input_textarea, 1280 - 300, 40); // 右上角位置
  lv_textarea_set_placeholder_text(g_input_textarea, "Enter class name");
  lv_textarea_set_max_length(g_input_textarea, 32); // 最大32个字符
  lv_obj_set_style_bg_color(g_input_textarea, lv_color_white(), 0);
  lv_obj_set_style_border_width(g_input_textarea, 1, 0);
  lv_obj_set_style_border_color(g_input_textarea, lv_color_hex(0xCCCCCC), 0);
  lv_obj_set_style_radius(g_input_textarea, 5, 0);

  // 添加输入框事件回调 - 点击时显示虚拟键盘
  lv_obj_add_event_cb(g_input_textarea, input_textarea_event_cb,
                      LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(g_input_textarea, input_textarea_event_cb,
                      LV_EVENT_FOCUSED, NULL);

  // 添加按钮
  g_add_button = lv_btn_create(page_obj);
  lv_obj_set_size(g_add_button, 80, 40);
  lv_obj_set_pos(g_add_button, 1280 - 90, 40); // 输入框右侧
  lv_obj_set_style_bg_color(g_add_button, lv_color_hex(0x4CAF50), 0); // 绿色
  lv_obj_set_style_text_color(g_add_button, lv_color_white(), 0);
  lv_obj_set_style_radius(g_add_button, 5, 0);

  // 添加按钮文字
  lv_obj_t *add_label = lv_label_create(g_add_button);
  lv_label_set_text(add_label, "Add");
  lv_obj_center(add_label);

  // 添加按钮事件
  lv_obj_add_event_cb(g_add_button, add_button_event_cb, LV_EVENT_CLICKED,
                      NULL);

  // 添加到焦点组
  if (g_detect_type_ctx.group) {
    lv_group_add_obj(g_detect_type_ctx.group, g_input_textarea);
    lv_group_add_obj(g_detect_type_ctx.group, g_add_button);
  }

  // 计算网格布局
  int buttons_per_row = 5;
  int total_rows =
      (g_detect_class_count + g_no_detect_class_count + buttons_per_row - 1) /
      buttons_per_row; // 向上取整

  // 设置左上角起始位置
  int start_x = 120; // 距离左边110像素
  int start_y = 120; // 距离顶部110像素

  // 添加标题 "Detect Type" - 在滚动容器外部
  lv_obj_t *title_label = lv_label_create(page_obj);
  lv_label_set_text(title_label, "Detect Type");
  lv_obj_set_style_text_font(title_label, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(title_label, lv_color_black(), 0);

  // 计算标题居中位置
  int title_width = 250;
  int title_x = (1280 - title_width) / 2;
  int title_y = 40; // 标题位置
  lv_obj_set_pos(title_label, title_x, title_y);

  // 创建滚动容器
  lv_obj_t *scroll_container = lv_obj_create(page_obj);
  lv_obj_set_size(scroll_container, 1280 - 40, 720 - start_y - 40); // 容器大小
  lv_obj_set_pos(scroll_container, 20, start_y);               // 容器位置
  lv_obj_set_style_bg_opa(scroll_container, LV_OPA_TRANSP, 0); // 透明背景
  lv_obj_set_style_border_width(scroll_container, 0, 0);       // 无边框
  lv_obj_set_scroll_dir(scroll_container, LV_DIR_VER); // 只允许垂直滚动

  // 在滚动容器内创建按钮网格
  int current_row = 0;
  int current_col = 0;

  for (int i = 0; i < g_detect_class_count + g_no_detect_class_count; i++) {
    // 创建按钮 - 父对象改为 scroll_container
    g_class_buttons[i] = lv_btn_create(scroll_container);
    lv_obj_set_size(g_class_buttons[i], BUTTON_WIDTH, BUTTON_HEIGHT);

    // 计算位置 - 相对于滚动容器
    int x = start_x - 20 +
            current_col * (BUTTON_WIDTH + BUTTON_MARGIN); // 调整x坐标
    int y = current_row * (BUTTON_HEIGHT + BUTTON_MARGIN); // 相对于容器顶部
    lv_obj_set_pos(g_class_buttons[i], x, y);

    // 判断是否在detect_list中
    int is_in_detect = 0;
    for (int j = 0; j < detect_count; j++) {
      if (strcmp(g_class_names[i], detect_list[j]) == 0) {
        is_in_detect = 1;
        break;
      }
    }

    if (strcmp(g_class_names[i], "person") == 0) {
      // 灰色
      lv_obj_set_style_bg_color(g_class_buttons[i], lv_color_hex(0x808080), 0);
      lv_obj_set_style_text_color(g_class_buttons[i], lv_color_white(), 0);
      // 禁用按钮点击效果
      lv_obj_add_state(g_class_buttons[i], LV_STATE_DISABLED);
    } else if (is_in_detect) {
      lv_obj_set_style_bg_color(g_class_buttons[i], lv_color_hex(0x1E3A8A), 0);
      lv_obj_set_style_text_color(g_class_buttons[i], lv_color_white(), 0);
    } else {
      // 白色
      lv_obj_set_style_bg_color(g_class_buttons[i], lv_color_white(), 0);
      lv_obj_set_style_text_color(g_class_buttons[i], lv_color_black(), 0);
    }

    // 设置按钮样式
    lv_obj_set_style_radius(g_class_buttons[i], 5, 0);
    lv_obj_set_style_border_width(g_class_buttons[i], 1, 0);
    lv_obj_set_style_border_color(g_class_buttons[i], lv_color_hex(0xCCCCCC),
                                  0);

    // 创建标签显示类名
    lv_obj_t *label = lv_label_create(g_class_buttons[i]);
    lv_label_set_text(label, g_class_names[i]);
    lv_obj_center(label);

    // 添加事件回调
    lv_obj_add_event_cb(g_class_buttons[i], class_button_event_cb,
                        LV_EVENT_CLICKED, NULL);

    // 添加到焦点组
    if (g_detect_type_ctx.group) {
      lv_group_add_obj(g_detect_type_ctx.group, g_class_buttons[i]);
    }

    // 更新行列位置
    current_col++;
    if (current_col >= buttons_per_row) {
      current_col = 0;
      current_row++;
    }
  }

  printf("Created %d buttons in %d rows with scrollable container\n",
         g_detect_class_count + g_no_detect_class_count, total_rows);
}

static int32_t detect_type_create(void) { return 0; }

static void detect_type_destroy(void) { return; }

static void detect_type_layout_ctrl(lv_obj_t *page_obj) {
  // 设置布局
}

static void detect_type_destroy_ctrl(void) { return; }

static void detect_type_add_indev(void) {
  lv_group_t *group = lv_port_indev_group_create();
  if (NULL == group)
    return;

  g_detect_type_ctx.group = group;
}

static void detect_type_delete_indev(void) {
  if (NULL != g_detect_type_ctx.group) {
    lv_port_indev_group_destroy(g_detect_type_ctx.group);
    g_detect_type_ctx.group = NULL;
  }
}

static void detect_type_page_create(lv_obj_t *page_obj) {
  detect_type_create_ctrl(page_obj);
  detect_type_layout_ctrl(page_obj);
}

static void detect_type_page_enter(lv_obj_t *page_obj) {
  detect_type_add_indev();
}

static void detect_type_page_exit(lv_obj_t *page_obj) {
  printf("detect_type_page_exit\n");

  // 隐藏虚拟键盘和背景遮罩
  if (g_keyboard != NULL) {
    lv_obj_add_flag(g_keyboard, LV_OBJ_FLAG_HIDDEN);
    printf("Virtual keyboard hidden on page exit\n");
  }

  if (g_keyboard_mask != NULL) {
    lv_obj_add_flag(g_keyboard_mask, LV_OBJ_FLAG_HIDDEN);
    printf("Keyboard mask hidden on page exit\n");
  }

  // 隐藏提示框
  hide_message_box();

  detect_type_delete_indev();
  detect_type_destroy();
}

static void detect_type_page_destroy(lv_obj_t *page_obj) {
  detect_type_destroy_ctrl();
}

void set_detect_type_add_class_callback(detect_type_operation_callback func) {
  g_detect_type_add_callback = func;
}

void set_detect_type_delete_class_callback(
    detect_type_operation_callback func) {
  g_detect_type_delete_callback = func;
}

static UI_PAGE_HANDLER_T detect_type_page = {.name = "detect_type",
                                             .init = NULL,
                                             .create = detect_type_page_create,
                                             .enter = detect_type_page_enter,
                                             .exit = detect_type_page_exit,
                                             .destroy =
                                                 detect_type_page_destroy};

UI_PAGE_REGISTER(detect_type_page)
