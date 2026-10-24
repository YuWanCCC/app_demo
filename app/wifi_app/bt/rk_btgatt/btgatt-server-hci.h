// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  BlueZ - Bluetooth protocol stack for Linux
 *  HCI Event Handler Header
 */

#ifndef BTGATT_SERVER_HCI_HAND_H
#define BTGATT_SERVER_HCI_HAND_H

/*
 * HCI Opcode and Data Conversion Macros
 * 
 * This section provides unified macros for converting between:
 * 1. OGF/OCF pairs ↔ 16-bit opcode
 * 2. 16-bit opcode ↔ HCI packet data (little-endian format)
 * 3. OGF/OCF pairs ↔ HCI packet data (direct conversion)
 * 
 * Usage examples:
 *   // Convert OGF/OCF to opcode
 *   uint16_t opcode = HCI_OPCODE(0x08, 0x0032);
 * 
 *   // Extract OGF/OCF from opcode
 *   uint8_t ogf = HCI_OGF(opcode);
 *   uint16_t ocf = HCI_OCF(opcode);
 * 
 *   // Put opcode into HCI command data
 *   HCI_CMD_OPCODE_TO_DATA(cmd_data, 1, ogf, ocf);
 * 
 *   // Extract opcode from HCI data
 *   uint16_t opcode = HCI_DATA_TO_OPCODE(cmd_data, 1);
 * 
 *   // Direct OGF/OCF extraction from HCI data
 *   uint8_t ogf; uint16_t ocf;
 *   HCI_DATA_TO_OGF_OCF(cmd_data, 1, ogf, ocf);
 */

// Basic opcode construction and extraction
#define HCI_OPCODE(ogf, ocf)         ((uint16_t)(((uint16_t)(ogf) << 10) | (uint16_t)(ocf)))
#define HCI_OGF(opcode)              ((uint8_t)(((uint16_t)(opcode) >> 10) & 0x3F))
#define HCI_OCF(opcode)              ((uint16_t)((uint16_t)(opcode) & 0x3FF))

// HCI Command packet construction macros
// Converts OGF/OCF to HCI command data format (little-endian opcode)
#define HCI_CMD_OPCODE_TO_DATA(data, offset, ogf, ocf) \
    do { \
        uint16_t opcode = HCI_OPCODE(ogf, ocf); \
        (data)[(offset)] = (uint8_t)(opcode & 0xFF); \
        (data)[(offset) + 1] = (uint8_t)((opcode >> 8) & 0xFF); \
    } while(0)

// HCI data to opcode extraction macros
// Extracts opcode from HCI data format (little-endian)
#define HCI_DATA_TO_OPCODE(data, offset) \
    ((uint16_t)((data)[(offset)] | (((uint16_t)(data)[(offset) + 1]) << 8)))

// Combined OGF/OCF extraction from HCI data
#define HCI_DATA_TO_OGF_OCF(data, offset, ogf_var, ocf_var) \
    do { \
        uint16_t _hci_opcode = HCI_DATA_TO_OPCODE(data, offset); \
        (ogf_var) = HCI_OGF(_hci_opcode); \
        (ocf_var) = HCI_OCF(_hci_opcode); \
    } while(0)

// Convenience macros for common operations
#define HCI_CMD_HEADER_SIZE          4  // HCI command header size (type + opcode + length)
#define HCI_EVT_HEADER_SIZE          3  // HCI event header size (type + event + length)

// HCI packet field offsets
#define HCI_PKT_TYPE_OFFSET          0  // Packet type field offset
#define HCI_CMD_OPCODE_OFFSET        1  // Command opcode offset in HCI command
#define HCI_CMD_PARAM_LEN_OFFSET     3  // Command parameter length offset
#define HCI_CMD_PARAM_OFFSET         4  // Command parameters start offset
#define HCI_EVT_CODE_OFFSET          1  // Event code offset in HCI event
#define HCI_EVT_PARAM_LEN_OFFSET     2  // Event parameter length offset
#define HCI_EVT_PARAM_OFFSET         3  // Event parameters start offset
#define HCI_EVT_CMD_COMPLETE_OPCODE_OFFSET  4  // Opcode offset in Command Complete event
#define HCI_EVT_CMD_COMPLETE_STATUS_OFFSET  6  // Status offset in Command Complete event
#define HCI_ACL_HANDLE_OFFSET        1  // ACL handle offset in ACL data
#define HCI_ACL_DATA_LEN_OFFSET      3  // ACL data length offset
#define HCI_ACL_DATA_OFFSET          5  // ACL data start offset

// LE event parameter offsets (relative to event parameter start)
#define LE_PHY_UPDATE_COMPLETE_STATUS_OFFSET    1  // Status in LE PHY Update Complete (subevent + status)
#define LE_PHY_UPDATE_COMPLETE_HANDLE_OFFSET    2  // Handle in LE PHY Update Complete
#define LE_PHY_UPDATE_COMPLETE_TX_PHY_OFFSET    4  // TX PHY in LE PHY Update Complete
#define LE_PHY_UPDATE_COMPLETE_RX_PHY_OFFSET    5  // RX PHY in LE PHY Update Complete
#define LE_CONNECTION_COMPLETE_STATUS_OFFSET    1  // Status in LE Connection Complete
#define LE_CONNECTION_COMPLETE_HANDLE_OFFSET    2  // Handle in LE Connection Complete
#define LE_CONNECTION_COMPLETE_ROLE_OFFSET      4  // Role in LE Connection Complete
#define DISCONNECTION_COMPLETE_STATUS_OFFSET    0  // Status in Disconnection Complete
#define DISCONNECTION_COMPLETE_HANDLE_OFFSET    1  // Handle in Disconnection Complete
#define DISCONNECTION_COMPLETE_REASON_OFFSET    3  // Reason in Disconnection Complete
#define CONNECTION_COMPLETE_STATUS_OFFSET       0  // Status in Connection Complete
#define CONNECTION_COMPLETE_HANDLE_OFFSET       1  // Handle in Connection Complete

// LE Read PHY Command Complete event parameter offsets (relative to Command Complete parameters start)
#define LE_READ_PHY_COMPLETE_HANDLE_OFFSET      4  // Handle in LE Read PHY Complete (num_packets + opcode + status + handle)
#define LE_READ_PHY_COMPLETE_TX_PHY_OFFSET      6  // TX PHY in LE Read PHY Complete
#define LE_READ_PHY_COMPLETE_RX_PHY_OFFSET      7  // RX PHY in LE Read PHY Complete

// Command Status event parameter offsets (relative to event parameter start)
#define HCI_EVT_CMD_STATUS_STATUS_OFFSET        0  // Status in Command Status event
#define HCI_EVT_CMD_STATUS_NUM_PACKETS_OFFSET   1  // Number of packets in Command Status event
#define HCI_EVT_CMD_STATUS_OPCODE_OFFSET        2  // Opcode in Command Status event

// LE Meta Event parameter offsets (relative to event parameter start)
#define HCI_EVT_LE_META_SUBEVENT_OFFSET         0  // LE subevent code offset

// Validate opcode components
#define HCI_IS_VALID_OPCODE(ogf, ocf) \
    (IS_VALID_OGF(ogf) && IS_VALID_OCF(ocf) && !IS_RESERVED_OGF(ogf))

// OGF and OCF validation macros
#define IS_VALID_OGF(ogf)            ((ogf) >= 0x01 && (ogf) <= 0x3F)
#define IS_VALID_OCF(ocf)            ((ocf) <= 0x3FF)
#define IS_RESERVED_OGF(ogf)         ((ogf) == 0x07 || ((ogf) >= 0x09 && (ogf) <= 0x3E))
#define IS_VALID_OGF_OCF(ogf, ocf)   (IS_VALID_OGF(ogf) && IS_VALID_OCF(ocf) && !IS_RESERVED_OGF(ogf))

// HCI Operation Group Fields (OGF) - only commonly used ones
#define OGF_LE_CTL                   0x08

// HCI Operation Command Fields (OCF) - only actually used ones
#define OCF_LE_READ_PHY              0x0030
#define OCF_LE_SET_DEFAULT_PHY       0x0031
#define OCF_LE_SET_PHY               0x0032

// Generic macro for extracting OGF and OCF from HCI data at any offset
// Uses the unified opcode conversion logic
#define EXTRACT_OGF_OCF_FROM_OFFSET(data, offset, ogf_var, ocf_var) \
    HCI_DATA_TO_OGF_OCF(data, offset, ogf_var, ocf_var)

// Individual extraction macros using unified logic
#define EXTRACT_OCF_FROM_OFFSET(data, offset) \
    HCI_OCF(HCI_DATA_TO_OPCODE(data, offset))

#define EXTRACT_OGF_FROM_OFFSET(data, offset) \
    HCI_OGF(HCI_DATA_TO_OPCODE(data, offset))

// Convenience macros for specific packet types
#define EXTRACT_OGF_OCF_FROM_CMD(data, ogf_var, ocf_var) \
    HCI_DATA_TO_OGF_OCF(data, HCI_CMD_OPCODE_OFFSET, ogf_var, ocf_var)

#define EXTRACT_OGF_OCF_FROM_EVENT(data, ogf_var, ocf_var) \
    HCI_DATA_TO_OGF_OCF(data, HCI_EVT_CMD_COMPLETE_OPCODE_OFFSET, ogf_var, ocf_var)

// PHY type constants for setting PHY (bitmask format)
#define PHY_1M_BIT     0x01    /* LE 1M PHY (1 Mbps) */
#define PHY_2M_BIT     0x02    /* LE 2M PHY (2 Mbps) */
#define PHY_CODED_BIT  0x04    /* LE Coded PHY (125/500 kbps) */

// PHY type constants for reading PHY (enumeration format)
#define PHY_1M_ENUM     0x01    /* LE 1M PHY (1 Mbps) */
#define PHY_2M_ENUM     0x02    /* LE 2M PHY (2 Mbps) */
#define PHY_CODED_ENUM  0x03    /* LE Coded PHY (125/500 kbps) */

// Global variables
extern int g_hci_fd;  // Global HCI file descriptor

int btgatt_hci_init(bdaddr_t *p_bdaddr);
void btgatt_hci_deinit(void);
void btgatt_hci_clean_connection(void);

#endif /* BTGATT_SERVER_HCI_HAND_H */