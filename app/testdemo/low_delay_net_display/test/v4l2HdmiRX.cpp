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
#include "drm_fourcc.h"
#include "xf86drm.h"
#include "xf86drmMode.h"
#include <map>
#include <sys/time.h>
#include <thread>
#include "vbltest.h"
#include "Timers.h"
#include "rt_trace.h"
#include "mpi_venc.h"
#include "mpi_vdec.h"
#include "SendApi.h"
#include "RecvApi.h"
#include "rk_comm_video.h"
#include "rk_mpi_vo.h"
#include "AdjustClkApi.h"
#include "mpi_vo.h"
//#include "rockchip_drm.h"
#include "netserver/tcp_server.h"
#include "netserver/tcp_comm.h"
#include "drm_api.h"
#include "mpi_vi.h"
#include <signal.h>
#define enable_drm 0
#define ENABLE_4K 1
#define ENABLE_VO 1

typedef  u_int32_t  uint32_t;
typedef  int64_t nsecs_t;
#define  DUMP_H264   "/data/h264.dump"
#define  RTP_PORT 60044
#define  RTCP_PORT 60044
#define  ENABLE_VI  0
#define  DST_IP    "192.168.5.11"
struct drmContext *drmCtx = NULL;
pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  g_cond  = PTHREAD_COND_INITIALIZER;  
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
	bool is_querycap;
	bool is_stream;
	bool process_stream;
	bool is_savefile;
	int  display_type;
	int  enable_vo;
	int  enable_encode;
	int  enable_adjust_clk;
	int  exit_thread;
	FILE  *dump_h264_file;
    int  firstFrame;
	enum v4l2_buf_type type;
	std::map<int, int> mExpFdMap;
	std::shared_ptr<TCPServer> tcp;

};

#if 0
enum {
	OUTPUT_PRIMARY = 0,
	OUTPUT_EXTERNAL,
	OUTPUT_MAX,
};

//qiuen drm
static std::map<int, int>  mFbidMap;
#define HWC_DISPLAY_PRIMARY 0
#define HWC_DISPLAY_EXTERNAL 1
struct DrmOutput {
	drmModeConnectorPtr connector;
	drmModeEncoderPtr encoder;
	drmModeCrtcPtr crtc;
	drmModeModeInfo mode;

	drmModePlaneResPtr plane_res;
	drmModePlanePtr plane;
	drmModeResPtr res;
	drmModeAtomicReq *req;
	drmModeObjectPropertiesPtr props;
	drmModePropertyPtr prop;
	uint32_t fbHandle;
	uint32_t fbId;
	int connected;
	int planeid;
	int panelOrientation;
} ;

typedef struct drm_bo {
  uint32_t width;
  uint32_t height;
  uint32_t format; /* DRM_FORMAT_* from drm_fourcc.h */
  uint32_t pitches[4];
  uint32_t offsets[4];
  uint32_t gem_handles[4];
  uint32_t fb_id;
  int acquire_fence_fd;
  void *priv;
} drm_bo_t;


struct drmContext {
	DrmOutput mOutputs[OUTPUT_MAX];
	int src_width;
	int src_height;
	int src_Stride;
	int dst_width;
	int dst_height;
	int format;
	int drmFd;
};
static RK_U32 set_vo_time_ms = 0;
#endif
struct rkcam_context *_ctx = NULL;
void SetVcntTimeCb() {
    SetVcntTime(get_vcnt_time());
}
#if 0
bool initDrm(drmContext* ctx)
{
    printf("initialize in\n");
    if (ctx == NULL) {
		printf("initialize fail.\n");
		return false;
	}
    const char *path = "/dev/dri/card0";
    ctx->drmFd = open(path, O_RDWR);
	
    if (ctx->drmFd < 0) {
        printf("failed to open Drm, error: %s\n", strerror(errno));
        return false;
    }
    printf("ctx->drmFd = %d", ctx->drmFd);
    
    return true;
}

void deinitDrm(drmContext* ctx)
{

    if (ctx->drmFd > 0) {
        close(ctx->drmFd);
        ctx->drmFd = 0;
    }

    for (const auto &fbidMap : mFbidMap) {
        int fbid = fbidMap.second;
        if (drmModeRmFB(ctx->drmFd, fbid))
            printf("Failed to rm fb\n");
    }

}
#endif
///////
void NetStatus(const uint32_t bitrate,
               const uint8_t fraction_loss,
               const uint32_t rtt) {
	
	printf(">>>>>bitrate=%d,fraction_loss=%d,rtt=%d\n", bitrate, fraction_loss, rtt);

}

void KeyFrameReq() {
   printf(">>>KeyFrameReq\n");
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
	ctx->is_stream = 0;
	return 0;
}

static int start_stream(struct rkcam_context *ctx) {
    //	rt_atrace_begin_body("STREAMON");
	if (ctx->is_stream > 0) {
       return 0;
	}
#if !ENABLE_VI
	ctx->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	int ret = ioctl(ctx->fd, VIDIOC_STREAMON, &ctx->type);
//	rt_atrace_end_body();
	if (ret < 0) {
		printf("ERROR stream on %d %s\n", errno, strerror(errno));
		return errno;
	}
#endif
	ctx->is_stream = 1;
	pthread_cond_signal(&g_cond);
	printf("start stream on\n");
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
        mFps =  ((mFrameCount - mLastFrameCount) * float(s2ns(1))) / diff;
        mLastFpsTime = now;
        mLastFrameCount = mFrameCount;
        printf(">>>>>>>>>mFrameCount = %d mFps = %2.3f,bitrate=%d\n",mFrameCount, mFps,_ctx->realBitRate);
        _ctx->realBitRate = 0;
		SetSendFps(mFps);
	}
	
}

static int sendCmd(struct rkcam_context* ctx, COMMAND_ID cmdid) {
	CommandData_t cmd_data;
	cmd_data.cmdId = htonl(cmdid);
	cmd_data.seqNum = 0;
	cmd_data.cmdLen = sizeof(CommandData_t);
	return ctx->tcp->Send((char*)&cmd_data, sizeof(CommandData_t));
}

static nsecs_t deq_time = 0;
static int skip_count = 0;
void  venc_callback(bool isKeyFrame, unsigned char* data, int len) {
	 rt_atrace_begin_body("send_data");
	 send_data(data, len);
	 rt_atrace_end_body();
	 if (!_ctx->firstFrame && skip_count>4) {
		sendCmd(_ctx, START_VSYNC);
		_ctx->firstFrame = 1;
	 }
	 _ctx->realBitRate += len*8;
	 showFps1();
}

int getSelectmV4l2FdTime(int fd)
{
    int ts = 0;
    fd_set fds;
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec =  1000 * 600;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);
   // printf("@%s(%d) select time begin\n",__FUNCTION__,__LINE__);
    ts = select(fd + 1, &fds, NULL, NULL, &tv);
   // ALOGD("@%s(%d) select time done.",__FUNCTION__,__LINE__);
    if(ts == 0)
	{
        printf("@%s(%d) select time out\n",__FUNCTION__,__LINE__);
	}
    return ts;
}


static int test_count = 0;
static  int process_stream(struct rkcam_context *ctx) {
	int ret;
	int i = 0, j = 0, loop = 0;
	if (ctx->process_stream==true) {
		return 0;
	} 
	ctx->process_stream = true;
    printf(">>>process_stream,%p\n", ctx);
	/////////////////////////////////
#if !ENABLE_VI
	memset(&ctx->req, 0, sizeof(ctx->req));
	ctx->req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	ctx->req.memory = V4L2_MEMORY_MMAP;
	ctx->req.count = ctx->mmap;
	ret = ioctl(ctx->fd, VIDIOC_REQBUFS, &ctx->req);
	if(ret < 0) {
		printf("ERROR request buffer  %d %s\n", errno, strerror(errno));
		return errno;
	}
	ctx->mExpFdMap.clear();
	ctx->buffers = (struct buffer *)calloc(ctx->req.count, sizeof(*ctx->buffers));
	for (i = 0; i < ctx->req.count; ++i) {
		struct v4l2_plane planes;
		memset(&ctx->buf, 0, sizeof(ctx->buf));
		memset(&planes, 0, sizeof(planes));
		ctx->buf.index = i;
		ctx->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		ctx->buf.memory = V4L2_MEMORY_MMAP;
		ctx->buf.flags = V4L2_BUF_FLAG_NO_CACHE_INVALIDATE | V4L2_BUF_FLAG_NO_CACHE_CLEAN;
		ctx->buf.m.planes = &planes;
		ctx->buf.length = 1;
		ret = ioctl(ctx->fd, VIDIOC_QUERYBUF, &ctx->buf);
		if (ret < 0) {
			printf("ERROR query buffer  %d %s\n", errno, strerror(errno));
			return errno;
		}
        
		ctx->buffers[i].length = ctx->buf.m.planes->length;
		ctx->buffers[i].start = (unsigned char *)mmap(0, ctx->buf.m.planes->length, PROT_READ | PROT_WRITE, MAP_SHARED, ctx->fd, ctx->buf.m.planes->m.mem_offset);
		if (ctx->buffers[i].start == MAP_FAILED) {
			printf("Unable map buffer: %d %s\n", strerror(errno));
			return -1;
		}
		ret = ioctl(ctx->fd, VIDIOC_QBUF, &ctx->buf);
		if (ret < 0) {
			printf("ERROR VIDIOC_QBUF %d %s\n", errno, strerror(errno));
			return errno;
		}
		struct v4l2_exportbuffer expbuf;
		memset(&expbuf, 0, sizeof(expbuf));
		expbuf.type = ctx->buf.type;
		expbuf.index = ctx->buf.index;
		ret = ioctl(ctx->fd, VIDIOC_EXPBUF, &expbuf);
		if (ret < 0) {
			printf("ERROR VIDIOC_EXPBUF %d %s\n", errno, strerror(errno));
			return errno;
		}
		ctx->mExpFdMap.insert(std::make_pair(i, expbuf.fd));
		printf("==exp fd=%d,index=%d,type=%d\n",expbuf.fd,expbuf.index,expbuf.type);
	}
#else
    ret = vi_init(ctx->width,ctx->height, 0, "/dev/video20");
    printf("=============vi_init\n");
#endif    
	///////////////////////enc/dec/RK_FMT_RGB888
	if (ctx->enable_encode > 0) {
		rt_atrace_begin_body("init_venc");
		if (strstr(ctx->pixel_format, "NV12") > 0) {
			init_venc(ctx->width,ctx->height, ctx->width, ctx->height, 0, ctx->max_bitrate, RK_FMT_YUV420SP, venc_callback, true, 3, 4);
		} else {
			init_venc(ctx->width,ctx->height, ctx->width, ctx->height, 0, ctx->max_bitrate, RK_FMT_RGB888, venc_callback,true, 3, 2);
		}
		std::thread  t(thread_get_stream, 0);
		t.detach();
		rt_atrace_end_body();
		ret = init_send(DST_IP, RTP_PORT, RTCP_PORT,\
				NetStatus,\
				KeyFrameReq);
		//printf(">>>>>>init_send:ret=%d\n",ret);
		ret = start_send();
		//printf(">>>>>start_send:ret=%d\n",ret);
		///////////////////////
	}
//   printf("=======pthread_cond_wait begin\n");
	if (!ctx->enable_vo) {
		pthread_mutex_lock(&g_mutex);
		pthread_cond_wait(&g_cond, &g_mutex);
		pthread_mutex_unlock(&g_mutex);
	}
#if !ENABLE_VI
	else
		start_stream(_ctx);
#endif 

	while(!_ctx->exit_thread){
	//	printf("deq buffer begin,ctx->is_stream=%d\n",ctx->is_stream);
#if !ENABLE_VI
	    rt_atrace_begin_body("deq buffer");

		ret = ioctl(ctx->fd, VIDIOC_DQBUF, &ctx->buf);
		rt_atrace_end_body();
		if (ret < 0) {
			printf("ERROR VIDIOC_DQBUF %d %s\n", errno, strerror(errno));
			return errno;
		}
		int currentFenceFd = (ctx->buf.timecode.userbits[3] << 24)
                    + (ctx->buf.timecode.userbits[2] << 16)
                    + (ctx->buf.timecode.userbits[1] << 8)
                    + (ctx->buf.timecode.userbits[0]);

                 if (currentFenceFd == 0 || currentFenceFd == 0xFFFFFFFF) {
                     currentFenceFd = -1;
                 } else {
		     close(currentFenceFd);
		}

		test_count++;
		int exp_fd = 0;
		std::map<int, int>::iterator it = ctx->mExpFdMap.find(ctx->buf.index);
		if (it != ctx->mExpFdMap.end()) {
			exp_fd = it->second;
		}
#else
        int exp_fd = get_frame();
#endif		
        if (exp_fd > 0) {
		  if (ctx->enable_vo > 0) {
			struct timespec ts;
			clock_gettime(CLOCK_MONOTONIC, &ts);
			set_vo_time_ms = (ts.tv_sec*1000000000 + ts.tv_nsec)/1000000;
			rt_atrace_begin_body("sendframe_vo");
			SetVoTime(set_vo_time_ms);
			int buffer_len = 0;
			if (strstr(_ctx->pixel_format, "NV12") > 0) {
				buffer_len = ctx->width*ctx->height*3/2;
			}

			if (strstr(_ctx->pixel_format, "BGR3") > 0) {
				buffer_len = ctx->width*ctx->height*3;
			}

			sendframe_vo_for_fd(exp_fd, buffer_len);
			showFps1();
			rt_atrace_end_body();
		  }
		  char strDeq[100];
		  memset(&strDeq[0], 0, 100);
		  sprintf(&strDeq[0], "deq set_frame,sequence=%d",ctx->buf.sequence);
		  skip_count++;
	    if (strstr(ctx->pixel_format, "NV12") > 0) {
			rt_atrace_begin_body(strDeq);
		    deq_time = systemTime();
		   if (ctx->enable_encode /*&& skip_count<10*/) {
		      set_frame(exp_fd, ctx->width*ctx->height*3/2);
		   }
			/*if (!ctx->firstFrame && skip_count>4) {
				rt_atrace_begin_body("START_VSYNC");
                sendCmd(ctx, START_VSYNC);
				ctx->firstFrame = 1;
				rt_atrace_end_body();
			}*/
			rt_atrace_end_body();
		} else {
			rt_atrace_begin_body(strDeq);
			if (ctx->enable_encode) {
            	set_frame(exp_fd, ctx->width*ctx->height*3);
			}
			rt_atrace_end_body();
		}
	  }
#if !ENABLE_VI
       // rt_atrace_begin_body("que buffer");
		ret = ioctl(ctx->fd, VIDIOC_QBUF, &ctx->buf);
		if (ret < 0) {
			printf("ERROR stream on %d %s\n", errno, strerror(errno));
			return errno;
		}
#endif
	//	rt_atrace_end_body();
		
	}
	stop_stream(ctx);
    deinit_venc();
	deinit_send();
#if ENABLE_VI
    deinit_vi();
#endif
	return 0;
}


static void HandlerTCPMessage(int sockfd,  char *buffer, int size) {
   
   CommandData_t *cmd_data = (CommandData_t*)buffer;
   cmd_data->cmdId = ntohl(cmd_data->cmdId);
   cmd_data->seqNum = ntohl(cmd_data->seqNum);
   printf("recv command id =%d, seqnum=%d\n", cmd_data->cmdId, cmd_data->seqNum);
   switch (cmd_data->cmdId) {
      case PROCESS_STREAM: {
	    std::shared_ptr<std::thread> process_thread;
        process_thread = make_shared<thread>(process_stream, _ctx);
	    process_thread->detach();
	    break;
	  }
      case START_STREAM: {
	     start_stream(_ctx);
	     break;
	  }
      case STOP_STREAM: {
	     stop_stream(_ctx);
	     break;
	  }
	  default:
	     printf("command id error.\n");
		 break;
   }

   return 0;
}

static int initTcp(struct rkcam_context *ctx) {
    ctx->tcp = std::make_shared<TCPServer>();
    ctx->tcp->RegisterRecvCallBack(HandlerTCPMessage);
    ctx->tcp->Process(SERVER_PORT);
}

static const char short_options[] = "d:lsiqFftw:h:p:A:aCcL:T:Q";
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
	{"display-type", required_argument, &flag, 5}, 
	{"enable-encode", required_argument, &flag, 6}, 
	{"enable-vo", required_argument, &flag, 7}, 
	{"enable-adjust-clk", required_argument, &flag, 8}, 
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
void handle_exit_signal(int signum) {
    printf("捕获到退出信号 (信号编号：%d)\n", signum);
    stop_stream(_ctx);
	_ctx->exit_thread = 1;
	stop_send();
	 deinit_send();
	exit(0);
}

int main(int argc, char **argv) {
	_ctx = (struct rkcam_context *)malloc(sizeof(rkcam_context));
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
	_ctx->firstFrame = 0;
	_ctx->exit_thread = 0;
	printf("enter main\n");
	parse_args(argc, argv, _ctx);
        _ctx->format = 2;
#if !ENABLE_VI
	_ctx->fd = open_device(_ctx);
    /*if (strstr(_ctx->pixel_format, "BGR3") < 0 \
	    && strstr(_ctx->pixel_format, "NV12") < 0) {
	   return -1;
	}*/
    if (signal(SIGTERM, handle_exit_signal) == SIG_ERR) {
      //  perror("注册信号处理函数失败");
        return 1;
    }

	if (signal(SIGINT, handle_exit_signal) == SIG_ERR) {
       // perror("注册信号处理函数失败");
        return 1;
    }

	if (_ctx->is_querycap)
		querycap_video(_ctx);

	switch(_ctx->format) {
		case 1:
			enmu_video_format(_ctx);
			break;
		case 2:
			if (is_v4l2_subdev(_ctx))
				set_camera_fmt(_ctx);
			else
				set_video_fmt(_ctx);
			break;
		case 3:
			get_video_fmt(_ctx);
			break;
		case 4:
			try_video_fmt(_ctx);
			break;
		default:
			break;
	}

	switch(_ctx->frame) {
		case 1:
			enmu_frame_size(_ctx);
			break;
		case 2:
			enmu_frame_interval(_ctx);
			break;
		default:
			break;
	}

	switch(_ctx->parm) {
		case  1:
			set_video_parm(_ctx);
			break;
		case 2:
			get_video_parm(_ctx);
			break;
		default:
			break;
	}

	switch(_ctx->vcrop) {
		case 1:
			query_video_crop(_ctx);
			break;
		case 2:
			set_video_crop(_ctx);
			break;
		case 3:
			get_video_crop(_ctx);
			break;
		default:
			break;
	}
#endif
  if (_ctx->enable_vo > 0) {
		drmCtx = (struct drmContext *)malloc(sizeof(drmContext));
		drmCtx->src_width = _ctx->width;
		drmCtx->src_height = _ctx->height;
		drmCtx->src_Stride = _ctx->width;
		bool b = initDrm(drmCtx);
		int vcnt_line = _ctx->display_h*0.8f;
		int vo_format = 0;
		if (strstr(_ctx->pixel_format, "NV12") > 0) {
			vo_format = RK_FMT_YUV420SP;
		} 
		
		if (strstr(_ctx->pixel_format, "BGR3") > 0) {
			vo_format = RK_FMT_BGR888;
		}
		switch (_ctx->display_type) {
			case 0:
				init_vo(_ctx->width, _ctx->height, _ctx->display_w, _ctx->display_h, vo_format,VO_INTF_HDMI,RK3588_VO_DEV_HD0,vcnt_line);
				break;
			case 1:
				init_vo(_ctx->width, _ctx->height, _ctx->display_w, _ctx->display_h, vo_format,VO_INTF_DP,RK3588_VO_DEV_HD2, vcnt_line);
				break;
			case 2:
				init_vo(_ctx->width, _ctx->height, _ctx->display_w, _ctx->display_h, vo_format,VO_INTF_EDP,RK3588_VO_DEV_HD2,vcnt_line);
				break;
			case 3:
				init_vo(_ctx->width, _ctx->height, _ctx->display_w, _ctx->display_h, vo_format,VO_INTF_MIPI,RK3588_VO_DEV_HD3,vcnt_line);
				break;
			default:
				init_vo(_ctx->width, _ctx->height, _ctx->display_w, _ctx->display_h, vo_format,VO_INTF_HDMI,RK3588_VO_DEV_HD0,vcnt_line);
				break;
		}
		std::shared_ptr<std::thread> process_thread;
		process_thread = make_shared<thread>(process_stream, _ctx);
		process_thread->detach();

		if (_ctx->enable_adjust_clk > 0) {
			SetDisplayType(_ctx->display_type);
			SetVsyncCallback(SetVcntTimeCb);
			std::thread  t(VsyncTread,  drmCtx->drmFd);
			t.detach();
		}
  }
  initTcp(_ctx);
  close_device(_ctx);
  if (_ctx->enable_vo > 0) {
     deinit_vo();
	 deinitDrm(drmCtx);
  }
  
  return 0;
}
