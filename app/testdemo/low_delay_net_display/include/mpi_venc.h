/*******************************************************************************
****文件路径         : \low_delay_net_display\api\mpi_venc.h
****作者名称         : error: git config user.name & please set dead value or install git
****文件版本         : V1.0.0
****创建日期         : 2023-03-06 15:34:52
****简要说明         :
****
****版权信息         : 2023 by ${git_name}, All Rights Reserved.
********************************************************************************/
#ifndef INCLUDE_MPI_VENC_H_
#define INCLUDE_MPI_VENC_H_

typedef void (*VENCDATA)(bool isKeyFrame, unsigned char *s, int len);

int init_venc(int width, int height, int vir_width, int vir_height, \
              int codecType, int bitrate, int format, VENCDATA func, \
              bool enSlice, int sliceMode, int sliceSize);

int set_frame(int bufferFd, int size);

int set_bitrate(int bitrate, int maxBitrate, int minBitrate);

void  keyframe_req();

void thread_get_stream(int flag);

int deinit_venc();


#endif