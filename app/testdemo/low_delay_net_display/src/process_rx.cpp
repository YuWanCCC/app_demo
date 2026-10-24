#include "process_rx.h"
#define  ENABLE_SOUND 0
struct rkcam_context *ctx = NULL;
void SetVcntTimeCb()
{
    SetVcntTime(get_vcnt_time());
}

int _init_vo() {

    RK_S32 ret = 0;
    // Initialize VO here
    int vcnt_line = ctx->display_h * 0.8f;
    switch (ctx->display_type)
    {
        case 0:
            ret = init_vo(ctx->width, ctx->height, ctx->display_w, ctx->display_h, RK_FMT_YUV420SP, VO_INTF_HDMI, RK3588_VO_DEV_HD0, vcnt_line);
            break;
        case 1:
            ret = init_vo(ctx->width, ctx->height, ctx->display_w, ctx->display_h, RK_FMT_YUV420SP, VO_INTF_DP, RK3588_VO_DEV_HD2, vcnt_line);
            break;
        case 2:
            ret = init_vo(ctx->width, ctx->height, ctx->display_w, ctx->display_h, RK_FMT_YUV420SP, VO_INTF_EDP, RK3588_VO_DEV_HD2, vcnt_line);
            break;
        case 3:
            ret = init_vo(ctx->width, ctx->height, ctx->display_w, ctx->display_h, RK_FMT_YUV420SP, VO_INTF_MIPI, RK3588_VO_DEV_HD3, vcnt_line);
            break;
        default:
            ret = init_vo(ctx->width, ctx->height, ctx->display_w, ctx->display_h, RK_FMT_YUV420SP, VO_INTF_HDMI, RK3588_VO_DEV_HD0, vcnt_line);
            break;
    }

    if (ret != RK_SUCCESS) {
        printf("init vo fail.\n");
        return -1;
    }

    if (ctx->enable_adjust_clk > 0)
    {
        const char *path = "/dev/dri/card0";
        ctx->drmFd = open(path, O_RDWR);
        if (ctx->drmFd <= 0) {
            printf("open %s fail.\n", path);
            return -1;
        }
        SetDisplayType(ctx->display_type);
        SetVsyncCallback(SetVcntTimeCb);
        std::thread  t(VsyncTread, ctx->drmFd);
        t.detach();
    }
    return 0;
}

//tcp
static void HandlerTCPMessage(char *buffer, int size)
{
    CommandData_t *cmd_data = (CommandData_t *)buffer;
    cmd_data->cmdId = ntohl(cmd_data->cmdId);
    cmd_data->seqNum = ntohl(cmd_data->seqNum);
    printf("recv command id =%d, seqnum=%d\n", cmd_data->cmdId, cmd_data->seqNum);
    switch (cmd_data->cmdId)
    {
        case START_VSYNC:
        {
            break;
        }
        default:
            printf("command id error.\n");
        break;
    }
}

static int initTcp()
{
    if (ctx == NULL) {
       return -1;
    }
    ctx->tcpclient = std::make_shared<TCPClient>();
    ctx->tcpclient->RegisterRecvCallBack(HandlerTCPMessage);
    return ctx->tcpclient->Setup(ctx->strIp, SERVER_PORT);
}

static int sendCmd(COMMAND_ID cmdid)
{
    CommandData_t cmd_data;
    cmd_data.cmdId = htonl(cmdid);
    cmd_data.seqNum = 0;
    cmd_data.cmdLen = sizeof(CommandData_t);
    return ctx->tcpclient->Send((char *)&cmd_data, sizeof(CommandData_t));
}
#include <mutex>
// Initialize VDEC here
static int test_count = 0;
static int test_seq = 0;
static void RecvData(unsigned char *data, int len, int channel)
{
    char trace_data[50] = {0};
    test_count++;
    test_seq++;
    sprintf(trace_data, "vdec_send_frame %d", test_seq);
    rt_atrace_begin_body(trace_data);
    if (ctx->enSlice) {
        if (test_count % 4 == 0)
        {
            vdec_send_frame(data, len, 1);
            test_seq = 0;
        }
        else
        {
            vdec_send_frame(data, len, 0);
        }
    } else {
        vdec_send_frame(data, len, 0);
    }

    if (ctx->_fp0) {
        printf("recv data test_count=%d,len=%d, writen\n",test_count,len);
        fwrite(data, 1, len, ctx->_fp0);
        fflush(ctx->_fp0);
    }
    rt_atrace_end_body();
}

static void RecvAudioData(unsigned char *data, int len, int channel)
{
#ifdef DUMP_PCM
    if (_ctx->fp_pcm) {
        printf("recv audio data test_count=%d,len=%d, writen\n",test_count,len);
        fwrite(data, 1, len, _ctx->fp_pcm);
        fflush(_ctx->fp_pcm);
    }
#endif
#if ENABLE_SOUND
    //printf("recv audio data len=%d, writen\n", len);
    int ret = sendData(data, len);
    if (ret != 0) {
        printf("send audio data failed, ret=%d\n", ret);
    }
#endif
    return;
}

int mFrameCount;
int mLastFrameCount = 0;
nsecs_t mLastFpsTime = 0;
float mFps = 0;
void showFps()
{
    mFrameCount++;
    nsecs_t now = systemTime();
    nsecs_t diff = now - mLastFpsTime;
    if (diff > ms2ns(500))
    {
        mFps = ((mFrameCount - mLastFrameCount) * float(s2ns(1))) / diff;
        mLastFpsTime = now;
        mLastFrameCount = mFrameCount;
        ctx->send_fps = mFps;
        SetSendFps(ctx->send_fps);
        printf(">>>>>>>>>---mFrameCount = %d mFps = %2.3f\n", mFrameCount, mFps);
    }
}
static int dec_count = 0;
void vdec_callback(int vir_width, int vir_height, int width, int height, void *obj)
{
    dec_count++;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    set_vo_time_ms = (ts.tv_sec * 1000000000 + ts.tv_nsec) / 1000000;
    rt_atrace_begin_body("sendframe_vo");
    SetVoTime(set_vo_time_ms);
    sendframe_vo(obj);
    showFps();
    rt_atrace_end_body();
}

int _init_vdec() {
    // Initialize VDEC here
    int ret = init_vdec(NULL, vdec_callback, ctx->width, ctx->height, ctx->enSlice);
    if (ret != RK_SUCCESS) {
        return -1;
    }
    return 0;
}

int _init_ao() {
    // Initialize AO here
    usleep(1000000);
    RK_S32 ret = init_ao(ctx->s32DevId, ctx->s32ChnNum, ctx->s32SampleRate, ctx->s32BitWidth, ctx->s32ReSmpSampleRate);
    if (ret != 0) {
        printf("init ao fail,ret=%d.\n", ret);
        return -1;
    }
    printf("+++init ao succ.\n");
    return 0;
}

int _init_recv() {
    // Initialize receiver here
    RK_S32 ret = init_recv(0, RTP_PORT, RecvData, RecvAudioData);
    if (ret != 0) {
        printf("init recv fail.\n");
        return -1;
    }
    return 0;
}

int init_rx() {
    // Initialize RX here
    int ret = 0;
#if ENABLE_DUMP
    ctx->_fp0 = fopen(DUMP_H265, "wb");
#endif
#ifdef ENABLE_PCM
    ctx->fp_pcm = fopen(DUMP_PCM, "wb");
    if (ctx->fp_pcm == NULL) {
        printf("open dump file %s failed", DUMP_PCM);
        return -1;
    }
#endif
    ret = _init_vo();
    if (ret != 0) {
        printf("init vo fail.\n");
        return -1;
    } else {
        printf("+++init vo succ.\n");
    }

    ret = _init_vdec();
    if (ret != 0) {
        printf("init vdec fail.\n");
        return -1;
    } else {
        printf("+++init vdec succ.\n");
    }
#if ENABLE_SOUND
    ret = _init_ao();
    if (ret != 0) {
        printf("init ai fail.\n");
        return -1;
    } else {
        printf("+++init ai succ.\n");
    }
#endif
    ret = _init_recv();
    if (ret != 0) {
        printf("init send fail.\n");
        return -1;
    } else {
        printf("+++init recv succ.\n");
    }

    while(!initTcp())
    {
        usleep(1000000);
    }
    return 0;
}

int deinit_rx() {
    // Deinitialize RX here
    deinit_vo();
    stop_recv(0);
    deinit_recv(0);
    close(ctx->drmFd);
    VsyncThreadExit();
    return 0;
}

int start_rx() {
    rt_atrace_begin_body("start_stream");
    int ret = sendCmd(PROCESS_STREAM);
    if (ret <= 0) {
        printf("send cmd fail.\n");
        return -1;
    }
    std::thread  t1(thread_vdec_get_frame, 0);
    t1.detach();
    ret = start_recv(0);
    if (ret != 0) {
        printf("start recv fail.\n");
        return -1;
    }
    usleep(500000);
    sendCmd(START_STREAM);
    rt_atrace_end_body();
    return 0;
}

