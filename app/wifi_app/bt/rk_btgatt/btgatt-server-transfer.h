// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  BlueZ - Bluetooth protocol stack for Linux
 *  GATT Server Transfer Service Header
 */

#ifndef BTGATT_SERVER_TRANSFER_H
#define BTGATT_SERVER_TRANSFER_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

// Socket path for local communication
#define BTGATT_TRANS_SOCK_PATH "/tmp/rk_btgatt_trans_sock"

// Service initialization and management
void populate_transfer_service(struct server *server);  // server can be NULL

#endif /* BTGATT_SERVER_TRANSFER_H */