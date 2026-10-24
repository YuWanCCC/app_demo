#ifndef _PROCESS_TX_H_
#define _PROCESS_TX_H_
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
#include "vbltest.h"
#include "Timers.h"
#include "rt_trace.h"
#include "mpi_venc.h"
#include "SendApi.h"
#include "RecvApi.h"
#include "rk_comm_video.h"
#include "rk_mpi_vo.h"
#include "AdjustClkApi.h"
#include "mpi_vo.h"
//#include "rockchip_drm.h"
#include "../netserver/tcp_server.h"
#include "../netserver/tcp_comm.h"
#include "drm_api.h"
#include "mpi_vi.h"
#include <signal.h>
#define enable_drm 0
#define ENABLE_4K 1
#define ENABLE_VO 1

typedef  u_int32_t  uint32_t;
typedef  int64_t nsecs_t;
#define  DUMP_H265   "/data/h265.dump"
#define  RTP_PORT 60044
#define  RTCP_PORT 60044
#define  ENABLE_VI  0
#define  DST_IP    "192.168.5.11"


struct buffer
{
    void *start;
    size_t length;
};

struct rkcam_context
{
    char device[255];
    char out_file[255];
    char pixel_format[255];
    int width;
    int height;
    int display_w;
    int display_h;
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
    int realBitRate;
    int max_bitrate;
    int min_bitrate;
    int init_bitrate;
    bool is_querycap;
    bool is_stream;
    bool process_stream;
    bool is_savefile;
    int  display_type;
    int  enable_vo;
    int  enable_encode;
    int  enable_adjust_clk;
    int  exit_thread;
    FILE  *dump_file;
    int  firstFrame;
    char strIp[50];
    enum v4l2_buf_type type;
    std::map<int, int> mExpFdMap;
    std::shared_ptr<TCPServer> tcp;
    int  enSlice;
    int  enable_dump;
    int  keyframe_req;
    //for audio
    int channel;
    int sampleRate;
    int bitWidth;
    int devId;
    int frame_time_ms;
    
};

extern struct rkcam_context *_ctx;
int initTcp();
int init_tx();
int deinit_tx();
int start_tx();
int stop_tx();

#endif