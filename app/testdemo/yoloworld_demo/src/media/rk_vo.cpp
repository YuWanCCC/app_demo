#include "rk_vo.h"

#include <cstdio>
#include <cstring>
#include <unistd.h>

static int g_vo_dev_id = 0;
static int g_vo_layer_id = 1;

int rk_vo_init(int width, int height) {
    int ret = 0;
	// VO init
	VO_PUB_ATTR_S VoPubAttr;
	VO_VIDEO_LAYER_ATTR_S stLayerAttr;
	VO_CSC_S VideoCSC;
	VO_CHN_ATTR_S VoChnAttr;
	RK_U32 u32DispBufLen;
	memset(&VoPubAttr, 0, sizeof(VO_PUB_ATTR_S));
	memset(&stLayerAttr, 0, sizeof(VO_VIDEO_LAYER_ATTR_S));
	memset(&VideoCSC, 0, sizeof(VO_CSC_S));
	memset(&VoChnAttr, 0, sizeof(VoChnAttr));

	VoPubAttr.enIntfType = VO_INTF_MIPI;
	VoPubAttr.enIntfSync = VO_OUTPUT_DEFAULT;

	ret = RK_MPI_VO_SetPubAttr(g_vo_dev_id, &VoPubAttr);
	if (ret != RK_SUCCESS) {
		printf("RK_MPI_VO_SetPubAttr %x\n", ret);
		return ret;
	}
	printf("RK_MPI_VO_SetPubAttr success\n");

	ret = RK_MPI_VO_Enable(g_vo_dev_id);
	if (ret != RK_SUCCESS) {
		printf("RK_MPI_VO_Enable err is %x\n", ret);
		return ret;
	}
	printf("RK_MPI_VO_Enable success\n");

	ret = RK_MPI_VO_GetLayerDispBufLen(g_vo_layer_id, &u32DispBufLen);
	if (ret != RK_SUCCESS) {
		printf("Get display buf len failed with error code %d!\n", ret);
		return ret;
	}
	printf("Get g_vo_layer_id %d disp buf len is %d.\n", g_vo_layer_id, u32DispBufLen);
	u32DispBufLen = 2;
	ret = RK_MPI_VO_SetLayerDispBufLen(g_vo_layer_id, u32DispBufLen);
	if (ret != RK_SUCCESS) {
		return ret;
	}
	printf("Agin Get g_vo_layer_id %d disp buf len is %d.\n", g_vo_layer_id, u32DispBufLen);

	/* get vo attribute*/
	ret = RK_MPI_VO_GetPubAttr(g_vo_dev_id, &VoPubAttr);
	if (ret) {
		printf("RK_MPI_VO_GetPubAttr fail!\n");
		return ret;
	}
	printf("RK_MPI_VO_GetPubAttr success\n");
	if ((VoPubAttr.stSyncInfo.u16Hact == 0) || (VoPubAttr.stSyncInfo.u16Vact == 0)) {
		VoPubAttr.stSyncInfo.u16Hact = height;
		VoPubAttr.stSyncInfo.u16Vact = width;
	}

	stLayerAttr.stDispRect.s32X = 0;
	stLayerAttr.stDispRect.s32Y = 0;
	stLayerAttr.stDispRect.u32Width = VoPubAttr.stSyncInfo.u16Hact;
	stLayerAttr.stDispRect.u32Height = VoPubAttr.stSyncInfo.u16Vact;
	stLayerAttr.stImageSize.u32Width = VoPubAttr.stSyncInfo.u16Hact;
	stLayerAttr.stImageSize.u32Height = VoPubAttr.stSyncInfo.u16Vact;
	printf("stLayerAttr W=%d, H=%d, format is %d\n", stLayerAttr.stDispRect.u32Width,
	         stLayerAttr.stDispRect.u32Height, stLayerAttr.enPixFormat);

	stLayerAttr.u32DispFrmRt = 30;
	stLayerAttr.enPixFormat = RK_FMT_RGB888;
	VideoCSC.enCscMatrix = VO_CSC_MATRIX_IDENTITY;
	VideoCSC.u32Contrast = 50;
	VideoCSC.u32Hue = 50;
	VideoCSC.u32Luma = 50;
	VideoCSC.u32Satuature = 50;
	RK_S32 u32VoChn = 0;

	/*bind layer0 to device hd0*/
	ret = RK_MPI_VO_BindLayer(g_vo_layer_id, g_vo_dev_id, VO_LAYER_MODE_GRAPHIC);
	if (ret != RK_SUCCESS) {
		printf("RK_MPI_VO_BindLayer g_vo_layer_id = %d error\n", g_vo_layer_id);
		return ret;
	}
	printf("RK_MPI_VO_BindLayer success\n");

	ret = RK_MPI_VO_SetLayerAttr(g_vo_layer_id, &stLayerAttr);
	if (ret != RK_SUCCESS) {
		printf("RK_MPI_VO_SetLayerAttr g_vo_layer_id = %d error\n", g_vo_layer_id);
		return ret;
	}
	printf("RK_MPI_VO_SetLayerAttr success\n");

	ret = RK_MPI_VO_SetLayerSpliceMode(g_vo_layer_id, VO_SPLICE_MODE_RGA);
	if (ret != RK_SUCCESS) {
		printf("RK_MPI_VO_SetLayerSpliceMode g_vo_layer_id = %d error\n", g_vo_layer_id);
		return ret;
	}
	printf("RK_MPI_VO_SetLayerSpliceMode success\n");

	ret = RK_MPI_VO_EnableLayer(g_vo_layer_id);
	if (ret != RK_SUCCESS) {
		printf("RK_MPI_VO_EnableLayer g_vo_layer_id = %d error\n", g_vo_layer_id);
		return ret;
	}
	printf("RK_MPI_VO_EnableLayer success\n");

	ret = RK_MPI_VO_SetLayerCSC(g_vo_layer_id, &VideoCSC);
	if (ret != RK_SUCCESS) {
		printf("RK_MPI_VO_SetLayerCSC error\n");
		return ret;
	}
	printf("RK_MPI_VO_SetLayerCSC success\n");

	VoChnAttr.bDeflicker = RK_FALSE;
	VoChnAttr.u32Priority = 0;
	VoChnAttr.stRect.s32X = 0;
	VoChnAttr.stRect.s32Y = 0;
	VoChnAttr.stRect.u32Width = stLayerAttr.stDispRect.u32Width;
	VoChnAttr.stRect.u32Height = stLayerAttr.stDispRect.u32Height;
	printf("VoChnAttr.stRect.u32Width %d, VoChnAttr.stRect.u32Height %d\n", VoChnAttr.stRect.u32Width, VoChnAttr.stRect.u32Height);
	if (VoPubAttr.enIntfType == VO_INTF_MIPI)
		VoChnAttr.enRotation = ROTATION_90;
	ret = RK_MPI_VO_SetChnAttr(g_vo_layer_id, 0, &VoChnAttr);

	ret = RK_MPI_VO_EnableChn(g_vo_layer_id, u32VoChn);
	if (ret != RK_SUCCESS) {
		printf("create %d layer %d ch vo failed!\n", g_vo_layer_id, u32VoChn);
		return ret;
	}
	printf("RK_MPI_VO_EnableChn success\n");
    return ret;
}

int rk_vo_deinit() {
    int ret = 0;
    int u32VoChn = 0;
	ret = RK_MPI_VO_DisableChn(g_vo_layer_id, u32VoChn);
	if (ret) {
		printf("RK_MPI_VO_DisableChn failed, ret is %#x\n", ret);
		return -1;
	}
	ret = RK_MPI_VO_DisableLayer(g_vo_layer_id);
	if (ret) {
		printf("RK_MPI_VO_DisableLayer failed, ret is %#x\n", ret);
		return -1;
	}
	ret = RK_MPI_VO_Disable(g_vo_dev_id);
	if (ret) {
		printf("RK_MPI_VO_Disable failed, ret is %#x\n", ret);
		return -1;
	}
	ret = RK_MPI_VO_UnBindLayer(g_vo_layer_id, g_vo_dev_id);
	if (ret) {
		printf("RK_MPI_VO_UnBindLayer failed, ret is %#x\n", ret);
		return -1;
	}
	ret = RK_MPI_VO_CloseFd();
	if (ret) {
		printf("RK_MPI_VO_CloseFd failed, ret is %#x\n", ret);
		return -1;
	}
    return ret;
}