// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  BlueZ - Bluetooth protocol stack for Linux
 *  GATT Server Main Header
 */

#ifndef BTGATT_SERVER_H
#define BTGATT_SERVER_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include "lib/bluetooth.h"
#include "lib/hci.h"
#include "lib/hci_lib.h"
#include "lib/l2cap.h"
#include "lib/uuid.h"
#include "lib/sdp.h"
#include "lib/sdp_lib.h"
#include "lib/mgmt.h"

#include "src/shared/mainloop.h"
#include "src/shared/util.h"
#include "src/shared/att.h"
#include "src/shared/queue.h"
#include "src/shared/timeout.h"
#include "src/shared/gatt-db.h"
#include "src/shared/gatt-server.h"
#include "src/shared/io.h"
#include "src/shared/mgmt.h"

// Default UUIDs for transfer service
#define DEFAULT_TRANSFER_SERVICE_UUID "0000180d-0000-1000-8000-00805f9b34fb"
#define DEFAULT_TRANSFER_WRITE_UUID   "00002a37-0000-1000-8000-00805f9b34fb"
#define DEFAULT_TRANSFER_NOTIFY_UUID  "00002a38-0000-1000-8000-00805f9b34fb"

// Server configuration structure
struct server_config {
    uint16_t mtu;                       // MTU size
    int sec_level;                      // security level
    uint8_t src_type;                   // source address type
    const char *adv_name;               // advertise name
    const char *trans_service_uuid;     // Transfer service UUID string
    const char *trans_write_uuid;       // Write characteristic UUID string
    const char *trans_notify_uuid;      // Notify characteristic UUID string
    bool enable_advertising;            // if enable advertising
    uint32_t phy_type;                  // PHY configuration mask (MGMT_PHY_LE_XXX)
    bool is_connected;                  // Flag to track if a device is already connected
    bool enable_auth;                   // Enable authentication
    bool enable_sec_level;              // Enable security level
    bool hci_monitor;                   // Enable HCI monitoring
};

// Server structure
struct server {
    int fd;
    struct bt_att *att;
    struct gatt_db *db;
    struct bt_gatt_server *gatt;

    uint8_t *device_name;
    size_t name_len;

    uint16_t gatt_svc_chngd_handle;
    bool svc_chngd_enabled;

    // Transfer service
    uint16_t trans_notify_handle;
    bool transfer_chngd_enabled;
    uint8_t transfer_ccc;
    int local_socket;  // Listening socket for data transfer
    int client_socket; // Connected client socket for data transfer
    int socket_watch_id;  // BlueZ mainloop watch ID for socket events
};

// Global variables declarations
extern struct mgmt *g_mgmt;
extern uint16_t mgmt_index;
extern struct server_config g_config;

// Unused macro to suppress compiler warnings
#define UNUSED(x) (void)(x)

// Utility macros
#define PRLOG(...) \
    do { \
        printf("[%s:%d] %s(): ", __FILE__, __LINE__, __func__); \
        printf(__VA_ARGS__); \
    } while (0)

// Include our module headers after defining common structures
#include "btgatt-server-transfer.h"
#include "btgatt-server-hci.h"
#include "btgatt-server-mgmt.h"

void confirm_write(struct gatt_db_attribute *attr, int err, void *user_data);

#endif /* BTGATT_SERVER_H */