// tracker_c_wrapper.h
#ifndef TRACKER_C_WRAPPER_H
#define TRACKER_C_WRAPPER_H

#include "common.h"
#ifdef __cplusplus
extern "C" {
#endif

// Opaque struct type for C
typedef struct TrackerC TrackerC;

typedef struct {
    int track_id;
    int tlwh[4];  // top-left-x, top-left-y, width, height
} TrackResult;

typedef struct {
    TrackResult* results;
    int size;
} TrackResultArray;

TrackResultArray tracker_get_results(TrackerC* tracker);

void tracker_free_results(TrackResultArray result_array);

// Create and destroy functions
TrackerC* tracker_create(float max_cosine_distance, int nn_budget);
void tracker_destroy(TrackerC* tracker);

// Expose main methods
void tracker_predict(TrackerC* tracker);
void tracker_update(TrackerC* tracker, const void* detections); // You will define how to pass `DETECTIONS` in C

// int tracker_is_confirmed(TrackerC* tracker);
// int tracker_time_since_update(TrackerC* tracker);

int deepsort_tracker(rknn_app_context_t *rknn_reid_ctx, image_buffer_t *src_image,
                   object_detect_result_list *od_results,
                   TrackerC *tracker,int tracker_det_id);

int deepsort_tracker_npu(rknn_app_context_t *rknn_reid_ctx, image_buffer_t *src_image,
                   object_detect_result_list *od_results,
                   int tracker_det_id, void *detections);

int deepsort_tracker_cpu(TrackerC *tracker, void *detections);
void tracker_get_version();

#ifdef __cplusplus
}
#endif

#endif // TRACKER_C_WRAPPER_H
