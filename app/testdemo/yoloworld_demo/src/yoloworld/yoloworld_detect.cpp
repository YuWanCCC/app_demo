#include "yoloworld_detect.h"

typedef struct {
  char classes_file[128];
  char clip_text_model[128];
  char yolo_world_model[128];
  char ckpt_model[128];
  char tracker_det_name[128];
  float max_cosine_dist;
  int nn_budget;
  float nms_threshold;
  float confidence_threshold;
} Config;

static int load_config(const char *filename, Config *cfg) {
  FILE *file = fopen(filename, "rb");
  if (!file) {
    perror("Cannot open config file");
    return -1;
  }

  fseek(file, 0, SEEK_END);
  long length = ftell(file);
  rewind(file);

  char *data = (char *)malloc(length + 1);
  fread(data, 1, length, file);
  data[length] = '\0';
  fclose(file);

  cJSON *json = cJSON_Parse(data);
  if (!json) {
    printf("Error parsing JSON: %s\n", cJSON_GetErrorPtr());
    free(data);
    return -1;
  }

  cJSON *classes_file = cJSON_GetObjectItem(json, "classes_file");
  if (cJSON_IsString(classes_file)) {
    strncpy(cfg->classes_file, classes_file->valuestring,
            sizeof(cfg->classes_file) - 1);
  }
  cJSON *clip_text_model = cJSON_GetObjectItem(json, "clip_text_model");
  if (cJSON_IsString(clip_text_model)) {
    strncpy(cfg->clip_text_model, clip_text_model->valuestring,
            sizeof(cfg->clip_text_model) - 1);
  }
  cJSON *yolo_world_model = cJSON_GetObjectItem(json, "yolo_world_model");
  if (cJSON_IsString(yolo_world_model)) {
    strncpy(cfg->yolo_world_model, yolo_world_model->valuestring,
            sizeof(cfg->yolo_world_model) - 1);
  }
  cJSON *ckpt_model = cJSON_GetObjectItem(json, "ckpt_model");
  if (cJSON_IsString(ckpt_model)) {
    strncpy(cfg->ckpt_model, ckpt_model->valuestring,
            sizeof(cfg->ckpt_model) - 1);
  }
  cJSON *max_cosine_dist = cJSON_GetObjectItem(json, "max_cosine_distance");
  if (cJSON_IsNumber(max_cosine_dist)) {
    cfg->max_cosine_dist = (float)(max_cosine_dist->valuedouble);
  }

  cJSON *nn_budget = cJSON_GetObjectItem(json, "nn_budget");
  if (cJSON_IsNumber(nn_budget)) {
    cfg->nn_budget = nn_budget->valueint;
  }

  cJSON *nms_threshold = cJSON_GetObjectItem(json, "nms_threshold");
  if (cJSON_IsNumber(nms_threshold)) {
    cfg->nms_threshold = (float)nms_threshold->valuedouble;
  }
  cJSON *confidence_threshold =
      cJSON_GetObjectItem(json, "confidence_threshold");
  if (cJSON_IsNumber(confidence_threshold)) {
    cfg->confidence_threshold = (float)confidence_threshold->valuedouble;
  }
  cJSON *tracker_det_name = cJSON_GetObjectItem(json, "tracker_det_name");
  if (cJSON_IsString(tracker_det_name)) {
    strncpy(cfg->tracker_det_name, tracker_det_name->valuestring,
            sizeof(cfg->tracker_det_name) - 1);
  }

  cJSON_Delete(json);
  free(data);
  return 0;
}

int yoloworld_detect_init(const char *cfg_file, det_yolo_world_t **detector,
                          TrackerC **tracker,
                          rknn_app_context_t **rknn_reid_ctx,
                          int &tracker_det_id) {
  Config config;
  if (load_config(cfg_file, &config) == 0) {
    printf("Config Loaded:\n");
    printf("classes_file: %s\n", config.classes_file);
    printf("clip_text_model: %s\n", config.clip_text_model);
    printf("yolo_world_model: %s\n", config.yolo_world_model);
    printf("ckpt_model: %s\n", config.ckpt_model);
    printf("max_cosine_distance: %f\n", config.max_cosine_dist);
    printf("nn_budget: %d\n", config.nn_budget);
    printf("nms_threshold: %f\n", config.nms_threshold);
    printf("confidence_threshold: %f\n", config.confidence_threshold);
    printf("tracker_det_name: %s\n", config.tracker_det_name);
  } else {
    printf("Failed to load config.\n");
    return -1;
  }
  // 获取检测器版本号
  detector_get_version();
  // 获取跟踪器版本号
  tracker_get_version();

  *detector = detector_init(config.clip_text_model, config.yolo_world_model,
                            config.classes_file, config.nms_threshold,
                            config.confidence_threshold);
  if (!detector) {
    printf("detector init fail.\n");
    return -1;
  }

  // 初始化跟踪器
  *tracker = tracker_create(config.max_cosine_dist, config.nn_budget);
  if (tracker == NULL) {
    printf("tracker init failed.\n");
    return -1;
  }

  *rknn_reid_ctx = init_feature(config.ckpt_model);
  if (rknn_reid_ctx == NULL) {
    printf("feature init fail\n");
    return -1;
  }

  for (int i = 0; i < (*detector)->classes_size_; i++) {
    if (strcmp((*detector)->classes_[i], config.tracker_det_name) == 0) {
      tracker_det_id = i;
      printf("tracker_det_id: %d: tracker_det_name: %s\n", i,
             (*detector)->classes_[i]);
    }
  }
  if (tracker_det_id < 0 || tracker_det_id > (*detector)->classes_size_) {
    tracker_det_id = -100;
    printf("no tracker class\n");
  }
  return 0;
}