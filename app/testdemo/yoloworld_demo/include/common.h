#ifndef _RKNN_MODEL_ZOO_COMMON_H_
#define _RKNN_MODEL_ZOO_COMMON_H_

/**
 * @brief Image pixel format
 *
 */

#include "rknn_api.h"
typedef enum {
    IMAGE_FORMAT_GRAY8,
    IMAGE_FORMAT_RGB888,
    IMAGE_FORMAT_RGBA8888,
    IMAGE_FORMAT_YUV420SP_NV21,
    IMAGE_FORMAT_YUV420SP_NV12,
} image_format_t;

/**
 * @brief Image buffer
 *
 */
typedef struct {
    int width;
    int height;
    int width_stride;
    int height_stride;
    image_format_t format;
    unsigned char *virt_addr;
    int size;
    int fd;
} image_buffer_t;

/**
 * @brief Image rectangle
 *
 */
typedef struct {
    int x;
    int y;
    int width;
    int height;
} image_rect_t;

/**
 * @brief Image obb rectangle
 *
 */
typedef struct {
    int x;
    int y;
    int w;
    int h;
    float angle;
} image_obb_box_t;

typedef struct {
    char *dma_buf_virt_addr;
    int dma_buf_fd;
    int size;
} det_dma_buf;

typedef struct {
    rknn_context rknn_ctx;
    rknn_input_output_num io_num;
    rknn_tensor_attr *input_attrs;
    rknn_tensor_attr *output_attrs;

    int model_channel;
    int model_width;
    int model_height;
    bool is_quant;

    rknn_tensor_mem *input_mems[2];
    rknn_tensor_mem *output_mems[6];
    rknn_tensor_attr *input_native_attrs;
    rknn_tensor_attr *output_native_attrs;

    void **output_buf;
    uint32_t *output_buf_size;
} rknn_app_context_t;

#define OBJ_NUMB_MAX_SIZE 128
#define OBJ_CLASS_NUM 80

typedef struct {
    image_rect_t box;
    float prop;
    int cls_id;
} object_detect_result;

/**
 * @brief LetterBox
 * 
 */
typedef struct {
    int x_pad;
    int y_pad;
    float scale;
} letterbox_t;

typedef struct {
    int id;
    int count;
    object_detect_result results[OBJ_NUMB_MAX_SIZE];
} object_detect_result_list;


typedef struct {
    uint32_t n_output;
    uint32_t is_quant;
    int32_t model_width;
    int32_t model_height;
    rknn_tensor_attr output_attrs[6];
    rknn_output outputs[6];
    rknn_tensor_attr output_native_attrs[6];
    letterbox_t letterbox;

} npu_object_detect_result;


typedef uint16_t clip_float16;

#endif  //_RKNN_MODEL_ZOO_COMMON_H_
