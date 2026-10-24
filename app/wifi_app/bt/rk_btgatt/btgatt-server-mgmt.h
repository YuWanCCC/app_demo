// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  BlueZ - Bluetooth protocol stack for Linux
 *  Management Interface Handler Header
 */

#ifndef BTGATT_SERVER_MGMT_HAND_H
#define BTGATT_SERVER_MGMT_HAND_H

#include <stdint.h>
#include <stdbool.h>
#include "lib/bluetooth.h"

// Forward declarations
struct mgmt;
struct bt_att;

// Function prototypes
void register_mgmt_callbacks(struct mgmt *mgmt, uint16_t index);

void cmd_setting(uint16_t op, uint8_t val);
void cmd_add_adv(void);
void cmd_rm_adv(void);
void cmd_privacy(void);
void cmd_io_cap(void);
void cmd_load_long_term_keys(void);
void cmd_load_irks(void);
const char* get_phy_type_description(uint32_t phy_mask);

#endif /* BTGATT_SERVER_MGMT_HAND_H */