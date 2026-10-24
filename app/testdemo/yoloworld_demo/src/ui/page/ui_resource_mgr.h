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

#ifndef _UI_RES_MANAGE_H_
#define _UI_RES_MANAGE_H_

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

extern lv_ft_info_t ui_font_title;
extern lv_ft_info_t ui_font_content;

typedef enum {
    EMOJI_STRAIN = 0,
    EMOJI_LIGHTNING,
    EMOJI_LOOK_UP,
    EMOJI_SPEECHLESS,
    EMOJI_SMILE,
    EMOJI_SMILE_2,
    EMOJI_DIZZY,
    EMOJI_WINK,
    EMOJI_LOOK_LEFT_RIGHT,
    EMOJI_COUNT  // 表情总数
} EmojiType;

// 每种表情对应的图片数量
#define EMOJI_STRAIN_COUNT 23
#define EMOJI_LIGHTNING_COUNT 22
#define EMOJI_LOOK_UP_COUNT 7
#define EMOJI_SPEECHLESS_COUNT 19
#define EMOJI_SMILE_COUNT 14
#define EMOJI_SMILE_2_COUNT 7
#define EMOJI_DIZZY_COUNT 13
#define EMOJI_WINK_COUNT 11
#define EMOJI_LOOK_LEFT_RIGHT_COUNT 7

// 使用宏定义来初始化数组
static const int emoji_image_counts[EMOJI_COUNT] = {
    EMOJI_STRAIN_COUNT,  // 严肃
    EMOJI_LIGHTNING_COUNT,  // 闪电
    EMOJI_LOOK_UP_COUNT,  // 上看
    EMOJI_SPEECHLESS_COUNT,  // 无语
    EMOJI_SMILE_COUNT,  // 笑
    EMOJI_SMILE_2_COUNT,  // 笑嘻
    EMOJI_DIZZY_COUNT,  // 晕机
    EMOJI_WINK_COUNT,  // 眨眼
    EMOJI_LOOK_LEFT_RIGHT_COUNT  // 左右看
};

extern lv_img_dsc_t img_look_up_1;
extern lv_img_dsc_t img_look_up_2;
extern lv_img_dsc_t img_look_up_3;
extern lv_img_dsc_t img_look_up_4;
extern lv_img_dsc_t img_look_up_5;
extern lv_img_dsc_t img_look_up_6;
extern lv_img_dsc_t img_look_up_7;

extern lv_img_dsc_t img_wink_1;
extern lv_img_dsc_t img_wink_2;
extern lv_img_dsc_t img_wink_3;
extern lv_img_dsc_t img_wink_4;
extern lv_img_dsc_t img_wink_5;
extern lv_img_dsc_t img_wink_6;
extern lv_img_dsc_t img_wink_7;
extern lv_img_dsc_t img_wink_8;
extern lv_img_dsc_t img_wink_9;
extern lv_img_dsc_t img_wink_10;
extern lv_img_dsc_t img_wink_11;

// 定义 EMOJI_STRAIN 表情的图像
extern lv_img_dsc_t img_strain_1;
extern lv_img_dsc_t img_strain_2;
extern lv_img_dsc_t img_strain_3;
extern lv_img_dsc_t img_strain_4;
extern lv_img_dsc_t img_strain_5;
extern lv_img_dsc_t img_strain_6;
extern lv_img_dsc_t img_strain_7;
extern lv_img_dsc_t img_strain_8;
extern lv_img_dsc_t img_strain_9;
extern lv_img_dsc_t img_strain_10;
extern lv_img_dsc_t img_strain_11;
extern lv_img_dsc_t img_strain_12;
extern lv_img_dsc_t img_strain_13;
extern lv_img_dsc_t img_strain_14;
extern lv_img_dsc_t img_strain_15;
extern lv_img_dsc_t img_strain_16;
extern lv_img_dsc_t img_strain_17;
extern lv_img_dsc_t img_strain_18;
extern lv_img_dsc_t img_strain_19;
extern lv_img_dsc_t img_strain_20;
extern lv_img_dsc_t img_strain_21;
extern lv_img_dsc_t img_strain_22;
extern lv_img_dsc_t img_strain_23;

// 定义 EMOJI_SMILE 表情的图像
extern lv_img_dsc_t img_smile_1;
extern lv_img_dsc_t img_smile_2;
extern lv_img_dsc_t img_smile_3;
extern lv_img_dsc_t img_smile_4;
extern lv_img_dsc_t img_smile_5;
extern lv_img_dsc_t img_smile_6;
extern lv_img_dsc_t img_smile_7;
extern lv_img_dsc_t img_smile_8;
extern lv_img_dsc_t img_smile_9;
extern lv_img_dsc_t img_smile_10;
extern lv_img_dsc_t img_smile_11;
extern lv_img_dsc_t img_smile_12;
extern lv_img_dsc_t img_smile_13;
extern lv_img_dsc_t img_smile_14;

// 定义 EMOJI_LIGHTNING 表情的图像
extern lv_img_dsc_t img_lightning_1;
extern lv_img_dsc_t img_lightning_2;
extern lv_img_dsc_t img_lightning_3;
extern lv_img_dsc_t img_lightning_4;
extern lv_img_dsc_t img_lightning_5;
extern lv_img_dsc_t img_lightning_6;
extern lv_img_dsc_t img_lightning_7;
extern lv_img_dsc_t img_lightning_8;
extern lv_img_dsc_t img_lightning_9;
extern lv_img_dsc_t img_lightning_10;
extern lv_img_dsc_t img_lightning_11;
extern lv_img_dsc_t img_lightning_12;
extern lv_img_dsc_t img_lightning_13;
extern lv_img_dsc_t img_lightning_14;
extern lv_img_dsc_t img_lightning_15;
extern lv_img_dsc_t img_lightning_16;
extern lv_img_dsc_t img_lightning_17;
extern lv_img_dsc_t img_lightning_18;
extern lv_img_dsc_t img_lightning_19;
extern lv_img_dsc_t img_lightning_20;
extern lv_img_dsc_t img_lightning_21;
extern lv_img_dsc_t img_lightning_22;

// 定义 EMOJI_SPEECHLESS 表情的图像
extern lv_img_dsc_t img_speechless_1;
extern lv_img_dsc_t img_speechless_2;
extern lv_img_dsc_t img_speechless_3;
extern lv_img_dsc_t img_speechless_4;
extern lv_img_dsc_t img_speechless_5;
extern lv_img_dsc_t img_speechless_6;
extern lv_img_dsc_t img_speechless_7;
extern lv_img_dsc_t img_speechless_8;
extern lv_img_dsc_t img_speechless_9;
extern lv_img_dsc_t img_speechless_10;
extern lv_img_dsc_t img_speechless_11;
extern lv_img_dsc_t img_speechless_12;
extern lv_img_dsc_t img_speechless_13;
extern lv_img_dsc_t img_speechless_14;
extern lv_img_dsc_t img_speechless_15;
extern lv_img_dsc_t img_speechless_16;
extern lv_img_dsc_t img_speechless_17;
extern lv_img_dsc_t img_speechless_18;
extern lv_img_dsc_t img_speechless_19;

// 定义 EMOJI_SMILE_2 表情的图像
extern lv_img_dsc_t img_smile_2_1;
extern lv_img_dsc_t img_smile_2_2;
extern lv_img_dsc_t img_smile_2_3;
extern lv_img_dsc_t img_smile_2_4;
extern lv_img_dsc_t img_smile_2_5;
extern lv_img_dsc_t img_smile_2_6;
extern lv_img_dsc_t img_smile_2_7;

// 定义 EMOJI_DIZZY 表情的图像
extern lv_img_dsc_t img_dizzy_1;
extern lv_img_dsc_t img_dizzy_2;
extern lv_img_dsc_t img_dizzy_3;
extern lv_img_dsc_t img_dizzy_4;
extern lv_img_dsc_t img_dizzy_5;
extern lv_img_dsc_t img_dizzy_6;
extern lv_img_dsc_t img_dizzy_7;
extern lv_img_dsc_t img_dizzy_8;
extern lv_img_dsc_t img_dizzy_9;
extern lv_img_dsc_t img_dizzy_10;
extern lv_img_dsc_t img_dizzy_11;
extern lv_img_dsc_t img_dizzy_12;
extern lv_img_dsc_t img_dizzy_13;

// 定义 EMOJI_LOOK_LEFT_RIGHT 表情的图像
extern lv_img_dsc_t img_look_left_right_1;
extern lv_img_dsc_t img_look_left_right_2;
extern lv_img_dsc_t img_look_left_right_3;
extern lv_img_dsc_t img_look_left_right_4;
extern lv_img_dsc_t img_look_left_right_5;
extern lv_img_dsc_t img_look_left_right_6;
extern lv_img_dsc_t img_look_left_right_7;

// 定义 EMOJI_STRAIN 表情的图像数组
extern const lv_img_dsc_t *strain_images[EMOJI_STRAIN_COUNT];

// 定义 EMOJI_SMILE 表情的图像数组
extern const lv_img_dsc_t *smile_images[EMOJI_SMILE_COUNT];

// 定义 EMOJI_LIGHTNING 表情的图像数组
extern const lv_img_dsc_t *lightning_images[EMOJI_LIGHTNING_COUNT];

// 定义 EMOJI_SPEECHLESS 表情的图像数组
extern const lv_img_dsc_t *speechless_images[EMOJI_SPEECHLESS_COUNT];

// 定义 EMOJI_SMILE_2 表情的图像数组
extern const lv_img_dsc_t *smile2_images[EMOJI_SMILE_2_COUNT];

// 定义 EMOJI_DIZZY 表情的图像数组
extern const lv_img_dsc_t *dizzy_images[EMOJI_DIZZY_COUNT];

// 定义 EMOJI_LOOK_LEFT_RIGHT 表情的图像数组
extern const lv_img_dsc_t *look_left_right_images[EMOJI_LOOK_LEFT_RIGHT_COUNT];

// 定义 EMOJI_LOOK_UP 表情的图像数组
extern const lv_img_dsc_t *look_up_images[EMOJI_LOOK_UP_COUNT];

// 定义 EMOJI_WINK 表情的图像数组
extern const lv_img_dsc_t *wink_images[EMOJI_WINK_COUNT];

int32_t ui_res_load(void);
void    ui_res_release(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif