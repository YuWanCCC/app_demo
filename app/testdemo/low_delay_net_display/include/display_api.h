enum DISPLAY_INTF {
    DRM_INTFACE,
    ROCKIT_VO_INTERFACE
};

struct display_attrs
{
    int w;
    int h;
    int format;
    int stride;
    int display_type;
    int output_id;
    int plane_id;
    int connector;
    int buffer_fd;
};

struct display_device
{

    int (*display_init)(struct display_attrs *attrs);
    int (*display_deinit)(struct display_attrs *attrs);

    int (*display_create_buffer)(struct display_attrs *attrs);
    int (*display_destory_buffer)();

    int (*send_frame)(struct display_attrs *attrs, int buffer_fd);

};

int init_display(struct display_device *disp, DISPLAY_INTF intf_type);

int deinit_display();



