#include <errno.h>
#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <map>
#include <sys/time.h>
#include <thread>
#include <arpa/inet.h>
#include "vbltest.h"
#include "Timers.h"
#include "rt_trace.h"
#include "mpi_venc.h"
#include "mpi_vdec.h"
#include "rk_mpi_vo.h"
#include "SendApi.h"
#include "RecvApi.h"
#include "rk_comm_video.h"
#include "mpi_vo.h"
#include "mpi_ao.h"
#include "../netserver/tcp_client.h"
#include "../netserver/tcp_comm.h"
#include "AdjustClkApi.h"
#include "drm_api.h"
#include "display_api.h"

#define ENABLE_HDMIRX 0
#define ENCODE_APP 1
#define ENABLE_ADJUST_VSYNC_CLK 0
#define ENABLE_DUMP 0
#define  DUMP_H264   "/data/h264.dump"
#define  DUMP_H265   "/data/h265_slice2.h265"
//#define  DUMP_PCM    "/data/audio.pcm"
#define  RTP_PORT 60044
#define  RTCP_PORT 60044

struct rkcam_context
{
    char device[255];
    char out_file[255];
    char pixel_format[255];
    int width;
    int height;
    int display_w;
    int display_h;
    int enable_adjust_clk;
    int code;
    int left;
    int top;
    int fps;
    int count;
    int skip;
    int mmap;
    int fd;
    int format;             //1: enmu       2: set   3:get   4: try
    int frame;              //1: size       2: interval
    int vcrop;               //1: cropcap    2:set    3: get
    int parm;               //1: set        2: get
    bool is_querycap;
    bool is_stream;
    bool is_savefile;
    FILE  *dump_h264_file;
    int  display_type;
    bool is_prepare;
    enum v4l2_buf_type type;
    std::map<int, int> mExpFdMap;
    std::shared_ptr<TCPClient> tcpclient;
    float  send_fps;
    float  recv_fps;
    float  update_send_fps;
    float  update_recv_fps;
    float  fps_diff;
    int    vsync_start;
    int    last_vpll_clk;
    int    best_vpll_clk;
    char   strIp[50];
    int    enSlice;
    /////for audio
    int s32DevId;
    int s32ChnNum;
    int s32SampleRate;
    int s32BitWidth;
    int s32ReSmpSampleRate;
    FILE  *fp_pcm;
    FILE  *_fp0;
    int  drmFd;
};

extern struct rkcam_context *ctx;
int init_rx();
int deinit_rx();
int start_rx();
int stop_rx();