/*
 * Copyright 2021 Rockchip Electronics Co. LTD
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
 *
 */
#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <stdio.h>

#ifndef BIT
#define BIT(x) (1 << x)
#endif

#define DISP_CONNECTOR_DPI       BIT(0)
#define DISP_CONNECTOR_HDMI0     BIT(1)
#define DISP_CONNECTOR_HDMI1     BIT(2)
#define DISP_CONNECTOR_DP0       BIT(3)
#define DISP_CONNECTOR_DP1       BIT(4)
#define DISP_CONNECTOR_EDP0      BIT(5)
#define DISP_CONNECTOR_EDP1      BIT(6)
#define DISP_CONNECTOR_MIPI0     BIT(7)
#define DISP_CONNECTOR_MIPI1     BIT(8)
#define DISP_CONNECTOR_LVDS0     BIT(9)
#define DISP_CONNECTOR_LVDS1     BIT(10)
#define DISP_CONNECTOR_CVBS      BIT(11)

#define DISP_MAX_CONNECTOR_TYPES 12
#define DISP_MAX_EDID_SIZE       512

#define DISP_MODE_FLAG_PHSYNC    BIT(0)
#define DISP_MODE_FLAG_PVSYNC    BIT(1)

enum disp_error_code
{
    ERR_SUCCESS = 0,
    ERR_FAILD = -1,
    ERR_INVALID_SOC = -2,
    ERR_INVALID_NULL_PTR = -3,
    ERR_INVALID_PARAM = -4,
    ERR_INVALID_NO_MEM = -5,
    ERR_INVALID_DEV = -6,
    ERR_NOT_SUPPORT = -7,
};

enum disp_plane_name
{
    RK_VOP_PLANE_CLUSTER0,
    RK_VOP_PLANE_CLUSTER1,
    RK_VOP_PLANE_CLUSTER2,
    RK_VOP_PLANE_CLUSTER3,
    RK_VOP_PLANE_ESMART0,
    RK_VOP_PLANE_ESMART1,
    RK_VOP_PLANE_ESMART2,
    RK_VOP_PLANE_ESMART3,
    RK_VOP_PLANE_SMART0,
    RK_VOP_PLANE_SMART1,
    RK_VOP_PLANE_SMART2,
    RK_VOP_PLANE_SMART3,
    RK_VOP_PLANE_MAX
};

enum disp_hdmi_mode
{
    DISP_HDMI_MODE_AUTO = 0,
    DISP_HDMI_MODE_HDMI,
    DISP_HDMI_MODE_DVI
};

enum disp_intf_color_format
{
    DISP_INTF_COLOR_FORMAT_RGB = 0,
    DISP_INTF_COLOR_FORMAT_YCBCR444,
    DISP_INTF_COLOR_FORMAT_YCBCR422,
    DISP_INTF_COLOR_FORMAT_YCBCR420,
    DISP_INTF_COLOR_FORMAT_AUTO,
    DISP_INTF_COLOR_FORMAT_BUTT
};

enum disp_intf_color_depth
{
    DISP_INTF_COLOR_DEPTH_AUTO = 0,
    DISP_INTF_COLOR_DEPTH_8BIT = 8,
    DISP_INTF_COLOR_DEPTH_10BIT = 10,
    DISP_INTF_COLOR_DEPTH_BUTT
};

enum disp_intf_color_space
{
    DISP_INTF_COLOR_SPACE_DEFAULT = 0,
    DISP_INTF_COLOR_SPACE_BT601 = 1,
    DISP_INTF_COLOR_SPACE_BT709 = 2,
    DISP_INTF_COLOR_SPACE_BT2020_CYCC = 8,
    DISP_INTF_COLOR_SPACE_BT2020_RGB = 9,
    DISP_INTF_COLOR_SPACE_BT2020_YCC = 10,
};

enum disp_quant_range
{
    DISP_QUANT_RANGE_DEFULT = 0,
    DISP_QUANT_RANGE_LIMITED,
    DISP_QUANT_RANGE_FULL
};

enum disp_plane_pixel_blend_mode
{
    DISP_PIXEL_BLEND_PRE_MULTIPLIED,
    DISP_PIXEL_BLEND_NON_PREMULTIPLIED,
    DISP_PIXEL_BLEND_NONE,
};

enum disp_hdr_type
{
    DISP_SDR,
    DISP_HDR_10,
    DISP_HDR_HLG,
    DISP_HDR_VIVID = 5,
    DISP_HDR_10PLUS = 8,
    DISP_HDR_BUTT
};

enum disp_frame_eotf
{
    DISP_FRAME_EOTF_GAMMA_DEFAULT,
    DISP_FRAME_EOTF_GAMMA_SRGB,
    DISP_FRAME_EOTF_GAMMA_BT709,
    DISP_FRAME_EOTF_SMPTE_ST2084,
    DISP_FRAME_EOTF_BT_2100_HLG,
};

enum disp_frame_colorspace
{
    DISP_FRAME_COLORSAPCE_DEFAULT,
    DISP_FRAME_COLORSAPCE_BT601,
    DISP_FRAME_COLORSAPCE_BT709,
    DISP_FRAME_COLORSAPCE_BT2020,
};

typedef void (*disp_callback_fn)(void *data);

struct disp_callback
{
    disp_callback_fn fn;
    void *data;
};

struct disp_hdr_static_metadata_infoframe
{
    uint8_t eotf;
    uint8_t metadata_type;
    struct
    {
        uint16_t x, y;
    } display_primaries[3];
    struct
    {
        uint16_t x, y;
    } white_point;
    uint16_t max_display_mastering_luminance;
    uint16_t min_display_mastering_luminance;
    uint16_t max_cll;
    uint16_t max_fall;
};

struct disp_hdr_output_static_metadata
{
    uint32_t metadata_type;
    union
    {
        struct disp_hdr_static_metadata_infoframe hdmi_metadata_type1;
    };
};

struct disp_edid
{
    uint32_t size;
    uint8_t data[DISP_MAX_EDID_SIZE];
};

struct disp_hdr_static_metadata
{
    uint8_t eotf;
    uint8_t metadata_type;
    uint16_t max_cll;
    uint16_t max_fall;
    uint16_t min_cll;
};

struct disp_intf_capacity
{
    uint32_t connected;

    struct disp_edid edid;
    struct disp_hdr_static_metadata static_metadata;
};

struct disp_intf_attrs
{
    uint32_t color_format;
    uint32_t color_depth;
    uint32_t color_space;
    uint32_t quant_range;
    struct disp_hdr_output_static_metadata *hdr_metadata;
};

struct disp_buf_attrs
{
    uint32_t format;
    uint64_t modifier;

    uint32_t width;
    uint32_t height;
    uint32_t vir_width;
    uint32_t vir_height;
    uint32_t stride;
};

struct disp_window
{
    uint32_t x, y;
    uint32_t w, h;
};

struct disp_frame_hdr_info
{
    uint32_t type;
    uint32_t size;
    void *regs;
};

struct disp_frame
{
    int fd;
    void *addr;
    uint32_t size;
    struct disp_buf_attrs attrs;

    struct disp_window crop;

    uint32_t pixel_blend_mode;
    uint32_t quant_range;
    uint32_t eotf;
    uint32_t colorspace;
    struct disp_frame_hdr_info hdrinfo;

    struct disp_callback release;
};

struct disp_mode
{
    uint32_t clock;
    uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew;
    uint16_t vdisplay, vsync_start, vsync_end, vtotal, vscan;

    uint32_t vrefresh;
    uint32_t flags;
};

struct disp_output_attrs
{
    uint32_t connectors;
    struct disp_mode mode;
};

struct disp_layer_attrs
{
    uint32_t zorder;
    struct disp_window dst;
};

typedef void (*disp_vsync_func)(uint32_t sec, uint32_t usec, void *data);

int disp_init(void);
int disp_destroy(void);

int disp_create_output(uint32_t id, struct disp_output_attrs *attrs);
int disp_destroy_output(uint32_t id);

int disp_add_vsync_callback(uint32_t id, disp_vsync_func func, void *data);
int disp_remove_vsync_callback(uint32_t id, disp_vsync_func func);

int disp_set_intf_attrs(uint32_t disp_if_id, struct disp_intf_attrs *attrs);
int disp_get_intf_attrs(uint32_t disp_if_id, struct disp_intf_attrs *attrs);
int disp_get_intf_capacity(uint32_t disp_if_id, struct disp_intf_capacity *capacity);

int disp_get_layer_attrs(uint32_t layer_id, struct disp_layer_attrs *attrs);
int disp_set_layer_attrs(uint32_t layer_id, struct disp_layer_attrs *attrs);

struct disp_frame *disp_create_buffer(struct disp_buf_attrs *attrs);
int disp_destroy_buffer(struct disp_frame *buf);

int disp_send_frame(uint32_t output_id, uint32_t layer_id, struct disp_frame *frame, int timeout);

char *disp_utils_get_plane_name(int layer_id);
char *disp_utils_get_intf_name(int intf);

#endif /* DISPLAY_H */