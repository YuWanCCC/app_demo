#include "process_rx.h"

static const char short_options[] = "d:lsiqFftw:h:W:H:p:A:aCcL:T:Q:D:S";
int flag;
static const struct option long_options[] =
{
    {"device", required_argument, NULL, 'd'},
    {"list-formats", no_argument, NULL, 'l'},
    {"list-framesize", no_argument, NULL, 's'},
    {"list-frameinterval", no_argument, NULL, 'i'},
    {"querycap", no_argument, NULL, 'q'},
    {"set-format", no_argument, NULL, 'F'},
    {"get-format", no_argument, NULL, 'f'},
    {"try-format", no_argument, NULL, 't'},
    {"width", required_argument, NULL, 'w'},
    {"height", required_argument, NULL, 'h'},
    {"display-w", required_argument, NULL, 'W'},
    {"display-h", required_argument, NULL, 'H'},
    {"pixel-format", required_argument, NULL, 'p'},
    {"set-parm", required_argument, NULL, 'A'},
    {"get-parm", no_argument, NULL, 'a'},
    {"set-crop", no_argument, NULL, 'C'},
    {"get-crop", no_argument, NULL, 'c'},
    {"left", required_argument, NULL, 'L'},
    {"top", required_argument, NULL, 'T'},
    {"cropcap", no_argument, NULL, 'Q'},
    {"stream-count", required_argument, &flag, 1},
    {"stream-skip", required_argument, &flag, 2},
    {"stream-mmap", required_argument, &flag, 3},
    {"stream-to", required_argument, &flag, 4},
    {"display-type", required_argument, &flag, 5},  /*HDMI:0,dp:1,edp:2,dsi:3*/
    {"enable-adjust-clk", required_argument, &flag, 6},
    {"dst-ip", required_argument, NULL, 'D'},
    {"enSlice", required_argument, NULL, 'S'},
    {0, 0, 0, 0}
};

static void parse_args(int argc, char **argv, struct rkcam_context *ctx)
{
    int c;
    int index;

    while (1)
    {
        c = getopt_long(argc, argv, short_options, long_options, &index);
        if (-1 == c)
            break;
        switch (c)
        {
        case 0:
            switch (flag)
            {
            case 1:
                ctx->count = atoi(optarg);
                break;
            case 2:
                ctx->skip = atoi(optarg);
                break;
            case 3:
                ctx->mmap = atoi(optarg);
                //ctx->is_stream = true;
                break;
            case 4:
                strcpy(ctx->out_file, optarg);
                ctx->is_savefile = true;
                break;
            case 5:
                ctx->display_type = atoi(optarg);
                break;
            case 6:
                ctx->enable_adjust_clk = atoi(optarg);
                break;
            }
            break;
        case 'd':
            strcpy(ctx->device, optarg);
            break;
        case 'l':
            ctx->format = 1;
            break;
        case 'q':
            ctx->is_querycap = true;
            break;
        case 's':
            ctx->frame = 1;
            break;
        case 'i':
            ctx->frame = 2;
            break;
        case 'F':
            ctx->format = 2;
            break;
        case 'f':
            ctx->format = 3;
            break;
        case 't':
            ctx->format = 4;
            break;
        case 'w':
            ctx->width = atoi(optarg);
            break;
        case 'h':
            ctx->height = atoi(optarg);
            break;
        case 'W':
            ctx->display_w = atoi(optarg);
            break;
        case 'H':
            ctx->display_h = atoi(optarg);
            break;
        case 'L':
            ctx->left = atoi(optarg);
            break;
        case 'T':
            ctx->top = atoi(optarg);
            break;
        case 'p':
            strcpy(ctx->pixel_format, optarg);
            break;
        case 'A':
            ctx->parm = 1;
            ctx->fps = atoi(optarg);
            break;
        case 'a':
            ctx->parm = 2;
            break;
        case 'Q':
            ctx->vcrop = 1;
            break;
        case 'C':
            ctx->vcrop = 2;
            break;
        case 'c':
            ctx->vcrop = 3;
            break;
        case 'D':
            strcpy(ctx->strIp, optarg);
            break;
        case 'S':
            ctx->enSlice = atoi(optarg);
            break;
        default:
            break;
        }
    }
}

int main(int argc, char **argv)
{
    int ret = 0;
    ctx = new rkcam_context();//(struct rkcam_context *)malloc(sizeof(rkcam_context));
    strcpy(ctx->pixel_format, "\0");
    ctx->width = 0;
    ctx->height = 0;
    ctx->code = 0;
    ctx->left = 0;
    ctx->top = 0;
    ctx->fps = 30;
    ctx->count = 1;
    ctx->skip = 0;
    ctx->mmap = 3;
    ctx->fd = -1;
    ctx->format = 0;
    ctx->frame = 0;
    ctx->vcrop = 0;
    ctx->parm = 0;
    ctx->is_querycap = false;
    ctx->is_stream = false;
    ctx->is_savefile = false;
    ctx->fps_diff = 0.0f;
    ctx->vsync_start = 0;
    //for audio
    ctx->s32DevId = 3; 
    ctx->s32ChnNum = 1;
    ctx->s32SampleRate = 48000;
    ctx->s32BitWidth = 16;
    ctx->s32ReSmpSampleRate = 48000; 
    printf("enter main\n");
    parse_args(argc, argv, ctx);
    ret = init_rx();
    if (ret != 0) {
        printf("init rx fail.\n");
        return -1;
    }
    ret = start_rx();
    if (ret != 0) {
        printf("start rx fail.\n");
        return -1;
    }
    while (1)
    {
        /* code */
        usleep(1000000);
    }
    
    ret = deinit_rx();
    if (ret != 0) {
        printf("deinit rx fail.\n");
        return -1;
    }
    delete ctx;
    ctx = NULL;
    printf("===exit main\n");

    return 0;
}
