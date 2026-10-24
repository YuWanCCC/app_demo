/*
 * Copyright 2021 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/poll.h>
#include <unistd.h>
#include <limits.h>
#include "sample_comm.h"
#ifdef MAX
#undef MAX
#endif
#define MAX(a, b) ((a) > (b) ? (a) : (b))
typedef struct svc_param {
	int resolution;
	int max_bps_h264;
	int min_bps_h264;
	int max_bps_h265;
	int min_bps_h265;
	int gop_fps_scale;
} svc_param;

static svc_param bitrate_first[] = {
    {3840 * 2160, 4000, 1600, 3500, 1000, 5}, {2880 * 1616, 2800, 1000, 2400, 700, 5},
    {1920 * 2160, 2400, 500, 2100, 400, 5},   {2560 * 1440, 2300, 500, 2000, 400, 5},
    {1920 * 1080, 2000, 500, 1600, 400, 5},   {1280 * 720, 1200, 400, 1000, 300, 5},
    {720 * 576, 800, 200, 700, 150, 5},       {640 * 360, 600, 200, 500, 150, 5},
};
static svc_param quality_first[] = {
    {3840 * 2160, 6000, 2400, 5400, 1600, 5}, {2880 * 1616, 4000, 1300, 3600, 1000, 5},
    {1920 * 2160, 3600, 1200, 3200, 900, 5},  {2560 * 1440, 3400, 1100, 3000, 800, 5},
    {1920 * 1080, 2500, 800, 2200, 700, 5},   {1280 * 720, 1600, 600, 1200, 400, 5},
    {720 * 576, 1100, 400, 900, 300, 5},      {640 * 360, 800, 300, 700, 250, 5},
};
static svc_param bitrate_quality_mean[] = {
    {3840 * 2160, 5000, 2000, 4500, 1300, 5}, {2880 * 1616, 3400, 1100, 2800, 800, 5},
    {1920 * 2160, 3200, 1000, 2500, 600, 5},  {2560 * 1440, 3000, 900, 2400, 500, 5},
    {1920 * 1080, 2500, 700, 2000, 500, 5},   {1280 * 720, 1400, 500, 1100, 300, 5},
    {720 * 576, 1000, 300, 800, 200, 5},      {640 * 360, 700, 200, 600, 150, 5},
};

static int find_closest_resolution(int input_resolution) {
	int closest_index = -1;
	int closest_diff = INT_MAX;
	for (int i = 0; i < sizeof(bitrate_first) / sizeof(bitrate_first[0]); i++) {
		int diff = abs(bitrate_first[i].resolution - input_resolution);
		if (diff < closest_diff) {
			closest_diff = diff;
			closest_index = i;
		}
	}
	return closest_index;
}

static RK_S32 SAMPLE_COMM_VENC_SET_ADVANCED_PARAMS(int venc_chn_id,
                                                   CODEC_TYPE_E codec_type) {
	int ret = 0;
	ret = RK_MPI_VENC_EnableSvc(venc_chn_id, RK_TRUE);
	if (ret != RK_SUCCESS) {
		RK_LOGE("RK_MPI_VENC_EnableSvc failed %X", ret);
		return ret;
	}
	VENC_RC_PARAM_S venc_rc_param;
	RK_MPI_VENC_GetRcParam(venc_chn_id, &venc_rc_param);
	venc_rc_param.s32FirstFrameStartQp = 33;
	if (codec_type == RK_CODEC_TYPE_H264) {
		venc_rc_param.stParamH264.u32StepQp = 8;
		venc_rc_param.stParamH264.u32MaxQp = 51;
		venc_rc_param.stParamH264.u32MinQp = 10;
		venc_rc_param.stParamH264.u32MaxIQp = 51;
		venc_rc_param.stParamH264.u32MinIQp = 10;
		venc_rc_param.stParamH264.u32FrmMinIQp = 27;
		venc_rc_param.stParamH264.u32FrmMinQp = 28;
		venc_rc_param.stParamH264.u32FrmMaxIQp = 45;
		venc_rc_param.stParamH264.u32FrmMaxQp = 48;
	} else if (codec_type == RK_CODEC_TYPE_H265) {
		venc_rc_param.stParamH265.u32StepQp = 8;
		venc_rc_param.stParamH265.u32MaxQp = 51;
		venc_rc_param.stParamH265.u32MinQp = 10;
		venc_rc_param.stParamH265.u32MaxIQp = 51;
		venc_rc_param.stParamH265.u32MinIQp = 10;
		venc_rc_param.stParamH265.u32FrmMinIQp = 27;
		venc_rc_param.stParamH265.u32FrmMinQp = 28;
		venc_rc_param.stParamH265.u32FrmMaxIQp = 45;
		venc_rc_param.stParamH265.u32FrmMaxQp = 48;
	} else {
		RK_LOGE("codec_type is %d, not support\n", codec_type);
		return -1;
	}
	RK_MPI_VENC_SetRcParam(venc_chn_id, &venc_rc_param);

	ret = RK_MPI_VENC_EnableMotionDeblur(venc_chn_id, 1);
	if (ret)
		RK_LOGE("RK_MPI_VENC_EnableMotionDeblur error! ret=%#x\n", ret);

	ret = RK_MPI_VENC_SetMotionDeblurStrength(venc_chn_id, 0);
	if (ret)
		RK_LOGE("RK_MPI_VENC_SetMotionDeblurStrength error! ret=%#x\n", ret);

	ret = RK_MPI_VENC_EnableMotionStaticSwitch(venc_chn_id, 0);
	if (ret)
		RK_LOGE("RK_MPI_VENC_EnableMotionStaticSwitch error! ret=%#x\n", ret);

	VENC_DEBREATHEFFECT_S debfrath_effect;
	memset(&debfrath_effect, 0, sizeof(VENC_DEBREATHEFFECT_S));
	debfrath_effect.bEnable = 0;
	debfrath_effect.s32Strength1 = 16;
	ret = RK_MPI_VENC_SetDeBreathEffect(venc_chn_id, &debfrath_effect);
	if (ret)
		RK_LOGE("RK_MPI_VENC_SetDeBreathEffect error! ret=%#x\n", ret);

	if (codec_type == RK_CODEC_TYPE_H264) {
		VENC_H264_TRANS_S pstH264Trans;
		RK_MPI_VENC_GetH264Trans(venc_chn_id, &pstH264Trans);
		pstH264Trans.chroma_qp_index_offset = 0;
		pstH264Trans.bScalingListValid = 1;
		RK_MPI_VENC_SetH264Trans(venc_chn_id, &pstH264Trans);
	} else if (codec_type == RK_CODEC_TYPE_H265) {
		VENC_H265_TRANS_S pstH265Trans;
		RK_MPI_VENC_GetH265Trans(venc_chn_id, &pstH265Trans);
		pstH265Trans.cb_qp_offset = 0;
		pstH265Trans.cr_qp_offset = 0;
		pstH265Trans.bScalingListEnabled = 1;
		RK_MPI_VENC_SetH265Trans(venc_chn_id, &pstH265Trans);
	}

	VENC_RC_PARAM2_S rc_param2;
	ret = RK_MPI_VENC_GetRcParam2(venc_chn_id, &rc_param2);
	if (ret)
		RK_LOGE("RK_MPI_VENC_GetRcParam2 error! ret=%#x\n", ret);

	int thrdI[] = {1, 2, 3, 3, 3, 3, 5, 5, 8, 8, 8, 15, 15, 20, 25, 25};
	for (int i = 0; i < sizeof(thrdI) / sizeof(thrdI[0]); i++) {
		rc_param2.u32ThrdI[i] = thrdI[i];
	}

	int thrdP[] = {1, 2, 3, 3, 3, 3, 5, 5, 8, 8, 8, 15, 15, 20, 25, 25};
	for (int i = 0; i < sizeof(thrdP) / sizeof(thrdP[0]); i++) {
		rc_param2.u32ThrdP[i] = thrdP[i];
	}

	int aqStepI[] = {-8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 7, 9, 9};
	for (int i = 0; i < sizeof(aqStepI) / sizeof(aqStepI[0]); i++) {
		rc_param2.s32AqStepI[i] = aqStepI[i];
	}

	int aqStepP[] = {-8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 7, 9, 9};
	for (int i = 0; i < sizeof(aqStepP) / sizeof(aqStepP[0]); i++) {
		rc_param2.s32AqStepP[i] = aqStepP[i];
	}

	ret = RK_MPI_VENC_SetRcParam2(venc_chn_id, &rc_param2);
	if (ret)
		RK_LOGE("RK_MPI_VENC_SetRcParam2 error! ret=%#x\n", ret);

	if (codec_type == RK_CODEC_TYPE_H264) {
		VENC_H264_QBIAS_S qbias;
		qbias.bEnable = 1;
		qbias.u32QbiasI = 600;
		qbias.u32QbiasP = 300;
		ret = RK_MPI_VENC_SetH264Qbias(venc_chn_id, &qbias);
		if (ret)
			RK_LOGE("RK_MPI_VENC_SetH264Qbias error! ret=%#x\n", ret);
	} else {
		VENC_H265_QBIAS_S qbias;
		qbias.bEnable = 1;
		qbias.u32QbiasI = 144;
		qbias.u32QbiasP = 85;
		ret = RK_MPI_VENC_SetH265Qbias(venc_chn_id, &qbias);
		if (ret)
			RK_LOGE("RK_MPI_VENC_SetH265Qbias error! ret=%#x\n", ret);
	}

	VENC_FILTER_S filter_s;
	RK_MPI_VENC_GetFilter(venc_chn_id, &filter_s);
	filter_s.u32StrengthI = 0;
	filter_s.u32StrengthP = 0;
	RK_MPI_VENC_SetFilter(venc_chn_id, &filter_s);

	if (codec_type == RK_CODEC_TYPE_H265) {
		VENC_H265_CU_DQP_S cu_dqp_s;
		RK_MPI_VENC_SetH265CuDqp(venc_chn_id, &cu_dqp_s);
		cu_dqp_s.u32CuDqp = 0;
		RK_MPI_VENC_SetH265CuDqp(venc_chn_id, &cu_dqp_s);
	}

	VENC_ANTI_RING_S anti_ring_s;
	RK_MPI_VENC_GetAntiRing(venc_chn_id, &anti_ring_s);
	anti_ring_s.u32AntiRing = 3;
	RK_MPI_VENC_SetAntiRing(venc_chn_id, &anti_ring_s);

	VENC_ANTI_LINE_S anti_line_s;
	RK_MPI_VENC_GetAntiLine(venc_chn_id, &anti_line_s);
	anti_line_s.u32AntiLine = 3;
	RK_MPI_VENC_SetAntiLine(venc_chn_id, &anti_line_s);

#ifdef RV1106B
	VENC_LAMBDA_S lambda_s;
	RK_MPI_VENC_GetLambda(venc_chn_id, &lambda_s);
	if (codec_type == RK_CODEC_TYPE_H264) {
		lambda_s.u32Lambda = 6;
		lambda_s.u32LambdaI = 6;
	} else {
		lambda_s.u32Lambda = 5;
		lambda_s.u32LambdaI = 5;
	}
	RK_MPI_VENC_SetLambda(venc_chn_id, &lambda_s);

	VENC_ANTI_FLICK_S anti_flick_s;
	RK_MPI_VENC_GetAntiFlick(venc_chn_id, &anti_flick_s);
	anti_flick_s.u32AntiFlick = 2;
	RK_MPI_VENC_SetAntiFlick(venc_chn_id, &anti_flick_s);

	VENC_STATIC_WEIGHT_S static_weight_s;
	RK_MPI_VENC_GetStaticWeight(venc_chn_id, &static_weight_s);
	static_weight_s.u32FrmNum = 0;
	static_weight_s.u32Madp16Th = 15;
	static_weight_s.u32Skip16Weight = 6;
	static_weight_s.u32Skip32Weight = 6;
	RK_MPI_VENC_SetStaticWeight(venc_chn_id, &static_weight_s);

	if (codec_type == RK_CODEC_TYPE_H264) {
		VENC_H264_QBIAS2_S qbias2_s;
		RK_MPI_VENC_GetH264Qbias2(venc_chn_id, &qbias2_s);
		qbias2_s.bEnable = 1;
		int qbias2[] = {3,  6,   13,  576, 576, 576, 576, 3,   6,
		                13, 576, 576, 683, 576, 341, 341, 341, 341};
		for (int i = 0; i < sizeof(qbias2) / sizeof(qbias2[0]); i++) {
			qbias2_s.u32Qbias2[i] = qbias2[i];
		}
		RK_MPI_VENC_SetH264Qbias2(venc_chn_id, &qbias2_s);
	} else {
		VENC_H265_QBIAS2_S qbias2_s;
		RK_MPI_VENC_GetH265Qbias2(venc_chn_id, &qbias2_s);
		qbias2_s.bEnable = 1;
		int qbias2[] = {3,  6,   13,  144, 144, 144, 144, 3,  6,
		                13, 144, 144, 171, 144, 85,  85,  85, 85};
		for (int i = 0; i < sizeof(qbias2) / sizeof(qbias2[0]); i++) {
			qbias2_s.u32Qbias2[i] = qbias2[i];
		}
		RK_MPI_VENC_SetH265Qbias2(venc_chn_id, &qbias2_s);
	}

	VENC_RC_PARAM3_S rc_param3;
	RK_MPI_VENC_GetRcParam3(venc_chn_id, &rc_param3);
	int aqRange[] = {15, 15, 31, 31, 31, 15, 15, 31, 31, 31};
	for (int i = 0; i < sizeof(aqRange) / sizeof(aqRange[0]); i++) {
		rc_param3.u32AqRange[i] = aqRange[i];
	}
	RK_MPI_VENC_SetRcParam3(venc_chn_id, &rc_param3);

	VENC_LIGHT_CHANGE_S light_change;
	RK_MPI_VENC_GetLightChange(venc_chn_id, &light_change);
	light_change.u32Level = 0;
	RK_MPI_VENC_SetLightChange(venc_chn_id, &light_change);

	if (codec_type == RK_CODEC_TYPE_H265) {
		RK_MPI_VENC_EnableTmvp(venc_chn_id, 0);
	}
#endif

	return ret;
}

RK_S32 SAMPLE_COMM_VENC_CreateChn(SAMPLE_VENC_CTX_S *ctx) {
	RK_S32 s32Ret = RK_FAILURE;
	VENC_RECV_PIC_PARAM_S stRecvParam;
	VENC_RC_PARAM_S pstRcParam;
	VENC_CHN_BUF_WRAP_S stVencChnBufWrap;
	VENC_CHN_REF_BUF_SHARE_S stVencChnRefBufShare;
	VENC_H264_TRANS_S stH264Trans;
	VENC_H265_TRANS_S stH265Trans;

	struct svc_param param =
	    bitrate_first[find_closest_resolution(ctx->u32Width * ctx->u32Height)];
	RK_U32 rate1 = 100 * ctx->u32Fps / 15;
	RK_U32 max_rate = 0;
	RK_U32 min_rate = 0;
	if (ctx->enCodecType == RK_CODEC_TYPE_H264) {
		max_rate = param.max_bps_h264 * rate1 / 100;
		min_rate = MAX(param.min_bps_h264 * rate1 / 100 * 0.75, param.min_bps_h264);
	} else {
		max_rate = param.max_bps_h265 * rate1 / 100;
		min_rate = MAX(param.min_bps_h265 * rate1 / 100 * 0.75, param.min_bps_h264);
	}
	switch (ctx->enCodecType) {
	case RK_CODEC_TYPE_H265:
		ctx->stChnAttr.stVencAttr.enType = RK_VIDEO_ID_HEVC;
		if (ctx->enRcMode == VENC_RC_MODE_H265CBR) {
			ctx->stChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265CBR;
			ctx->stChnAttr.stRcAttr.stH265Cbr.u32Gop = ctx->u32Gop;
			ctx->stChnAttr.stRcAttr.stH265Cbr.u32BitRate = ctx->u32BitRate;
			// frame rate: in u32Fps/1, out u32Fps/1.
			ctx->stChnAttr.stRcAttr.stH265Cbr.fr32DstFrameRateDen = 1;
			ctx->stChnAttr.stRcAttr.stH265Cbr.fr32DstFrameRateNum = ctx->u32Fps;
			ctx->stChnAttr.stRcAttr.stH265Cbr.u32SrcFrameRateDen = 1;
			ctx->stChnAttr.stRcAttr.stH265Cbr.u32SrcFrameRateNum = ctx->u32Fps;
		} else if (ctx->enRcMode == VENC_RC_MODE_H265VBR) {
			ctx->stChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265VBR;
			ctx->stChnAttr.stRcAttr.stH265Vbr.u32BitRate = max_rate;
			ctx->stChnAttr.stRcAttr.stH265Vbr.u32MaxBitRate = max_rate;
			ctx->stChnAttr.stRcAttr.stH265Vbr.u32MinBitRate = min_rate;
			ctx->stChnAttr.stRcAttr.stH265Vbr.u32Gop = ctx->u32Fps * param.gop_fps_scale;
			ctx->stChnAttr.stRcAttr.stH265Vbr.u32SrcFrameRateNum = ctx->u32Fps;
			ctx->stChnAttr.stRcAttr.stH265Vbr.u32SrcFrameRateDen = 1;
			ctx->stChnAttr.stRcAttr.stH265Vbr.fr32DstFrameRateNum = ctx->u32Fps;
			ctx->stChnAttr.stRcAttr.stH265Vbr.fr32DstFrameRateDen = 1;
		}
		break;
	case RK_CODEC_TYPE_MJPEG:
		ctx->stChnAttr.stVencAttr.enType = RK_VIDEO_ID_MJPEG;
		if (ctx->enRcMode == VENC_RC_MODE_MJPEGCBR) {
			ctx->stChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGCBR;
			// frame rate: in u32Fps/1, out u32Fps/1.
			ctx->stChnAttr.stRcAttr.stMjpegCbr.fr32DstFrameRateDen = 1;
			ctx->stChnAttr.stRcAttr.stMjpegCbr.fr32DstFrameRateNum = ctx->u32Fps;
			ctx->stChnAttr.stRcAttr.stMjpegCbr.u32SrcFrameRateDen = 1;
			ctx->stChnAttr.stRcAttr.stMjpegCbr.u32SrcFrameRateNum = ctx->u32Fps;
			ctx->stChnAttr.stRcAttr.stMjpegCbr.u32BitRate =
			    ctx->u32Width * ctx->u32Height * 8;
		} else if (ctx->enRcMode == VENC_RC_MODE_MJPEGVBR) {
			ctx->stChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGVBR;
			// frame rate: in u32Fps/1, out u32Fps/1.
			ctx->stChnAttr.stRcAttr.stMjpegVbr.fr32DstFrameRateDen = 1;
			ctx->stChnAttr.stRcAttr.stMjpegVbr.fr32DstFrameRateNum = ctx->u32Fps;
			ctx->stChnAttr.stRcAttr.stMjpegVbr.u32SrcFrameRateDen = 1;
			ctx->stChnAttr.stRcAttr.stMjpegVbr.u32SrcFrameRateNum = ctx->u32Fps;
			ctx->stChnAttr.stRcAttr.stMjpegVbr.u32BitRate =
			    ctx->u32Width * ctx->u32Height * 8;
		}
		break;
	case RK_CODEC_TYPE_JPEG:
		RK_LOGE("RK_CODEC_TYPE_JPEG setting");
		ctx->stChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGCBR;
		ctx->stChnAttr.stVencAttr.enType = RK_VIDEO_ID_JPEG;
		ctx->stChnAttr.stVencAttr.stAttrJpege.bSupportDCF = RK_FALSE;
		ctx->stChnAttr.stVencAttr.stAttrJpege.stMPFCfg.u8LargeThumbNailNum = 0;
		ctx->stChnAttr.stVencAttr.stAttrJpege.enReceiveMode = VENC_PIC_RECEIVE_SINGLE;
		break;
	case RK_CODEC_TYPE_H264:
	default:
		ctx->stChnAttr.stVencAttr.enType = RK_VIDEO_ID_AVC;
		if (ctx->enRcMode == VENC_RC_MODE_H264CBR) {
			ctx->stChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
			ctx->stChnAttr.stRcAttr.stH264Cbr.u32Gop = ctx->u32Gop;
			ctx->stChnAttr.stRcAttr.stH264Cbr.u32BitRate = ctx->u32BitRate;
			// frame rate: in u32Fps/1, out u32Fps/1.
			ctx->stChnAttr.stRcAttr.stH264Cbr.fr32DstFrameRateDen = 1;
			ctx->stChnAttr.stRcAttr.stH264Cbr.fr32DstFrameRateNum = ctx->u32Fps;
			ctx->stChnAttr.stRcAttr.stH264Cbr.u32SrcFrameRateDen = 1;
			ctx->stChnAttr.stRcAttr.stH264Cbr.u32SrcFrameRateNum = ctx->u32Fps;
		} else if (ctx->enRcMode == VENC_RC_MODE_H264VBR) {
			ctx->stChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264VBR;
			ctx->stChnAttr.stRcAttr.stH264Vbr.u32BitRate = max_rate;
			ctx->stChnAttr.stRcAttr.stH264Vbr.u32MaxBitRate = max_rate;
			ctx->stChnAttr.stRcAttr.stH264Vbr.u32MinBitRate = min_rate;
			ctx->stChnAttr.stRcAttr.stH264Vbr.u32Gop = ctx->u32Fps * param.gop_fps_scale;
			ctx->stChnAttr.stRcAttr.stH264Vbr.u32SrcFrameRateNum = ctx->u32Fps;
			ctx->stChnAttr.stRcAttr.stH264Vbr.u32SrcFrameRateDen = 1;
			ctx->stChnAttr.stRcAttr.stH264Vbr.fr32DstFrameRateNum = ctx->u32Fps;
			ctx->stChnAttr.stRcAttr.stH264Vbr.fr32DstFrameRateDen = 1;
		}
		break;
	}

	ctx->stChnAttr.stVencAttr.enPixelFormat = ctx->enPixelFormat;
	ctx->stChnAttr.stVencAttr.u32MaxPicWidth = ctx->u32Width;
	ctx->stChnAttr.stVencAttr.u32MaxPicHeight = ctx->u32Height;
	ctx->stChnAttr.stVencAttr.u32PicWidth = ctx->u32Width;
	ctx->stChnAttr.stVencAttr.u32PicHeight = ctx->u32Height;
	ctx->stChnAttr.stVencAttr.u32VirWidth = RK_ALIGN_2(ctx->u32Width);
	ctx->stChnAttr.stVencAttr.u32VirHeight = RK_ALIGN_2(ctx->u32Height);
	ctx->stChnAttr.stVencAttr.u32Profile =
	    (ctx->stChnAttr.stVencAttr.enType == RK_VIDEO_ID_AVC) ? H264E_PROFILE_HIGH
	                                                          : H265E_PROFILE_MAIN;
	if (ctx->u32StreamBufCnt) {
		ctx->stChnAttr.stVencAttr.u32StreamBufCnt = ctx->u32StreamBufCnt;
	} else {
		ctx->stChnAttr.stVencAttr.u32StreamBufCnt = 6;
	}
	RK_LOGE("V_C:%d bufferCnt:%d", ctx->s32ChnId,
	        ctx->stChnAttr.stVencAttr.u32StreamBufCnt);
	if (ctx->u32BuffSize) {
		ctx->stChnAttr.stVencAttr.u32BufSize = ctx->u32BuffSize;
	} else {
		ctx->stChnAttr.stVencAttr.u32BufSize = ctx->u32Width * ctx->u32Height * 3 / 2;
	}

	s32Ret = RK_MPI_VENC_CreateChn(ctx->s32ChnId, &ctx->stChnAttr);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("RK_MPI_VENC_CreateChn failed %x", s32Ret);
		return s32Ret;
	}
	if (ctx->enable_buf_share) {
		VENC_CHN_REF_BUF_SHARE_S stVencChnRefBufShare;
		memset(&stVencChnRefBufShare, 0, sizeof(VENC_CHN_REF_BUF_SHARE_S));
		stVencChnRefBufShare.bEnable = 1;
		s32Ret = RK_MPI_VENC_SetChnRefBufShareAttr(ctx->s32ChnId, &stVencChnRefBufShare);
		if (s32Ret != RK_SUCCESS) {
			printf("RK_MPI_VENC_SetChnRefBufShareAttr failed,ret=%#x\n", s32Ret);
		}
	}

	if (ctx->bWrapIfEnable) {
		memset(&stVencChnBufWrap, 0, sizeof(stVencChnBufWrap));
		stVencChnBufWrap.bEnable = RK_TRUE;
		stVencChnBufWrap.u32BufLine = ctx->u32BufferLine;

		memset(&stVencChnRefBufShare, 0, sizeof(VENC_CHN_REF_BUF_SHARE_S));
		stVencChnRefBufShare.bEnable = RK_TRUE;

		s32Ret = RK_MPI_VENC_SetChnBufWrapAttr(ctx->s32ChnId, &stVencChnBufWrap);
		if (s32Ret != RK_SUCCESS) {
			RK_LOGE("RK_MPI_VENC_SetChnBufWrapAttr failure:%X", s32Ret);
			return RK_FAILURE;
		}
		RK_MPI_VENC_SetChnRefBufShareAttr(ctx->s32ChnId, &stVencChnRefBufShare);
	}
	// FIXME: Add support for RV1126
#if defined(RV1106) || defined(RV1106B)
	// VBR: default enable svc
	if (ctx->enRcMode == VENC_RC_MODE_H264VBR || ctx->enRcMode == VENC_RC_MODE_H265VBR) {
		SAMPLE_COMM_VENC_SET_ADVANCED_PARAMS(ctx->s32ChnId, ctx->enCodecType);
	}
#endif

	/* ifenable Svc */
	if (ctx->bSvcIfEnable) {
		s32Ret = RK_MPI_VENC_EnableSvc(ctx->s32ChnId, ctx->bSvcIfEnable);
		if (s32Ret != RK_SUCCESS) {
			RK_LOGE("RK_MPI_VENC_EnableSvc failed %X", s32Ret);
			return s32Ret;
		}
	}
	/* ifenable Motion Deblur */
	if (ctx->bMotionDeblurIfEnable) {
		s32Ret =
		    RK_MPI_VENC_EnableMotionDeblur(ctx->s32ChnId, ctx->bMotionDeblurIfEnable);
		if (s32Ret != RK_SUCCESS) {
			RK_LOGE("RK_MPI_VENC_EnableSvc failed %X", s32Ret);
			return s32Ret;
		}
	}

	stRecvParam.s32RecvPicNum = -1;
	s32Ret = RK_MPI_VENC_StartRecvFrame(ctx->s32ChnId, &stRecvParam);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("create %d ch venc failed", ctx->s32ChnId);
		return s32Ret;
	}

	if (ctx->stChnAttr.stVencAttr.enType == RK_VIDEO_ID_JPEG) {
		VENC_JPEG_PARAM_S stJpegParam;
		memset(&stJpegParam, 0, sizeof(VENC_JPEG_PARAM_S));
		stJpegParam.u32Qfactor = ctx->u32Qfactor;
		s32Ret = RK_MPI_VENC_SetJpegParam(ctx->s32ChnId, &stJpegParam);
		if (s32Ret != RK_SUCCESS) {
			RK_LOGE("RK_MPI_VENC_SetJpegParam failure:%X", s32Ret);
			return s32Ret;
		}
	} else if (ctx->stChnAttr.stVencAttr.enType == RK_VIDEO_ID_MJPEG) {
		VENC_MJPEG_PARAM_S stMjpegParam;
		memset(&stMjpegParam, 0, sizeof(stMjpegParam));
		memset(&stMjpegParam.u8YQt, 1, sizeof(stMjpegParam.u8YQt));
		memset(&stMjpegParam.u8CbQt, 1, sizeof(stMjpegParam.u8CbQt));
		memset(&stMjpegParam.u8CrQt, 1, sizeof(stMjpegParam.u8CrQt));
		stMjpegParam.u32MCUPerECS = 100;
		RK_MPI_VENC_SetMjpegParam(ctx->s32ChnId, &stMjpegParam);
	}

	if (ctx->bComboIfEnable) {
		VENC_COMBO_ATTR_S stComboAttr;
		memset(&stComboAttr, 0, sizeof(VENC_COMBO_ATTR_S));
		stComboAttr.bEnable = RK_TRUE;
		stComboAttr.s32ChnId = ctx->u32ComboChnId;
		RK_MPI_VENC_SetComboAttr(ctx->s32ChnId, &stComboAttr);
	}

	if (ctx->getStreamCbFunc) {
		ctx->stFrame.pstPack = (VENC_PACK_S *)(malloc(sizeof(VENC_PACK_S)));
		pthread_create(&ctx->getStreamThread, 0, ctx->getStreamCbFunc, (void *)(ctx));
	}

	return RK_SUCCESS;
}

RK_S32 SAMPLE_COMM_VENC_SendStream(SAMPLE_VENC_CTX_S *ctx, void *pdata, RK_S32 width,
                                   RK_S32 height, RK_S32 size,
                                   COMPRESS_MODE_E enCompressMode) {
	RK_S32 s32Ret = RK_FAILURE;
	MB_BLK blk = RK_NULL;
	RK_U8 *pVirAddr = RK_NULL;
	RK_S32 s32ReachEOS = 0;
	VIDEO_FRAME_INFO_S stFrame;

__RETRY0:
	blk = RK_MPI_MB_GetMB(ctx->pool, size, RK_TRUE);
	if (RK_NULL == blk) {
		RK_LOGE("RK_MPI_MB_GetMB fail %x", blk);
		usleep(2000llu);
		goto __RETRY0;
	}

	pVirAddr = (RK_U8 *)(RK_MPI_MB_Handle2VirAddr(blk));

	pVirAddr = pdata;

	RK_MPI_SYS_MmzFlushCache(blk, RK_FALSE);

	stFrame.stVFrame.pMbBlk = blk;
	stFrame.stVFrame.u32Width = width;
	stFrame.stVFrame.u32Height = height;
	stFrame.stVFrame.u32VirWidth = width;
	stFrame.stVFrame.u32VirHeight = height;
	stFrame.stVFrame.enPixelFormat = RK_FMT_YUV420SP;
	stFrame.stVFrame.u32FrameFlag |= s32ReachEOS ? FRAME_FLAG_SNAP_END : 0;
	stFrame.stVFrame.enCompressMode = enCompressMode;

__RETRY1:
	s32Ret = RK_MPI_VENC_SendFrame(ctx->s32ChnId, &stFrame, -1);
	if (s32Ret == RK_SUCCESS) {
		RK_MPI_MB_ReleaseMB(blk);
	} else {
		RK_LOGE("RK_MPI_VENC_SendFrame fail %x", s32Ret);
		usleep(10000llu);
		goto __RETRY1;
	}

	return s32Ret;
}

RK_S32 SAMPLE_COMM_VENC_GetStream(SAMPLE_VENC_CTX_S *ctx, void **pdata) {
	RK_S32 s32Ret = RK_FAILURE;

	s32Ret = RK_MPI_VENC_GetStream(ctx->s32ChnId, &ctx->stFrame, 2000);
	if (s32Ret == RK_SUCCESS) {
		*pdata = RK_MPI_MB_Handle2VirAddr(ctx->stFrame.pstPack->pMbBlk);
	} else {
		RK_LOGE("RK_MPI_VENC_GetStream chnid:%d fail %#X", ctx->s32ChnId, s32Ret);
	}

	return s32Ret;
}

RK_S32 SAMPLE_COMM_VENC_ReleaseStream(SAMPLE_VENC_CTX_S *ctx) {
	RK_S32 s32Ret = RK_FAILURE;

	s32Ret = RK_MPI_VENC_ReleaseStream(ctx->s32ChnId, &ctx->stFrame);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("RK_MPI_VENC_ReleaseStream fail %x", s32Ret);
	}

	return s32Ret;
}

RK_S32 SAMPLE_COMM_VENC_DestroyChn(SAMPLE_VENC_CTX_S *ctx) {
	RK_S32 s32Ret = RK_FAILURE;

	s32Ret = RK_MPI_VENC_StopRecvFrame(ctx->s32ChnId);
	if (s32Ret != RK_SUCCESS) {
		return s32Ret;
	}
	RK_LOGE("destroy enc chn:%d", ctx->s32ChnId);
	s32Ret = RK_MPI_VENC_DestroyChn(ctx->s32ChnId);
	if (s32Ret != RK_SUCCESS) {
		RK_LOGE("RK_MPI_VDEC_DestroyChn fail %x", s32Ret);
	}

	if (ctx->stFrame.pstPack) {
		free(ctx->stFrame.pstPack);
	}

	return RK_SUCCESS;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
