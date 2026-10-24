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

#include "lv_port_disp.h"
#include "lvgl/common/lv_dma_alloc.h"
#include "lvgl/drivers/disp.h"

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

static lv_disp_t *disp_dev;
static int32_t dma_fd;
static lv_color_t *draw_buf = NULL;
static int32_t use_dma_heap = false;
static size_t buf_size = 0;

void lv_port_disp_init(lv_disp_rot_t rotate_disp) {
    lv_coord_t lcd_w, lcd_h;
    /*-------------------------
     * Initialize your display
     * -----------------------*/
    disp_init(NULL, rotate_disp);
    disp_get_sizes(&lcd_w, &lcd_h, NULL);
    /*-----------------------------
     * Create a buffer for drawing
     *----------------------------*/
    const char *heap_name = NULL;
    int32_t flush_cache = false;
    static lv_disp_draw_buf_t draw_buf_dsc;

    uint32_t size_in_px_cnt = lcd_w * lcd_h;
    buf_size = size_in_px_cnt * (LV_COLOR_SIZE / 8);

    if (0 == access(RV1106_CMA_HEAP_PATH, R_OK)) {
        use_dma_heap = true;
        flush_cache = true;
        heap_name = RV1106_CMA_HEAP_PATH;
    } else if (0 == access(DMA_HEAP_DMA32_UNCACHED_PATH, R_OK)) {
        heap_name = DMA_HEAP_DMA32_UNCACHED_PATH;
        use_dma_heap = true;
    }

    if (use_dma_heap) {
        printf("alloc buffer from \"%s\".\n", heap_name);
        if (0 > dma_buf_alloc(heap_name, buf_size, &dma_fd, (void **)&draw_buf)) {
            printf("alloc src %s failed!\n", heap_name);
            return;
        }
    } else {
        draw_buf = malloc(buf_size);
        if (draw_buf == NULL) {
            printf("alloc draw_buf failed!\n");
            return;
        }
    }

    if (flush_cache && dma_fd)
        dma_sync_device_to_cpu(dma_fd);

    lv_disp_draw_buf_init(&draw_buf_dsc, draw_buf, NULL, size_in_px_cnt);   /*Initialize the display buffer*/

    /*-----------------------------------
     * Register the display in LVGL
     *----------------------------------*/
    static lv_disp_drv_t disp_drv;                  /*Descriptor of a display driver*/
    lv_disp_drv_init(&disp_drv);                    /*Basic initialization*/

    /*Set up the functions to access to your display*/

    /*Set the resolution of the display*/
    if (rotate_disp == LV_DISP_ROT_NONE) {
        disp_drv.hor_res = lcd_w;
        disp_drv.ver_res = lcd_h;
        disp_drv.sw_rotate = 0;
        disp_drv.rotated = LV_DISP_ROT_NONE;
    } else if (rotate_disp == LV_DISP_ROT_180) {
        disp_drv.hor_res = lcd_w;
        disp_drv.ver_res = lcd_h;
        disp_drv.sw_rotate = 1;
        disp_drv.rotated = LV_DISP_ROT_180;
    } else if (rotate_disp == LV_DISP_ROT_90) {
        disp_drv.hor_res = lcd_h;
        disp_drv.ver_res = lcd_w;
        disp_drv.sw_rotate = 0;
        disp_drv.rotated = LV_DISP_ROT_NONE;
    } else if (rotate_disp == LV_DISP_ROT_270) {
        disp_drv.hor_res = lcd_h;
        disp_drv.ver_res = lcd_w;
        disp_drv.sw_rotate = 1;
        disp_drv.rotated = LV_DISP_ROT_180;
    }

    /*Used to copy the buffer's content to the display*/
    disp_drv.flush_cb = disp_flush;

    /*Set a display buffer*/
    disp_drv.draw_buf = &draw_buf_dsc;

    /*Required for Example 3)*/
    //disp_drv.full_refresh = 1;

    /* Fill a memory array with a color if you have GPU.
     * Note that, in lv_conf.h you can enable GPUs that has built-in support in LVGL.
     * But if you have a different GPU you can use with this callback.*/
    //disp_drv.gpu_fill_cb = gpu_fill;

    /*Finally register the driver*/
    disp_dev = lv_disp_drv_register(&disp_drv);

    disp_drv.screen_transp = LV_COLOR_SCREEN_TRANSP;
}

void lv_port_disp_deinit(void) {
    if (NULL != disp_dev) {
        lv_disp_remove(disp_dev);
        disp_dev = NULL;
    }

    if (use_dma_heap) {
        if (draw_buf != NULL)
            dma_buf_free(buf_size, &dma_fd, draw_buf);
    } else {
        if (draw_buf != NULL) {
            free(draw_buf);
            draw_buf = NULL;
        }
    }

    disp_exit();
}