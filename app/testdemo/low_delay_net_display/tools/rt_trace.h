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

#ifndef SRC_RT_BASE_RT_TRACE_H_
#define SRC_RT_BASE_RT_TRACE_H_

#include "rt_type.h"

#if defined(__cplusplus)
#define __BEGIN_DECLS extern "C" {
#define __END_DECLS }
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif

__BEGIN_DECLS

void rt_atrace_begin_body(const char* name);
void rt_atrace_end_body();
void rt_atrace_async_begin_body(const char* name, INT32 cookie);
void rt_atrace_async_end_body(const char* name, INT32 cookie);
void rt_atrace_int_body(const char* name, INT32 value);
void rt_atrace_int64_body(const char* name, INT64 value);

__END_DECLS

#endif  // SRC_RT_BASE_RT_TRACE_H_
