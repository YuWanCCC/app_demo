#include "process_tx.h"

#define ENABLE_SLICE 1

static const char short_options[] = "d:lsiqFftw:h:p:A:aCcL:T:Q:D:S:B";
int flag;
static const struct option long_options[] = {
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
    {"fps", required_argument, NULL, 'A'},
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
    {"display-type", required_argument, &flag, 5},
    {"enable-encode", required_argument, &flag, 6},
    {"enable-vo", required_argument, &flag, 7},
    {"enable-adjust-clk", required_argument, &flag, 8},
    {"dst-ip", required_argument, NULL, 'D'},
    {"enSlice", required_argument, NULL, 'S'},
    {"bitrate", required_argument, NULL, 'B'},
    {"dump-stream",required_argument, NULL, 'O'},
    {0, 0, 0, 0}
};

static void parse_args(int argc, char **argv, struct rkcam_context *ctx) {
    int c;
    int index;

    while (1) {
        c = getopt_long(argc, argv, short_options, long_options, &index);
        if (-1 == c)
            break;
        switch (c) {
        case 0:
            switch (flag) {
            case 1:
                ctx->count = atoi(optarg);
                break;
            case 2:
                ctx->skip = atoi(optarg);
                break;
            case 3:
                ctx->mmap = atoi(optarg);
                // ctx->is_stream = true;
                break;
            case 4:
                strcpy(ctx->out_file, optarg);
                ctx->is_savefile = true;
                break;
            case 5:
                ctx->display_type = atoi(optarg);
                break;
            case 6:
                ctx->enable_encode = atoi(optarg);
                break;
            case 7:
                ctx->enable_vo = atoi(optarg);
                break;
            case 8:
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
            break;
        case 'i':
            break;
        case 'F':
            break;
        case 'f':
            break;
        case 't':
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
            if (strstr(ctx->pixel_format, "NV12") != nullptr) {
                ctx->format = RK_FMT_YUV420SP;
                break;
            }
            if (strstr(ctx->pixel_format, "YUV422") != nullptr) {
                ctx->format = RK_FMT_YUV422P;
                break;
            }
            if (strstr(ctx->pixel_format, "YUV444") != nullptr) {
                ctx->format = RK_FMT_YUV444SP;
                break;
            }
            if (strstr(ctx->pixel_format, "BGR3") != nullptr) {
                ctx->format = RK_FMT_BGR888;
                break;
            }
            if (strstr(ctx->pixel_format, "RGB3") != nullptr) {
                ctx->format = RK_FMT_RGB888;
                break;
            }
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
#ifdef ENABLE_SLICE
            ctx->enSlice = atoi(optarg);
#else
            ctx->enSlice = 0;
#endif
            break;
        case 'B':
            ctx->init_bitrate = atoi(optarg);
            break;
        case 'O':
            ctx->enable_dump = atoi(optarg);
            break;
        default:
            break;
        }
    }
}
#include <iostream>
#include <cstring>
#include <chrono>


int main(int argc, char **argv) {
    struct timespec ts;
    unsigned long long times;

    _ctx = new rkcam_context();
    strcpy(_ctx->pixel_format, "\0");
    _ctx->width = 0;
    _ctx->height = 0;
    _ctx->code = 0;
    _ctx->left = 0;
    _ctx->top = 0;
    _ctx->fps = 60;
    _ctx->count = 1;
    _ctx->skip = 0;
    _ctx->mmap = 3;
    _ctx->fd = -1;
    _ctx->format = 0;
    _ctx->frame = 0;
    _ctx->vcrop = 0;
    _ctx->parm = 0;
    _ctx->is_querycap = false;
    _ctx->is_stream = false;
    _ctx->is_savefile = false;
    _ctx->firstFrame = 0;
    _ctx->exit_thread = 0;
    int drmFd = 0;
    int ret = 0;
    clock_gettime(CLOCK_BOOTTIME, &ts);
    times = ts.tv_sec * 1000 + (ts.tv_nsec / 1000000);

    printf("===enter main time is %llu \n", times);
    strcpy(_ctx->device, "/dev/video1");
    strcpy(_ctx->pixel_format, "NV12");
        memcpy(_ctx->strIp, DST_IP, strlen(DST_IP));

    _ctx->width = 1920;
    _ctx->height = 1080;

    parse_args(argc, argv, _ctx);
    //for audio
    _ctx->channel = 2;
    _ctx->sampleRate = 48000;
    _ctx->bitWidth = 16;
    _ctx->devId = 1;
    _ctx->frame_time_ms = 4;
    ret = init_tx();
    if (ret != 0) {
        printf("init tx fail.\n");
        return -1;
    }
    ret = start_tx();
    if (ret != 0) {
        printf("start tx fail.\n");
        return -1;
    }
    
    while (1) {
        sleep(1);
    }

    ret = deinit_tx();
    if (ret != 0) {
        printf("deinit tx fail.\n");
        return -1;
    }
    delete _ctx;
    _ctx = NULL;

    return 0;
}
