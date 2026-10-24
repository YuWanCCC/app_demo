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
#include "../netserver/tcp_client.h"
#include "../netserver/tcp_comm.h"
#include "AdjustClkApi.h"
#include "../api/mpp_api.h"
#include "../api/drm_api.h"
#include "../api/display_api.h"

//#include "rockchip_drm.h"
#define ENABLE_4K 1
#define enable_drm 0
#define ENABLE_HDMIRX 0
#define ENABLE_MPP 0
#define ENCODE_APP 1
#define ENABLE_ADJUST_VSYNC_CLK 0
typedef  u_int32_t  uint32_t;
typedef  int64_t nsecs_t;
#define  DUMP_H264   "/data/h264.dump"
#define  DUMP_H265   "/data/h265dump.h265"
#define  RTP_PORT 60044
#define  RTCP_PORT 60044
#define  DST_IP    "127.0.0.1"

FILE  *_fp0 = NULL;
struct drmContext *drmCtx = NULL;

struct buffer 
{
	void *start;
	size_t length;
};

struct rkcam_context {
	char device[255];
	char out_file[255];
	char pixel_format[255];
	struct v4l2_frmsizeenum fsize;
	struct v4l2_fmtdesc fmtdesc;
	struct v4l2_capability cap;
	struct v4l2_frmivalenum frmival;
	struct v4l2_format fmt;
	struct v4l2_streamparm setfps;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_requestbuffers req;
	struct v4l2_buffer buf;
	struct buffer *buffers;

	struct v4l2_subdev_format subdev_fmt;

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
};

struct rkcam_context *_ctx = NULL;


void NetStatus(const uint32_t bitrate,
               const uint8_t fraction_loss,
               const uint32_t rtt) {
	
	printf(">>>>>bitrate=%d,fraction_loss=%d,rtt=%d\n", bitrate, fraction_loss, rtt);

}


void KeyFrameReq() {
   printf(">>>KeyFrameReq\n");
}


void SetVcntTimeCb() {
    SetVcntTime(get_vcnt_time());
}

static int test_count = 0;
void RecvData(unsigned char* data, int len, int channel) {
	test_count++;
	rt_atrace_begin_body("vdec_send_frame");
#if ENABLE_MPP	
  
  if (test_count % 4 == 0) {
	send_packet(data, len, 1);
  } else {
	send_packet(data, len, 0);
  }
 /* if ((data[4] & 0x0f) == 0x06) {
     if (data[4] != 0x26) {
	   data[4] = 0x26;
       send_packet(data, len, 1);
	 } else {
	   send_packet(data, len, 0);
	 }
  } else if ((data[4] & 0x0f) == 0x02) {
     if (data[4] != 0x02) {
	   data[4] = 0x02;
       send_packet(data, len, 1);
	 } else {
	   send_packet(data, len, 0);
	 }
  } else {
	send_packet(data, len, 0);
  }*/
 //fwrite(data, 1, len, _fp0);
#else
if (test_count % 4 == 0) {
    vdec_send_frame(data, len, 1);
} else {
    vdec_send_frame(data, len, 0);
}
#endif
	rt_atrace_end_body();
#if 0
	int vcnt_line = _ctx->display_h*0.8f;
	if (test_count == 0) {
	    switch (_ctx->display_type) {
			case 0:
			    init_vo(_ctx->width, _ctx->height, _ctx->display_w, _ctx->display_h, RK_FMT_YUV420SP,VO_INTF_HDMI,RK3588_VO_DEV_HD0,vcnt_line);
			    break;
			case 1:
			    init_vo(_ctx->width, _ctx->height, _ctx->display_w, _ctx->display_h, RK_FMT_YUV420SP,VO_INTF_DP,RK3588_VO_DEV_HD2, vcnt_line);
			    break;
			case 2:
			    init_vo(_ctx->width, _ctx->height, _ctx->display_w, _ctx->display_h, RK_FMT_YUV420SP,VO_INTF_EDP,RK3588_VO_DEV_HD2,vcnt_line);
			    break;
			case 3:
			    init_vo(_ctx->width, _ctx->height, _ctx->display_w, _ctx->display_h, RK_FMT_YUV420SP,VO_INTF_MIPI,RK3588_VO_DEV_HD3,vcnt_line);
				break;
			default:
			    init_vo(_ctx->width, _ctx->height, _ctx->display_w, _ctx->display_h, RK_FMT_YUV420SP,VO_INTF_HDMI,RK3588_VO_DEV_HD0,vcnt_line);
			    break;
		}
       if (_ctx->enable_adjust_clk > 0) {
			SetDisplayType(_ctx->display_type);
			SetVsyncCallback(SetVcntTimeCb);
			std::thread  t(VsyncTread, drmCtx->drmFd);
			t.detach();
	   }

	}
   
#endif
}

//----------------------------------

uint32_t stringtopixelformat(char *str) {
	int i;
	int j = 0;
	uint32_t t = 0;

	while (*str != '\0'){
		i = str[0];
		t += i << (8 * j);
		str++;
		j++;
	}
	if (j == 3)
		t += 0x20 << 24;
	return t;
}

static int dump_to_raw(void *ptr, struct rkcam_context *ctx, int flag) {
	FILE *fp;

	if (flag == ctx->skip)
		fp = fopen(ctx->out_file, "w");
	else
		fp = fopen(ctx->out_file, "a+");
	if (fp == NULL) {
		printf("ERROR creat file %s\n", ctx->out_file);
		return -1;
	}

	fwrite(ptr, ctx->buf.m.planes->bytesused, 1, fp);
	fclose(fp);

	return 0;
}

static bool is_v4l2_subdev(struct rkcam_context *ctx) {
	if (strstr(ctx->device, "v4l-subdev"))                 //    /dev/v4l-subdev*   or   /dev/video*
		return true;
	else
		return false;
}

static int open_device(struct rkcam_context *ctx) {
	int fd;

	fd = open(ctx->device, O_RDWR);
	if (fd < 0) {
		printf("open %s failed\n", ctx->device);
		return fd;
	}

	return fd;
}

static int close_device(struct rkcam_context *ctx) {
	int i = 0;

	close(ctx->fd);
	if(ctx->is_stream) {
		for (i = 0; i < ctx->req.count; i++)
			munmap(ctx->buffers[i].start, ctx->buffers[i].length);
	}
	return 0;
}

static int enmu_frame_size(struct rkcam_context *ctx) {
	if (!strcmp(ctx->pixel_format, "\0")) {
		printf("Please input pixelformat\n");
		return -1;
	}
	memset(&ctx->fsize, 0, sizeof(ctx->fsize));
	ctx->fsize.pixel_format = stringtopixelformat(ctx->pixel_format);
	ctx->fsize.index = 0;
	printf("ioctl: VIDIOC_ENUM_FRAMESIZES\n");
	if (ioctl(ctx->fd, VIDIOC_ENUM_FRAMESIZES, &ctx->fsize) >= 0) {
		 if (ctx->fsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
			printf("\tsize        : width = %u, height = %u\n\n",
					ctx->fsize.discrete.width, ctx->fsize.discrete.height);
		 } else if (ctx->fsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
			printf("\tsize        : min {width = %u, height = %u}\n"
			       "\t            : max {width = %u, height = %u}\n\n",
				ctx->fsize.stepwise.min_width, ctx->fsize.stepwise.min_height,
				ctx->fsize.stepwise.max_width, ctx->fsize.stepwise.max_height);
		 } else if (ctx->fsize.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
			printf("\tsize        : min      {width = %u, height = %u}\n"
			       "\t            : max      {width = %u, height = %u}\n"
			       "\t            : stepwize {width = %u, height = %u}\n\n",
				ctx->fsize.stepwise.min_width, ctx->fsize.stepwise.min_height,
				ctx->fsize.stepwise.max_width, ctx->fsize.stepwise.max_height,
				ctx->fsize.stepwise.step_width, ctx->fsize.stepwise.step_height);
		}
	} else {
		printf("ERROR enmu frame size %d %s\n", errno, strerror(errno));
		return errno;
	}

	return 0;
}

static int enmu_frame_interval(struct rkcam_context *ctx) {
	int ret;

	if (!ctx->width && !ctx->height && !strcmp(ctx->pixel_format, "\0")) {
		printf("please intput a parameter\n");
		return -1;
	}

	memset(&ctx->frmival, 0, sizeof(ctx->frmival));
	if (strcmp(ctx->pixel_format, "\0"))
		ctx->frmival.pixel_format = stringtopixelformat(ctx->pixel_format);
	if (ctx->width)
		ctx->frmival.width = ctx->width;
	if (ctx->height)
		ctx->frmival.height = ctx->height;
	ctx->frmival.index = 0 ;
	if (!ctx->is_stream)
		printf("ioctl: VIDIOC_ENUM_FRAMEINTERVALS\n");
	ret = ioctl(ctx->fd, VIDIOC_ENUM_FRAMEINTERVALS, &ctx->frmival);
	if (ret >= 0) {
		if (ctx->is_stream) {
			ctx->fps = ctx->frmival.discrete.denominator;
			return 0;
		}
		if (ctx->frmival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
			printf("\tInterval: Discrete        %ums   %ufps\n\n", ctx->frmival.discrete.numerator/ctx->frmival.discrete.denominator, ctx->frmival.discrete.denominator);
		} else if (ctx->frmival.type == V4L2_FRMIVAL_TYPE_CONTINUOUS) {
			printf("\tInterval: Continuous        %ums   %ufps\n\n", ctx->frmival.discrete.numerator * 1000 / ctx->frmival.discrete.denominator, ctx->frmival.discrete.denominator);
		} else if (ctx->frmival.type == V4L2_FRMIVAL_TYPE_STEPWISE) {
			printf("\tInterval: Stepwise       min: %u/%u\n", ctx->frmival.stepwise.min.numerator, ctx->frmival.stepwise.min.denominator);
			printf("\t                         max: %u/%u\n", ctx->frmival.stepwise.max.numerator, ctx->frmival.stepwise.max.denominator);
			printf("\t                         max: %u/%u\n\n", ctx->frmival.stepwise.step.numerator, ctx->frmival.stepwise.step.denominator);
		}
	} else {
		printf("ERROR enmu frame interval %d %s\n", errno, strerror(errno));
		return errno;
	}

	return 0;
}
static int enmu_video_format(struct rkcam_context *ctx) {

	printf("ioctl: VIDIOC_ENUM_FMT\n\n");
	memset(&ctx->fmtdesc, 0, sizeof(ctx->fmtdesc));
	ctx->fmtdesc.index = 0;
	ctx->fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	while (ioctl(ctx->fd, VIDIOC_ENUM_FMT, &ctx->fmtdesc) >= 0) {
		printf("\tIndex       : %d\n", ctx->fmtdesc.index);
		printf("\tName        : %s\n", ctx->fmtdesc.description);
		printf("\tType        : 0x%08x\n", ctx->fmtdesc.type);
		printf("\tPixel Format: '%c%c%c%c'\n", ctx->fmtdesc.pixelformat & 0xFF, (ctx->fmtdesc.pixelformat >> 8) & 0xFF,
			(ctx->fmtdesc.pixelformat >> 16) & 0xFF,(ctx->fmtdesc.pixelformat >> 24) & 0xFF);
		printf("\tFlags       : %d\n\n", ctx->fmtdesc.flags);
		ctx->fmtdesc.index++;
	}

	if (errno != EINVAL) {
		printf("ERROR enumerating frame formats: %d\n", errno);
		return errno;
	}

	return 0;
}

static int set_video_fmt(struct rkcam_context *ctx) {
	int ret;

	if (!ctx->width || !ctx->height || !strcmp(ctx->pixel_format, "\0")) {
		printf("please check the parameter\n");
		return -1;
	}
	memset(&ctx->fmt, 0, sizeof(ctx->fmt));
	ctx->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	ctx->fmt.fmt.pix_mp.width = ctx->width;
	ctx->fmt.fmt.pix_mp.height = ctx->height;
	ctx->fmt.fmt.pix_mp.pixelformat = stringtopixelformat(ctx->pixel_format);
	ret = ioctl(ctx->fd, VIDIOC_S_FMT, &ctx->fmt);
	if (ret < 0)
		printf("ERROR set video fmt %d %s\n", errno, strerror(errno));
	else
		printf("set fmt succ\n");

	return ret;

}

static int set_camera_fmt(struct rkcam_context *ctx) {
	int ret;

	if (!ctx->width || !ctx->height) {
		printf("please check the parameter\n");
		return -1;
	}

	memset(&ctx->subdev_fmt, 0, sizeof(ctx->subdev_fmt));
	ctx->subdev_fmt.pad = 0;
	ctx->subdev_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	ret = ioctl(ctx->fd, VIDIOC_SUBDEV_G_FMT, &ctx->subdev_fmt);

	ctx->subdev_fmt.format.width = ctx->width;
	ctx->subdev_fmt.format.height = ctx->height;
	ret = ioctl(ctx->fd, VIDIOC_SUBDEV_S_FMT, &ctx->subdev_fmt);
	if (ret < 0)
		printf("ERROR set camera fmt %d %s\n", errno, strerror(errno));
	else
		printf("set fmt succ\n");

	return ret;

}

static int get_video_fmt(struct rkcam_context *ctx) {
	int ret;

	printf("ioctl: VIDIOC_G_FMT\n\n");
	memset(&ctx->fmt, 0, sizeof(ctx->fmt));
	ctx->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	ret = ioctl(ctx->fd, VIDIOC_G_FMT, &ctx->fmt);
	if (ret >= 0) {
		printf("Stream format informations:\n");
		printf("\tType                 :0x%08x\n", ctx->fmt.type);
		printf("\tWidth Height         :%dx%d\n", ctx->fmt.fmt.pix_mp.width, ctx->fmt.fmt.pix_mp.height);
		printf("\tPixel Format         :'%c%c%c%c'\n", ctx->fmt.fmt.pix_mp.pixelformat & 0xFF, (ctx->fmt.fmt.pix_mp.pixelformat >> 8) & 0xFF,
			(ctx->fmt.fmt.pix_mp.pixelformat >> 16) & 0xFF,(ctx->fmt.fmt.pix_mp.pixelformat >> 24) & 0xFF);
		printf("\tField                :0x%08x\n", ctx->fmt.fmt.pix_mp.field);
		printf("\tColorspace           :0x%08x\n", ctx->fmt.fmt.pix_mp.colorspace);
		printf("\tQuantization         :%s\n", ctx->fmt.fmt.pix_mp.quantization);
		printf("\tNumber planes        :%d\n", ctx->fmt.fmt.pix_mp.num_planes);
		printf("\t    Bytesperline     :%d\n", ctx->fmt.fmt.pix_mp.plane_fmt[0].bytesperline);
		printf("\t    Sizeimage        :0x%08x\n", ctx->fmt.fmt.pix_mp.plane_fmt[0].sizeimage);
	} else {
		printf("ERROR get video fmt %d %s\n", errno, strerror(errno));
		return errno;
	}
	return 0;
}
static int querycap_video(struct rkcam_context *ctx) {
	int ret;

	printf("ioctl: VIDIOC_QUERYCAP\n\n");
	memset(&ctx->cap,0, sizeof(ctx->cap));
	ret = ioctl(ctx->fd, VIDIOC_QUERYCAP, &ctx->cap);
	if (ret >= 0) {
		printf("Driver Informations:\n");
		printf("\tDriver name:       %s\n", ctx->cap.driver);
		printf("\tCard type:         %s\n", ctx->cap.card);
		printf("\tBus info:          %s\n", ctx->cap.bus_info);
		printf("\tDriver version:    %u.%u.%u\n", (ctx->cap.version >>16) & 0xFF,
						(ctx->cap.version >>8) & 0xFF, ctx->cap.version & 0xFF);
		printf("\tCapabilities:      0x%08X\n", ctx->cap.capabilities);
		printf("\tDevice caps:       0x%08X\n", ctx->cap.device_caps);
	} else {
		printf("ERROR equerycap %d %s\n", errno, strerror(errno));
		return errno;
	}

	return 0;
}

static int try_video_fmt(struct rkcam_context *ctx) {
	int ret;

	if (!strcmp(ctx->pixel_format, "\0")) {
		printf("please check the parameter\n");
		return -1;
	}
	printf("ioctl: VIDIOC_TRY_FMT\n\n");
	memset(&ctx->fmt, 0, sizeof(ctx->fmt));
	ctx->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	ctx->fmt.fmt.pix_mp.pixelformat = stringtopixelformat(ctx->pixel_format);
	ret = ioctl(ctx->fd, VIDIOC_TRY_FMT, &ctx->fmt);
	if (ret >= 0) {
		if(errno == EINVAL)
			printf("\tnot support format %s\n",ctx->pixel_format);
		else
			printf("\tsupport format %s\n", ctx->pixel_format);
	} else {
		printf("ERROR try  fmt %d %s\n", errno, strerror(errno));
		return errno;
	}

	return 0;
}

static int set_video_parm(struct rkcam_context *ctx) {
	int ret;

	if (!ctx->fps) {
		printf("please input the fps\n");
		return -1;
	}
	printf("ioctl: VIDIOC_S_PARM\n\n");
	memset(&ctx->setfps, 0, sizeof(ctx->setfps));
	ctx->setfps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	ctx->setfps.parm.capture.capturemode = 1;
	ctx->setfps.parm.capture.timeperframe.numerator = 1;
	ctx->setfps.parm.capture.timeperframe.denominator = ctx->fps;
	ret = ioctl(ctx->fd, VIDIOC_S_PARM, &ctx->setfps);
	if(ret >= 0) {
		printf("set video parm succ\n");
	} else {
		printf("ERROR set  video parm %d %s\n", errno, strerror(errno));
		return errno;
	}
	return 0;
}

static int get_video_parm(struct rkcam_context *ctx) {
	int ret;

	printf("ioctl: VIDIOC_G_PARM\n\n");
	memset(&ctx->setfps, 0, sizeof(ctx->setfps));
	ctx->setfps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	ret = ioctl(ctx->fd, VIDIOC_G_PARM, &ctx->setfps);
	if(ret >= 0) {
		printf("\tFps:%ufps\n\n", ctx->setfps.parm.capture.timeperframe.denominator);
	} else {
		printf("ERROR get video parm %d %s\n", errno, strerror(errno));
		return errno;
	}
	return 0;
}

static int query_video_crop(struct rkcam_context *ctx) {
	int ret;

	printf("ioctl: VIDIOC_CROPCAP\n\n");
	memset(&ctx->cropcap, 0, sizeof(ctx->cropcap));
	ctx->cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	ret = ioctl(ctx->fd, VIDIOC_CROPCAP, &ctx->cropcap);
	if (ret >= 0) {
		printf("\tBounds:            Left:%d, Top:%d, Width:%d, Height:%d\n", ctx->cropcap.bounds.left,
					ctx->cropcap.bounds.top, ctx->cropcap.bounds.width, ctx->cropcap.bounds.height);
		printf("\tDefault:           Left:%d, Top:%d, Width:%d, Height:%d\n", ctx->cropcap.defrect.left,
					ctx->cropcap.defrect.top, ctx->cropcap.defrect.width, ctx->cropcap.defrect.height);
		printf("\tPixelaspect:       %d/%d\n\n", ctx->cropcap.pixelaspect.numerator, ctx->cropcap.pixelaspect.denominator);
	} else {
		printf("ERROR query video crop %d %s\n", errno, strerror(errno));
		return errno;
	}
	return 0;
}

static int set_video_crop(struct rkcam_context *ctx) {
	int ret;

	if (ctx->width == 0 || ctx->height == 0) {
		printf("Please check the parameter\n");
		return -1;
	}

	printf("ioctl: VIDIOC_S_CROP\n\n");
	memset(&ctx->crop, 0, sizeof(ctx->crop));
	ctx->crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	ctx->crop.c.left = ctx->left;
	ctx->crop.c.top = ctx->top;
	ctx->crop.c.width = ctx->width;
	ctx->crop.c.height = ctx->height;
	ret = ioctl(ctx->fd, VIDIOC_S_CROP, &ctx->crop);
	if (ret >= 0) {
		printf("set video crop succ\n");
	} else {
		printf("ERROR set video crop %d %s\n", errno, strerror(errno));
		return errno;
	}
	return 0;
}

static int get_video_crop(struct rkcam_context *ctx) {
	int ret;

	printf("ioctl: VIDIOC_G_CROP\n\n");
	memset(&ctx->crop, 0, sizeof(ctx->crop));
	ctx->crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	ret = ioctl(ctx->fd, VIDIOC_G_CROP, &ctx->crop);
	if (ret >= 0) {
		printf("\tCrop:        Left:%d, Top:%d, Width:%d, Height:%d\n\n", ctx->crop.c.left,
						ctx->crop.c.top, ctx->crop.c.width, ctx->crop.c.height);
	} else {
		printf("ERROR get video crop %d %s\n", errno, strerror(errno));
		return errno;
	}
	return 0;
}

static int stop_stream(struct rkcam_context *ctx) {
	int ret;

	ctx->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	ret = ioctl(ctx->fd, VIDIOC_STREAMOFF, &ctx->type);
	if(ret < 0) {
		printf("ERROR stop stream  %d %s\n", errno, strerror(errno));
		return errno;
	}
	return 0;
}

int mFrameCount;
int mLastFrameCount = 0;
nsecs_t mLastFpsTime = 0;
float mFps = 0;
void showFps() {

    mFrameCount++;
    nsecs_t now = systemTime();
    nsecs_t diff = now - mLastFpsTime;
    if (diff > ms2ns(500)) {
        mFps =  ((mFrameCount - mLastFrameCount) * float(s2ns(1))) / diff;
        mLastFpsTime = now;
        mLastFrameCount = mFrameCount;
		_ctx->send_fps = mFps;
		SetSendFps(_ctx->send_fps);
        printf(">>>>>>>>>mFrameCount = %d mFps = %2.3f\n",mFrameCount, mFps);
    }
}

nsecs_t vsync_time = 0;
static void HandlerTCPMessage(char *buffer, int size) {
   
   CommandData_t *cmd_data = (CommandData_t*)buffer;
   cmd_data->cmdId = ntohl(cmd_data->cmdId);
   cmd_data->seqNum = ntohl(cmd_data->seqNum);
   printf("recv command id =%d, seqnum=%d\n", cmd_data->cmdId, cmd_data->seqNum);
   switch (cmd_data->cmdId) {
      case START_VSYNC: {

	  }

	  default:
	     printf("command id error.\n");
		 break;
   }

   return 0;
}

static int initTcp(struct rkcam_context *ctx) {
    ctx->tcpclient = std::make_shared<TCPClient>();
	ctx->tcpclient->RegisterRecvCallBack(HandlerTCPMessage);
    return ctx->tcpclient->Setup(CONNECT_IP,SERVER_PORT);
}

static int sendCmd(struct rkcam_context* ctx, COMMAND_ID cmdid) {
	CommandData_t cmd_data;
	cmd_data.cmdId = htonl(cmdid);
	cmd_data.seqNum = 0;
	cmd_data.cmdLen = sizeof(CommandData_t);
	return ctx->tcpclient->Send((char*)&cmd_data, sizeof(CommandData_t));
}

void vdec_callback(int bufferFd) {
	if (bufferFd > 0) {
		rt_atrace_begin_body("SetDrmPlane");
		SetDrmPlane(drmCtx, 1, bufferFd);
		rt_atrace_end_body();
	}
}
static int first_frame = 0;
static int skip_count = 0;
nsecs_t last_frame_time = 0;
void vdec_callback2(int vir_width,int vir_height, int width, int height, void* obj) {
    struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	set_vo_time_ms = (ts.tv_sec*1000000000 + ts.tv_nsec)/1000000;
	rt_atrace_begin_body("sendframe_vo");
	SetVoTime(set_vo_time_ms);
	sendframe_vo(obj);
	showFps();
	rt_atrace_end_body();
}

void vdec_callback3(int bufferFd) {
	skip_count++;
	if (bufferFd > 0 && skip_count > 10) {
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		set_vo_time_ms = (ts.tv_sec*1000000000 + ts.tv_nsec)/1000000;
		rt_atrace_begin_body("sendframe_vo");
		SetVoTime(set_vo_time_ms);
		sendframe_vo_for_fd(bufferFd, 3840*2160*3/2);
		showFps();
		rt_atrace_end_body();
	}
}

static  int start_stream(struct rkcam_context *ctx) {
	int ret;
	int i = 0, j = 0, loop = 0;
	
    printf(">>>start_stream\n");

	memset(&ctx->req, 0, sizeof(ctx->req));

	////////////////////////////
	drmCtx = (struct drmContext *)malloc(sizeof(drmContext));
	drmCtx->src_width = ctx->width;
	drmCtx->src_height = ctx->height;
	drmCtx->src_Stride = ctx->width;
	drmCtx->display_type = ctx->display_type;
	bool b = initDrm(drmCtx);
#if enable_drm
	if (b) {
		detect(drmCtx, 1);
	} else {
		printf("========init drm fail.\n");
		return -1;
	}
	 
#else
    ///////////////////////////
#if ENABLE_HDMIRX
	init_vo(3840, 2160, 3840, 2160, RK_FMT_YUV420SP, VO_INTF_MIPI, RK3588_VO_DEV_HD3);
#else
    //init_vo(3840, 2160, 3840, 2160, RK_FMT_YUV420SP,VO_INTF_DP,RK3588_VO_DEV_HD2);
	//init_vo(3840, 2160, 3840, 2160, RK_FMT_YUV420SP,VO_INTF_HDMI,RK3588_VO_DEV_HD0);
#endif
	int vcnt_line = _ctx->display_h*0.8f;
	if (test_count == 0) {
	    switch (_ctx->display_type) {
			case 0:
			    init_vo(_ctx->width, _ctx->height, _ctx->display_w, _ctx->display_h, RK_FMT_YUV420SP,VO_INTF_HDMI,RK3588_VO_DEV_HD0,vcnt_line);
			    break;
			case 1:
			    init_vo(_ctx->width, _ctx->height, _ctx->display_w, _ctx->display_h, RK_FMT_YUV420SP,VO_INTF_DP,RK3588_VO_DEV_HD2, vcnt_line);
			    break;
			case 2:
			    init_vo(_ctx->width, _ctx->height, _ctx->display_w, _ctx->display_h, RK_FMT_YUV420SP,VO_INTF_EDP,RK3588_VO_DEV_HD2,vcnt_line);
			    break;
			case 3:
			    init_vo(_ctx->width, _ctx->height, _ctx->display_w, _ctx->display_h, RK_FMT_YUV420SP,VO_INTF_MIPI,RK3588_VO_DEV_HD3,vcnt_line);
				break;
			default:
			    init_vo(_ctx->width, _ctx->height, _ctx->display_w, _ctx->display_h, RK_FMT_YUV420SP,VO_INTF_HDMI,RK3588_VO_DEV_HD0,vcnt_line);
			    break;
		}
       if (_ctx->enable_adjust_clk > 0) {
			SetDisplayType(_ctx->display_type);
			SetVsyncCallback(SetVcntTimeCb);
			std::thread  t(VsyncTread, drmCtx->drmFd);
			t.detach();
	   }

	}
	//std::thread  t(test_vblank_thread, drmCtx->drmFd);
	//t.detach();
	//setVsyncCallback(vsync_cb,ctx->display_type);
#endif  //enable drm

#if ENABLE_MPP
	init_rk_mpp(vdec_callback3);
	for (i=0; i<3; i++) {
		int size = drmCtx->src_width*drmCtx->src_height*2;
	    int bufferFd = buffer_create(drmCtx, drmCtx->src_width, drmCtx->src_height, size);
		//printf("+++++buffer_create,i=%d,drmfd=%d\n",i,bufferFd);
		commit_buffer_to_group(bufferFd, size);
		init_MB(bufferFd, drmCtx->src_width*drmCtx->src_height*3/2);
	}
	std::thread  t1(rk_mpp_decode_thread);
#else
    init_vdec(vdec_callback,vdec_callback2);
    std::thread  t1(thread_vdec_get_frame, 0);
#endif
	init_recv(0, RTP_PORT, RecvData);
	start_recv(0);
	usleep(1000000);
	sendCmd(_ctx, START_STREAM);
    _fp0 = fopen(DUMP_H265, "wb");
    ctx->is_prepare = true;
	printf("start_stream is done\n");
    t1.join();
#if enable_drm
	deinitDrm(drmCtx);
#else
    deinit_vo();
#endif
	stop_recv(0);
    deinit_recv(0);
   // fclose(_fp0);

	return 0;
}

static const char short_options[] = "d:lsiqFftw:h:W:H:p:A:aCcL:T:Q";
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
	{0, 0, 0, 0}
};

static void parse_args(int argc, char **argv, struct rkcam_context *ctx)
{
	int c;
	int index;

	while(1) {
		c = getopt_long(argc, argv, short_options, long_options, &index);
		if (-1 == c)
			break;
		switch(c) {
			case 0:
				switch(flag) {
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
			default:
				break;
		}
	}
}


int main(int argc, char **argv) {

	_ctx = (struct rkcam_context*)malloc(sizeof(rkcam_context));
	strcpy(_ctx->device, "/dev/video0");
	strcpy(_ctx->out_file, "/data/out.raw");
	strcpy(_ctx->pixel_format, "\0");
	_ctx->width = 0;
	_ctx->height = 0;
	_ctx->code = 0;
	_ctx->left = 0;
	_ctx->top = 0;
	_ctx->fps = 30;
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
	_ctx->is_savefile =false;
	_ctx->fps_diff = 0.0f;
	_ctx->vsync_start = 0;
	printf("enter main\n");
	parse_args(argc, argv, _ctx);
    //hdmi_phy_clk_init();
#if !enable_drm
	deinit_vo();
#endif
	_ctx->fd = open_device(_ctx);
    while (!initTcp(_ctx)) {
       usleep(1000000);
	}
    int r = sendCmd(_ctx, PROCESS_STREAM);
	if (r > 0)
	   start_stream(_ctx);
	
	close_device(_ctx);
    VsyncThreadExit();
	return 0;
}
