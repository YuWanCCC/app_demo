
#ifndef INCLUDE_MPI_VI_H_
#define INCLUDE_MPI_VI_H_
RK_S32 vi_init(int width, int height, int devId, const char *entityName, int format);
RK_S32 get_frame();
RK_S32 deinit_vi();

#endif