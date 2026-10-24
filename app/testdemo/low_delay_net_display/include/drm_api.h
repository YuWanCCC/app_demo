#ifndef INCLUDE_DRM_API_H_
#define INCLUDE_DRM_API_H_
#include "drm_fourcc.h"
#include "xf86drm.h"
#include "xf86drmMode.h"
#include <map>
#include <vector>
enum
{
    OUTPUT_PRIMARY = 0,
    OUTPUT_EXTERNAL,
    OUTPUT_MAX,
};
//qiuen drm
static std::map<int, int>  mFbidMap;
static unsigned int set_vo_time_ms = 0;
#define HWC_DISPLAY_PRIMARY 0
#define HWC_DISPLAY_EXTERNAL 1

typedef struct DrmBuffer
{
    unsigned int bo_handle;
    unsigned int fb_handle;
    int dbuf_fd;
} DrmBuffer_t;

struct DrmOutput
{
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

typedef struct drm_bo
{
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


typedef struct drmContext
{
    DrmOutput mOutputs[OUTPUT_MAX];
    int src_width;
    int src_height;
    int src_Stride;
    int dst_width;
    int dst_height;
    int format;
    int drmFd;
    int display_type;
    std::vector<DrmBuffer_t *> drm_buffers;
} drmContext_t;

int buffer_create(drmContext_t *ctx, int w, int h, uint64_t size);

int buffer_destory(int drmfd);

uint32_t getDrmEncoder(int device);

int getOutputIndex(int device);

void resetOutput(drmContext *ctx, int index);

int FindPlane(drmContext *ctx, int device);

int getFbid(drmContext *ctx, int bufferFd);

bool SetDrmPlane(drmContext *ctx, int device, int bufferFd);

bool initDrm(drmContext *ctx);

void deinitDrm(drmContext *ctx);

bool detect(drmContext *ctx, int device);


#endif









