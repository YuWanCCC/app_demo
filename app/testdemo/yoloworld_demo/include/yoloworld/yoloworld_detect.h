#pragma once

#include "FeatureTensor_rknn_c.h"
#include "YOLOWorldDetector.h"
#include "easy_timer.h"
#include "tracker_c_wrapper.h"
#include "model.h"
extern "C" {
#include "cJSON.h"
}

int yoloworld_detect_init(const char *cfg_file, det_yolo_world_t **detector,
                          TrackerC **tracker,
                          rknn_app_context_t **rknn_reid_ctx,
                          int &tracker_det_id);
