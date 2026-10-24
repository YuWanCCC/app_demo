
// #include "model.h"
#include "dataType.h"
#include <chrono>
#include <cmath>
#include <exception>
#include <fstream>
#include <iostream>
#include <limits>
#include <numeric>
#include <string>
#include <vector>
#include <stdexcept> 
#include "common.h"

typedef unsigned char uint8;

rknn_app_context_t* init_feature(const char *model_path);
int get_rects_feature(rknn_app_context_t *rknn_reid_ctx,image_buffer_t *image ,void *detections);
void deinit_feature(rknn_app_context_t* rknn_reid_ctx);



