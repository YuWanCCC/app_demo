#ifndef INCLUDE_MPI_VDEC_H_
#define INCLUDE_MPI_VDEC_H_

typedef void (*VDECDATA)(int bufferFd);
typedef void (*VDECDATA2)(int vir_width, int vir_height, int width, int height, void *obj);
int init_vdec(VDECDATA func, VDECDATA2 func2, int width, int height, int enSlice);
int vdec_send_frame(unsigned char *data, int len, int slice_end);
void thread_vdec_get_frame(int flag);
int vdec_deinit();

#endif
