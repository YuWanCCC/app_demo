// Copyright 2021 Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "freetype2/ft2build.h"
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_STROKER_H
#include FT_IMAGE_H

#include <wchar.h>

int create_font(const char *font_path, int font_size);
int destroy_font();
int set_font_size(int font_size);
int get_font_size();
unsigned int set_font_color(unsigned int font_color);
unsigned int get_font_color();
void draw_argb8888_buffer(unsigned int *buffer, int buf_w, int buf_h);
void draw_argb8888_wchar(unsigned char *buffer, int buf_w, int buf_h,
                         const wchar_t wch);
void draw_argb8888_text(unsigned char *buffer, int buf_w, int buf_h,
                        const wchar_t *wstr, int rect_x, int rect_y);
int wstr_get_actual_advance_x(const wchar_t *wstr);
