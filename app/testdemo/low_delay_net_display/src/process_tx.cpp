#include "process_tx.h"
#include "mpi_vi.h"
#include "mpi_ai.h"
#define ENABLE_SOUND 0
struct drmContext *drmCtx = NULL;
pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t g_cond = PTHREAD_COND_INITIALIZER;
struct rkcam_context *_ctx;

void SetVcntTimeCb() {
    SetVcntTime(get_vcnt_time());
}

void NetStatus(const uint32_t bitrate, const uint8_t fraction_loss, const uint32_t rtt) {
    printf(">>>>>bitrate=%d,fraction_loss=%d,rtt=%d,setBitrate=%d,maxbitrate=%d,minbitrate=%d\n", bitrate/1000, fraction_loss, rtt,bitrate/1000, _ctx->max_bitrate/1000, _ctx->min_bitrate/1000);
    //set_bitrate(bitrate/1000, _ctx->max_bitrate/1000, _ctx->min_bitrate/1000);
}

void KeyFrameReq() {
    if (_ctx->keyframe_req == 1) {
       return;
    }
    printf(">>>KeyFrameReq\n");
    keyframe_req();
    _ctx->keyframe_req = 1;
}

uint32_t stringtopixelformat(char *str) {
    int i;
    int j = 0;
    uint32_t t = 0;

    while (*str != '\0') {
        i = str[0];
        t += i << (8 * j);
        str++;
        j++;
    }
    if (j == 3)
        t += 0x20 << 24;
    return t;
}

static int stop_stream() {
    _ctx->is_stream = 0;
    return 0;
}

static int start_stream(struct rkcam_context *ctx) {
    struct timespec ts;
    unsigned long long times;
    clock_gettime(CLOCK_BOOTTIME, &ts);
    times = ts.tv_sec * 1000 + (ts.tv_nsec / 1000000);
    if (ctx->is_stream > 0) {
        return 0;
    }
    ctx->is_stream = 1;
    pthread_cond_signal(&g_cond);
    printf("start stream on time is %llu\n", times);
    return 0;
}

int gsFrameCcount = 0;
int mFrameCount;
int mLastFrameCount = 0;
nsecs_t mLastFpsTime = 0;
float mFps = 0;
void showFps1() {
    mFrameCount++;
    nsecs_t now = systemTime();
    nsecs_t diff = now - mLastFpsTime;
    if (diff > ms2ns(1000)) {
        mFps = ((mFrameCount - mLastFrameCount) * float(s2ns(1))) / diff;
        mLastFpsTime = now;
        mLastFrameCount = mFrameCount;
        printf(">>>>>>>>>mFrameCount = %d mFps = %2.3f,bitrate=%d\n", mFrameCount, mFps, _ctx->realBitRate);
        _ctx->realBitRate = 0;
        SetSendFps(mFps);
    }
}

static int sendCmd(struct rkcam_context *ctx, COMMAND_ID cmdid) {
    CommandData_t cmd_data;
    cmd_data.cmdId = htonl(cmdid);
    cmd_data.seqNum = 0;
    cmd_data.cmdLen = sizeof(CommandData_t);
    return ctx->tcp->Send((char *)&cmd_data, sizeof(CommandData_t));
}

static int estimate_max_bitrate(int width, int height, int fps) {
    int bitrate = width * height * fps * 0.2 * 0.2;
    return bitrate;
}

static int estimate_min_bitrate(int width, int height, int fps) {
    int bitrate = width * height * fps * 0.2 * 0.05;
    return bitrate;
}

static nsecs_t deq_time = 0;
static int test_count = 0;
static int ifisfirst = 1;
void venc_callback(bool isKeyFrame, unsigned char *data, int len) {
    struct timespec ts;
    unsigned long long times;

    if (_ctx->enable_dump > 0) {
        if (_ctx->dump_file != NULL) {
            printf("recv data test_count=%d,len=%d, writen\n", test_count, len);
            fwrite(data, 1, len, _ctx->dump_file);
            fflush(_ctx->dump_file);
        }
    }

    if (isKeyFrame && _ctx->keyframe_req == 1) {
        _ctx->keyframe_req = 0;
    }
    
    rt_atrace_begin_body("send_data");
    //if (test_count < 500) {
        // 记录开始时间
        //auto start = std::chrono::high_resolution_clock::now();
        if (ifisfirst == 1 && isKeyFrame) {
            clock_gettime(CLOCK_BOOTTIME, &ts);
            times = ts.tv_sec * 1000 + (ts.tv_nsec / 1000000);

            printf("begin to send first  key frame time is %llu \n", times);
            ifisfirst = 0;
        }
        send_data(data, len);
        // 记录结束时间
        //auto end = std::chrono::high_resolution_clock::now();     
               // 计算耗时（微秒）
        //auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
        //test_count++;
    //}
    rt_atrace_end_body();
    if (!_ctx->firstFrame) {
        sendCmd(_ctx, START_VSYNC);
        _ctx->firstFrame = 1;
    }
    _ctx->realBitRate += len * 8;
    showFps1();
    return;
}

void ai_callbak(unsigned char *data, int len) {
    send_data(data, len, 1);
    return;
}

static int _set_vo(int exp_fd) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    set_vo_time_ms = (ts.tv_sec * 1000000000 + ts.tv_nsec) / 1000000;
    rt_atrace_begin_body("sendframe_vo");
    SetVoTime(set_vo_time_ms);
    int buffer_len = 0;
    if (strstr(_ctx->pixel_format, "NV12") != nullptr) {
        buffer_len = _ctx->width * _ctx->height * 3 / 2;
    } else if (strstr(_ctx->pixel_format, "BGR3") != nullptr) {
        buffer_len = _ctx->width * _ctx->height * 3;
    } else if (strstr(_ctx->pixel_format, "YUV444") != nullptr) {
        buffer_len = _ctx->width * _ctx->height * 3;
    } else {
        buffer_len = _ctx->width * _ctx->height * 3;
    }
    sendframe_vo_for_fd(exp_fd, buffer_len);
    showFps1();
    rt_atrace_end_body();
    return 0;
}

static int process_stream() {
    int ret;
    struct timespec ts;
    unsigned long long times;

    printf("enter process_stream\n");
    if (!_ctx->enable_vo) {
        clock_gettime(CLOCK_BOOTTIME, &ts);
        times = ts.tv_sec * 1000 + (ts.tv_nsec / 1000000);
        printf("begin to wait cond %llu \n", times);
        pthread_mutex_lock(&g_mutex);
        pthread_cond_wait(&g_cond, &g_mutex);
        pthread_mutex_unlock(&g_mutex);
        clock_gettime(CLOCK_BOOTTIME, &ts);
        times = ts.tv_sec * 1000 + (ts.tv_nsec / 1000000);
        printf("okay to get cond %llu \n", times);

    }
    printf("start process_stream\n");
    while (!_ctx->exit_thread) {
        int exp_fd = get_frame();
        if (exp_fd > 0) {
            if (_ctx->enable_vo > 0) {
                ret = _set_vo(exp_fd);
                if (ret < 0) {
                    printf("set vo fail\n");
                    return -1;
                }
            }
            int buffer_len = 0;
            if (strstr(_ctx->pixel_format, "NV12") != nullptr) {
                buffer_len = _ctx->width * _ctx->height * 3 / 2;
            } else if (strstr(_ctx->pixel_format, "BGR3") != nullptr) {
                buffer_len = _ctx->width * _ctx->height * 3;
            } else if (strstr(_ctx->pixel_format, "YUV444") != nullptr) {
                buffer_len = _ctx->width * _ctx->height * 3;
            } else {
                buffer_len = _ctx->width * _ctx->height * 3;
            }
            char strDeq[100];
            memset(&strDeq[0], 0, 100);
            strcpy(&strDeq[0], "deq set_frame\0");
            rt_atrace_begin_body(strDeq);
            set_frame(exp_fd, buffer_len);
            rt_atrace_end_body();
        }
    }
    return 0;
}

static int _start_send() {
    int ret = start_send();
    if (ret != 0) {
        printf("start_send fail.\n");
        return -1;
    }
    return 0;
}

#if ENABLE_SOUND
static int _start_ai() {
    std::shared_ptr<std::thread> ai_thread;
    ai_thread = std::make_shared<std::thread>(getDataThread);
    ai_thread->detach();
    printf("start ai succ.\n");
    return 0;
}
#endif

static int _start_venc() {
    std::shared_ptr<std::thread> venc_thread;
    venc_thread = std::make_shared<std::thread>(thread_get_stream, 0);
    venc_thread->detach();
    printf("start venc succ.\n");
    return 0;
}

static int _start_process() {
    std::shared_ptr<std::thread> process_thread;
    process_thread = std::make_shared<std::thread>(process_stream);
    process_thread->detach();
    printf("start process succ.\n");
    return 0;
}

static int _init_send() {
    int ret = 0;
    ret = init_send(_ctx->strIp, RTP_PORT, RTCP_PORT, NetStatus, KeyFrameReq, _ctx->max_bitrate / 2, _ctx->min_bitrate, _ctx->max_bitrate);
    if (ret != 0) {
        printf("init send fail.\n");
        return -1;
    }
    printf("init send succ.\n");
    return ret;
}

static int _init_venc() {
    int ret = 0;
    int minBitrate = estimate_min_bitrate(_ctx->width, _ctx->height, _ctx->fps);
    int maxBitrate = estimate_max_bitrate(_ctx->width, _ctx->height, _ctx->fps);
    _ctx->max_bitrate = maxBitrate;
    _ctx->min_bitrate = minBitrate;
    if (_ctx->enable_dump > 0) {
        _ctx->dump_file = fopen(DUMP_H265, "wb");
        if (_ctx->dump_file == NULL) {
            printf("open dump file fail.\n");
            return -1;
        }
        printf("open dump file succ.\n");
    }
    rt_atrace_begin_body("init_venc");
    if (_ctx->enSlice)
        ret = init_venc(_ctx->width, _ctx->height, _ctx->width, _ctx->height, 0, _ctx->init_bitrate, _ctx->format, venc_callback, true, 3, 4);
    else
        ret = init_venc(_ctx->width, _ctx->height, _ctx->width, _ctx->height, 0, _ctx->init_bitrate, _ctx->format, venc_callback, false, 0, 0);

    if (ret != 0) {
        printf("init venc fail.\n");
        return -1;
    }
    rt_atrace_end_body();
    printf("init venc succ,width=%d,height=%d, max_bitrate=%d, minBitrate=%d, fps=%d.\n", _ctx->width, _ctx->height, _ctx->max_bitrate, _ctx->min_bitrate, _ctx->fps);
    return ret;
}

static int _init_vi() {
    int ret = 0;
    ret = vi_init(_ctx->width, _ctx->height, 8, _ctx->device, _ctx->format);
    if (ret != 0) {
        printf("init vi fail,ret=%d.\n", ret);
        return -1;
    }
    printf("+++init vi succ.\n");
    return ret;
}
#if ENABLE_SOUND
static int _init_ai() {
    int ret = 0;
    ret = init_ai(_ctx->channel, _ctx->sampleRate, _ctx->bitWidth, _ctx->devId, _ctx->frame_time_ms, ai_callbak);
    if (ret != 0) {
        printf("init ai fail,ret=%d.\n", ret);
        return -1;
    }

    printf("+++init ai succ.\n");
    return ret;
}

static int _deinit_ai() {
    int ret = 0;
    ret = deinit_ai();
    if (ret != 0) {
        printf("deinit ai fail,ret=%d.\n", ret);
        return -1;
    }
    printf("+++deinit ai succ.\n");
    return ret;
}
#endif

static int _init_vo() {
    int ret = 0;
    if (_ctx->enable_vo > 0) {
        int vcnt_line = _ctx->display_h * 0.8f;
        int vo_format = _ctx->format;
        switch (_ctx->display_type) {
        case 0:
            ret = init_vo(_ctx->width, _ctx->height, _ctx->display_w, _ctx->display_h, vo_format, VO_INTF_HDMI, RK3588_VO_DEV_HD0, vcnt_line);
            break;
        case 1:
            ret = init_vo(_ctx->width, _ctx->height, _ctx->display_w, _ctx->display_h, vo_format, VO_INTF_DP, RK3588_VO_DEV_HD2, vcnt_line);
            break;
        case 2:
            ret = init_vo(_ctx->width, _ctx->height, _ctx->display_w, _ctx->display_h, vo_format, VO_INTF_EDP, RK3588_VO_DEV_HD2, vcnt_line);
            break;
        case 3:
            ret = init_vo(_ctx->width, _ctx->height, _ctx->display_w, _ctx->display_h, vo_format, VO_INTF_MIPI, RK3588_VO_DEV_HD3, vcnt_line);
            break;
        default:
            ret = init_vo(_ctx->width, _ctx->height, _ctx->display_w, _ctx->display_h, vo_format, VO_INTF_HDMI, RK3588_VO_DEV_HD0, vcnt_line);
            break;
        }
        if (ret != 0) {
            printf("init vo fail.\n");
            return -1;
        }
        printf("init vo succ.\n");
    }
    return ret;
}

static int _start_vsync() {
    if (_ctx->enable_adjust_clk > 0) {
        const char *path = "/dev/dri/card0";
        int drmFd = open(path, O_RDWR);
        SetDisplayType(_ctx->display_type);
        SetVsyncCallback(SetVcntTimeCb);
        std::shared_ptr<std::thread> mVsyncTread;
        mVsyncTread = std::make_shared<std::thread>(VsyncTread, drmFd);
        mVsyncTread->detach();
        printf("start vsync succ.\n");
    }
    return 0;
}

static void HandlerTCPMessage(int sockfd, char *buffer, int size) {
    CommandData_t *cmd_data = (CommandData_t *)buffer;
    cmd_data->cmdId = ntohl(cmd_data->cmdId);
    cmd_data->seqNum = ntohl(cmd_data->seqNum);
    printf("recv command id =%d, seqnum=%d\n", cmd_data->cmdId, cmd_data->seqNum);
    switch (cmd_data->cmdId) {
    case PROCESS_STREAM: {
       /* std::shared_ptr<std::thread> process_thread;
        process_thread = std::make_shared<std::thread>(process_stream);
        process_thread->detach();*/
        break;
    }
    case START_STREAM: {
        start_stream(_ctx);
        break;
    }
    case STOP_STREAM: {
        stop_stream();
        break;
    }
    default:
        printf("command id error.\n");
        break;
    }
    return;
}

int initTcp() {
    _ctx->tcp = std::make_shared<TCPServer>();
    _ctx->tcp->RegisterRecvCallBack(HandlerTCPMessage);
    _ctx->tcp->Process(SERVER_PORT);
    return 0;
}

int init_tx() {
    int ret = 0;
    ret = _init_vi();
    if (ret != 0) {
        printf("init vi fail.\n");
        return -1;
    }

    ret = _init_vo();
    if (ret != 0) {
        printf("init vo fail.\n");
        return -1;
    }

    ret = _init_venc();
    if (ret != 0) {
        printf("init venc fail.\n");
        return -1;
    }
#if ENABLE_SOUND
    ret = _init_ai();
    if (ret != 0) {
        printf("init ai fail.\n");
        return -1;
    }
#endif
    ret = _init_send();
    if (ret != 0) {
        printf("init send fail.\n");
        return -1;
    }

    ret = initTcp();
    if (ret != 0) {
        printf("init tcp fail.\n");
        return -1;
    }
    return 0;
}

int start_tx() {
    int ret = 0;
    _ctx->exit_thread = 0;
    ret = _start_venc();
    if (ret != 0) {
        printf("start venc fail.\n");
        return -1;
    }
#if ENABLE_SOUND
    ret = _start_ai();
    if (ret != 0) {
        printf("start ai fail.\n");
        return -1;
    }
#endif
    ret = _start_send();
    if (ret != 0) {
        printf("start send fail.\n");
        return -1;
    }
    ret = _start_process();
    if (ret != 0) {
        printf("start process fail.\n");
        return -1;
    }
    ret = _start_vsync();
    if (ret != 0) {
        printf("start vsync fail.\n");
        return -1;
    }
    return 0;
}

int stop_tx() {
    _ctx->exit_thread = 1;
    stop_stream();
    return 0;
}

int deinit_tx() {
    deinit_venc();
    deinit_send();
#if ENABLE_SOUND
    deinit_ai();
    deinit_vi();
#endif
    if (_ctx->enable_vo > 0) {
        deinit_vo();
    }
    if (_ctx->enable_dump > 0) {
        if (_ctx->dump_file != NULL) {
            fclose(_ctx->dump_file);
            _ctx->dump_file = NULL;
        }
    }
    return 0;
}
