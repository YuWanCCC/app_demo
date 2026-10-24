#include <sys/time.h>
#include <thread>
#include "drm_fourcc.h"
#include "xf86drm.h"
#include "xf86drmMode.h"
#include "rk_mpi_vo.h"
#include "rk_comm_video.h"
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>

#include "AdjustClkApi.h"
#include "Timers.h"
#include "rt_trace.h"
#define INTERNAL_FPS 60
#define MAX_DIFF 6
static float  sum_diff_fps = 0;
static float  last_sum_diff_fps = 0;
static int  sum_fps_count = 0;
static int  find_best_clk = 0;
static int  last_vcnt_diff = 0;
static int  best_vpll_clk = 0;
static int  find_best_clk_count = 0;
static int  diff_direct = 0;
static int  skip_first = 0;
int  _hdmi_fd0 = -1;
static int mFrameCount[2];
static int mLastFrameCount[2];
static nsecs_t mLastFpsTime[2];
static float mFps[2];
static float mSend_fps = 0;
static float mRecv_fps = 0;
static RK_U32  mVcnt_time = 0;
static RK_U32  mSetVoTime = 0;
static int vsync_start = 0;
static int mDisplayType = 0;
static int mThreadExit = 0;
VSYNC_SETVCNT_CB  mSetVcntCB;
static int mAdjusClkCnt = 0;

void HdmiPhyDynamicAdjusClk(int resolution, int value, int phy)
{
    /*int clk_rates[][3] =
    {
        {148498900, 148500000, 148512000},
       // {296998120, 297000000, 297001880}, //297016270 297002500 | 296998900<br>296997000 |
        {296987000, 297000100, 297013000},
        {593976240, 594000000,  594023760},//4K RGB输出
    };*/

    int clk_rates[][3] = 
    {
        {-10, 0, 10},
        {-10, 0, 10},
        {-10, 0,  10},	  
    };

    
    int *pclk;
    char str[128] = {0};
    switch (resolution)
    {
    case VO_OUTPUT_1080P30:
    case VO_OUTPUT_1080P60:
        pclk = clk_rates[0];
        break;
    case VO_OUTPUT_3840x2160_30:
    case VO_OUTPUT_3840x2160_60:
        pclk = clk_rates[1];

        break;
    }
    mAdjusClkCnt++;
    sprintf(str, "%d", pclk[value + 1]);
    if (_hdmi_fd0 > 0 && phy == 0)
    {
        write(_hdmi_fd0, str, strlen(str) + 1);
    }
}


void HdmiPhyClkInit()
{
    char phy0[128] = {"/sys/kernel/debug/hdptxphy0/rate"};

    _hdmi_fd0 = open(phy0, O_RDWR);
    if (_hdmi_fd0 < 0)
    {
        printf("open path:%s fail \n", phy0);
        return ;
    }
}

void HdmiPhyClkDeinit(void)
{
    if (_hdmi_fd0 > 0)
    {
        close(_hdmi_fd0);
        _hdmi_fd0 = -1;
    }

}

float ShowFps(FPS_TYPE type)
{
    mFrameCount[type]++;
    nsecs_t now = systemTime();
    nsecs_t diff = now - mLastFpsTime[type];
    if (diff > ms2ns(500))
    {
        mFps[type] = ((mFrameCount[type] - mLastFrameCount[type]) * float(s2ns(1))) / diff;
        mLastFpsTime[type] = now;
        mLastFrameCount[type] = mFrameCount[type];
        if (type == SEND_TYPE)
        {
            mSend_fps = mFps[SEND_TYPE];
            return mSend_fps;
        }
        else
        {
            mRecv_fps = mFps[RECV_TYPE];
            return mRecv_fps;
        }
        //printf(">>>>>>>>>diff mFrameCount = %d mFps = %2.3f\n",mFrameCount[type], mFps[type]);
    }
	return 0;
}

void  SetSendFps(float fps)
{
    mSend_fps = fps;
}

void  SetVcntTime(RK_U32 vcnt_time)
{
    mVcnt_time = vcnt_time;
}

void  SetVoTime(RK_U32 vo_time)
{
    mSetVoTime = vo_time;
}

void SetDisplayType(int display_type)
{
    mDisplayType = display_type;
}

void  SetVsyncCallback(VSYNC_SETVCNT_CB func)
{
    mSetVcntCB = func;
}

int  GetVsyncStart()
{
    return vsync_start;
}

void VsyncThreadExit()
{
    mThreadExit = 1;
}

static int clk = 0;
void VsyncForAdjustClk()
{

    struct timespec ts;
    //int clk = 0;
    char cmd_str[100] = {0};
    sum_fps_count++;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    RK_U32 now_ms = (ts.tv_sec * 1000000000 + ts.tv_nsec) / 1000000;
    ShowFps(RECV_TYPE);
    if (mRecv_fps == 0 ||  mSend_fps == 0)
    {
        //printf("========mRecv_fps=%d,mSend_fps=%d\n",mRecv_fps,mSend_fps);
        return;
    }
    mSetVcntCB(); //update vcnt time
    int diff = mVcnt_time - mSetVoTime;
    if ((diff > 7 || diff < 0) && vsync_start == 0)
    {
        if (mDisplayType == 0)
        {
            system("echo 0 > /sys/kernel/debug/dri/0/video_port0/enable");
            usleep(1000);
            system("echo 1 > /sys/kernel/debug/dri/0/video_port0/enable");
        }
        if (mDisplayType == 1 || mDisplayType == 2)
        {
            system("echo 0 > /sys/kernel/debug/dri/0/video_port2/enable");
            usleep(1000);
            system("echo 1 > /sys/kernel/debug/dri/0/video_port2/enable");
        }
        return;
    }
    else
    {
        vsync_start = 1;
    }
    float fps_diff = mRecv_fps - mSend_fps;
    if (fps_diff >= 1)
    {
        return;
    }
    //printf("=====fps_diff=%f,mRecv_fps=%f,mSend_fps=%f\n",fps_diff,mRecv_fps,mSend_fps);
    if (diff >= 0 && diff <= MAX_DIFF && sum_fps_count % INTERNAL_FPS == 0)
    {
        vsync_start = 1;
        find_best_clk = 1;
        if (diff >= 1)
        {
            clk = 0;
        }
        last_vcnt_diff = diff;
    }
    if (diff > MAX_DIFF && (sum_fps_count % INTERNAL_FPS) == 0)
    {
            clk = 100;
    }
    if (diff < 0 && (sum_fps_count % INTERNAL_FPS) == 0)
    {
        if (clk >= 0)
        {
            clk = -100;
        }
    }
    if (clk != 0)
    {
        if (mDisplayType == 0)
        {
           HdmiPhyDynamicAdjusClk(VO_OUTPUT_3840x2160_60, clk > 0 ? 1 : -1, 0);
        }
        
        else if (mDisplayType == 1 || mDisplayType == 2)
        {
            char cmd_str[100] = {0};
            sprintf(cmd_str, "echo %d > sys/kernel/debug/dri/0/video_port2/vsync_adjust", clk);
            system(cmd_str);
        }
        else if (mDisplayType == 3)
        {
            char cmd_str[100] = {0};
            sprintf(cmd_str, "echo %d > sys/kernel/debug/dri/0/video_port3/vsync_adjust", clk);
            system(cmd_str);
        }
        else
        {
            HdmiPhyDynamicAdjusClk(VO_OUTPUT_3840x2160_60, clk > 0 ? 1 : -1, 0);
        }
    }

    if (sum_fps_count % INTERNAL_FPS == 0)
    {
        printf(">>>>>>>>>>>>>>>now_ms=%u, vcnt=%u, setvotime=%u,diff=%d,recv_fps=%f,send_fps=%f,fps_diff=%f,best_vpll_clk=%d,find_best_clk=%d,diff_direct=%d\n", \
               now_ms, mVcnt_time, mSetVoTime, diff, mRecv_fps, mSend_fps, mRecv_fps - mSend_fps, clk, find_best_clk, diff_direct);
        sum_fps_count = 0;
        clk = 0;
        
    }
    return;

}

int VsyncTread(int drmFd)
{
    const char *device = NULL, *module = NULL;
    int c, fd, ret;
    drmVBlank vbl;
    drmEventContext evctx;
    //struct vbl_info handler_info;
    fd = drmFd;
    opterr = 0;
    HdmiPhyClkInit();
    printf("====VsyncTread\n");
    /* Get current count first */
    while (!mThreadExit)
    {
        rt_atrace_begin_body("drmWaitVBlank");
        memset(&vbl, 0, sizeof(vbl));
        uint32_t high_crtc = 0;
        switch (mDisplayType)
        {
        case 0:
            high_crtc = (0 << DRM_VBLANK_HIGH_CRTC_SHIFT);
            break;
        case 1:
        case 2:
            high_crtc = (2 << DRM_VBLANK_HIGH_CRTC_SHIFT);
            break;
        case 3:
            high_crtc = (3 << DRM_VBLANK_HIGH_CRTC_SHIFT);
            break;
        default:
            high_crtc = (0 << DRM_VBLANK_HIGH_CRTC_SHIFT);
            break;
        }

        vbl.request.type = (drmVBlankSeqType)(DRM_VBLANK_RELATIVE | (high_crtc & DRM_VBLANK_HIGH_CRTC_MASK));
        vbl.request.sequence = 1;
        ret = drmWaitVBlank(fd, &vbl);
        if (ret != 0)
        {
            printf("drmWaitVBlank (relative) failed ret: %i\n", ret);
            return -1;
        }
        rt_atrace_end_body();
        VsyncForAdjustClk();
    }
    HdmiPhyClkDeinit();
    printf("VsyncTread exit.\n");
	return 0;
}
