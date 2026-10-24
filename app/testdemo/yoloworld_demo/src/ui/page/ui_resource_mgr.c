/**
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

#include "lv_img_dec.h"
#include "ui_resource_mgr.h"

#include <stdio.h>
#include <limits.h>

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/


/**********************
 *  GLOBAL VARIABLES
 **********************/
lv_ft_info_t ui_font_title;
lv_ft_info_t ui_font_content;

lv_img_dsc_t *icon_play = NULL;
lv_img_dsc_t *icon_yolo = NULL;
lv_img_dsc_t *icon_world = NULL;
lv_img_dsc_t *icon_add = NULL;
lv_img_dsc_t *icon_return = NULL;

/**********************
 *      MACROS
 **********************/

#define ICON_RELEASE(icon_index)                \
    do {                                        \
        if (NULL != icon_index) {               \
            lv_free_img(&icon_index);           \
            icon_index = NULL;                  \
        }                                       \
    } while (0)

/**********************
 *   STATIC FUNCTIONS
 **********************/

static lv_img_dsc_t* ui_dec_icon(const char * const icon_name) {
    if (NULL == icon_name) {
        LV_LOG_ERROR("The icon named pointer is NULL.");
        return NULL;
    }

    char icon_path[PATH_MAX] = {0};

    const char * icon_dir = "/oem/usr/share/ui/";
    if (NULL == icon_dir) {
        LV_LOG_ERROR("Icon path does not exist.");
        return NULL;
    }

    snprintf(icon_path, sizeof(icon_path), "%s%s", icon_dir, icon_name);
    return lv_dec_img(icon_path);
}

static int32_t ui_font_init(lv_ft_info_t *info, uint32_t weight) {
    if (NULL == info || 0 == weight) {
        LV_LOG_ERROR("Invalid font configuration parameter.");
        return -1;
    }

    const char * font_path = "/oem/usr/share/fonts/PuHuiTi.ttf";
    if (NULL == font_path) {
        LV_LOG_ERROR("Font path does not exist.");
        return -1;
    }

    info->name = font_path;
    info->weight = weight;
    info->style = FT_FONT_STYLE_NORMAL;

    if (!lv_ft_font_init(info)) {
        LV_LOG_ERROR("Failed to create font.");
        return -1;
    }

    return 0;
}

static int32_t ui_icon_load(void) {
    icon_play = ui_dec_icon("play.png");
    icon_yolo = ui_dec_icon("yolo.png");
    icon_world = ui_dec_icon("world.png");
    icon_add = ui_dec_icon("add.png");
    icon_return = ui_dec_icon("return.png");
    return 0;
}

static void ui_icon_release(void) {
    ICON_RELEASE(icon_play);
    ICON_RELEASE(icon_yolo);
    ICON_RELEASE(icon_world);
    ICON_RELEASE(icon_add);
    ICON_RELEASE(icon_return);
    return;
}

static int32_t ui_font_load(void) {
    if (0 != ui_font_init(&ui_font_title, 32))
        LV_LOG_ERROR("Title font initialization failed.");

    if (0 != ui_font_init(&ui_font_content, 16))
        LV_LOG_ERROR("Content font initialization failed.");

    return 0;
}

static void ui_font_release(void) {
    if (NULL != ui_font_title.font) {
        lv_ft_font_destroy(ui_font_title.font);
        ui_font_title.font = NULL;
    }

    if (NULL != ui_font_content.font) {
        lv_ft_font_destroy(ui_font_content.font);
        ui_font_content.font = NULL;
    }
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

int32_t ui_res_load(void) {
    int32_t ret = 0;

    ret |= ui_icon_load();
    ret |= ui_font_load();
    return ret;
}
void ui_res_release(void) {
    ui_icon_release();
    ui_font_release();
}
