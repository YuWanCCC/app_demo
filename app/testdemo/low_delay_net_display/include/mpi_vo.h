/*******************************************************************************
****文件路径         : \low_delay_net_display\api\mpi_vo.h
****作者名称         : error: git config user.name & please set dead value or install git
****文件版本         : V1.0.0
****创建日期         : 2023-03-06 11:10:39
****简要说明         :
****
****版权信息         : 2023 by ${git_name}, All Rights Reserved.
********************************************************************************/

#ifndef INCLUDE_MPI_VO_H_
#define INCLUDE_MPI_VO_H_
#define RK3588_VO_DEV_HD0          0
#define RK3588_VO_DEV_HD1          1
#define RK3588_VO_DEV_HD2          2
#define RK3588_VO_DEV_HD3          3
#define  VO_LAYER_CLUSTER0         0
#define  VO_LAYER_CLUSTER1         1
#define  VO_LAYER_CLUSTER2         2
#define  VO_LAYER_CLUSTER3         3
#define  VO_LAYER_ESMART0          4
#define  VO_LAYER_ESMART1          5
#define  VO_LAYER_ESMART2          6
#define  VO_LAYER_ESMART3          7
int init_vo(int vir_w, int vir_h, int w, int h, int format, int intf_type, int vo_dev, int vcnt_line);

int deinit_vo();

int sendframe_vo(void *obj);

int init_MB(int fd, int len);

int sendframe_vo_for_fd(int fd, int len);

void pushFrame(void *obj);

void vsync_callback();

void switch_vsync(int status);

RK_U32  get_vcnt_time();


#endif
