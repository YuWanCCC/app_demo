/*
 * Copyright 2021 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "rt_trace.h"
#include "rt_common.h"

#define HAVE_TRACE

#ifdef HAVE_TRACE

#ifdef __cplusplus
#define CC_LIKELY(exp)      (__builtin_expect(!!(exp), true))
#define CC_UNLIKELY(exp)    (__builtin_expect(!!(exp), false))
#else
#define CC_LIKELY(exp)    (__builtin_expect(!!(exp), 1))
#define CC_UNLIKELY(exp)  (__builtin_expect(!!(exp), 0))
#endif

#define RT_TRACE_MESSAGE_LENGTH 1024

static INT32 trace_marker_fd   = -1;
static INT32 init_trace_ok      = 0;

static INT32 rt_trace_init_once(void) {
    if (init_trace_ok == 0) {
        static const char *trace_paths[] = {
            "/sys/kernel/debug/tracing/trace_marker",
            "/debug/tracing/trace_marker",
            "/debugfs/tracing/trace_marker",
        };

        UINT32 i;
        for (i = 0; i < RT_ARRAY_ELEMS(trace_paths); i++) {
            if (!access(trace_paths[i], F_OK)) {
                trace_marker_fd = open(trace_paths[i], O_WRONLY | O_CLOEXEC);
                if (trace_marker_fd >= 0)
                    break;
            }
        }

        if (trace_marker_fd == -1) {
            printf("Error opening trace file: %s (%d)\n", strerror(errno), errno);
            return -1;
        }
    }

    init_trace_ok = 1;
    return 0;
}

#define WRITE_MSG(format_begin, format_end, name, value) { \
    char buf[RT_TRACE_MESSAGE_LENGTH]; \
    if (CC_UNLIKELY(!init_trace_ok)) \
        rt_trace_init_once(); \
    INT32 pid = getpid(); \
    INT32 len = snprintf(buf, sizeof(buf), format_begin "%s" format_end, pid, \
        name, value); \
    if (len >= static_cast<INT32>(sizeof(buf))) { \
        INT32 name_len = strlen(name) - (len - sizeof(buf)) - 1; \
        printf("Truncated name in %s: %s\n", __FUNCTION__, name); \
        len = snprintf(buf, sizeof(buf), format_begin "%.*s" format_end, pid, \
            name_len, name, value); \
    } \
    write(trace_marker_fd, buf, len); \
}

void rt_atrace_begin_body(const char* name) {
    WRITE_MSG("B|%d|", "%s", name, "");
}

void rt_atrace_end_body() {
    WRITE_MSG("E|%d", "%s", "", "");
}

void rt_atrace_async_begin_body(const char* name, INT32 cookie) {
    WRITE_MSG("S|%d|", "|%" PRId32, name, cookie);
}

void rt_atrace_async_end_body(const char* name, INT32 cookie) {
    WRITE_MSG("F|%d|", "|%" PRId32, name, cookie);
}

void rt_atrace_int_body(const char* name, INT32 value) {
    WRITE_MSG("C|%d|", "|%" PRId32, name, value);
}

void rt_atrace_int64_body(const char* name, INT64 value) {
    WRITE_MSG("C|%d|", "|%" PRId64, name, value);
}

#else

void rt_atrace_begin_body(__attribute__((unused)) const char* name) {
}

void rt_atrace_end_body() {
}

void rt_atrace_async_begin_body(__attribute__((unused)) const char* name, __attribute__((unused)) INT32 cookie) {
}

void rt_atrace_async_end_body(__attribute__((unused)) const char* name, __attribute__((unused)) INT32 cookie) {
}

void rt_atrace_int_body(__attribute__((unused)) const char* name, __attribute__((unused)) INT32 value) {
}

void rt_atrace_int64_body(__attribute__((unused)) const char* name, __attribute__((unused)) INT64 value) {
}

#endif
