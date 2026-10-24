#ifndef __RK_DMA_HEAP_H__
#define __RK_DMA_HEAP_H__

#include <stdint.h>
#include <stddef.h>

#ifdef  __cplusplus
extern "C"{
#endif

typedef struct rkdma_buf_t {
	int size;
	int fd;
	void *vir_addr;
} rkdma_buf;

int rk_dma_heap_alloc(int size, int *buf_fd, void **buf_virt_addr);

int rk_dma_heap_release(int buf_fd, void *buf_virt_addr, size_t size);

int rk_dma_heap_sync_start(int buf_fd, void **sync);

int rk_dma_heap_sync_end(int buf_fd, void *sync);

#ifdef  __cplusplus
}
#endif /* end of __cplusplus */

#endif /* end of #ifndef __RK_DMA_HEAP_H__ */