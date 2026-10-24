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

#include "mpu_aov.h"
#include "utils/common.h"
#include "utils/log.h"
#include "utils/kernel_list.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <net/if.h>   // must be included later than <net/if.h>
#include <linux/if.h> // must be included later than <net/if.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/input.h>

/**********************
 *  STATIC PROTOTYPES
 **********************/

struct event_cb_obj {
    AOV_EVENT_CB cb;
    struct list_head head;
};

/**********************
 *  STATIC VARIABLES
 **********************/
static struct event_cb_obj g_event_hand_cb_list = {
    .cb   = NULL,
    .head = LIST_HEAD_INIT(g_event_hand_cb_list.head)
};

static pthread_mutex_t g_event_lock = PTHREAD_MUTEX_INITIALIZER;

/**********************
 *      MACROS
 **********************/

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void aov_event_cb_loop(MPU_AOV_EVENT_T event) {
    struct event_cb_obj *pos = NULL, *n = NULL;

    pthread_mutex_lock(&g_event_lock);
    list_for_each_entry_safe(pos, n, &g_event_hand_cb_list.head, head) {
        if (NULL != pos->cb)
            pos->cb(event);
    }
    pthread_mutex_unlock(&g_event_lock);
}

static void aov_notify_callback(RKADK_AOV_EVENT_E event, void *msg) {
    switch(event) {
        case RKADK_AOV_ENTER_SLEEP:
            RKADK_LOGD("+++++ RKADK_AOV_ENTER_SLEEP +++++");

            RKADK_AOV_WakeupLock();

            aov_event_cb_loop(MPU_AOV_EVENT_BEFORE_ENTER);

            RKADK_AOV_EnterSleep();

            aov_event_cb_loop(MPU_AOV_EVENT_AFTER_EXIT);

            RKADK_AOV_WakeupUnlock();

            RKADK_LOGD("+++++ RKADK_AOV_EXIT_SLEEP +++++");
        break;

        default:
            RKADK_LOGD("Unknown event: %d", event);
        break;
    }
}
/**********************
 *   GLOBAL FUNCTIONS
 **********************/

int32_t mpu_aov_init(void) {
    int32_t suspend_time = 1000;
    RKADK_AOV_ARG_S aov_arg;

    memset(&aov_arg, 0, sizeof(aov_arg));
    aov_arg.pfnNotifyCallback = aov_notify_callback;
    RKADK_AOV_Init(&aov_arg);

    RKADK_LOGD("s32SuspendTime: %d", suspend_time);
    RKADK_AOV_SetSuspendTime(suspend_time);

    return 0;
}

void mpu_aov_deinit(void) {
    RKADK_AOV_DeInit();
}

void mpu_aov_wakeup_lock() {
    RKADK_AOV_WakeupLock();
}

void mpu_aov_wakeup_unlock() {
    RKADK_AOV_WakeupUnlock();
}

void mpu_aov_event_cb_register(AOV_EVENT_CB cb) {
    struct event_cb_obj *pos = NULL, *n = NULL, *cb_obj = NULL;

    pthread_mutex_lock(&g_event_lock);

    list_for_each_entry_safe(pos, n, &g_event_hand_cb_list.head, head) {
        if (cb == pos->cb) {
            CVR_ERR("Failed to register callback function!\n");
            goto out;
        }
    }

    cb_obj = calloc(1, sizeof(struct event_cb_obj));
    if (NULL == cb_obj) {
        CVR_ERR("calloc failed!\n");
        goto out;
    }
    cb_obj->cb = cb;

    list_add_tail(&cb_obj->head, &g_event_hand_cb_list.head);
out:
    pthread_mutex_unlock(&g_event_lock);
}

void mpu_aov_event_cb_unregister(AOV_EVENT_CB cb) {
    struct event_cb_obj *pos = NULL, *n = NULL;

    pthread_mutex_lock(&g_event_lock);
    list_for_each_entry_safe(pos, n, &g_event_hand_cb_list.head, head) {
        if (cb == pos->cb) {
            list_del(&pos->head);
            free(pos);
            break;
        }
    }
    pthread_mutex_unlock(&g_event_lock);
}