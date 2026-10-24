#include "common.h"

#include "media_init.h"
#include "sys.h"
#include "ui_detect_type.h"
#include "yoloworld_detect.h"

#include <mutex>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define DETECTOR_INIT_PATH "/oem/usr/share/models/config.json"
// #define SAVE_FILE_DEBUG
#define DETECT_PROP_THRESHOLD 0.45

#define USER_DETECT_PATH "/userdata/detect_classes.txt"
#define USER_NO_DETECT_PATH "/userdata/no_detect_classes.txt"
#define OEM_DETECT_PATH "/oem/usr/share/models/detect_classes.txt"
#define OEM_NO_DETECT_PATH "/oem/usr/share/models/no_detect_classes.txt"

// 坐标转换函数：将640x640的坐标转换为1920x1080的实际分辨率坐标
static void convert_coordinates_640_to_1920x1080(int *x, int *y, int *width,
                                                 int *height) {
  // 计算缩放比例
  float scale_x = 1920.0f / 640.0f; // 3.0
  float scale_y = 1080.0f / 640.0f; // 1.6875

  // 转换坐标
  *x = (int)(*x * scale_x);
  *y = (int)(*y * scale_y);
  *width = (int)(*width * scale_x);
  *height = (int)(*height * scale_y);

  // 确保坐标不超出边界
  if (*x < 0)
    *x = 0;
  if (*y < 0)
    *y = 0;
  if (*x + *width > 1920)
    *width = 1920 - *x;
  if (*y + *height > 1080)
    *height = 1080 - *y;
}

// CPU 后处理任务节点
typedef struct CPUTaskNode {
  npu_object_detect_result od_results_npu;
  VIDEO_FRAME_INFO_S frame; // 使用值拷贝
  struct CPUTaskNode *next;
} CPU_TASK_NODE_S;

// CPU 结果队列节点（用于 tracker 处理）
typedef struct CPUTaskResultNode {
  object_detect_result_list od_results;
  VIDEO_FRAME_INFO_S frame; // 使用值拷贝
  struct CPUTaskResultNode *next;
} CPU_TASK_RESULT_NODE_S;

// CPU 任务队列
typedef struct {
  CPU_TASK_NODE_S *front;
  CPU_TASK_NODE_S *rear;
  pthread_mutex_t mutex;
} CPU_TASK_QUEUE_S;

typedef struct TrackerCPUTaskNode {
  DETECTIONS *detections;
  object_detect_result_list od_results;
  bool need_free_detections; // 标记是否需要释放 detections 内存
  struct TrackerCPUTaskNode *next;
} TRACKER_CPU_TASK_NODE_S;

typedef struct {
  TRACKER_CPU_TASK_NODE_S *front;
  TRACKER_CPU_TASK_NODE_S *rear;
  pthread_mutex_t mutex;
} TRACKER_CPU_TASK_QUEUE_S;

// CPU 结果队列
typedef struct {
  CPU_TASK_RESULT_NODE_S *front;
  CPU_TASK_RESULT_NODE_S *rear;
  pthread_mutex_t mutex;
} CPU_TASK_RESULT_QUEUE_S;

// 检测类型任务节点
typedef struct DetectTypeTaskNode {
  char classname[256];
  bool is_add; // true: 添加, false: 删除
  struct DetectTypeTaskNode *next;
} DETECT_TYPE_TASK_NODE_S;

// 检测类型任务队列
typedef struct {
  DETECT_TYPE_TASK_NODE_S *front;
  DETECT_TYPE_TASK_NODE_S *rear;
  pthread_mutex_t mutex;
} DETECT_TYPE_TASK_QUEUE_S;

static det_yolo_world_t *g_detector = NULL;
static TrackerC *g_tracker = NULL;
static rknn_app_context_t *g_rknn_reid_ctx = NULL;
static int g_tracker_det_id = -100;
static bool g_yoloworld_time_consume_debug = false;
static bool g_is_usbcam = false;
struct timespec g_last_results_time = {0, 0}, g_current_results_time = {0, 0};
struct timespec npu_start_time, npu_end_time, tracker_start_time,
    tracker_end_time, cpu_start_time, cpu_end_time, detections_start_time,
    detections_end_time, deepsort_tracker_npu_start, deepsort_tracker_npu_end,
    deepsort_tracker_cpu_start, deepsort_tracker_cpu_end;
static bool g_need_clear = false;
static bool g_need_update = false;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

// 任务队列全局变量
static CPU_TASK_QUEUE_S g_cpu_task_queue;
static TRACKER_CPU_TASK_QUEUE_S g_tracker_cpu_task_queue;
static CPU_TASK_RESULT_QUEUE_S g_cpu_result_queue;
static DETECT_TYPE_TASK_QUEUE_S g_detect_type_task_queue;
static pthread_t g_cpu_thread;
static pthread_t g_tracker_cpu_thread;
static bool g_quit_detect_threads = false;

// 深拷贝 DETECTIONS 的辅助函数
static DETECTIONS *deep_copy_detections(const DETECTIONS *src) {
  if (!src)
    return NULL;

  DETECTIONS *dst = new DETECTIONS();
  dst->reserve(src->size()); // 预分配空间以提高效率

  for (const auto &detection : *src) {
    DETECTION_ROW new_detection;
    new_detection.tlwh = detection.tlwh;
    new_detection.confidence = detection.confidence;
    new_detection.feature = detection.feature;
    dst->push_back(new_detection);
  }

  return dst;
}

// 释放深拷贝的 DETECTIONS
static void free_detections_copy(DETECTIONS *detections) {
  if (detections) {
    delete detections;
  }
}

static void sigterm_handler(int32_t sig) {
  if (SIGINT == sig || SIGTERM == sig)
    sys_ctl(SYSTEM_CTRL_QUIT);
}

// 初始化 CPU 任务队列
static void init_cpu_task_queue() {
  g_cpu_task_queue.front = g_cpu_task_queue.rear = NULL;
  pthread_mutex_init(&g_cpu_task_queue.mutex, NULL);
}

static void init_tracker_cpu_task_queue() {
  g_tracker_cpu_task_queue.front = g_tracker_cpu_task_queue.rear = NULL;
  pthread_mutex_init(&g_tracker_cpu_task_queue.mutex, NULL);
}

// 初始化 CPU 结果队列
static void init_cpu_result_queue() {
  g_cpu_result_queue.front = g_cpu_result_queue.rear = NULL;
  pthread_mutex_init(&g_cpu_result_queue.mutex, NULL);
}

// 初始化检测类型任务队列
static void init_detect_type_task_queue() {
  g_detect_type_task_queue.front = g_detect_type_task_queue.rear = NULL;
  pthread_mutex_init(&g_detect_type_task_queue.mutex, NULL);
}

// 添加 CPU 任务到队列
static void add_cpu_task(const npu_object_detect_result *od_results_npu,
                         const VIDEO_FRAME_INFO_S *frame) {
  CPU_TASK_NODE_S *newNode = (CPU_TASK_NODE_S *)malloc(sizeof(CPU_TASK_NODE_S));
  memcpy(&newNode->od_results_npu, od_results_npu,
         sizeof(npu_object_detect_result));
  memcpy(&newNode->frame, frame, sizeof(VIDEO_FRAME_INFO_S));
  newNode->next = NULL;

  pthread_mutex_lock(&g_cpu_task_queue.mutex);
  if (g_cpu_task_queue.rear == NULL) {
    g_cpu_task_queue.front = g_cpu_task_queue.rear = newNode;
  } else {
    g_cpu_task_queue.rear->next = newNode;
    g_cpu_task_queue.rear = newNode;
  }
  pthread_mutex_unlock(&g_cpu_task_queue.mutex);
}

static void add_tracker_cpu_task(const DETECTIONS *detections,
                                 const object_detect_result_list *od_results) {
  TRACKER_CPU_TASK_NODE_S *newNode =
      (TRACKER_CPU_TASK_NODE_S *)malloc(sizeof(TRACKER_CPU_TASK_NODE_S));

  // 打印添加到任务队列的detections信息
  printf("=== Adding to Task Queue ===\n");
  printf("detections pointer: %p\n", (void *)detections);
  if (detections) {
    printf("detections size: %zu\n", detections->size());
  } else {
    printf("detections is NULL\n");
  }
  printf("od_results count: %d\n", od_results->count);
  printf("=== End Adding to Task Queue ===\n");

  newNode->detections = (DETECTIONS *)detections; // 直接赋值指针
  newNode->need_free_detections = true;

  memcpy(&newNode->od_results, od_results, sizeof(object_detect_result_list));
  newNode->next = NULL;

  pthread_mutex_lock(&g_tracker_cpu_task_queue.mutex);
  if (g_tracker_cpu_task_queue.rear == NULL) {
    g_tracker_cpu_task_queue.front = g_tracker_cpu_task_queue.rear = newNode;
  } else {
    g_tracker_cpu_task_queue.rear->next = newNode;
    g_tracker_cpu_task_queue.rear = newNode;
  }
  pthread_mutex_unlock(&g_tracker_cpu_task_queue.mutex);
}

// 从 CPU 队列中取出任务
static CPU_TASK_NODE_S *pop_cpu_task() {
  pthread_mutex_lock(&g_cpu_task_queue.mutex);
  if (g_cpu_task_queue.front == NULL) {
    pthread_mutex_unlock(&g_cpu_task_queue.mutex);
    return NULL;
  }
  CPU_TASK_NODE_S *temp = g_cpu_task_queue.front;
  g_cpu_task_queue.front = g_cpu_task_queue.front->next;
  if (g_cpu_task_queue.front == NULL) {
    g_cpu_task_queue.rear = NULL;
  }
  pthread_mutex_unlock(&g_cpu_task_queue.mutex);
  return temp;
}

static TRACKER_CPU_TASK_NODE_S *pop_tracker_cpu_task() {
  pthread_mutex_lock(&g_tracker_cpu_task_queue.mutex);
  if (g_tracker_cpu_task_queue.front == NULL) {
    pthread_mutex_unlock(&g_tracker_cpu_task_queue.mutex);
    return NULL;
  }
  TRACKER_CPU_TASK_NODE_S *temp = g_tracker_cpu_task_queue.front;
  g_tracker_cpu_task_queue.front = g_tracker_cpu_task_queue.front->next;
  if (g_tracker_cpu_task_queue.front == NULL) {
    g_tracker_cpu_task_queue.rear = NULL;
  }
  pthread_mutex_unlock(&g_tracker_cpu_task_queue.mutex);
  return temp;
}

// 添加 CPU 结果到结果队列
static void add_cpu_result(const object_detect_result_list *od_results,
                           const VIDEO_FRAME_INFO_S *frame) {
  CPU_TASK_RESULT_NODE_S *newNode =
      (CPU_TASK_RESULT_NODE_S *)malloc(sizeof(CPU_TASK_RESULT_NODE_S));
  memcpy(&newNode->od_results, od_results, sizeof(object_detect_result_list));
  memcpy(&newNode->frame, frame, sizeof(VIDEO_FRAME_INFO_S));
  newNode->next = NULL;

  pthread_mutex_lock(&g_cpu_result_queue.mutex);
  if (g_cpu_result_queue.rear == NULL) {
    g_cpu_result_queue.front = g_cpu_result_queue.rear = newNode;
  } else {
    g_cpu_result_queue.rear->next = newNode;
    g_cpu_result_queue.rear = newNode;
  }
  pthread_mutex_unlock(&g_cpu_result_queue.mutex);
}

// 从 CPU 结果队列中取出结果
static CPU_TASK_RESULT_NODE_S *pop_cpu_result() {
  pthread_mutex_lock(&g_cpu_result_queue.mutex);
  if (g_cpu_result_queue.front == NULL) {
    pthread_mutex_unlock(&g_cpu_result_queue.mutex);
    return NULL;
  }
  CPU_TASK_RESULT_NODE_S *temp = g_cpu_result_queue.front;
  g_cpu_result_queue.front = g_cpu_result_queue.front->next;
  if (g_cpu_result_queue.front == NULL) {
    g_cpu_result_queue.rear = NULL;
  }
  pthread_mutex_unlock(&g_cpu_result_queue.mutex);
  return temp;
}

// 添加检测类型任务到队列
static void add_detect_type_task(const char *classname, bool is_add) {
  DETECT_TYPE_TASK_NODE_S *newNode =
      (DETECT_TYPE_TASK_NODE_S *)malloc(sizeof(DETECT_TYPE_TASK_NODE_S));
  strncpy(newNode->classname, classname, sizeof(newNode->classname) - 1);
  newNode->classname[sizeof(newNode->classname) - 1] = '\0';
  newNode->is_add = is_add;
  newNode->next = NULL;

  pthread_mutex_lock(&g_detect_type_task_queue.mutex);
  if (g_detect_type_task_queue.rear == NULL) {
    g_detect_type_task_queue.front = g_detect_type_task_queue.rear = newNode;
  } else {
    g_detect_type_task_queue.rear->next = newNode;
    g_detect_type_task_queue.rear = newNode;
  }
  pthread_mutex_unlock(&g_detect_type_task_queue.mutex);
}

// 从检测类型任务队列中取出任务
static DETECT_TYPE_TASK_NODE_S *pop_detect_type_task() {
  pthread_mutex_lock(&g_detect_type_task_queue.mutex);
  if (g_detect_type_task_queue.front == NULL) {
    pthread_mutex_unlock(&g_detect_type_task_queue.mutex);
    return NULL;
  }
  DETECT_TYPE_TASK_NODE_S *temp = g_detect_type_task_queue.front;
  g_detect_type_task_queue.front = g_detect_type_task_queue.front->next;
  if (g_detect_type_task_queue.front == NULL) {
    g_detect_type_task_queue.rear = NULL;
  }
  pthread_mutex_unlock(&g_detect_type_task_queue.mutex);
  return temp;
}

// 检查文件是否存在
static int file_exists(const char *path) { return access(path, F_OK) == 0; }

// 拷贝文件
static void copy_file(const char *src, const char *dst) {
  FILE *in = fopen(src, "r");
  FILE *out = fopen(dst, "w");
  if (!in || !out) {
    if (in)
      fclose(in);
    if (out)
      fclose(out);
    printf("Failed to copy %s to %s\n", src, dst);
    return;
  }
  char buf[256];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
    fwrite(buf, 1, n, out);
  }
  fclose(in);
  fclose(out);
}

static void detect_type_add_callback(void *ctx, char *classname) {
  printf("detect_type_add_callback: %s\n", classname);
  add_detect_type_task(classname, true);
}

static void detect_type_delete_callback(void *ctx, char *classname) {
  printf("detect_type_delete_callback: %s\n", classname);
  add_detect_type_task(classname, false);
}

#ifdef SAVE_FILE_DEBUG
static FILE *g_fp = NULL;
#endif
static unsigned char g_userdata[1024] = {0};
static int g_userdata_len = 0;
static void rgb888_buffer_cb(const VIDEO_FRAME_INFO_S &frame) {
  // 处理检测类型任务队列
  DETECT_TYPE_TASK_NODE_S *detect_task = pop_detect_type_task();
  while (detect_task != NULL) {
    if (detect_task->is_add) {
      detector_clip_text_add(g_detector, detect_task->classname);
      printf("Processed detect_type_add: %s\n", detect_task->classname);
    } else {
      detector_clip_text_delete(g_detector, detect_task->classname);
      printf("Processed detect_type_delete: %s\n", detect_task->classname);
    }
    free(detect_task);
    detect_task = pop_detect_type_task();
  }

  if (g_yoloworld_time_consume_debug) {
    clock_gettime(CLOCK_MONOTONIC, &g_current_results_time);
    if (g_last_results_time.tv_sec == 0) {
      g_last_results_time = g_current_results_time;
    } else {
      double elapsed =
          (g_current_results_time.tv_sec - g_last_results_time.tv_sec) * 1000 +
          (g_current_results_time.tv_nsec - g_last_results_time.tv_nsec) /
              1000000;
      printf("====== execution time diff: %.2f milliseconds======\n", elapsed);
      g_last_results_time = g_current_results_time;
    }
  }

  // 优先处理CPU结果
  CPU_TASK_RESULT_NODE_S *result = pop_cpu_result();
  if (result != NULL) {
    // 处理跟踪器
    image_buffer_t src_image = {0};
    src_image.width = result->frame.stVFrame.u32Width;
    src_image.height = result->frame.stVFrame.u32Height;
    src_image.width_stride = result->frame.stVFrame.u32Width;
    src_image.height_stride = result->frame.stVFrame.u32Height;
    src_image.format = IMAGE_FORMAT_YUV420SP_NV12;
    src_image.virt_addr = (unsigned char *)RK_MPI_MB_Handle2VirAddr(
        result->frame.stVFrame.pMbBlk);
    src_image.fd = RK_MPI_MB_Handle2Fd(result->frame.stVFrame.pMbBlk);
    src_image.size = result->frame.stVFrame.u32Width *
                     result->frame.stVFrame.u32Height * 3 / 2;

    if (g_yoloworld_time_consume_debug) {
      clock_gettime(CLOCK_MONOTONIC, &tracker_start_time);
    }

    DETECTIONS *detections = new DETECTIONS();

    // 记录detections处理开始时间

    if (g_yoloworld_time_consume_debug) {
      clock_gettime(CLOCK_MONOTONIC, &deepsort_tracker_npu_start);
    }
    int ret =
        deepsort_tracker_npu(g_rknn_reid_ctx, &src_image, &result->od_results,
                             g_tracker_det_id, detections);
    if (g_yoloworld_time_consume_debug) {
      clock_gettime(CLOCK_MONOTONIC, &deepsort_tracker_npu_end);
      double detections_elapsed = (deepsort_tracker_npu_end.tv_sec -
                                   deepsort_tracker_npu_start.tv_sec) *
                                      1000 +
                                  (deepsort_tracker_npu_end.tv_nsec -
                                   deepsort_tracker_npu_start.tv_nsec) /
                                      1000000;
      printf(
          "deepsort_tracker_npu processing execution time: %.2f milliseconds, "
          "found %zu detections\n",
          detections_elapsed, detections->size());
    }

    // 打印detections中的值
    // printf("=== DETECTIONS Debug Info ===\n");
    // printf("detections size: %zu\n", detections->size());
    // for (size_t i = 0; i < detections->size(); i++) {
    //     const DETECTION_ROW& detection = (*detections)[i];
    //     printf("Detection[%zu]:\n", i);
    //     printf("  tlwh: [%.2f, %.2f, %.2f, %.2f]\n",
    //            detection.tlwh(0), detection.tlwh(1),
    //            detection.tlwh(2), detection.tlwh(3));
    //     printf("  confidence: %.4f\n", detection.confidence);
    //     printf("  feature size: %zu\n", detection.feature.size());
    //     // 打印前几个特征值作为示例
    //     printf("  feature (first 5): [");
    //     for (int j = 0; j < 5 && j < detection.feature.size(); j++) {
    //         printf("%.4f", detection.feature(j));
    //         if (j < 4 && j < detection.feature.size() - 1) printf(", ");
    //     }
    //     printf("]\n");
    // }
    // printf("=== End DETECTIONS Debug ===\n");

    if (g_yoloworld_time_consume_debug) {
      clock_gettime(CLOCK_MONOTONIC, &tracker_end_time);
      double tracker_elapsed =
          (tracker_end_time.tv_sec - tracker_start_time.tv_sec) * 1000 +
          (tracker_end_time.tv_nsec - tracker_start_time.tv_nsec) / 1000000;
      printf("deepsort_tracker execution time: %.2f milliseconds\n",
             tracker_elapsed);
    }
    if (ret != 0) {
      printf("deepsort fail\n");
    }

    add_tracker_cpu_task(detections, &result->od_results);

    // 释放buffer
    media_release_vpss_frame(&result->frame);

    // 释放结果节点
    free(result);
  }

  // 准备图像数据
  image_buffer_t src_image = {0};
  src_image.width = frame.stVFrame.u32Width;
  src_image.height = frame.stVFrame.u32Height;
  src_image.width_stride = frame.stVFrame.u32Width;
  src_image.height_stride = frame.stVFrame.u32Height;
  src_image.format = IMAGE_FORMAT_YUV420SP_NV12;
  src_image.virt_addr =
      (unsigned char *)RK_MPI_MB_Handle2VirAddr(frame.stVFrame.pMbBlk);
  src_image.fd = RK_MPI_MB_Handle2Fd(frame.stVFrame.pMbBlk);
  src_image.size = frame.stVFrame.u32Width * frame.stVFrame.u32Height * 3 / 2;

#ifdef SAVE_FILE_DEBUG
  if (g_fp == NULL) {
    g_fp = fopen("/data/yuv.bin", "wb");
  }
  fwrite(src_image.virt_addr, 1, src_image.size, g_fp);
  fflush(g_fp);
#endif

  npu_object_detect_result od_results_npu = {0};
  if (g_yoloworld_time_consume_debug) {
    clock_gettime(CLOCK_MONOTONIC, &npu_start_time);
  }
  detector_yolo_world_npu(g_detector, &src_image, &od_results_npu);
  if (g_yoloworld_time_consume_debug) {
    clock_gettime(CLOCK_MONOTONIC, &npu_end_time);
    double npu_elapsed =
        (npu_end_time.tv_sec - npu_start_time.tv_sec) * 1000 +
        (npu_end_time.tv_nsec - npu_start_time.tv_nsec) / 1000000;
    printf("detector_yolo_world_npu execution time: %.2f milliseconds\n",
           npu_elapsed);
  }

  add_cpu_task(&od_results_npu, &frame);
}

// CPU 后处理线程函数
static void *cpu_post_process_thread(void *arg) {
  printf("CPU post-process thread started\n");

  while (!g_quit_detect_threads) {
    CPU_TASK_NODE_S *task = pop_cpu_task();
    if (task != NULL) {
      // 执行 CPU 后处理
      object_detect_result_list od_results = {0};

      if (g_yoloworld_time_consume_debug) {
        clock_gettime(CLOCK_MONOTONIC, &cpu_start_time);
      }
      detector_yolo_world_cpu(&task->od_results_npu, 0.45f, 0.45f, &od_results);
      if (g_yoloworld_time_consume_debug) {
        clock_gettime(CLOCK_MONOTONIC, &cpu_end_time);
        double elapsed =
            (cpu_end_time.tv_sec - cpu_start_time.tv_sec) * 1000 +
            (cpu_end_time.tv_nsec - cpu_start_time.tv_nsec) / 1000000;
        printf("detector_yolo_world_cpu execution time: %.2f milliseconds, "
               "found %d "
               "objects\n",
               elapsed, od_results.count);
      }

      // 将处理结果添加到结果队列
      add_cpu_result(&od_results, &task->frame);

      // 释放任务节点
      free(task);
    } else {
      usleep(10 * 1000); // 10ms
    }
  }

  printf("CPU post-process thread exited\n");
  return NULL;
}

static void *tracker_cpu_post_process_thread(void *arg) {
  printf("Tracker CPU post-process thread started\n");

  while (!g_quit_detect_threads) {
    TRACKER_CPU_TASK_NODE_S *task = pop_tracker_cpu_task();
    if (task != NULL) {
      if (task->od_results.count == 0 && g_need_clear) {
        media_add_clear_rgn_task(NULL);
        g_need_clear = false;
      }
      // 重置g_userdata长度
      g_userdata_len = 0;
      // 处理检测结果
      for (int i = 0; i < task->od_results.count; i++) {
        if (task->od_results.results[i].prop >= DETECT_PROP_THRESHOLD &&
            task->od_results.results[i].cls_id != 0) {

          // 坐标转换：从640x640转换为1920x1080
          int box_x = task->od_results.results[i].box.x;
          int box_y = task->od_results.results[i].box.y;
          int box_width = task->od_results.results[i].box.width;
          int box_height = task->od_results.results[i].box.height;

          convert_coordinates_640_to_1920x1080(&box_x, &box_y, &box_width,
                                               &box_height);

          // 将检测结果信息插入到g_userdata中
          char od_info[256];
          int written = snprintf(
              od_info, sizeof(od_info),
              "OD:cls_id=%d,box=[%d,%d,%d,%d],prop=%.2f,name=%s;",
              task->od_results.results[i].cls_id, box_x, box_y, box_width,
              box_height, task->od_results.results[i].prop,
              g_detector->classes_[task->od_results.results[i].cls_id]);

          // 检查是否还有空间写入
          if (g_userdata_len + written < sizeof(g_userdata)) {
            memcpy(g_userdata + g_userdata_len, od_info, written);
            g_userdata_len += written;
          } else {
            // 空间不足，截断但不跳出循环，继续执行原来的逻辑
            int remaining = sizeof(g_userdata) - g_userdata_len;
            if (remaining > 0) {
              memcpy(g_userdata + g_userdata_len, od_info, remaining);
              g_userdata_len = sizeof(g_userdata);
            }
          }

          // 坐标转换：从640x640转换为1920x1080
          int rect_x = task->od_results.results[i].box.x;
          int rect_y = task->od_results.results[i].box.y;
          int rect_width = task->od_results.results[i].box.width;
          int rect_height = task->od_results.results[i].box.height;

          convert_coordinates_640_to_1920x1080(&rect_x, &rect_y, &rect_width,
                                               &rect_height);

          RGN_RECT_S rect = {
              .x = rect_x,
              .y = rect_y,
              .w = rect_width,
              .h = rect_height,
              .color_index = RGN_COLOR_LUT_INDEX_0,
          };

          snprintf(rect.text, sizeof(rect.text), "%s:%.2f",
                   g_detector->classes_[task->od_results.results[i].cls_id],
                   task->od_results.results[i].prop);
          media_add_rgn_rect(rect);
          g_need_update = true;
        }
      }

      // 打印task中的detections值
      // printf("=== Task DETECTIONS Debug Info ===\n");
      // printf("task->detections size: %zu\n", task->detections->size());
      // for (size_t i = 0; i < task->detections->size(); i++) {
      //     const DETECTION_ROW& detection = (*task->detections)[i];
      //     printf("Task Detection[%zu]:\n", i);
      //     printf("  tlwh: [%.2f, %.2f, %.2f, %.2f]\n",
      //            detection.tlwh(0), detection.tlwh(1),
      //            detection.tlwh(2), detection.tlwh(3));
      //     printf("  confidence: %.4f\n", detection.confidence);
      //     printf("  feature size: %zu\n", detection.feature.size());
      // }
      // printf("=== End Task DETECTIONS Debug ===\n");

      // 记录task detections处理开始时间
      if (g_yoloworld_time_consume_debug) {
        clock_gettime(CLOCK_MONOTONIC, &deepsort_tracker_cpu_start);
      }
      deepsort_tracker_cpu(g_tracker, task->detections);
      // 记录task detections处理结束时间并计算耗时
      if (g_yoloworld_time_consume_debug) {
        clock_gettime(CLOCK_MONOTONIC, &deepsort_tracker_cpu_end);
        double deepsort_tracker_cpu_elapsed =
            (deepsort_tracker_cpu_end.tv_sec -
             deepsort_tracker_cpu_start.tv_sec) *
                1000 +
            (deepsort_tracker_cpu_end.tv_nsec -
             deepsort_tracker_cpu_start.tv_nsec) /
                1000000;
        printf("deepsort_tracker_cpu processing execution time: %.2f "
               "milliseconds, "
               "processed %zu detections\n",
               deepsort_tracker_cpu_elapsed, task->detections->size());
      }

      TrackResultArray res = tracker_get_results(g_tracker);
      for (int i = 0; i < res.size; i++) {
        // 坐标转换：从640x640转换为1920x1080
        int tlwh_x = res.results[i].tlwh[0];
        int tlwh_y = res.results[i].tlwh[1];
        int tlwh_width = res.results[i].tlwh[2];
        int tlwh_height = res.results[i].tlwh[3];

        convert_coordinates_640_to_1920x1080(&tlwh_x, &tlwh_y, &tlwh_width,
                                             &tlwh_height);

        // 将跟踪结果信息插入到g_userdata中
        char tracker_info[256];
        int written =
            snprintf(tracker_info, sizeof(tracker_info),
                     "TR:track_id=%d,tlwh=[%d,%d,%d,%d],name=%s;",
                     res.results[i].track_id, tlwh_x, tlwh_y, tlwh_width,
                     tlwh_height, g_detector->classes_[g_tracker_det_id]);

        // 检查是否还有空间写入
        if (g_userdata_len + written < sizeof(g_userdata)) {
          memcpy(g_userdata + g_userdata_len, tracker_info, written);
          g_userdata_len += written;
        } else {
          // 空间不足，截断但不跳出循环，继续执行原来的逻辑
          int remaining = sizeof(g_userdata) - g_userdata_len;
          if (remaining > 0) {
            memcpy(g_userdata + g_userdata_len, tracker_info, remaining);
            g_userdata_len = sizeof(g_userdata);
          }
        }

        // 坐标转换：从640x640转换为1920x1080
        int rect_x = res.results[i].tlwh[0];
        int rect_y = res.results[i].tlwh[1];
        int rect_width = res.results[i].tlwh[2];
        int rect_height = res.results[i].tlwh[3];

        convert_coordinates_640_to_1920x1080(&rect_x, &rect_y, &rect_width,
                                             &rect_height);

        RGN_RECT_S rect = {
            .x = rect_x,
            .y = rect_y,
            .w = rect_width,
            .h = rect_height,
            .color_index = RGN_COLOR_LUT_INDEX_1,
        };
        snprintf(rect.text, sizeof(rect.text), "%s %d",
                 g_detector->classes_[g_tracker_det_id],
                 res.results[i].track_id);
        media_add_rgn_rect(rect);
        g_need_update = true;
      }

      tracker_free_results(res);

      if (g_need_update) {
        media_add_update_rgn_task(NULL);
        g_need_clear = true;
        g_need_update = false;
      }

      // 确保字符串以null结尾
      if (g_userdata_len < sizeof(g_userdata)) {
        g_userdata[g_userdata_len] = '\0';
      } else {
        g_userdata[sizeof(g_userdata) - 1] = '\0';
      }

      // 插入用户数据到编码器
      media_insert_userdata2venc(g_userdata, g_userdata_len);

      // 释放任务节点
      if (task->need_free_detections) {
        free_detections_copy(task->detections);
      }
      free(task);
    } else {
      usleep(10 * 1000); // 10ms
    }
  }

  printf("Tracker CPU post-process thread exited\n");
  return NULL;
}

int main(int argc, char **argv) {
  char *env_var = getenv("YOLOWORLD_TIME_CONSUME_DEBUG");
  printf("env_var: %s\n", env_var);
  g_yoloworld_time_consume_debug =
      (env_var != NULL && strcmp(env_var, "1") == 0);
  printf("g_yoloworld_time_consume_debug: %d\n",
         g_yoloworld_time_consume_debug);

  char *env_var_usbcam = getenv("YOLOWORLD_USB_CAM");
  printf("env_var_usbcam: %s\n", env_var_usbcam);
  g_is_usbcam = (env_var_usbcam != NULL && strcmp(env_var_usbcam, "1") == 0);
  printf("g_is_usbcam: %d\n", g_is_usbcam);

  // 加载配置文件
  copy_file(OEM_DETECT_PATH, USER_DETECT_PATH);

  copy_file(OEM_NO_DETECT_PATH, USER_NO_DETECT_PATH);

  timespec start_time, end_time;
  clock_gettime(CLOCK_MONOTONIC, &start_time);
  yoloworld_detect_init(DETECTOR_INIT_PATH, &g_detector, &g_tracker,
                        &g_rknn_reid_ctx, g_tracker_det_id);
  clock_gettime(CLOCK_MONOTONIC, &end_time);
  double elapsed = (end_time.tv_sec - start_time.tv_sec) * 1000 +
                   (end_time.tv_nsec - start_time.tv_nsec) / 1000000;
  printf("yoloworld_detect_init execution time: %.2f milliseconds\n", elapsed);

  for (int i = 0; i < g_detector->classes_size_; i++) {
    printf("id: %d, class_name: %s\n", i, g_detector->classes_[i]);
  }

  media_set_rgb888_buffer_cb(rgb888_buffer_cb);
  media_init(g_is_usbcam);

  set_detect_type_add_class_callback(detect_type_add_callback);
  set_detect_type_delete_class_callback(detect_type_delete_callback);

  // 初始化任务队列
  init_cpu_task_queue();
  init_cpu_result_queue();
  init_detect_type_task_queue();

  // 启动 CPU 后处理线程
  if (pthread_create(&g_cpu_thread, NULL, cpu_post_process_thread, NULL) != 0) {
    printf("Failed to create CPU post-process thread\n");
    return -1;
  }

  if (pthread_create(&g_tracker_cpu_thread, NULL,
                     tracker_cpu_post_process_thread, NULL) != 0) {
    printf("Failed to create tracker CPU post-process thread\n");
    return -1;
  }

  sys_init();

  sys_loop();

  // 停止检测线程
  g_quit_detect_threads = true;
  pthread_join(g_cpu_thread, NULL);
  pthread_join(g_tracker_cpu_thread, NULL);

  while (g_cpu_task_queue.front != NULL) {
    CPU_TASK_NODE_S *temp = g_cpu_task_queue.front;
    g_cpu_task_queue.front = g_cpu_task_queue.front->next;
    free(temp);
  }
  pthread_mutex_destroy(&g_cpu_task_queue.mutex);

  while (g_cpu_result_queue.front != NULL) {
    CPU_TASK_RESULT_NODE_S *temp = g_cpu_result_queue.front;
    g_cpu_result_queue.front = g_cpu_result_queue.front->next;
    free(temp);
  }
  pthread_mutex_destroy(&g_cpu_result_queue.mutex);

  while (g_tracker_cpu_task_queue.front != NULL) {
    TRACKER_CPU_TASK_NODE_S *temp = g_tracker_cpu_task_queue.front;
    g_tracker_cpu_task_queue.front = g_tracker_cpu_task_queue.front->next;
    free(temp);
  }
  pthread_mutex_destroy(&g_tracker_cpu_task_queue.mutex);

  while (g_detect_type_task_queue.front != NULL) {
    DETECT_TYPE_TASK_NODE_S *temp = g_detect_type_task_queue.front;
    g_detect_type_task_queue.front = g_detect_type_task_queue.front->next;
    free(temp);
  }
  pthread_mutex_destroy(&g_detect_type_task_queue.mutex);

  sys_deinit();
  media_deinit();

  return 0;
}
