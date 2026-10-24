#pragma once

// #include "opencv2/opencv.hpp"
// #include "opencv2/imgproc.hpp"
#include <fstream>
#include <sstream>
#include <cstring>

#include "common.h"

#if 0
class detect_result
{
public:
    int classId;
    float confidence;
    cv::Rect_<float> box;
};
#endif

typedef struct{
    rknn_app_context_t* rknn_clip_ctx;
    rknn_app_context_t* rknn_yolo_world_ctx;
    det_dma_buf* dma_buf;
    clip_float16 *clip_text_;
    int clip_text_size_;
    float confidence_threshold_;
    float nms_threshold_;
    char** classes_;
    int classes_size_;

}det_yolo_world_t;

void detector_get_version() ;

det_yolo_world_t *detector_init(const char *clip_model_path, 
                                const char *yolo_model_path,
                                const char *class_file,
                                float nms_threshold,
                                float confidence_threshold);

int detector_yolo_world_npu(det_yolo_world_t *detector, image_buffer_t *src_image, npu_object_detect_result *od_results_npu);
int detector_yolo_world_cpu(npu_object_detect_result *od_results_npu,
                                  const float nms_threshold,
                                  const float box_conf_threshold,
                                  object_detect_result_list *od_results);

int detector_yolo_world(det_yolo_world_t *detector, image_buffer_t *src_image, object_detect_result_list *od_results);
void detector_deinit(det_yolo_world_t *detector);
// void detector_draw_frame(det_yolo_world_t *detector,cv::Mat & frame, std::vector<detect_result> &results);


int detector_clip_text_add(det_yolo_world_t *detector, char *new_class);
int detector_clip_text_delete(det_yolo_world_t *detector, char *delete_class);


// typedef struct {
//     rknn_app_context_t *ctx;
//     image_buffer_t input_img;
//     clip_float16 *text_input;
//     object_detect_result_list *od_results;
// } inference_param_t;

// void* inference_thread(void* arg);

// class YOLOWorldDetector
// {
// public:
//     int init(std::string clip_model_path,std::string yolo_model_path);
//     void deinit();
//     void detect(cv::Mat & frame, std::vector<detect_result> &result);
//     int ClipText();
//     std::vector<std::string> classes_;
//     void draw_frame(cv::Mat & frame, std::vector<detect_result> &results);
// private:

//     char **clip_inputs;
//     float *clip_text_;
//     int clip_text_size_ = 0;
//     // cv::dnn::Net net;
//     // rknn_clip_context rknn_clip_ctx;
//     rknn_app_context_t rknn_yolo_world_ctx;
//     const float confidence_threshold_ =0.25f;
//     const float nms_threshold_ = 0.4f;

//     const int model_input_width_ = 640;
//     const int model_input_height_ = 640;

// };
