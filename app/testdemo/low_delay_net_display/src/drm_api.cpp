#include "drm_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <sys/mman.h>

int buffer_create(drmContext_t *ctx, int w, int h,
                  uint64_t size)
{
    struct drm_mode_create_dumb gem;
    struct drm_mode_destroy_dumb gem_destroy;
    int ret;
    struct drm_prime_handle prime;
    struct DrmBuffer *b;
    void *map = NULL;
    memset(&gem, 0, sizeof gem);
    gem.width = w;
    gem.height = h;
    gem.bpp = 16;
    gem.size = size;
    b = (DrmBuffer_t *)malloc(sizeof(DrmBuffer_t));
    ret = ioctl(ctx->drmFd, DRM_IOCTL_MODE_CREATE_DUMB, &gem);
    if (ret < 0)
    {
        printf(">>>>CREATE_DUMB failed: %s\n");
        return -1;
    }
    printf(">>>bo %u %ux%u bpp %u size %lu (%lu)\n", gem.handle, gem.width, gem.height, gem.bpp, gem.size, size);
    b->bo_handle = gem.handle;

    memset(&prime, 0, sizeof prime);
    prime.handle = b->bo_handle;

    ret = ioctl(ctx->drmFd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &prime);
    if (ret < 0)
    {
        printf(">>>>PRIME_HANDLE_TO_FD failed: %s\n", ENOSTR);
        goto fail_gem;
    }
    {
        b->dbuf_fd = prime.fd;
        unsigned char *mapAddr = (unsigned char *)mmap(0, size, PROT_READ, MAP_SHARED, prime.fd, 0);
        if (mapAddr == MAP_FAILED)
        {
            //printf("mmap failed,err=%s!\n",strerror(errno));
            return -errno;
        }
        // memset(mapAddr, 0, size);
        ctx->drm_buffers.push_back(b);
        printf(">>>>>>>===dbuf_fd = %d,addr=%p\n", prime.fd, mapAddr);
    }

    return prime.fd;

fail_prime:
    close(b->dbuf_fd);

fail_gem:
    memset(&gem_destroy, 0, sizeof gem_destroy);
    gem_destroy.handle = b->bo_handle,
    ret = ioctl(ctx->drmFd, DRM_IOCTL_MODE_DESTROY_DUMB, &gem_destroy);
    printf("DESTROY_DUMB failed: %s\n", ENOSTR);

    return -1;
}


int buffer_destory(drmContext_t *ctx)
{
    /*int ret;
    struct drm_mode_destroy_dumb gem_destroy;
    memset(&gem_destroy, 0, sizeof gem_destroy);
    gem_destroy.handle = b->bo_handle,
    ret = ioctl(drmfd, DRM_IOCTL_MODE_DESTROY_DUMB, &gem_destroy);
    printf("DESTROY_DUMB failed: %s\n", ENOSTR);*/
    return 0;
}



uint32_t getDrmEncoder(int device)
{
//  printf("====%d,%d, %d, %d\n",device, HWC_DISPLAY_PRIMARY,HWC_DISPLAY_EXTERNAL,DRM_MODE_ENCODER_TMDS);
    if (device == HWC_DISPLAY_PRIMARY)
        return 2;
    else if (device == HWC_DISPLAY_EXTERNAL)
        return DRM_MODE_ENCODER_TMDS;
    return DRM_MODE_ENCODER_NONE;
}


int getOutputIndex(int device)
{
    switch (device)
    {
    case HWC_DISPLAY_PRIMARY:
        return OUTPUT_PRIMARY;
    case HWC_DISPLAY_EXTERNAL:
        return OUTPUT_EXTERNAL;
    default:
        printf("invalid display device");
        break;
    }

    return -1;
}

void resetOutput(drmContext *ctx, int index)
{
    DrmOutput *output = &ctx->mOutputs[index];

    output->connected = false;
    memset(&output->mode, 0, sizeof(drmModeModeInfo));

    if (output->connector)
    {
        drmModeFreeConnector(output->connector);
        output->connector = 0;
    }
    if (output->encoder)
    {
        drmModeFreeEncoder(output->encoder);
        output->encoder = 0;
    }
    if (output->crtc)
    {
        drmModeFreeCrtc(output->crtc);
        output->crtc = 0;
    }
    if (output->fbId)
    {
        drmModeRmFB(ctx->drmFd, output->fbId);
        output->fbId = 0;
    }

}


int FindPlane(drmContext *ctx, int device)
{

    drmModePlanePtr plane;
    drmModeObjectPropertiesPtr props;
    drmModePropertyPtr prop;
    int find_plan_id = 0;
    int outputIndex = getOutputIndex(device);
    if (outputIndex < 0)
    {
        printf("invalid device\n");
        return false;
    }
    DrmOutput *output = &ctx->mOutputs[outputIndex];
    if (!output->connected)
    {
        printf("device is not connected,outputIndex=%d\n", outputIndex);
        return false;
    }
    for (uint32_t i = 0; i < output->plane_res->count_planes; i++)
    {
        plane = drmModeGetPlane(ctx->drmFd, output->plane_res->planes[i]);
        props = drmModeObjectGetProperties(ctx->drmFd, plane->plane_id, DRM_MODE_OBJECT_PLANE);
        if (!props)
        {
            printf("Failed to found props plane[%d] %s\n", plane->plane_id, strerror(errno));
            return -ENODEV;
        }
        for (uint32_t j = 0; j < props->count_props; j++)
        {
            prop = drmModeGetProperty(ctx->drmFd, props->props[j]);
            if (!strcmp(prop->name, "ASYNC_COMMIT"))
            {
                //     printf("find ASYNC_COMMIT plane id=%d value=%lld\n", plane->plane_id, (long long)props->prop_values[j]);
                if (props->prop_values[j] != 0)
                {
                    find_plan_id = plane->plane_id;
                    if (prop)
                        drmModeFreeProperty(prop);
                    break;
                }
            }
            if (prop)
                drmModeFreeProperty(prop);
        }
        if (props)
            drmModeFreeObjectProperties(props);
        if (plane)
            drmModeFreePlane(plane);
        if (find_plan_id > 0)
        {
            break;
        }
    }
    if (find_plan_id == 0)
    {
        return -1;
    }
    else
    {
        output->planeid = find_plan_id;
    }
//   printf("FindSidebandPlane find_plan_id=%d\n", find_plan_id);
    return find_plan_id;
}



int getFbid(drmContext *ctx, int bufferFd)
{


    int fd = 0;
    int ret = 0;
    int src_w = 0;
    int src_h = 0;
    int src_format = 0;
    int src_stride = 0;
    drm_bo_t bo;
    memset(&bo, 0, sizeof(drm_bo_t));
    fd = bufferFd;
    std::map<int, int>::iterator it = mFbidMap.find(fd);
    int fbid = 0;
    if (it == mFbidMap.end())
    {
        uint32_t gem_handle;
        size_t plane_size;
        ret = drmPrimeFDToHandle(ctx->drmFd, fd, &gem_handle);
        src_w = ctx->src_width;
        src_h = ctx->src_height;
        src_format = ctx->format;
#if 1
        plane_size = 2;
        // printf("plane_size = %zu", plane_size);
#if  ENABLE_4K
        src_stride = ctx->src_Stride;
#else
        src_stride = 2304;//ctx->src_Stride;
#endif
        bo.width = src_w;
        bo.height = src_h;
        bo.format = DRM_FORMAT_NV12;
        bo.pitches[0] = src_stride;
        bo.gem_handles[0] = gem_handle;
        bo.offsets[0] = 0;
        bo.pitches[1] = bo.pitches[0];
        bo.gem_handles[1] = gem_handle;
        bo.offsets[1] = bo.pitches[1] * bo.height;
#else
        plane_size = 1;
        printf("plane_size = %zu", plane_size);
        src_stride = ctx->src_Stride;
        bo.width = src_w;
        bo.height = src_h;
        bo.format = DRM_FORMAT_RGB888;
        bo.pitches[0] = src_stride * 3;
        bo.gem_handles[0] = gem_handle;
        bo.offsets[0] = 0;
        bo.pitches[1] = 0;
        bo.gem_handles[1] = 0;
        bo.offsets[1] = 0;
#endif

        printf("qiuen width=%d,height=%d,format=%x,fd=%d,src_stride=%d\n", bo.width, bo.height, bo.format, fd, src_stride);
        ret = drmModeAddFB2(ctx->drmFd, bo.width, bo.height, bo.format, bo.gem_handles, \
                            bo.pitches, bo.offsets, &bo.fb_id, 0);
        printf("qiuen >>>>drmModeAddFB2 ret = %s,fd=%d,fbid=%d\n", strerror(ret), fd,  bo.fb_id);
        fbid = bo.fb_id;
        mFbidMap.insert(std::pair<int, int>(fd, fbid));
    }
    else
    {
        fbid = it->second;
    }
    if (fbid <= 0)
    {
        printf("fbid is error.\n");
        return -1;
    }
    //printf("====getFbid\n");
    return fbid;
}


bool SetDrmPlane(drmContext *ctx, int device, int bufferFd)
{
    //  printf("%s come in, device=%d", __FUNCTION__, device );
    int ret = 0;
    int plane_id = FindPlane(ctx, device);
    int fb_id = getFbid(ctx, bufferFd);
    int flags = 0;
    int src_left = 0;
    int src_top = 0;
    int src_right = 0;
    int src_bottom = 0;
    int dst_left = 0;
    int dst_top = 0;
    int dst_right = 0;
    int dst_bottom = 0;
    int src_w = 0;
    int src_h = 0;
    int dst_w = 0;
    int dst_h = 0;
    DrmOutput *output = &ctx->mOutputs[device];
    dst_w = output->crtc->width;
    dst_h = output->crtc->height;
    src_w = ctx->src_width;
    src_h = ctx->src_height;
    //printf("dst_w %d dst_h %d src_w %d src_h %d in \n", dst_w, dst_h, src_w, src_h);
    //printf("ctx->drmFd=%d plane_id=%d output->crtc->crtc_id=%d fb_id=%d flags=%d\n", ctx->drmFd, plane_id, output->crtc->crtc_id, fb_id, flags);

//  ret = drmModeSetCrtc(ctx->drmFd, output->crtc->crtc_id, fb_id, 0, 0,
    //                                  &output->connector->connector_id, 1, &output->connector->modes[0]);
    if (plane_id > 0)
    {
        ret = drmModeSetPlane(ctx->drmFd, plane_id,
                              output->crtc->crtc_id, fb_id, flags,
                              dst_left, dst_top,
                              dst_w, dst_h,
                              0, 0,
                              src_w << 16, src_h << 16);
        //printf("drmModeSetPlane ret=%s\n", strerror(ret));
    }
//    printf("%s end.", __FUNCTION__);
    return true;
}

bool initDrm(drmContext *ctx)
{
    printf("initialize in\n");
    if (ctx == NULL)
    {
        printf("initialize fail.\n");
        return false;
    }
    const char *path = "/dev/dri/card0";
    ctx->drmFd = open(path, O_RDWR);

    if (ctx->drmFd < 0)
    {
        printf("failed to open Drm, error: %s\n", strerror(errno));
        return false;
    }
    printf("ctx->drmFd = %d", ctx->drmFd);

    return true;
}

void deinitDrm(drmContext *ctx)
{
    for (int i = 0; i < OUTPUT_MAX; i++)
    {
        resetOutput(ctx, i);
    }

    if (ctx->drmFd > 0)
    {
        close(ctx->drmFd);
        ctx->drmFd = 0;
    }

    for (const auto &fbidMap : mFbidMap)
    {
        int fbid = fbidMap.second;
        if (drmModeRmFB(ctx->drmFd, fbid))
            printf("Failed to rm fb\n");
    }

}



bool detect(drmContext *ctx, int device)
{
    int outputIndex = getOutputIndex(device);
    if (outputIndex < 0)
    {
        return false;
    }

    resetOutput(ctx, outputIndex);
    drmModeConnectorPtr connector = NULL;
    DrmOutput *output = &ctx->mOutputs[outputIndex];
    bool ret = false;
    // get drm resources
    drmModeResPtr resources = drmModeGetResources(ctx->drmFd);
    if (!resources)
    {
        printf("fail to get drm resources, error: %s\n", strerror(errno));
        return false;
    }

    ret = drmSetClientCap(ctx->drmFd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    if (ret)
    {
        printf("Failed to set atomic cap %s\n", strerror(errno));
        return ret;
    }
    ret = drmSetClientCap(ctx->drmFd, DRM_CLIENT_CAP_ATOMIC, 1);
    if (ret)
    {
        printf("Failed to set atomic cap %s", strerror(errno));
        return ret;
    }

    output->res = resources;
    // printf("qiuen resources->count_connectors=%d\n",resources->count_connectors);

    // find connector for the given device
    for (int i = 0; i < resources->count_connectors; i++)
    {
        if (!resources->connectors || !resources->connectors[i])
        {
            printf("fail to get drm resources connectors, error: %s", strerror(errno));
            continue;
        }
        connector = drmModeGetConnector(ctx->drmFd, resources->connectors[i]);
        if (!connector)
        {
            printf("qiuen drmModeGetConnector failed\n");
            continue;
        }

        if (connector->connection != DRM_MODE_CONNECTED)
        {
            printf("qiuen +++device %d is not connected\n", device);
            if (i == resources->count_connectors - 1)
            {
                drmModeFreeConnector(connector);
                ret = true;
                break;
            }
            continue;
        }
        if (connector->connector_type != ctx->display_type)
            continue;

        output->connector = connector;
        output->connected = true;
        printf("qiuen connector %d connected,connectorid=%d,encoderid=%d,type=%d\n", \
               outputIndex, connector->connector_id, connector->encoders[0], connector->connector_type);
        // get proper encoder for the given connector
        if (connector->encoders[0] > 0)
        {
            printf("Drm connector has encoder attached on device %d\n", device);
            output->encoder = drmModeGetEncoder(ctx->drmFd, connector->encoders[0]);
            if (!output->encoder)
            {
                printf("failed to get encoder from a known encoder id");
                // fall through to get an encoder
            }
        }

        if (!output->encoder)
        {
            printf("qiuen getting encoder for device %d\n", device);
            drmModeEncoderPtr encoder;
            for (int j = 0; j < resources->count_encoders; j++)
            {
                if (!resources->encoders || !resources->encoders[j])
                {
                    printf("fail to get drm resources encoders, error: %s\n", strerror(errno));
                    continue;
                }

                encoder = drmModeGetEncoder(ctx->drmFd, resources->encoders[i]);
                if (!encoder)
                {
                    printf("drmModeGetEncoder failed\n");
                    continue;
                }
                if (encoder->encoder_type == getDrmEncoder(device))
                {
                    output->encoder = encoder;
                    break;
                }
                drmModeFreeEncoder(encoder);
                encoder = NULL;
            }
        }
        if (!output->encoder)
        {
            printf("failed to get drm encoder\n");
            break;
        }

        // get an attached crtc or spare crtc
        /* if (output->encoder->crtc_id) {
             printf("Drm encoder has crtc attached on device %d\n", device);
             output->crtc = drmModeGetCrtc(ctx->drmFd, output->encoder->crtc_id);
             if (!output->crtc) {
                 printf("failed to get crtc from a known crtc id\n");
                 // fall through to get a spare crtc
             }
         }*/
        if (!output->crtc)
        {
            printf("getting crtc for device %d\n", device);
            drmModeCrtcPtr crtc;
            for (int j = 0; j < resources->count_crtcs; j++)
            {
                if (!resources->crtcs || !resources->crtcs[j])
                {
                    printf("fail to get drm resources crtcs, error: %s\n", strerror(errno));
                    continue;
                }

                crtc = drmModeGetCrtc(ctx->drmFd, resources->crtcs[j]);
                if (!crtc)
                {
                    printf("drmModeGetCrtc failed\n");
                    continue;
                }
                // check if legal crtc to the encoder
                //  printf("qiuen output->crtcid=%d\n",crtc->crtc_id);
                if (output->encoder->possible_crtcs & (1 << j))
                {
                    output->crtc = crtc;
                    printf("qiuen w=%d,h=%d\n", output->connector->modes[0].vdisplay, output->connector->modes[0].hdisplay);
                    ret = drmModeSetCrtc(ctx->drmFd, output->crtc->crtc_id, 0, 0, 0,
                                         &output->connector->connector_id, 1, &output->connector->modes[0]);
                    //  printf("qiuen =============== output->crtcid=%d,ret=%d\n",output->crtc->crtc_id,ret);
                    break;
                }


                /*drmModeObjectPropertiesPtr props;
                drmModePropertyPtr prop;
                props = drmModeObjectGetProperties(ctx->drmFd, crtc->crtc_id, DRM_MODE_OBJECT_CRTC);
                if (!props) {
                   printf("qiuen Failed to found props crtc[%d] %s\n",
                       crtc->crtc_id, strerror(errno));
                   continue;
                }
                for (uint32_t i = 0; i < props->count_props; i++) {
                   prop = drmModeGetProperty(ctx->drmFd, props->props[i]);
                   if (!strcmp(prop->name, "ACTIVE")) {
                       printf("qiuen Crtc id=%d is ACTIVE.", crtc->crtc_id);
                       if (props->prop_values[i]) {
                           output->crtc = crtc;
                           printf("qiuen Crtc id=%d is active\n",crtc->crtc_id);
                           break;
                       }
                   }
                }*/
            }
        }
        if (!output->crtc)
        {
            printf("failed to get drm crtc");
            break;
        }
        printf("qiuen connector %d connected,connectorid=%d,encoderid=%d,type=%d,crtcid=%d\n", \
               outputIndex, connector->connector_id, connector->encoders[0], connector->connector_type, output->crtc->crtc_id);
        output->plane_res = drmModeGetPlaneResources(ctx->drmFd);
        printf("drmModeGetPlaneResources successful.");
        break;
    }

    if (output->crtc)
    {
        output->props = drmModeObjectGetProperties(ctx->drmFd, output->crtc->crtc_id, DRM_MODE_OBJECT_CRTC);
        if (!output->props)
        {
            printf("Failed to found props crtc[%d] %s\n", output->crtc->crtc_id, strerror(errno));
        }
    }

    drmModeFreeResources(resources);

    return ret;
}
