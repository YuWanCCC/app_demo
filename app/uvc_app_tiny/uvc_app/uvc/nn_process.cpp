/*
 * Copyright (C) 2019 Rockchip Electronics Co., Ltd.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL), available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "nn_process.h"
#include "uvc_mpi_config.h"
#include "uvc_mpi_vi.h"

#include "param.h"
#include "rockiva.h"

#include "uvc_log.h"
#include <cstring>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "rk_common.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_sys.h"
#include <sys/prctl.h>
#include <thread>
#include <unistd.h>

#define VI_MAX_INPUT_COUNT 8
#define NN_DYNAMIC_DEBUG_FPS "/tmp/nn_fps"

typedef struct _NN_DEBUG_FILE_INFO {
  int debugCnt;
  FILE *pDebugFile;
} NNDegbuFileInfo;

typedef struct _NN_PROCESS_CTX {
  int getCount;
  int doCount;
  bool mStart;
  std::thread *mThread;
  UVC_MPI_CFG *mUvcCfg;
  UVC_VI_CHN_CTX_S viChnCtx;
  NNDegbuFileInfo mDebug;
  struct timeval enterTime;
} NNProcessCtx;

typedef struct _NN_VI_BUF_MAP {
  int frameId;
  void *frame;
  UVC_VI_CHN_CTX_S viCtx;
} NNViBufMap;

static pthread_mutex_t mapMutex = PTHREAD_MUTEX_INITIALIZER;
static NNViBufMap VI_INPUT_BUF_MAP[VI_MAX_INPUT_COUNT];

extern "C" int nn_process_release_frame(UVC_VI_CHN_CTX_S viCtx, void *frame) {
  MB_BLK blk = (MB_BLK)frame;

  if (blk) {
    VIDEO_FRAME_INFO_S stViFrameInfo;
    stViFrameInfo.stVFrame.pMbBlk = blk;
    RK_MPI_VI_ReleaseChnFrame(viCtx.pipeId, viCtx.channelId, &stViFrameInfo);
  }
  return 0;
}

extern "C" int nn_process_release_frame_by_id(int frameId) {
  pthread_mutex_lock(&mapMutex);
  for (int i = 0; i < VI_MAX_INPUT_COUNT; i++) {
    if (VI_INPUT_BUF_MAP[i].frameId == frameId) {
      nn_process_release_frame(VI_INPUT_BUF_MAP[i].viCtx,
                               VI_INPUT_BUF_MAP[i].frame);
      LOG_DEBUG("release i:%d id:%d frame:%p\n", i, frameId,
                VI_INPUT_BUF_MAP[i].frame);
      VI_INPUT_BUF_MAP[i].frame = NULL;
      break;
    }
  }
  pthread_mutex_unlock(&mapMutex);

  return 0;
}

extern "C" int nn_process_insert_frame_by_id(NNProcessCtx *ctx, void *frame) {
  pthread_mutex_lock(&mapMutex);
  for (int i = 0; i < VI_MAX_INPUT_COUNT; i++) {
    if (VI_INPUT_BUF_MAP[i].frame == NULL) {
      VI_INPUT_BUF_MAP[i].frameId = ctx->doCount;
      VI_INPUT_BUF_MAP[i].frame = frame;
      VI_INPUT_BUF_MAP[i].viCtx = ctx->viChnCtx;
      LOG_DEBUG("insert i:%d id:%d frame:%p\n", i, ctx->doCount, frame);
      break;
    }
  }
  pthread_mutex_unlock(&mapMutex);

  return 0;
}

NNProcessCtx *getNNProcessCtx(void *ctx) {
  return reinterpret_cast<NNProcessCtx *>(ctx);
}

int NNProcess::initialize() {
  NNProcessCtx *ctx = getNNProcessCtx(mCtx);
  pthread_mutex_lock(&mapMutex);
  memset(VI_INPUT_BUF_MAP, 0, sizeof(NNViBufMap) * VI_MAX_INPUT_COUNT);
  pthread_mutex_unlock(&mapMutex);

  return 0;
}

int NNProcess::deinitialize() {
  NNProcessCtx *ctx = getNNProcessCtx(mCtx);
  releaseBuf();

  if (ctx)
    free(ctx);

  return 0;
}

void NNProcess::releaseBuf() {
  pthread_mutex_lock(&mapMutex);
  for (int i = 0; i < VI_MAX_INPUT_COUNT; i++) {
    if (VI_INPUT_BUF_MAP[i].frame) {
      nn_process_release_frame(VI_INPUT_BUF_MAP[i].viCtx,
                               VI_INPUT_BUF_MAP[i].frame);
      LOG_INFO("release i:%d id:%d frame:%p\n", i, VI_INPUT_BUF_MAP[i].frameId,
               VI_INPUT_BUF_MAP[i].frame);
      VI_INPUT_BUF_MAP[i].frame = NULL;
    }
  }
  pthread_mutex_unlock(&mapMutex);
}

NNProcess::NNProcess() : mCtx(NULL) {
  NNProcessCtx *ctx = (NNProcessCtx *)calloc(sizeof(NNProcessCtx), 1);
  mCtx = reinterpret_cast<void *>(ctx);

  initialize();
}

NNProcess::~NNProcess() { deinitialize(); }

int NNProcess::configVi() {
  NNProcessCtx *ctx = getNNProcessCtx(mCtx);
  uvc_vi_config(ctx->mUvcCfg, MPI_VI_CHANNEL_TYPE_NN);
  ctx->viChnCtx = uvc_get_vi_chn_ctx(*ctx->mUvcCfg, MPI_VI_CHANNEL_TYPE_NN);

  return 0;
}

int NNProcess::startVi() {
  NNProcessCtx *ctx = getNNProcessCtx(mCtx);
  uvc_vi_start(*ctx->mUvcCfg, MPI_VI_CHANNEL_TYPE_NN);

  return 0;
}

int NNProcess::stopVi() {
  NNProcessCtx *ctx = getNNProcessCtx(mCtx);
  uvc_vi_stop(*ctx->mUvcCfg, MPI_VI_CHANNEL_TYPE_NN);

  return 0;
}

int NNProcess::configProcess(UVC_MPI_CFG *uvcCfg) {
  NNProcessCtx *ctx = getNNProcessCtx(mCtx);

  ctx->mUvcCfg = uvcCfg;
  configVi();

  return 0;
}

int NNProcess::startProcess() {
  NNProcessCtx *ctx = getNNProcessCtx(mCtx);

  rkuvc_rockiva_init(
      ctx->mUvcCfg->vi_cfg[MPI_VI_CHANNEL_TYPE_NN].assign_width,
      ctx->mUvcCfg->vi_cfg[MPI_VI_CHANNEL_TYPE_NN].assign_height);

  startVi();

  ctx->mStart = true;
  // if use one thread do uvc. do not call this thread
  ctx->mThread = new std::thread(threadLoop, reinterpret_cast<void *>(this));

  return 0;
}

int NNProcess::stopProcess() {
  NNProcessCtx *ctx = getNNProcessCtx(mCtx);
  LOG_INFO("nn process exit enter\n");
  ctx->mStart = false;
  if (ctx->mThread) {
    ctx->mThread->join();
    delete ctx->mThread;
    ctx->mThread = nullptr;
  }

  releaseBuf();
  stopVi();

  rkuvc_rockiva_deinit();

  LOG_INFO("nn process exit ok\n");

  return 0;
}

static int process(NNProcessCtx *ctx) {
  RK_S32 s32Ret;
  VIDEO_FRAME_INFO_S videoFrame;
  RK_VOID *data;
  RK_S32 fd;
  RK_U32 width, height, seq;
  // for debug
  RK_U64 size;
  UVC_VI_CHN_CTX_S *viChnCtx = &ctx->viChnCtx;

  s32Ret = RK_MPI_VI_GetChnFrame(viChnCtx->pipeId, viChnCtx->channelId,
                                 &videoFrame, 1000);
  if (s32Ret == RK_SUCCESS) {
    fd = RK_MPI_MB_Handle2Fd(videoFrame.stVFrame.pMbBlk);
    width = videoFrame.stVFrame.u32Width;
    height = videoFrame.stVFrame.u32Height;
    seq = videoFrame.stVFrame.u32TimeRef;
    LOG_DEBUG("nn w:%d h:%d seq:%d fd:%d\n", width, height, seq, fd);
    // // for debug file
    // if (ctx->mUvcCfg->common_cfg.uvc_debug &&
    //     ctx->mDebug.debugCnt < ctx->mUvcCfg->common_cfg.nn_debug_cnt) {
    //   data = RK_MPI_MB_Handle2VirAddr(videoFrame.stVFrame.pMbBlk);
    //   size = RK_MPI_MB_GetSize(videoFrame.stVFrame.pMbBlk);
    //   RK_MPI_SYS_MmzFlushCache(videoFrame.stVFrame.pMbBlk, RK_TRUE);
    //   if (!ctx->mDebug.pDebugFile) {
    //     ctx->mDebug.pDebugFile =
    //         fopen(ctx->mUvcCfg->common_cfg.nn_debug_file, "w+b");
    //     LOG_INFO("open nn debug file:%s\n",
    //              ctx->mUvcCfg->common_cfg.nn_debug_file);
    //   }
    //   if (ctx->mDebug.pDebugFile) {
    //     fwrite(data, 1, size, ctx->mDebug.pDebugFile);
    //     ctx->mDebug.debugCnt++;
    //     if (ctx->mDebug.debugCnt >= ctx->mUvcCfg->common_cfg.nn_debug_cnt) {
    //       LOG_INFO("close nn debug file\n");
    //       fflush(ctx->mDebug.pDebugFile);
    //       fclose(ctx->mDebug.pDebugFile);
    //       ctx->mDebug.pDebugFile = NULL;
    //     }
    //   }
    // }
    ctx->getCount++;
    if (ctx->getCount) { // in 3 to 1   % 3 == 1
      ctx->doCount++;
      rkuvc_rockiva_write_nv12_frame_by_fd(width, height, ctx->doCount, fd);
      nn_process_insert_frame_by_id(ctx, (void *)videoFrame.stVFrame.pMbBlk);
      if (ctx->mUvcCfg->common_cfg.uvc_debug &&
          !access(NN_DYNAMIC_DEBUG_FPS, 0)) {
        if (ctx->doCount % 100 == 0) {
          struct timeval now_time;
          gettimeofday(&now_time, NULL);
          int64_t use_timems =
              (now_time.tv_sec * 1000 + now_time.tv_usec / 1000) -
              (ctx->enterTime.tv_sec * 1000 + ctx->enterTime.tv_usec / 1000);
          ctx->enterTime.tv_sec = now_time.tv_sec;
          ctx->enterTime.tv_usec = now_time.tv_usec;
          LOG_INFO("nn process fps:%0.1f\n", (1000.0 * 100) / use_timems);
        }
      }
    } else {
      RK_MPI_VI_ReleaseChnFrame(viChnCtx->pipeId, viChnCtx->channelId,
                                &videoFrame);
    }
  } else {
    LOG_INFO("get frame error ret is %x pipe is %d chn is %d \n", s32Ret,
             viChnCtx->pipeId, viChnCtx->channelId);
    usleep(5000);
  }

  return 0;
}

int NNProcess::doProcess() {
  NNProcessCtx *ctx = getNNProcessCtx(mCtx);
  process(ctx);

  return 0;
}

void *NNProcess::threadLoop(void *arg) {
  prctl(PR_SET_NAME, "nnProcessThread");
  NNProcess *nnProcess = reinterpret_cast<NNProcess *>(arg);
  NNProcessCtx *ctx = getNNProcessCtx(nnProcess->getCtx());
  int ret = 0;

  // when streamon start
  while (ctx->mStart) {
    ret = process(ctx);
    if (ret != 0)
      break;
  }
  LOG_INFO("nn thread exit ret:%d\n", ret);

  return NULL;
}
