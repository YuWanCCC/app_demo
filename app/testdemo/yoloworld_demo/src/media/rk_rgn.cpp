#include "rk_rgn.h"
#include "font_factory.h"
#include "media_init.h"

#include <cstdio>
#include <cstring>
#include <iconv.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define MAX_WCH_BYTE 128

static RGN_HANDLE g_rgn_handle = 0;

typedef struct Node {
  RGN_RECT_S rect;
  struct Node *next;
} RGN_RECT_NODE_S;

typedef struct {
  RGN_RECT_NODE_S *front;
  RGN_RECT_NODE_S *rear;
  pthread_mutex_t mutex;
} RGN_RECT_QUEUE_S;

typedef enum { TASK_TYPE_UPDATE, TASK_TYPE_CLEAR } TASK_TYPE_E;

// 任务结构体
typedef struct TaskNode {
  TASK_TYPE_E type;
  void (*func)(void *);
  void *arg;
  struct TaskNode *next;
} RGN_TASK_NODE_S;

// 任务队列结构体
typedef struct {
  RGN_TASK_NODE_S *front;
  RGN_TASK_NODE_S *rear;
  pthread_mutex_t mutex;
} TaskQueue;

static RGN_RECT_QUEUE_S g_rgn_rect_queue;
static pthread_t g_rgn_rect_thread;
static bool g_quit = false;
static TaskQueue g_task_queue;

static int draw_rect_argb8888(RK_U8 *buffer, RK_U32 width, RK_U32 height,
                              int rgn_x, int rgn_y, int rgn_w, int rgn_h,
                              int line_pixel, COLOR_INDEX_E color_index);

static int iconv_utf8_to_wchar(const char *in, wchar_t *out);

static void rk_init_rgn_rect_queue() {
  g_rgn_rect_queue.front = g_rgn_rect_queue.rear = NULL;
  pthread_mutex_init(&g_rgn_rect_queue.mutex, NULL);
}

static RGN_RECT_NODE_S *rk_pop_rgn_rect() {
  pthread_mutex_lock(&g_rgn_rect_queue.mutex);
  if (g_rgn_rect_queue.front == NULL) {
    pthread_mutex_unlock(&g_rgn_rect_queue.mutex);
    return NULL;
  }
  RGN_RECT_NODE_S *temp = g_rgn_rect_queue.front;
  RGN_RECT_S *rect = &temp->rect;
  g_rgn_rect_queue.front = g_rgn_rect_queue.front->next;
  if (g_rgn_rect_queue.front == NULL) {
    g_rgn_rect_queue.rear = NULL;
  }
  pthread_mutex_unlock(&g_rgn_rect_queue.mutex);
  return temp;
}

// 任务队列初始化
static void init_task_queue() {
  g_task_queue.front = g_task_queue.rear = NULL;
  pthread_mutex_init(&g_task_queue.mutex, NULL);
}

// 添加任务到队列
static void add_task(TASK_TYPE_E type, void (*func)(void *), void *arg) {
  RGN_TASK_NODE_S *newNode = (RGN_TASK_NODE_S *)malloc(sizeof(RGN_TASK_NODE_S));
  newNode->type = type;
  newNode->func = func;
  newNode->arg = arg;
  newNode->next = NULL;

  pthread_mutex_lock(&g_task_queue.mutex);
  if (g_task_queue.rear == NULL) {
    g_task_queue.front = g_task_queue.rear = newNode;
  } else {
    g_task_queue.rear->next = newNode;
    g_task_queue.rear = newNode;
  }
  pthread_mutex_unlock(&g_task_queue.mutex);
}

// 从队列中取出任务
static RGN_TASK_NODE_S *pop_task() {
  pthread_mutex_lock(&g_task_queue.mutex);
  if (g_task_queue.front == NULL) {
    pthread_mutex_unlock(&g_task_queue.mutex);
    return NULL;
  }
  RGN_TASK_NODE_S *temp = g_task_queue.front;
  g_task_queue.front = g_task_queue.front->next;
  if (g_task_queue.front == NULL) {
    g_task_queue.rear = NULL;
  }
  pthread_mutex_unlock(&g_task_queue.mutex);
  return temp;
}

static void rk_rgn_update_task_func(void *arg) {
  // printf("rk_rgn_update_task_func\n");
  int ret = 0;
  RGN_CANVAS_INFO_S stCanvasInfo;
  RGN_RECT_NODE_S *node = NULL;
  int x, y, w, h;
  COLOR_INDEX_E border_color;
  int border_width = 4;
  wchar_t wch[MAX_WCH_BYTE] = {0};

  ret = RK_MPI_RGN_GetCanvasInfo(g_rgn_handle, &stCanvasInfo);
  if (ret != RK_SUCCESS) {
    printf("RK_MPI_RGN_GetCanvasInfo failed with %#x!\n", ret);
    return;
  }
  memset((void *)stCanvasInfo.u64VirAddr, 0,
         stCanvasInfo.u32VirWidth * stCanvasInfo.u32VirHeight * 4);
  do {
    node = rk_pop_rgn_rect();
    if (node != NULL) {
      RGN_RECT_S *rect = &node->rect;
      x = rect->x;
      y = rect->y;
      w = rect->w;
      h = rect->h;
      border_color = rect->color_index;
      if (rect->color_index == RGN_COLOR_LUT_INDEX_0) {
        set_font_color(0xff00ff00);
      } else if (rect->color_index == RGN_COLOR_LUT_INDEX_1) {
        set_font_color(0xffff0000);
      }
      memset(wch, 0, sizeof(wch));
      iconv_utf8_to_wchar(rect->text, wch);
      draw_argb8888_text((RK_U8 *)stCanvasInfo.u64VirAddr,
                         stCanvasInfo.u32VirWidth, stCanvasInfo.u32VirHeight,
                         wch, x, y);
      draw_rect_argb8888((RK_U8 *)stCanvasInfo.u64VirAddr,
                         stCanvasInfo.u32VirWidth, stCanvasInfo.u32VirHeight, x,
                         y, w, h, border_width, border_color);
      // printf("Processing rectangle: (%d, %d, %d, %d)\n", rect->x, rect->y,
      //        rect->w, rect->h);
      free(node);
    }
  } while (node != NULL);

  ret = RK_MPI_RGN_UpdateCanvas(g_rgn_handle);
  if (ret != RK_SUCCESS) {
    printf("RK_MPI_RGN_UpdateCanvas failed with %#x!\n", ret);
  }
  return;
}

static void rk_rgn_clear_task_func(void *arg) {
  // printf("rk_rgn_clear_task_func\n");
  int ret = 0;
  RGN_CANVAS_INFO_S stCanvasInfo;
  ret = RK_MPI_RGN_GetCanvasInfo(g_rgn_handle, &stCanvasInfo);
  if (ret != RK_SUCCESS) {
    printf("RK_MPI_RGN_GetCanvasInfo failed with %#x!\n", ret);
    return;
  }
  memset((void *)stCanvasInfo.u64VirAddr, 0,
         stCanvasInfo.u32VirWidth * stCanvasInfo.u32VirHeight * 4);
  ret = RK_MPI_RGN_UpdateCanvas(g_rgn_handle);
  if (ret != RK_SUCCESS) {
    printf("RK_MPI_RGN_UpdateCanvas failed with %#x!\n", ret);
  }
  return;
}

static int draw_rect_argb8888(RK_U8 *buffer, RK_U32 width, RK_U32 height,
                              int rgn_x, int rgn_y, int rgn_w, int rgn_h,
                              int line_pixel, COLOR_INDEX_E color_index) {
  int i;
  RK_U8 *ptr = buffer;
  RK_U8 value[4] = {255, 0, 0,
                    0}; // BGRA format, default to black with full opacity

  if (color_index == RGN_COLOR_LUT_INDEX_0) {
    value[0] = 255; // A
    value[1] = 0;   // R
    value[2] = 255; // G
    value[3] = 0;   // B
  } else if (color_index == RGN_COLOR_LUT_INDEX_1) {
    value[0] = 255; // A
    value[1] = 255; // R
    value[2] = 0;   // G
    value[3] = 0;   // B
  }

  if (line_pixel > 4) {
    printf("line_pixel > 4, not support\n");
    return -1;
  }

  // Draw top line
  ptr += (width * rgn_y + rgn_x) * 4;
  for (i = 0; i < line_pixel; i++) {
    for (int j = 0; j < rgn_w; j++) {
      memcpy(ptr + j * 4, value, 4);
    }
    ptr += width * 4;
  }

  // Draw left/right line
  for (i = 0; i < (rgn_h - line_pixel * 2); i++) {
    for (int j = 0; j < line_pixel; j++) {
      memcpy(ptr + j * 4, value, 4);
      memcpy(ptr + (rgn_w - line_pixel + j) * 4, value, 4);
    }
    ptr += width * 4;
  }

  // Draw bottom line
  for (i = 0; i < line_pixel; i++) {
    for (int j = 0; j < rgn_w; j++) {
      memcpy(ptr + j * 4, value, 4);
    }
    ptr += width * 4;
  }

  return 0;
}

static int iconv_utf8_to_wchar(const char *in, wchar_t *out) {
  char entry[128] = {'\0'};
  // LOG_DEBUG("in is %s\n", in);
  // LOG_DEBUG("strlen(in) is %ld\n", strlen(in));
#if 0
	int ret = mbstowcs(out, in, strlen(in));
	LOG_DEBUG("mbstowcs ret is %d\n", ret);
	if (ret == -1)
		return ret;
	out[ret] = '\0';
	return ret;
#endif
  int ret;
  size_t src_len = strlen(in);
  // the bytes that Chinese and English in UTF-8 are different,
  // and cannot be based on src_len calculates out_len
  size_t out_len = MAX_WCH_BYTE;
  // To have this temporary variable,
  // otherwise iconv will directly overwrite the original pointer
  char *tmp_out_buffer = (char *)out;
  iconv_t cd = iconv_open("WCHAR_T", "UTF-8");
  if (cd == (iconv_t)-1) {
    perror("iconv_open error");
    return -1;
  }
  memset(entry, 0, 128);
  memcpy(entry, in, src_len); // iconv maybe change the char *
  memset(entry + src_len, 0, 1);
  char *tmp_in_buffer = (char *)entry;
  ret = iconv(cd, &tmp_in_buffer, (size_t *)&src_len, &tmp_out_buffer,
              (size_t *)&out_len);
  if (ret == -1)
    perror("iconv error");
  iconv_close(cd);
  out[(MAX_WCH_BYTE > out_len ? MAX_WCH_BYTE - out_len
                              : out_len - MAX_WCH_BYTE) /
      4] = '\0';
  // LOG_DEBUG("out_len is %d\n", out_len);
  // LOG_DEBUG("wcslen(out) is %ld\n", wcslen(out));
  // for (int i = 0; i < strlen(display_text); i++) {
  // 	LOG_INFO("111 display_text [%02x]\n", display_text[i]);
  // }
  // for (int i = 0; i < wcslen(osd_data.text.wch); i++) {
  // 	LOG_INFO("222 osd_data.text.wch [%04x]\n", osd_data.text.wch[i]);
  // }

  return 0;
}

static void *rk_rgn_rect_thread_func(void *arg) {
  int ret = 0;
  RGN_CANVAS_INFO_S stCanvasInfo;
  int x, y, w, h;
  COLOR_INDEX_E border_color;
  int border_width = 3;
  wchar_t wch[MAX_WCH_BYTE] = {0};

  while (!g_quit) {
    // 处理任务队列
    RGN_TASK_NODE_S *task = pop_task();
    if (task != NULL) {
      if (task->func != NULL) {
        task->func(task->arg);
      }
      free(task);
    }
    usleep(50 * 1000);
  }
  return NULL;
}

int rk_rgn_init(int width, int height) {
  int ret = 0;
  RGN_CANVAS_INFO_S stCanvasInfo;
  memset(&stCanvasInfo, 0, sizeof(stCanvasInfo));
  RGN_ATTR_S stRgnAttr;
  memset(&stRgnAttr, 0, sizeof(stRgnAttr));

  // 初始化任务队列
  init_task_queue();

  stRgnAttr.enType = OVERLAY_EX_RGN;
  stRgnAttr.unAttr.stOverlay.enPixelFmt = RK_FMT_ARGB8888;
  stRgnAttr.unAttr.stOverlay.u32CanvasNum = 2;
  stRgnAttr.unAttr.stOverlay.stSize.u32Width = width;
  stRgnAttr.unAttr.stOverlay.stSize.u32Height = height;
  stRgnAttr.unAttr.stOverlay.enVProcDev = VIDEO_PROC_DEV_RGA;

  RGN_CHN_ATTR_S stRgnChnAttr;
  memset(&stRgnChnAttr, 0, sizeof(stRgnChnAttr));
  stRgnChnAttr.bShow = RK_TRUE;
  stRgnChnAttr.enType = OVERLAY_EX_RGN;
  stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = 0;
  stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y = 0;
  stRgnChnAttr.unChnAttr.stOverlayChn.u32BgAlpha = 0;
  stRgnChnAttr.unChnAttr.stOverlayChn.u32FgAlpha = 255;
  stRgnChnAttr.unChnAttr.stOverlayChn.u32Layer = 0;

  MPP_CHN_S stMppChn;
  memset(&stMppChn, 0, sizeof(stMppChn));
  stMppChn.enModId = RK_ID_VPSS;
  stMppChn.s32DevId = 0;
  stMppChn.s32ChnId = 0;

  ret = RK_MPI_RGN_Create(g_rgn_handle, &stRgnAttr);
  if (ret != RK_SUCCESS) {
    printf("RK_MPI_RGN_CreateChn failed %#x\n", ret);
    return ret;
  }

  ret = RK_MPI_RGN_AttachToChn(g_rgn_handle, &stMppChn, &stRgnChnAttr);
  if (ret != RK_SUCCESS) {
    printf("RK_MPI_RGN_AttachToChn failed %#x\n", ret);
    return ret;
  }

  ret = RK_MPI_RGN_GetCanvasInfo(g_rgn_handle, &stCanvasInfo);
  if (ret != RK_SUCCESS) {
    printf("RK_MPI_RGN_GetCanvasInfo failed %#x\n", ret);
    return ret;
  }
  memset((void *)stCanvasInfo.u64VirAddr, 0x00,
         stCanvasInfo.u32VirWidth * stCanvasInfo.u32VirHeight * 4);
  ret = RK_MPI_RGN_UpdateCanvas(g_rgn_handle);
  if (ret != RK_SUCCESS) {
    printf("RK_MPI_RGN_UpdateCanvas failed %#x\n", ret);
    return ret;
  }

  ret = create_font("/oem/usr/share/fonts/SourceHanSansCN.ttf", 32);
  if (ret != 0) {
    printf("create_font failed %#x\n", ret);
    return ret;
  }

  rk_init_rgn_rect_queue();
  pthread_create(&g_rgn_rect_thread, NULL, rk_rgn_rect_thread_func, NULL);
  return 0;
}

int rk_rgn_deinit() {
  int ret = 0;
  g_quit = true;
  pthread_join(g_rgn_rect_thread, NULL);
  pthread_mutex_destroy(&g_rgn_rect_queue.mutex);
  while (g_rgn_rect_queue.front != NULL) {
    RGN_RECT_NODE_S *temp = g_rgn_rect_queue.front;
    g_rgn_rect_queue.front = g_rgn_rect_queue.front->next;
    free(temp);
  }

  destroy_font();

  MPP_CHN_S stMppChn;
  memset(&stMppChn, 0, sizeof(stMppChn));
  stMppChn.enModId = RK_ID_VPSS;
  stMppChn.s32DevId = 0;
  stMppChn.s32ChnId = 0;
  ret = RK_MPI_RGN_DetachFromChn(g_rgn_handle, &stMppChn);
  if (ret != RK_SUCCESS)
    printf("RK_MPI_RGN_DetachFrmChn (%d) to venc0 failed with %#x\n",
           g_rgn_handle, ret);

  // destory region
  ret = RK_MPI_RGN_Destroy(g_rgn_handle);
  if (ret != RK_SUCCESS)
    printf("RK_MPI_RGN_Destroy [%d] failed with %#x\n", g_rgn_handle, ret);

  while (g_task_queue.front != NULL) {
    RGN_TASK_NODE_S *temp = g_task_queue.front;
    g_task_queue.front = g_task_queue.front->next;
    free(temp);
  }
  pthread_mutex_destroy(&g_task_queue.mutex);

  return ret;
}

// 对外媒体接口
void media_add_rgn_rect(RGN_RECT_S rect) {
  RGN_RECT_NODE_S *newNode = (RGN_RECT_NODE_S *)malloc(sizeof(RGN_RECT_NODE_S));
  memcpy(&newNode->rect, &rect, sizeof(RGN_RECT_S));
  newNode->next = NULL;

  pthread_mutex_lock(&g_rgn_rect_queue.mutex);
  if (g_rgn_rect_queue.rear == NULL) {
    g_rgn_rect_queue.front = g_rgn_rect_queue.rear = newNode;
  } else {
    g_rgn_rect_queue.rear->next = newNode;
    g_rgn_rect_queue.rear = newNode;
  }
  pthread_mutex_unlock(&g_rgn_rect_queue.mutex);
}

// 对外接口：添加更新任务
void media_add_update_rgn_task(void *arg) {
  add_task(TASK_TYPE_UPDATE, rk_rgn_update_task_func, arg);
}

// 对外接口：添加清除任务
void media_add_clear_rgn_task(void *arg) {
  add_task(TASK_TYPE_CLEAR, rk_rgn_clear_task_func, arg);
}
