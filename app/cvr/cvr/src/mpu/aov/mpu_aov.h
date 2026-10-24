/*
 * Copyright (c) 2024 Rockchip, Inc. All Rights Reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

 #ifndef _MPU_AOV_H_
 #define _MPU_AOV_H_

 #include "utils/common.h"

 #include "rkadk_aov.h"
 #include "rkadk_common.h"

 #include <stdint.h>
 #include <string.h>

 #ifdef __cplusplus
 extern "C" {
 #endif

 typedef enum {
     MPU_AOV_EVENT_BEFORE_ENTER = 0,
     MPU_AOV_EVENT_AFTER_EXIT,
     MPU_AOV_EVENT_BUTT
 } MPU_AOV_EVENT_T;

 typedef void (*AOV_EVENT_CB)(MPU_AOV_EVENT_T);

 int32_t mpu_aov_init(void);
 void    mpu_aov_deinit(void);
 void    mpu_aov_wakeup_lock();
 void    mpu_aov_wakeup_unlock();
 void    mpu_aov_event_cb_register(AOV_EVENT_CB cb);
 void    mpu_aov_event_cb_unregister(AOV_EVENT_CB cb);

 #ifdef __cplusplus
 }  /* end of extern "C" */
 #endif

 #endif