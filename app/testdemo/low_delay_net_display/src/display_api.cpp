#include "display_api.h"
#include <map>
#include "drm_fourcc.h"
extern "C" {
#include "display.h"
}
static std::map<int, struct disp_frame *> drm_map_buffer;



//drm impl
int  drm_init(struct display_attrs *attrs)
{
    struct disp_output_attrs output_attrs;
    disp_init();
    output_attrs.connectors = attrs->connector;

    /*if (attrs->w==3840 && attrs->h==2160)
         parse_modeline("594.0 3840 4016 4104 4400 2160 2168 2178 2250 +hsync +vsync", &output_attrs.mode);
    else
         parse_modeline("148.5 1920 2008 2052 2200 1080 1084 1089 1125 +hsync +vsync", &output_attrs.mode);*/

    disp_create_output(attrs->output_id, &output_attrs);

    return 0;
}

int drm_deint(struct display_attrs *attrs)
{
    disp_destroy_output(attrs->output_id);
    disp_destroy();
    return 0;
}

int drm_create_buffer(struct display_attrs *attrs)
{

    struct disp_frame *fb;
    struct disp_buf_attrs buf_attr;
    buf_attr.stride = attrs->stride;
    buf_attr.width = attrs->w;
    buf_attr.height = attrs->h;
    buf_attr.format = attrs->format;
    buf_attr.modifier = DRM_FORMAT_MOD_INVALID;
    fb = disp_create_buffer(&buf_attr);
    if (!fb)
    {
        disp_destroy();
        return -1;
    }
    drm_map_buffer[fb->fd] = fb;
    return fb->fd;
}

int drm_destory_buffer()
{

    // 使用基于范围的for循环遍历map
    for (const auto &pair : drm_map_buffer)
    {
        struct disp_frame *fb = pair.second;
        disp_destroy_buffer(fb);
    }
    return 0;
}

int drm_send_frame(struct display_attrs *attrs, int buffer_fd)
{
    int fd = buffer_fd;
    std::map<int, struct disp_frame *>::iterator it = drm_map_buffer.find(fd);
    if (it != drm_map_buffer.end())
    {
        struct disp_frame *fb = it->second;
        disp_send_frame(attrs->output_id, attrs->plane_id, fb, 0);
    }
    return 0;
}


//vo impl
int init_display(struct display_device *disp, DISPLAY_INTF intf_type)
{

    if (intf_type == DRM_INTFACE)    //
    {
        disp->display_init = drm_init;
        disp->display_deinit = drm_deint;
        disp->display_create_buffer = drm_create_buffer;
        disp->display_destory_buffer = drm_destory_buffer;
        disp->send_frame = drm_send_frame;
    }
}

int deinit_display()
{

}

