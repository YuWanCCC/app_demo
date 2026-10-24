// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  BlueZ - Bluetooth protocol stack for Linux
 *  HCI Event Handler Implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <endian.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "btgatt-server.h"

// HCI Monitor configuration
#define HCI_MONITOR_ENABLED     0

// HCI Command Status
#define HCI_SUCCESS             0x00

// HCI packet validation macros (only used in this file)
#define HCI_CMD_MIN_LEN         4  // HCI command minimum length
#define HCI_EVT_MIN_LEN         3  // HCI event minimum length
#define HCI_ACL_MIN_LEN         5  // HCI ACL minimum length

// HCI packet type validation
#define IS_VALID_HCI_CMD_LEN(len)    ((len) >= HCI_CMD_MIN_LEN)
#define IS_VALID_HCI_EVT_LEN(len)    ((len) >= HCI_EVT_MIN_LEN)
#define IS_VALID_HCI_ACL_LEN(len)    ((len) >= HCI_ACL_MIN_LEN)

// HCI Event Codes (only used in this file)
#define EVT_LE_META_EVENT               0x3E
#define EVT_CMD_STATUS                  0x0F
#define EVT_CMD_COMPLETE                0x0E
#define EVT_DISCONNECTION_COMPLETE      0x05
#define EVT_CONNECTION_COMPLETE         0x03
#define HCI_EVT_LE_PHY_UPDATE_COMPLETE  0x0C

// LE_LINK definition for Linux kernel compatibility
#ifndef LE_LINK
#define LE_LINK 0x80
#endif

// HCI Channel definitions
#ifndef HCI_CHANNEL_RAW
#define HCI_CHANNEL_RAW 0
#endif

#ifndef HCI_CHANNEL_MONITOR
#define HCI_CHANNEL_MONITOR 2
#endif

#ifndef HCI_DEV_NONE
#define HCI_DEV_NONE 0xffff
#endif

// Socket address structure for HCI
#ifndef AF_BLUETOOTH
#define AF_BLUETOOTH 31
#endif

#ifndef BTPROTO_HCI
#define BTPROTO_HCI 1
#endif

#ifndef SOL_HCI
#define SOL_HCI 0
#endif

#ifndef HCI_FILTER
#define HCI_FILTER 2
#endif

// Connection handle cache with timestamp
#define CONNECTION_TIMEOUT_SECONDS 10  // Maximum time to wait for connection handle

/* Event code for PHY update complete - using HCI_EVT_LE_PHY_UPDATE_COMPLETE defined above */

/* Command opcodes for PHY operations - Updated to match Bluetooth Core Specification */
#define OCF_LE_SET_PHY         0x0032    /* Set PHY preferences (Bluetooth 5.0+) */
#define OCF_LE_READ_PHY        0x0030    /* Read current PHY configuration (Bluetooth 5.0+) */
#define OCF_LE_SET_DEFAULT_PHY 0x0031    /* Set default PHY preferences (Bluetooth 5.0+) */

// HCI Command Complete Event structure
struct hci_rp_le_set_phy {
    uint8_t  status;
    uint16_t handle;
    uint8_t  tx_phy;
    uint8_t  rx_phy;
} __attribute__ ((packed));

/* LE Read PHY command parameters */
typedef struct {
    uint16_t handle;     /* Connection handle */
} __attribute__ ((packed)) le_read_phy_cp;

/* LE Read PHY response parameters */
typedef struct {
    uint8_t  status;     /* Command status */
    uint16_t handle;     /* Connection handle */
    uint8_t  tx_phy;     /* Current TX PHY */
    uint8_t  rx_phy;     /* Current RX PHY */
} __attribute__ ((packed)) le_read_phy_rp;

/* LE Set Default PHY command parameters */
typedef struct {
    uint8_t  all_phys;   /* All PHYs preferences (0x00 for no preferences) */
    uint8_t  tx_phys;    /* TX PHY preferences (1=1M, 2=2M, 4=Coded) */
    uint8_t  rx_phys;    /* RX PHY preferences (1=1M, 2=2M, 4=Coded) */
} __attribute__ ((packed)) le_btgatt_hci_set_default_phy_cp;

/* LE PHY definitions and structures */
typedef struct {
    uint16_t handle;     /* Connection handle */
    uint8_t  all_phys;   /* All PHYs preferences (0x00 for no preferences) */
    uint8_t  tx_phys;    /* TX PHY preferences (1=1M, 2=2M, 4=Coded) */
    uint8_t  rx_phys;    /* RX PHY preferences (1=1M, 2=2M, 4=Coded) */
    uint16_t phy_opts;   /* PHY options (0x0001=S2, 0x0002=S8 for Coded PHY) */
} __attribute__ ((packed)) le_set_phy_cp;

/* Global variables */
static const char *g_uart_dev = NULL;
static int g_hci_dev_id = -1;
uint16_t g_conn_handle = 0;
bool g_phy_configured = false;
static bool g_phy_config_running = false;
static bool g_hci_monitor_enabled = false;
static pthread_t phy_config_thread;


#if HCI_MONITOR_ENABLED

/* HCI packet types */
#define HCI_COMMAND_PKT     0x01
#define HCI_ACLDATA_PKT     0x02
#define HCI_SCODATA_PKT     0x03
#define HCI_EVENT_PKT       0x04
#define HCI_VENDOR_PKT      0xff

/**
 * HCI packet monitor function
 */
// HCI Monitor packet opcodes (from Linux kernel hci_mon.h)
#define HCI_MON_NEW_INDEX    0
#define HCI_MON_DEL_INDEX    1
#define HCI_MON_COMMAND_PKT  2
#define HCI_MON_EVENT_PKT    3
#define HCI_MON_ACL_TX_PKT   4
#define HCI_MON_ACL_RX_PKT   5
#define HCI_MON_SCO_TX_PKT   6
#define HCI_MON_SCO_RX_PKT   7
#define HCI_MON_OPEN_INDEX   8
#define HCI_MON_CLOSE_INDEX  9
#define HCI_MON_INDEX_INFO   10
#define HCI_MON_VENDOR_DIAG  11
#define HCI_MON_SYSTEM_NOTE  12
#define HCI_MON_USER_LOGGING 13
#define HCI_MON_CTRL_OPEN    14
#define HCI_MON_CTRL_CLOSE   15
#define HCI_MON_CTRL_COMMAND 16
#define HCI_MON_CTRL_EVENT   17
#define HCI_MON_ISO_TX_PKT   18
#define HCI_MON_ISO_RX_PKT   19

// HCI Monitor header structure
struct hci_mon_hdr {
    uint16_t opcode;
    uint16_t index;
    uint16_t len;
} __attribute__((packed));

/* HCI Monitor variables */
static int g_hci_monitor_fd = -1;
static bool g_hci_monitor_running = false;
static pthread_t hci_raw_monitor_thread;

#endif /* HCI_MONITOR_ENABLED */


/**
 * Convert PHY type value to human-readable string
 */
const char *get_phy_str(uint8_t phy) {
    switch (phy) {
        case PHY_1M_ENUM:    return "1M PHY (1 Mbps)";
        case PHY_2M_ENUM:    return "2M PHY (2 Mbps)";
        case PHY_CODED_ENUM: return "Coded PHY (125/500 kbps)";
        case PHY_CODED_BIT:  return "Coded PHY (125/500 kbps)";
        default:             return "Unknown PHY";
    }
}

#if HCI_MONITOR_ENABLED
/**
 * Get HCI command name from opcode
 */
static const char *get_hci_cmd_name(uint8_t ogf, uint16_t ocf) {
    // Special case for invalid/corrupted packets
    if (ogf == 0x00 && ocf == 0x0000) {
        return "Invalid_Command_Opcode";
    }
    
    switch (ogf) {
        case 0x01: // Link Control Commands
            switch (ocf) {
                case 0x0001: return "HCI_Inquiry";
                case 0x0002: return "HCI_Inquiry_Cancel";
                case 0x0003: return "HCI_Periodic_Inquiry_Mode";
                case 0x0004: return "HCI_Exit_Periodic_Inquiry_Mode";
                case 0x0005: return "HCI_Create_Connection";
                case 0x0006: return "HCI_Disconnect";
                case 0x0007: return "HCI_Add_SCO_Connection";
                case 0x0008: return "HCI_Accept_Connection_Request";
                case 0x0009: return "HCI_Reject_Connection_Request";
                case 0x000A: return "HCI_Link_Key_Request_Reply";
                case 0x000B: return "HCI_Link_Key_Request_Negative_Reply";
                case 0x000C: return "HCI_PIN_Code_Request_Reply";
                case 0x000D: return "HCI_PIN_Code_Request_Negative_Reply";
                case 0x000E: return "HCI_Change_Connection_Packet_Type";
                case 0x000F: return "HCI_Authentication_Requested";
                case 0x0010: return "HCI_Set_Connection_Encryption";
                case 0x0011: return "HCI_Change_Connection_Link_Key";
                case 0x0012: return "HCI_Master_Link_Key";
                case 0x0013: return "HCI_Remote_Name_Request";
                case 0x0014: return "HCI_Remote_Name_Request_Cancel";
                case 0x0015: return "HCI_Read_Remote_Supported_Features";
                case 0x0016: return "HCI_Read_Remote_Extended_Features";
                case 0x0017: return "HCI_Read_Remote_Version_Information";
                case 0x0018: return "HCI_Read_Clock_Offset";
                case 0x0019: return "HCI_Read_LMP_Handle";
                case 0x001C: return "HCI_Setup_Synchronous_Connection";
                case 0x001D: return "HCI_Accept_Synchronous_Connection_Request";
                case 0x001E: return "HCI_Reject_Synchronous_Connection_Request";
                case 0x0028: return "HCI_IO_Capability_Request_Reply";
                case 0x0029: return "HCI_User_Confirmation_Request_Reply";
                case 0x002A: return "HCI_User_Confirmation_Request_Negative_Reply";
                case 0x002B: return "HCI_User_Passkey_Request_Reply";
                case 0x002C: return "HCI_User_Passkey_Request_Negative_Reply";
                case 0x002D: return "HCI_Remote_OOB_Data_Request_Reply";
                case 0x002E: return "HCI_Remote_OOB_Data_Request_Negative_Reply";
                case 0x002F: return "HCI_IO_Capability_Request_Negative_Reply";
                case 0x0030: return "HCI_Create_Physical_Link";
                case 0x0031: return "HCI_Accept_Physical_Link";
                case 0x0032: return "HCI_Disconnect_Physical_Link";
                case 0x0033: return "HCI_Create_Logical_Link";
                case 0x0034: return "HCI_Accept_Logical_Link";
                case 0x0035: return "HCI_Disconnect_Logical_Link";
                case 0x0036: return "HCI_Logical_Link_Cancel";
                case 0x0037: return "HCI_Flow_Spec_Modify";
                case 0x0038: return "HCI_Enhanced_Setup_Synchronous_Connection";
                case 0x0039: return "HCI_Enhanced_Accept_Synchronous_Connection_Request";
                case 0x003A: return "HCI_Truncated_Page";
                case 0x003B: return "HCI_Truncated_Page_Cancel";
                case 0x003C: return "HCI_Set_Connectionless_Slave_Broadcast";
                case 0x003D: return "HCI_Set_Connectionless_Slave_Broadcast_Receive";
                case 0x003E: return "HCI_Start_Synchronization_Train";
                case 0x003F: return "HCI_Receive_Synchronization_Train";
                case 0x0040: return "HCI_Remote_OOB_Extended_Data_Request_Reply";
                default: return "Unknown_Link_Control_Command";
            }
            
        case 0x02: // Link Policy Commands
            switch (ocf) {
                case 0x0001: return "HCI_Hold_Mode";
                case 0x0002: return "HCI_Sniff_Mode";
                case 0x0003: return "HCI_Exit_Sniff_Mode";
                case 0x0004: return "HCI_Park_State";
                case 0x0005: return "HCI_Exit_Park_State";
                case 0x0006: return "HCI_QoS_Setup";
                case 0x0007: return "HCI_Role_Discovery";
                case 0x0008: return "HCI_Switch_Role";
                case 0x0009: return "HCI_Read_Link_Policy_Settings";
                case 0x000A: return "HCI_Write_Link_Policy_Settings";
                case 0x000B: return "HCI_Read_Default_Link_Policy_Settings";
                case 0x000C: return "HCI_Write_Default_Link_Policy_Settings";
                case 0x000D: return "HCI_Flow_Specification";
                case 0x000E: return "HCI_Sniff_Subrating";
                default: return "Unknown_Link_Policy_Command";
            }
            
        case 0x03: // Controller and Baseband Commands
            switch (ocf) {
                case 0x0001: return "HCI_Set_Event_Mask";
                case 0x0003: return "HCI_Reset";
                case 0x0005: return "HCI_Set_Event_Filter";
                case 0x0008: return "HCI_Flush";
                case 0x0009: return "HCI_Read_PIN_Type";
                case 0x000A: return "HCI_Write_PIN_Type";
                case 0x000B: return "HCI_Create_New_Unit_Key";
                case 0x000D: return "HCI_Read_Stored_Link_Key";
                case 0x0011: return "HCI_Write_Stored_Link_Key";
                case 0x0012: return "HCI_Delete_Stored_Link_Key";
                case 0x0013: return "HCI_Write_Local_Name";
                case 0x0014: return "HCI_Read_Local_Name";
                case 0x0015: return "HCI_Read_Connection_Accept_Timeout";
                case 0x0016: return "HCI_Write_Connection_Accept_Timeout";
                case 0x0017: return "HCI_Read_Page_Timeout";
                case 0x0018: return "HCI_Write_Page_Timeout";
                case 0x0019: return "HCI_Read_Scan_Enable";
                case 0x001A: return "HCI_Write_Scan_Enable";
                case 0x001B: return "HCI_Read_Page_Scan_Activity";
                case 0x001C: return "HCI_Write_Page_Scan_Activity";
                case 0x001D: return "HCI_Read_Inquiry_Scan_Activity";
                case 0x001E: return "HCI_Write_Inquiry_Scan_Activity";
                case 0x001F: return "HCI_Read_Authentication_Enable";
                case 0x0020: return "HCI_Write_Authentication_Enable";
                case 0x0021: return "HCI_Read_Encryption_Mode";
                case 0x0022: return "HCI_Write_Encryption_Mode";
                case 0x0023: return "HCI_Read_Class_of_Device";
                case 0x0024: return "HCI_Write_Class_of_Device";
                case 0x0025: return "HCI_Read_Voice_Setting";
                case 0x0026: return "HCI_Write_Voice_Setting";
                case 0x0027: return "HCI_Read_Automatic_Flush_Timeout";
                case 0x0028: return "HCI_Write_Automatic_Flush_Timeout";
                case 0x0029: return "HCI_Read_Num_Broadcast_Retransmissions";
                case 0x002A: return "HCI_Write_Num_Broadcast_Retransmissions";
                case 0x002B: return "HCI_Read_Hold_Mode_Activity";
                case 0x002C: return "HCI_Write_Hold_Mode_Activity";
                case 0x002D: return "HCI_Read_Transmit_Power_Level";
                case 0x002E: return "HCI_Read_Synchronous_Flow_Control_Enable";
                case 0x002F: return "HCI_Write_Synchronous_Flow_Control_Enable";
                case 0x0031: return "HCI_Set_Controller_To_Host_Flow_Control";
                case 0x0033: return "HCI_Host_Buffer_Size";
                case 0x0035: return "HCI_Host_Number_Of_Completed_Packets";
                case 0x0036: return "HCI_Read_Link_Supervision_Timeout";
                case 0x0037: return "HCI_Write_Link_Supervision_Timeout";
                case 0x0038: return "HCI_Read_Number_Of_Supported_IAC";
                case 0x0039: return "HCI_Read_Current_IAC_LAP";
                case 0x003A: return "HCI_Write_Current_IAC_LAP";
                case 0x003B: return "HCI_Read_Page_Scan_Period_Mode";
                case 0x003C: return "HCI_Write_Page_Scan_Period_Mode";
                case 0x003D: return "HCI_Read_Page_Scan_Mode";
                case 0x003E: return "HCI_Write_Page_Scan_Mode";
                case 0x003F: return "HCI_Set_AFH_Host_Channel_Classification";
                case 0x0042: return "HCI_Read_Inquiry_Scan_Type";
                case 0x0043: return "HCI_Write_Inquiry_Scan_Type";
                case 0x0044: return "HCI_Read_Inquiry_Mode";
                case 0x0045: return "HCI_Write_Inquiry_Mode";
                case 0x0046: return "HCI_Read_Page_Scan_Type";
                case 0x0047: return "HCI_Write_Page_Scan_Type";
                case 0x0048: return "HCI_Read_AFH_Channel_Assessment_Mode";
                case 0x0049: return "HCI_Write_AFH_Channel_Assessment_Mode";
                case 0x0051: return "HCI_Read_Extended_Inquiry_Response";
                case 0x0052: return "HCI_Write_Extended_Inquiry_Response";
                case 0x0053: return "HCI_Refresh_Encryption_Key";
                case 0x0055: return "HCI_Read_Simple_Pairing_Mode";
                case 0x0056: return "HCI_Write_Simple_Pairing_Mode";
                case 0x0057: return "HCI_Read_Local_OOB_Data";
                case 0x0058: return "HCI_Read_Inquiry_Response_Transmit_Power_Level";
                case 0x0059: return "HCI_Write_Inquiry_Transmit_Power_Level";
                case 0x005A: return "HCI_Read_Default_Erroneous_Data_Reporting";
                case 0x005B: return "HCI_Write_Default_Erroneous_Data_Reporting";
                case 0x005F: return "HCI_Enhanced_Flush";
                case 0x0060: return "HCI_Send_Keypress_Notification";
                case 0x0061: return "HCI_Read_Logical_Link_Accept_Timeout";
                case 0x0062: return "HCI_Write_Logical_Link_Accept_Timeout";
                case 0x0063: return "HCI_Set_Event_Mask_Page_2";
                case 0x0064: return "HCI_Read_Location_Data";
                case 0x0065: return "HCI_Write_Location_Data";
                case 0x0066: return "HCI_Read_Local_AMP_Info";
                case 0x0067: return "HCI_Read_Local_AMP_ASSOC";
                case 0x0068: return "HCI_Write_Remote_AMP_ASSOC";
                case 0x0069: return "HCI_Read_Flow_Control_Mode";
                case 0x006A: return "HCI_Write_Flow_Control_Mode";
                case 0x006B: return "HCI_Read_Data_Block_Size";
                case 0x006D: return "HCI_Enable_AMP_Receiver_Reports";
                case 0x006E: return "HCI_AMP_Test_End";
                case 0x006F: return "HCI_AMP_Test";
                case 0x0070: return "HCI_Write_LE_Host_Support";
                case 0x0071: return "HCI_Read_LE_Host_Support";
                case 0x0072: return "HCI_Set_MWS_Channel_Parameters";
                case 0x0073: return "HCI_Set_External_Frame_Configuration";
                case 0x0074: return "HCI_Set_MWS_Signaling";
                case 0x0075: return "HCI_Set_MWS_Transport_Layer";
                case 0x0076: return "HCI_Set_MWS_Scan_Frequency_Table";
                case 0x0077: return "HCI_Get_MWS_Transport_Layer_Configuration";
                case 0x0078: return "HCI_Set_MWS_PATTERN_Configuration";
                case 0x0079: return "HCI_Set_Triggered_Clock_Capture";
                case 0x007A: return "HCI_Truncated_Page";
                case 0x007B: return "HCI_Truncated_Page_Cancel";
                case 0x007C: return "HCI_Set_Connectionless_Slave_Broadcast";
                case 0x007D: return "HCI_Set_Connectionless_Slave_Broadcast_Receive";
                case 0x007E: return "HCI_Start_Synchronization_Train";
                case 0x007F: return "HCI_Receive_Synchronization_Train";
                case 0x0080: return "HCI_Set_Reserved_LT_ADDR";
                case 0x0081: return "HCI_Delete_Reserved_LT_ADDR";
                case 0x0082: return "HCI_Set_Connectionless_Slave_Broadcast_Data";
                case 0x0083: return "HCI_Read_Synchronization_Train_Parameters";
                case 0x0084: return "HCI_Write_Synchronization_Train_Parameters";
                case 0x0085: return "HCI_Remote_OOB_Extended_Data_Request_Reply";
                case 0x0086: return "HCI_Read_Secure_Connections_Host_Support";
                case 0x0087: return "HCI_Write_Secure_Connections_Host_Support";
                case 0x0088: return "HCI_Read_Authenticated_Payload_Timeout";
                case 0x0089: return "HCI_Write_Authenticated_Payload_Timeout";
                case 0x008A: return "HCI_Read_Local_OOB_Extended_Data";
                case 0x008B: return "HCI_Write_Secure_Connections_Test_Mode";
                case 0x008C: return "HCI_Read_Extended_Page_Timeout";
                case 0x008D: return "HCI_Write_Extended_Page_Timeout";
                case 0x008E: return "HCI_Read_Extended_Inquiry_Length";
                case 0x008F: return "HCI_Write_Extended_Inquiry_Length";
                case 0x0090: return "HCI_LE_Set_Host_Feature";
                default: return "Unknown_Controller_Baseband_Command";
            }
            
        case 0x04: // Informational Parameters Commands
            switch (ocf) {
                case 0x0001: return "HCI_Read_Local_Version_Information";
                case 0x0002: return "HCI_Read_Local_Supported_Commands";
                case 0x0003: return "HCI_Read_Local_Supported_Features";
                case 0x0004: return "HCI_Read_Local_Extended_Features";
                case 0x0005: return "HCI_Read_Buffer_Size";
                case 0x0007: return "HCI_Read_Country_Code";
                case 0x0009: return "HCI_Read_BD_ADDR";
                case 0x000A: return "HCI_Read_Data_Block_Size";
                case 0x000B: return "HCI_Read_Local_Supported_Codecs";
                case 0x000C: return "HCI_Read_Local_Simple_Pairing_Options";
                case 0x000D: return "HCI_Read_Local_Supported_Codecs_V2";
                default: return "Unknown_Informational_Command";
            }
            
        case 0x05: // Status Parameters Commands
            switch (ocf) {
                case 0x0001: return "HCI_Read_Failed_Contact_Counter";
                case 0x0002: return "HCI_Reset_Failed_Contact_Counter";
                case 0x0003: return "HCI_Read_Link_Quality";
                case 0x0005: return "HCI_Read_RSSI";
                case 0x0006: return "HCI_Read_AFH_Channel_Map";
                case 0x0007: return "HCI_Read_Clock";
                case 0x0008: return "HCI_Read_Encryption_Key_Size";
                case 0x0009: return "HCI_Read_Local_AMP_Info";
                case 0x000A: return "HCI_Read_Local_AMP_ASSOC";
                case 0x000B: return "HCI_Get_MWS_Transport_Layer_Configuration";
                case 0x000C: return "HCI_Set_Triggered_Clock_Capture";
                default: return "Unknown_Status_Command";
            }
            
        case 0x06: // Testing Commands
            switch (ocf) {
                case 0x0001: return "HCI_Read_Loopback_Mode";
                case 0x0002: return "HCI_Write_Loopback_Mode";
                case 0x0003: return "HCI_Enable_Device_Under_Test_Mode";
                case 0x0004: return "HCI_Write_Simple_Pairing_Debug_Mode";
                case 0x0007: return "HCI_Enable_AMP_Receiver_Reports";
                case 0x0008: return "HCI_AMP_Test_End";
                case 0x0009: return "HCI_AMP_Test";
                case 0x000A: return "HCI_Write_Secure_Connections_Test_Mode";
                default: return "Unknown_Testing_Command";
            }
            
        case 0x08: // LE Controller Commands
            switch (ocf) {
                case 0x0001: return "HCI_LE_Set_Event_Mask";
                case 0x0002: return "HCI_LE_Read_Buffer_Size";
                case 0x0003: return "HCI_LE_Read_Local_Supported_Features";
                case 0x0005: return "HCI_LE_Set_Random_Address";
                case 0x0006: return "HCI_LE_Set_Advertising_Parameters";
                case 0x0007: return "HCI_LE_Read_Advertising_Channel_TX_Power";
                case 0x0008: return "HCI_LE_Set_Advertising_Data";
                case 0x0009: return "HCI_LE_Set_Scan_Response_Data";
                case 0x000A: return "HCI_LE_Set_Advertise_Enable";
                case 0x000B: return "HCI_LE_Set_Scan_Parameters";
                case 0x000C: return "HCI_LE_Set_Scan_Enable";
                case 0x000D: return "HCI_LE_Create_Connection";
                case 0x000E: return "HCI_LE_Create_Connection_Cancel";
                case 0x000F: return "HCI_LE_Read_White_List_Size";
                case 0x0010: return "HCI_LE_Clear_White_List";
                case 0x0011: return "HCI_LE_Add_Device_To_White_List";
                case 0x0012: return "HCI_LE_Remove_Device_From_White_List";
                case 0x0013: return "HCI_LE_Connection_Update";
                case 0x0014: return "HCI_LE_Set_Host_Channel_Classification";
                case 0x0015: return "HCI_LE_Read_Channel_Map";
                case 0x0016: return "HCI_LE_Read_Remote_Used_Features";
                case 0x0017: return "HCI_LE_Encrypt";
                case 0x0018: return "HCI_LE_Rand";
                case 0x0019: return "HCI_LE_Start_Encryption";
                case 0x001A: return "HCI_LE_Long_Term_Key_Request_Reply";
                case 0x001B: return "HCI_LE_Long_Term_Key_Request_Negative_Reply";
                case 0x001C: return "HCI_LE_Read_Supported_States";
                case 0x001D: return "HCI_LE_Receiver_Test";
                case 0x001E: return "HCI_LE_Transmitter_Test";
                case 0x001F: return "HCI_LE_Test_End";
                case 0x0020: return "HCI_LE_Remote_Connection_Parameter_Request_Reply";
                case 0x0021: return "HCI_LE_Remote_Connection_Parameter_Request_Negative_Reply";
                case 0x0022: return "HCI_LE_Set_Data_Length";
                case 0x0023: return "HCI_LE_Read_Suggested_Default_Data_Length";
                case 0x0024: return "HCI_LE_Write_Suggested_Default_Data_Length";
                case 0x0025: return "HCI_LE_Read_Local_P256_Public_Key";
                case 0x0026: return "HCI_LE_Generate_DHKey";
                case 0x0027: return "HCI_LE_Add_Device_To_Resolving_List";
                case 0x0028: return "HCI_LE_Remove_Device_From_Resolving_List";
                case 0x0029: return "HCI_LE_Clear_Resolving_List";
                case 0x002A: return "HCI_LE_Read_Resolving_List_Size";
                case 0x002B: return "HCI_LE_Read_Peer_Resolvable_Address";
                case 0x002C: return "HCI_LE_Read_Local_Resolvable_Address";
                case 0x002D: return "HCI_LE_Set_Address_Resolution_Enable";
                case 0x002E: return "HCI_LE_Set_Resolvable_Private_Address_Timeout";
                case 0x002F: return "HCI_LE_Read_Maximum_Data_Length";
                case 0x0030: return "HCI_LE_Read_PHY";
                case 0x0031: return "HCI_LE_Set_Default_PHY";
                case 0x0032: return "HCI_LE_Set_PHY";
                case 0x0033: return "HCI_LE_Enhanced_Receiver_Test";
                case 0x0034: return "HCI_LE_Enhanced_Transmitter_Test";
                case 0x0035: return "HCI_LE_Set_Advertising_Set_Random_Address";
                case 0x0036: return "HCI_LE_Set_Extended_Advertising_Parameters";
                case 0x0037: return "HCI_LE_Set_Extended_Advertising_Data";
                case 0x0038: return "HCI_LE_Set_Extended_Scan_Response_Data";
                case 0x0039: return "HCI_LE_Set_Extended_Advertising_Enable";
                case 0x003A: return "HCI_LE_Read_Maximum_Advertising_Data_Length";
                case 0x003B: return "HCI_LE_Read_Number_of_Supported_Advertising_Sets";
                case 0x003C: return "HCI_LE_Remove_Advertising_Set";
                case 0x003D: return "HCI_LE_Clear_Advertising_Sets";
                case 0x003E: return "HCI_LE_Set_Periodic_Advertising_Parameters";
                case 0x003F: return "HCI_LE_Set_Periodic_Advertising_Data";
                case 0x0040: return "HCI_LE_Set_Periodic_Advertising_Enable";
                case 0x0041: return "HCI_LE_Set_Extended_Scan_Parameters";
                case 0x0042: return "HCI_LE_Set_Extended_Scan_Enable";
                case 0x0043: return "HCI_LE_Extended_Create_Connection";
                case 0x0044: return "HCI_LE_Periodic_Advertising_Create_Sync";
                case 0x0045: return "HCI_LE_Periodic_Advertising_Create_Sync_Cancel";
                case 0x0046: return "HCI_LE_Periodic_Advertising_Terminate_Sync";
                case 0x0047: return "HCI_LE_Add_Device_To_Periodic_Advertiser_List";
                case 0x0048: return "HCI_LE_Remove_Device_From_Periodic_Advertiser_List";
                case 0x0049: return "HCI_LE_Clear_Periodic_Advertiser_List";
                case 0x004A: return "HCI_LE_Read_Periodic_Advertiser_List_Size";
                case 0x004B: return "HCI_LE_Read_Transmit_Power";
                case 0x004C: return "HCI_LE_Read_RF_Path_Compensation";
                case 0x004D: return "HCI_LE_Write_RF_Path_Compensation";
                case 0x004E: return "HCI_LE_Set_Privacy_Mode";
                case 0x004F: return "HCI_LE_Read_Buffer_Size_v2";
                case 0x0050: return "HCI_LE_Read_ISO_TX_Sync";
                case 0x0051: return "HCI_LE_Set_CIG_Parameters";
                case 0x0052: return "HCI_LE_Set_CIG_Parameters_Test";
                case 0x0053: return "HCI_LE_Create_CIS";
                case 0x0054: return "HCI_LE_Remove_CIG";
                case 0x0055: return "HCI_LE_Accept_CIS_Request";
                case 0x0056: return "HCI_LE_Reject_CIS_Request";
                case 0x0057: return "HCI_LE_Create_BIG";
                case 0x0058: return "HCI_LE_Create_BIG_Test";
                case 0x0059: return "HCI_LE_Terminate_BIG";
                case 0x005A: return "HCI_LE_BIG_Create_Sync";
                case 0x005B: return "HCI_LE_BIG_Terminate_Sync";
                case 0x005C: return "HCI_LE_Request_Peer_SCA";
                case 0x005D: return "HCI_LE_Setup_ISO_Data_Path";
                case 0x005E: return "HCI_LE_Remove_ISO_Data_Path";
                case 0x005F: return "HCI_LE_ISO_Transmit_Test";
                case 0x0060: return "HCI_LE_ISO_Receive_Test";
                case 0x0061: return "HCI_LE_ISO_Read_Test_Counters";
                case 0x0062: return "HCI_LE_ISO_Test_End";
                case 0x0063: return "HCI_LE_Set_Host_Feature";
                case 0x0064: return "HCI_LE_Read_ISO_Link_Quality";
                case 0x0065: return "HCI_LE_Enhanced_Read_Transmit_Power_Level";
                case 0x0066: return "HCI_LE_Read_Remote_Transmit_Power_Level";
                case 0x0067: return "HCI_LE_Set_Path_Loss_Reporting_Parameters";
                case 0x0068: return "HCI_LE_Set_Path_Loss_Reporting_Enable";
                case 0x0069: return "HCI_LE_Set_Transmit_Power_Reporting_Enable";
                case 0x006A: return "HCI_LE_Transmitter_Test_v4";
                case 0x006B: return "HCI_LE_Set_Data_Related_Address_Changes";
                case 0x006C: return "HCI_LE_Set_Default_Subrate";
                case 0x006D: return "HCI_LE_Subrate_Request";
                default: return "Unknown_LE_Command";
            }
            
        default:
            return "Unknown_Command_Group";
    }
}

/**
 * Get HCI event name from event code
 */
static const char *get_le_subevent_name(uint8_t subevent_code) {
    switch (subevent_code) {
        case 0x01: return "LE_Connection_Complete";
        case 0x02: return "LE_Advertising_Report";
        case 0x03: return "LE_Connection_Update_Complete";
        case 0x04: return "LE_Read_Remote_Features_Complete";
        case 0x05: return "LE_Long_Term_Key_Request";
        case 0x06: return "LE_Remote_Connection_Parameter_Request";
        case 0x07: return "LE_Data_Length_Change";
        case 0x08: return "LE_Read_Local_P256_Public_Key_Complete";
        case 0x09: return "LE_Generate_DHKey_Complete";
        case 0x0A: return "LE_Enhanced_Connection_Complete";
        case 0x0B: return "LE_Directed_Advertising_Report";
        case 0x0C: return "LE_PHY_Update_Complete";
        case 0x0D: return "LE_Extended_Advertising_Report";
        case 0x0E: return "LE_Periodic_Advertising_Sync_Established";
        case 0x0F: return "LE_Periodic_Advertising_Report";
        case 0x10: return "LE_Periodic_Advertising_Sync_Lost";
        case 0x11: return "LE_Scan_Timeout";
        case 0x12: return "LE_Advertising_Set_Terminated";
        case 0x13: return "LE_Scan_Request_Received";
        case 0x14: return "LE_Channel_Selection_Algorithm";
        case 0x15: return "LE_Connectionless_IQ_Report";
        case 0x16: return "LE_Connection_IQ_Report";
        case 0x17: return "LE_CTE_Request_Failed";
        case 0x18: return "LE_Periodic_Advertising_Sync_Transfer_Received";
        case 0x19: return "LE_CIS_Established";
        case 0x1A: return "LE_CIS_Request";
        case 0x1B: return "LE_Create_BIG_Complete";
        case 0x1C: return "LE_Terminate_BIG_Complete";
        case 0x1D: return "LE_BIG_Sync_Established";
        case 0x1E: return "LE_BIG_Sync_Lost";
        case 0x1F: return "LE_Request_Peer_SCA_Complete";
        case 0x20: return "LE_Path_Loss_Threshold";
        case 0x21: return "LE_Transmit_Power_Reporting";
        case 0x22: return "LE_BIGInfo_Advertising_Report";
        case 0x23: return "LE_Subrate_Change";
        default: return "Unknown_LE_Subevent";
    }
}

static const char *get_hci_event_name(uint8_t event_code) {
    switch (event_code) {
        case 0x01: return "Inquiry_Complete";
        case 0x02: return "Inquiry_Result";
        case 0x03: return "Connection_Complete";
        case 0x04: return "Connection_Request";
        case 0x05: return "Disconnection_Complete";
        case 0x06: return "Authentication_Complete";
        case 0x07: return "Remote_Name_Request_Complete";
        case 0x08: return "Encryption_Change";
        case 0x09: return "Change_Connection_Link_Key_Complete";
        case 0x0A: return "Master_Link_Key_Complete";
        case 0x0B: return "Read_Remote_Supported_Features_Complete";
        case 0x0C: return "Read_Remote_Version_Information_Complete";
        case 0x0D: return "QoS_Setup_Complete";
        case 0x0E: return "Command_Complete";
        case 0x0F: return "Command_Status";
        case 0x10: return "Hardware_Error";
        case 0x11: return "Flush_Occurred";
        case 0x12: return "Role_Change";
        case 0x13: return "Number_Of_Completed_Packets";
        case 0x14: return "Mode_Change";
        case 0x15: return "Return_Link_Keys";
        case 0x16: return "PIN_Code_Request";
        case 0x17: return "Link_Key_Request";
        case 0x18: return "Link_Key_Notification";
        case 0x19: return "Loopback_Command";
        case 0x1A: return "Data_Buffer_Overflow";
        case 0x1B: return "Max_Slots_Change";
        case 0x1C: return "Read_Clock_Offset_Complete";
        case 0x1D: return "Connection_Packet_Type_Changed";
        case 0x1E: return "QoS_Violation";
        case 0x1F: return "Page_Scan_Mode_Change";
        case 0x20: return "Page_Scan_Repetition_Mode_Change";
        case 0x21: return "Flow_Specification_Complete";
        case 0x22: return "Inquiry_Result_with_RSSI";
        case 0x23: return "Read_Remote_Extended_Features_Complete";
        case 0x2C: return "Synchronous_Connection_Complete";
        case 0x2D: return "Synchronous_Connection_Changed";
        case 0x2E: return "Sniff_Subrating";
        case 0x2F: return "Extended_Inquiry_Result";
        case 0x30: return "Encryption_Key_Refresh_Complete";
        case 0x31: return "IO_Capability_Request";
        case 0x32: return "IO_Capability_Response";
        case 0x33: return "User_Confirmation_Request";
        case 0x34: return "User_Passkey_Request";
        case 0x35: return "Remote_OOB_Data_Request";
        case 0x36: return "Simple_Pairing_Complete";
        case 0x38: return "Link_Supervision_Timeout_Changed";
        case 0x39: return "Enhanced_Flush_Complete";
        case 0x3B: return "User_Passkey_Notification";
        case 0x3C: return "Keypress_Notification";
        case 0x3D: return "Remote_Host_Supported_Features_Notification";
        case 0x3E: return "LE_Meta_Event";
        case 0x40: return "Physical_Link_Complete";
        case 0x41: return "Channel_Selected";
        case 0x42: return "Disconnection_Physical_Link_Complete";
        case 0x43: return "Physical_Link_Loss_Early_Warning";
        case 0x44: return "Physical_Link_Recovery";
        case 0x45: return "Logical_Link_Complete";
        case 0x46: return "Disconnection_Logical_Link_Complete";
        case 0x47: return "Flow_Spec_Modify_Complete";
        case 0x48: return "Number_Of_Completed_Data_Blocks";
        case 0x49: return "AMP_Start_Test";
        case 0x4A: return "AMP_Test_End";
        case 0x4B: return "AMP_Receiver_Report";
        case 0x4C: return "Short_Range_Mode_Change_Complete";
        case 0x4D: return "AMP_Status_Change";
        case 0x4E: return "Triggered_Clock_Capture";
        case 0x4F: return "Synchronization_Train_Complete";
        case 0x50: return "Synchronization_Train_Received";
        case 0x51: return "Connectionless_Slave_Broadcast_Receive";
        case 0x52: return "Connectionless_Slave_Broadcast_Timeout";
        case 0x53: return "Truncated_Page_Complete";
        case 0x54: return "Slave_Page_Response_Timeout";
        case 0x55: return "Connectionless_Slave_Broadcast_Channel_Map_Change";
        case 0x56: return "Inquiry_Response_Notification";
        case 0x57: return "Authenticated_Payload_Timeout_Expired";
        case 0x58: return "SAM_Status_Change";
        default: return "Unknown_Event";
    }
}

/**
 * Parse and display HCI command packet
 */
static void parse_hci_command(const uint8_t *data, int len) {
    // Validate minimum packet length
    if (!IS_VALID_HCI_CMD_LEN(len)) {
        printf("[HCI] CMD_INVALID: Packet length=%d (minimum required: %d)\n", len, HCI_CMD_MIN_LEN);
        return;
    }
    
    // Extract OGF and OCF using predefined macros
    uint8_t ogf;
    uint16_t ocf;
    EXTRACT_OGF_OCF_FROM_CMD(data, ogf, ocf);
    uint8_t param_len = data[HCI_CMD_PARAM_LEN_OFFSET];
    
    // Validate OGF and OCF early
    if (!IS_VALID_OGF_OCF(ogf, ocf)) {
        if (ogf == 0 && ocf == 0) {
            printf("[HCI] CMD_ERROR: Invalid command (OGF=0, OCF=0) detected\n");
        } else if (!IS_VALID_OGF(ogf)) {
            printf("[HCI] CMD_ERROR: Invalid OGF=0x%02X (valid range: 0x01-0x3F, excluding reserved)\n", ogf);
        } else if (!IS_VALID_OCF(ocf)) {
            printf("[HCI] CMD_ERROR: Invalid OCF=0x%03X (valid range: 0x000-0x3FF)\n", ocf);
        } else if (IS_RESERVED_OGF(ogf)) {
            printf("[HCI] CMD_ERROR: Reserved OGF=0x%02X detected\n", ogf);
        }
        printf("[HCI] CMD_ERROR: Raw packet: ");
        for (int i = 0; i < (len < 8 ? len : 8); i++) {
            printf("0x%02X ", data[i]);
        }
        printf("(len=%d)\n", len);
        return;  // Early return for invalid commands
    }
    
    // Validate parameter length
    if (len < HCI_CMD_MIN_LEN + param_len) {
        printf("[HCI] CMD: %s (OGF=0x%02X, OCF=0x%03X) param_len=%d [TRUNCATED: expected %d bytes, got %d]\n", 
               get_hci_cmd_name(ogf, ocf), ogf, ocf, param_len, HCI_CMD_MIN_LEN + param_len, len);
        return;
    }
    
    printf("[HCI] CMD: %s (OGF=0x%02X, OCF=0x%03X) param_len=%d\n", 
           get_hci_cmd_name(ogf, ocf), ogf, ocf, param_len);
    
    // Show parameters for important commands
    if (len >= 4 + param_len && param_len > 0) {
        if (ogf == OGF_LE_CTL && ocf == OCF_LE_SET_PHY) { // LE Set PHY
            if (param_len >= 7) {
                uint16_t handle = data[HCI_CMD_PARAM_OFFSET] | (data[HCI_CMD_PARAM_OFFSET + 1] << 8);
                uint8_t all_phys = data[HCI_CMD_PARAM_OFFSET + 2];
                uint8_t tx_phys = data[HCI_CMD_PARAM_OFFSET + 3];
                uint8_t rx_phys = data[HCI_CMD_PARAM_OFFSET + 4];
                uint16_t phy_opts = data[HCI_CMD_PARAM_OFFSET + 5] | (data[HCI_CMD_PARAM_OFFSET + 6] << 8);
                printf("[HCI]   Handle: 0x%04X, All_PHYs: 0x%02X, TX_PHYs: 0x%02X, RX_PHYs: 0x%02X, Options: 0x%04X\n",
                      handle, all_phys, tx_phys, rx_phys, phy_opts);
            }
        } else if (ogf == OGF_LE_CTL && ocf == OCF_LE_READ_PHY) { // LE Read PHY
            if (param_len >= 2) {
                uint16_t handle = data[HCI_CMD_PARAM_OFFSET] | (data[HCI_CMD_PARAM_OFFSET + 1] << 8);
                printf("[HCI]   Handle: 0x%04X\n", handle);
            }
        } else if (ogf == OGF_LE_CTL && ocf == OCF_LE_SET_DEFAULT_PHY) { // LE Set Default PHY
            if (param_len >= 3) {
                uint8_t all_phys = data[HCI_CMD_PARAM_OFFSET];
                uint8_t tx_phys = data[HCI_CMD_PARAM_OFFSET + 1];
                uint8_t rx_phys = data[HCI_CMD_PARAM_OFFSET + 2];
                printf("[HCI]   All_PHYs: 0x%02X, TX_PHYs: 0x%02X, RX_PHYs: 0x%02X\n", all_phys, tx_phys, rx_phys);
            }
        }
    }
    
    // HCI command sent
}

/**
 * Parse and display HCI event packet
 */
static void parse_hci_event(const uint8_t *data, int len) {
    if (len < 2) {
        printf("[HCI] EVT: INVALID Packet length=%d\n", len);
        return;
    }
    
    uint8_t event_code = data[HCI_EVT_CODE_OFFSET];
        uint8_t param_len = data[HCI_EVT_PARAM_LEN_OFFSET];
    
    // Parse important events and build detailed information
    if (len >= 3 + param_len && param_len > 0) {
        if (event_code == EVT_CMD_COMPLETE) { // Command Complete
            if (param_len >= 3) {
                uint8_t num_packets = data[HCI_EVT_PARAM_OFFSET];
                // Extract OGF and OCF using predefined macros
                uint8_t ogf;
                uint16_t ocf;
                EXTRACT_OGF_OCF_FROM_EVENT(data, ogf, ocf);
                uint8_t status = (param_len > 3) ? data[HCI_EVT_CMD_COMPLETE_STATUS_OFFSET] : 0;
                
                // Validate OGF and OCF
                if (!IS_VALID_OGF_OCF(ogf, ocf)) {
                    printf("[HCI] EVT: INVALID CMD Complete (OGF=0x%02X, OCF=0x%03X), param_len=%d, Status: 0x%02X, Packets: %d\n",
                            ogf, ocf, param_len, status, num_packets);
                    return;
                }
                
                // Parse LE Read PHY response
                if (ogf == OGF_LE_CTL && ocf == OCF_LE_READ_PHY && param_len >= 7) {
                    uint16_t handle = data[HCI_EVT_PARAM_OFFSET + LE_READ_PHY_COMPLETE_HANDLE_OFFSET] | (data[HCI_EVT_PARAM_OFFSET + LE_READ_PHY_COMPLETE_HANDLE_OFFSET + 1] << 8);
                    uint8_t tx_phy = data[HCI_EVT_PARAM_OFFSET + LE_READ_PHY_COMPLETE_TX_PHY_OFFSET];
                    uint8_t rx_phy = data[HCI_EVT_PARAM_OFFSET + LE_READ_PHY_COMPLETE_RX_PHY_OFFSET];
                    printf("[HCI] EVT: %s Complete (OGF=0x%02X, OCF=0x%03X), param_len=%d, Status: 0x%02X, Packets: %d\n",
                            get_hci_cmd_name(ogf, ocf), ogf, ocf, param_len, status, num_packets);
                    printf("[HCI]   Handle: 0x%04X, TX_PHY: %s (0x%02X), RX_PHY: %s (0x%02X)\n",
                            handle, get_phy_str(tx_phy), tx_phy, get_phy_str(rx_phy), rx_phy);
                    return;
                } else {
                    printf("[HCI] EVT: %s Complete (OGF=0x%02X, OCF=0x%03X), param_len=%d, Status: 0x%02X, Packets: %d\n",
                            get_hci_cmd_name(ogf, ocf), ogf, ocf, param_len, status, num_packets);
                    return;
                }
            }
        } else if (event_code == EVT_CMD_STATUS) { // Command Status
            if (param_len >= 4) {
                uint8_t status = data[HCI_EVT_PARAM_OFFSET + HCI_EVT_CMD_STATUS_STATUS_OFFSET];
                uint8_t num_packets = data[HCI_EVT_PARAM_OFFSET + HCI_EVT_CMD_STATUS_NUM_PACKETS_OFFSET];
                // Extract OGF and OCF using predefined macros
                uint8_t ogf;
                uint16_t ocf;
                EXTRACT_OGF_OCF_FROM_OFFSET(data, HCI_EVT_PARAM_OFFSET + HCI_EVT_CMD_STATUS_OPCODE_OFFSET, ogf, ocf);
                
                // Validate OGF and OCF
                if (!IS_VALID_OGF_OCF(ogf, ocf)) {
                    printf("[HCI] EVT: INVALID_CMD Status (code=0x%02X, OGF=0x%02X, OCF=0x%03X), param_len=%d, Status: 0x%02X, Packets: %d\n",
                            event_code, ogf, ocf, param_len, status, num_packets);
                    printf("[HCI] EVT_ERROR: Command Status with invalid OGF/OCF detected\n");
                    return;
                } else {
                    printf("[HCI] EVT: %s Status (code=0x%02X, OGF=0x%02X, OCF=0x%03X), param_len=%d, Status: 0x%02X, Packets: %d\n",
                            get_hci_cmd_name(ogf, ocf), event_code, ogf, ocf, param_len, status, num_packets);
                    // Command Status事件表示命令已被接收并正在处理，Status字段表示命令是否被成功接收
                    if (status != 0x00) {
                        printf("[HCI]   Note: Command was not accepted for processing (Status indicates error)\n");
                    } else {
                        printf("[HCI]   Note: Command accepted and is being processed\n");
                    }
                    return;
                }
            }
        } else if (event_code == EVT_LE_META_EVENT) { // LE Meta Event
            if (param_len >= 1) {
                uint8_t subevent = data[HCI_EVT_PARAM_OFFSET + HCI_EVT_LE_META_SUBEVENT_OFFSET];
                
                if (subevent == 0x0C && param_len >= 5) { // LE PHY Update Complete
                    uint8_t status = data[HCI_EVT_PARAM_OFFSET + LE_PHY_UPDATE_COMPLETE_STATUS_OFFSET];
                    uint16_t handle = data[HCI_EVT_PARAM_OFFSET + LE_PHY_UPDATE_COMPLETE_HANDLE_OFFSET] | (data[HCI_EVT_PARAM_OFFSET + LE_PHY_UPDATE_COMPLETE_HANDLE_OFFSET + 1] << 8);
                    uint8_t tx_phy = data[HCI_EVT_PARAM_OFFSET + LE_PHY_UPDATE_COMPLETE_TX_PHY_OFFSET];
                    uint8_t rx_phy = data[HCI_EVT_PARAM_OFFSET + LE_PHY_UPDATE_COMPLETE_RX_PHY_OFFSET];
                    printf("[HCI] EVT: %s (code=0x%02X), param_len=%d, Status: 0x%02X (%s)\n", 
                           get_hci_event_name(event_code), event_code, param_len, status, 
                           (status == 0x00) ? "Success" : "Error");
                    printf("[HCI]   LE Subevent: %s (0x%02X), Handle: 0x%04X, TX_PHY: %s (0x%02X), RX_PHY: %s (0x%02X)\n",
                           get_le_subevent_name(subevent), subevent, handle, 
                           get_phy_str(tx_phy), tx_phy, get_phy_str(rx_phy), rx_phy);
                    return;
                } else if (subevent == 0x01 && param_len >= 18) { // LE Connection Complete
                    uint8_t status = data[HCI_EVT_PARAM_OFFSET + LE_CONNECTION_COMPLETE_STATUS_OFFSET];
                    uint16_t handle = data[HCI_EVT_PARAM_OFFSET + LE_CONNECTION_COMPLETE_HANDLE_OFFSET] | (data[HCI_EVT_PARAM_OFFSET + LE_CONNECTION_COMPLETE_HANDLE_OFFSET + 1] << 8);
                    uint8_t role = data[HCI_EVT_PARAM_OFFSET + LE_CONNECTION_COMPLETE_ROLE_OFFSET];
                    printf("[HCI] EVT: %s (code=0x%02X), param_len=%d, Status: 0x%02X (%s)\n", 
                           get_hci_event_name(event_code), event_code, param_len, status,
                           (status == 0x00) ? "Success" : "Error");
                    printf("[HCI]   LE Subevent: %s (0x%02X), Handle: 0x%04X, Role: %s\n",
                           get_le_subevent_name(subevent), subevent, handle, 
                           (role == 0) ? "Master" : "Slave");
                    return;
                } else if (subevent == 0x05 && param_len >= 12) { // LE Long Term Key Request
                    uint16_t handle = data[HCI_EVT_PARAM_OFFSET + LE_CONNECTION_COMPLETE_HANDLE_OFFSET] | (data[HCI_EVT_PARAM_OFFSET + LE_CONNECTION_COMPLETE_HANDLE_OFFSET + 1] << 8);
                    printf("[HCI] EVT: %s (code=0x%02X), param_len=%d\n", 
                           get_hci_event_name(event_code), event_code, param_len);
                    printf("[HCI]   LE Subevent: %s (0x%02X), Handle: 0x%04X\n",
                           get_le_subevent_name(subevent), subevent, handle);
                    return;
                } else {
                    printf("[HCI] EVT: %s (code=0x%02X), param_len=%d\n", 
                           get_hci_event_name(event_code), event_code, param_len);
                    printf("[HCI]   LE Subevent: %s (0x%02X)\n", get_le_subevent_name(subevent), subevent);
                    return;
                }            }
        } else if (event_code == EVT_DISCONNECTION_COMPLETE) { // Disconnection Complete
            if (param_len >= 4) {
                uint8_t status = data[HCI_EVT_PARAM_OFFSET + DISCONNECTION_COMPLETE_STATUS_OFFSET];
                uint16_t handle = data[HCI_EVT_PARAM_OFFSET + DISCONNECTION_COMPLETE_HANDLE_OFFSET] | (data[HCI_EVT_PARAM_OFFSET + DISCONNECTION_COMPLETE_HANDLE_OFFSET + 1] << 8);
                uint8_t reason = data[HCI_EVT_PARAM_OFFSET + DISCONNECTION_COMPLETE_REASON_OFFSET];
                printf("[HCI] EVT: %s (code=0x%02X), param_len=%d, Status: 0x%02X (%s)\n", 
                       get_hci_event_name(event_code), event_code, param_len, status,
                       (status == 0x00) ? "Success" : "Error");
                printf("[HCI]   Handle: 0x%04X, Reason: 0x%02X\n", handle, reason);
                return;
            }
        } else if (event_code == EVT_CONNECTION_COMPLETE) { // Connection Complete
            if (param_len >= 11) {
                uint8_t status = data[HCI_EVT_PARAM_OFFSET + CONNECTION_COMPLETE_STATUS_OFFSET];
                uint16_t handle = data[HCI_EVT_PARAM_OFFSET + CONNECTION_COMPLETE_HANDLE_OFFSET] | (data[HCI_EVT_PARAM_OFFSET + CONNECTION_COMPLETE_HANDLE_OFFSET + 1] << 8);
                printf("[HCI] EVT: %s (code=0x%02X), param_len=%d, Status: 0x%02X (%s)\n", 
                       get_hci_event_name(event_code), event_code, param_len, status,
                       (status == 0x00) ? "Success" : "Error");
                printf("[HCI]   Handle: 0x%04X\n", handle);
                return;
            }
        }
    }
    
    // Output single consolidated log line for other events
    printf("[HCI] EVT: %s (code=0x%02X), param_len=%d\n", get_hci_event_name(event_code), event_code, param_len);
}

/**
 * Parse and display HCI ACL data packet
 */
static void parse_hci_acl_data(const uint8_t *data, int len, bool is_tx) {
    if (len < 4) {
        printf("[HCI] ACL_%s: INVALID Packet length=%d\n", is_tx ? "TX" : "RX", len);
        return;
    }
    
    uint16_t handle_flags = data[HCI_ACL_HANDLE_OFFSET] | (data[HCI_ACL_HANDLE_OFFSET + 1] << 8);
    uint16_t handle = handle_flags & 0x0FFF;
    uint8_t pb_flag = (handle_flags >> 12) & 0x03;
    uint8_t bc_flag = (handle_flags >> 14) & 0x03;
    uint16_t data_len = data[HCI_ACL_DATA_LEN_OFFSET] | (data[HCI_ACL_DATA_LEN_OFFSET + 1] << 8);
    
    const char *pb_str = "Unknown";
    switch (pb_flag) {
        case 0x00: pb_str = "First_Fragment"; break;
        case 0x01: pb_str = "Continuing_Fragment"; break;
        case 0x02: pb_str = "Complete_PDU"; break;
        case 0x03: pb_str = "Reserved"; break;
    }
    
    printf("[HCI] ACL_%s: Handle=0x%04X PB=%s BC=%d DataLen=%d\n",
          is_tx ? "TX" : "RX", handle, pb_str, bc_flag, data_len);
    
    // ACL data packet processed
}

static void hci_monitor_packet(const uint8_t *data, int len) {
    if (!g_hci_monitor_enabled || len < 6) { // Need at least hci_mon_hdr size
        return;
    }
    
    // Parse HCI monitor header
    struct hci_mon_hdr *hdr = (struct hci_mon_hdr *)data;
    uint16_t opcode = le16toh(hdr->opcode);
    uint16_t index = le16toh(hdr->index);
    uint16_t payload_len = le16toh(hdr->len);
    
    // Skip monitor header to get actual HCI data
    const uint8_t *hci_data = data + 6;
    int hci_len = len - 6;
    
    // Validate payload length
    if (hci_len < (int)payload_len) {
        printf("[HCI] ERROR: Invalid payload length: expected %d, got %d\n", payload_len, hci_len);
        return;
    }
    
    switch (opcode) {
        case HCI_MON_COMMAND_PKT:
            if (hci_len >= 1) {
                // Reconstruct HCI command packet format
                uint8_t cmd_packet[256];
                cmd_packet[0] = HCI_COMMAND_PKT;
                if (hci_len < (int)(sizeof(cmd_packet) - 1)) {
                     memcpy(&cmd_packet[1], hci_data, hci_len);
                     parse_hci_command(cmd_packet, hci_len + 1);
                 } else {
                     printf("[HCI] CMD_ERROR: packet too large: %d bytes\n", hci_len);
                 }
            }
            break;
            
        case HCI_MON_EVENT_PKT:
            if (hci_len >= 1) {
                // Reconstruct HCI event packet format
                uint8_t evt_packet[256];
                evt_packet[0] = HCI_EVENT_PKT;
                if (hci_len < (int)(sizeof(evt_packet) - 1)) {
                     memcpy(&evt_packet[1], hci_data, hci_len);
                     parse_hci_event(evt_packet, hci_len + 1);
                 } else {
                     printf("[HCI] EVT_ERROR: packet too large: %d bytes\n", hci_len);
                 }
            }
            break;
            
        case HCI_MON_ACL_TX_PKT:
        case HCI_MON_ACL_RX_PKT:
            if (hci_len >= 1) {
                // Reconstruct HCI ACL packet format
                uint8_t acl_packet[256];
                acl_packet[0] = HCI_ACLDATA_PKT;
                if (hci_len < (int)(sizeof(acl_packet) - 1)) {
                     memcpy(&acl_packet[1], hci_data, hci_len);
                     parse_hci_acl_data(acl_packet, hci_len + 1, (opcode == HCI_MON_ACL_TX_PKT));
                 } else {
                     printf("[HCI] ACL_%s_ERROR: packet too large: %d bytes\n", 
                            (opcode == HCI_MON_ACL_TX_PKT) ? "TX" : "RX", hci_len);
                 }
            }
            break;
            
        case HCI_MON_SCO_TX_PKT:
        case HCI_MON_SCO_RX_PKT:
            printf("[HCI] SCO_%s: SCO data packet, len=%d\n", 
                   (opcode == HCI_MON_SCO_TX_PKT) ? "TX" : "RX", hci_len);
            break;
            
        case HCI_MON_OPEN_INDEX:
            printf("[HCI] OPEN_INDEX: %d\n", index);
            break;
            
        case HCI_MON_CLOSE_INDEX:
            printf("[HCI] CLOSE_INDEX: %d\n", index);
            break;
            
        case HCI_MON_INDEX_INFO:
            printf("[HCI] INDEX_INFO: %d\n", index);
            break;
            
        case HCI_MON_VENDOR_DIAG:
            printf("[HCI] VENDOR_DIAG: len=%d\n", hci_len);
            break;
            
        case HCI_MON_SYSTEM_NOTE:
            printf("[HCI] SYSTEM_NOTE: len=%d\n", hci_len);
            break;
            
        case HCI_MON_USER_LOGGING:
            printf("[HCI] USER_LOGGING: len=%d\n", hci_len);
            break;
            
        case HCI_MON_CTRL_OPEN:
            printf("[HCI] CTRL_OPEN: index=%d\n", index);
            break;
            
        case HCI_MON_CTRL_CLOSE:
            printf("[HCI] CTRL_CLOSE: index=%d\n", index);
            break;
            
        case HCI_MON_CTRL_COMMAND:
            if (hci_len >= 3) {
                uint8_t ogf, ocf;
                EXTRACT_OGF_OCF_FROM_OFFSET(data, 1, ogf, ocf);
                uint16_t cmd_opcode = HCI_DATA_TO_OPCODE(data, HCI_CMD_OPCODE_OFFSET);
        uint8_t param_len = (hci_len > 3) ? data[HCI_CMD_PARAM_LEN_OFFSET] : 0;
                printf("[HCI] CTRL_CMD: %s (opcode=0x%04X) param_len=%d len=%d\n", 
                       get_hci_cmd_name(ogf, ocf), cmd_opcode, param_len, hci_len);
            } else {
                printf("[HCI] CTRL_CMD: len=%d (insufficient data)\n", hci_len);
            }
            break;
            
        case HCI_MON_CTRL_EVENT:
            if (hci_len >= 3) {
                uint8_t event_code = data[HCI_EVT_CODE_OFFSET];
        uint8_t param_len = data[HCI_EVT_PARAM_LEN_OFFSET];
                
                // Parse important events with detailed info in single log
                if (event_code == EVT_CMD_COMPLETE && hci_len >= 7) { // Command Complete
                    uint8_t ogf, ocf;
                    EXTRACT_OGF_OCF_FROM_OFFSET(data, 4, ogf, ocf);
                    uint16_t evt_opcode = HCI_DATA_TO_OPCODE(data, HCI_EVT_CMD_COMPLETE_OPCODE_OFFSET);
                    uint8_t status = data[HCI_EVT_CMD_COMPLETE_STATUS_OFFSET];
                    printf("[HCI] CTRL_EVT: Command Complete - %s (opcode=0x%04X) status=0x%02X (%s) param_len=%d len=%d\n", 
                           get_hci_cmd_name(ogf, ocf), evt_opcode, status, (status == 0) ? "Success" : "Error", param_len, hci_len);
                } else if (event_code == EVT_CMD_STATUS && hci_len >= 7) { // Command Status
                    uint8_t status = data[HCI_EVT_PARAM_OFFSET];
                    uint8_t ogf, ocf;
                    EXTRACT_OGF_OCF_FROM_OFFSET(data, HCI_EVT_CMD_COMPLETE_OPCODE_OFFSET, ogf, ocf);
                    uint16_t evt_opcode = HCI_DATA_TO_OPCODE(data, HCI_EVT_CMD_COMPLETE_OPCODE_OFFSET);
                    printf("[HCI] CTRL_EVT: Command Status - %s (opcode=0x%04X) status=0x%02X (%s) param_len=%d len=%d\n", 
                           get_hci_cmd_name(ogf, ocf), evt_opcode, status, (status == 0) ? "Success" : "Error", param_len, hci_len);
                } else {
                    printf("[HCI] CTRL_EVT: %s (code=0x%02X) param_len=%d len=%d\n", 
                           get_hci_event_name(event_code), event_code, param_len, hci_len);
                }
            } else {
                printf("[HCI] CTRL_EVT: len=%d (insufficient data)\n", hci_len);
            }
            break;
            
        case HCI_MON_ISO_TX_PKT:
        case HCI_MON_ISO_RX_PKT:
            printf("[HCI] ISO_%s: len=%d\n", 
                   (opcode == HCI_MON_ISO_TX_PKT) ? "TX" : "RX", hci_len);
            break;
            
        case HCI_MON_NEW_INDEX:
            printf("[HCI] NEW_INDEX: %d\n", index);
            break;
            
        case HCI_MON_DEL_INDEX:
            printf("[HCI] DEL_INDEX: %d\n", index);
            break;
            
        default:
            printf("[HCI] UNKNOWN: opcode=0x%04X, index=%d, len=%d\n", 
                   opcode, index, payload_len);
            break;
    }
}

/**
 * HCI event monitor thread function (non-intrusive version)
 * 
 * This implementation uses HCI_CHANNEL_MONITOR to avoid interfering with
 * the normal HCI communication used by BlueZ. The monitor channel provides
 * a copy of all HCI traffic without consuming the original messages.
 */
static void *hci_raw_monitor_thread_func(void *arg)
{
    UNUSED(arg);
    int monitor_fd;
    struct sockaddr_hci addr;
    uint8_t buffer[1024];
    int len;
    
    PRLOG("HCI event monitor thread started\n");
    
    // Create HCI monitor socket (non-intrusive)
    monitor_fd = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
    if (monitor_fd < 0) {
        PRLOG("Failed to create HCI monitor socket: %s\n", strerror(errno));
        return NULL;
    }
    
    // Set up monitor socket address (btmon-style)
    memset(&addr, 0, sizeof(addr));
    addr.hci_family = AF_BLUETOOTH;
    addr.hci_dev = HCI_DEV_NONE;  // Monitor all devices like btmon does
    addr.hci_channel = HCI_CHANNEL_MONITOR;  // Use monitor channel to avoid interference
    
    // Bind to monitor channel
    if (bind(monitor_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        PRLOG("Failed to bind HCI monitor socket: %s\n", strerror(errno));
        PRLOG("Note: HCI monitor channel may not be supported on this kernel\n");
        PRLOG("Falling back to command-only monitoring\n");
        close(monitor_fd);
        
        // Fallback: Only monitor commands we send, not all HCI traffic
        g_hci_monitor_fd = -1;
        g_hci_monitor_running = true;
        
        PRLOG("HCI command-only monitor started (non-intrusive mode)\n");
        
        // Just wait for shutdown signal
        while (g_hci_monitor_running) {
            usleep(100000); // 100ms
        }
        
        PRLOG("HCI command-only monitor thread exiting\n");
        return NULL;
    }
    
    g_hci_monitor_fd = monitor_fd;
    g_hci_monitor_running = true;
    
    while (g_hci_monitor_running) {
        len = recv(monitor_fd, buffer, sizeof(buffer), MSG_DONTWAIT);
        if (len < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(10000); // 10ms
                continue;
            }
            PRLOG("HCI monitor socket recv error: %s\n", strerror(errno));
            break;
        }
        
        if (len > 0 && g_hci_monitor_enabled) {
            // Monitor HCI traffic (copy, not consuming original)
            // Note: is_tx parameter is not meaningful for monitor channel data
            // as direction is determined by the monitor opcode itself
            hci_monitor_packet(buffer, len);
        }
    }
    
    close(monitor_fd);
    g_hci_monitor_fd = -1;
    PRLOG("HCI monitor thread exiting\n");
    return NULL;
}

#endif /* HCI_MONITOR_ENABLED */


/**
 * Query connection handle from HCI as fallback
 */
static uint16_t query_connection_handle_from_hci(void)
{
    int fd;
    struct hci_conn_list_req *cl;
    struct hci_conn_info *ci;
    int i;
    uint16_t conn_handle = 0;
    
    PRLOG("Querying connection handle from HCI...\n");
    
    fd = hci_open_dev(g_hci_dev_id);
    if (fd < 0) {
        PRLOG("Failed to open HCI device %d for query\n", g_hci_dev_id);
        return 0;
    }
    
    cl = malloc(10 * sizeof(*ci) + sizeof(*cl));
    if (!cl) {
        PRLOG("Failed to allocate memory for connection list\n");
        hci_close_dev(fd);
        return 0;
    }
    
    cl->dev_id = g_hci_dev_id;
    cl->conn_num = 10;
    ci = cl->conn_info;
    
    if (ioctl(fd, HCIGETCONNLIST, (void *) cl) < 0) {
        PRLOG("Failed to get connection list\n");
        free(cl);
        hci_close_dev(fd);
        return 0;
    }
    
    // Reduce verbosity - only log when connection found or no connections
    if (cl->conn_num == 0) {
        PRLOG("No active connections found\n");
    } else {
        for (i = 0; i < cl->conn_num; i++, ci++) {
            // Look for LE connections (LE_LINK type = 0x80 or ACL_LINK type = 0x01)
            if ((ci->type == LE_LINK || ci->type == ACL_LINK) && ci->state == BT_CONNECTED) {
                conn_handle = ci->handle;
                PRLOG("Found active %s connection: handle=0x%04x\n", 
                      (ci->type == LE_LINK) ? "LE" : "ACL", conn_handle);
                break;
            }
        }
        
        if (conn_handle == 0) {
            PRLOG("No active LE/ACL connections found (checked %d connections)\n", cl->conn_num);
        }
    }
    
    free(cl);
    hci_close_dev(fd);
    
    if (conn_handle == 0) {
        PRLOG("No active LE connections found\n");
    }
    
    return conn_handle;
}

/**
 * Get current PHY configuration for a connection
 * 
 * Reads the current PHY settings for the specified connection handle
 * using HCI LE Read PHY command
 */
static int get_current_phy(uint16_t conn_handle, uint8_t *tx_phy, uint8_t *rx_phy) {
    int dd;
    int ret = -1;
    struct hci_request req;
    le_read_phy_cp cp;
    le_read_phy_rp rp;
    
    if (!tx_phy || !rx_phy || conn_handle == 0) {
        PRLOG("Invalid parameters for get_current_phy\n");
        return -1;
    }
    
    if ((dd = hci_open_dev(g_hci_dev_id)) < 0) {
        PRLOG("Failed to open HCI device %d: %s\n", g_hci_dev_id, strerror(errno));
        return -1;
    }
    
    cp.handle = htobs(conn_handle);
    
    memset(&req, 0, sizeof(req));
    req.ogf = OGF_LE_CTL;
    req.ocf = OCF_LE_READ_PHY;
    req.cparam = &cp;
    req.clen = sizeof(cp);
    req.rparam = &rp;
    req.rlen = sizeof(rp);
    
    PRLOG("Sending HCI LE Read PHY command (handle=0x%04x)...\n", conn_handle);
    if (hci_send_req(dd, &req, 1000) < 0) {
        PRLOG("Failed to send HCI LE Read PHY command: %s\n", strerror(errno));
        goto done;
    } else if (rp.status != 0) {
        PRLOG("HCI LE Read PHY command failed with status 0x%02x\n", rp.status);
        goto done;
    }
    
    *tx_phy = rp.tx_phy;
    *rx_phy = rp.rx_phy;
    ret = 0;

done:
    hci_close_dev(dd);
    return ret;
}

/**
  * Configure PHY settings for a connection
 * 
 * This function configures the PHY settings for a connection using HCI commands.
 * It supports 1M, 2M, and Coded PHY types with different options.
 * 
 * @param conn_handle Connection handle to configure
 * @param phy_type PHY type to configure (bitmask of MGMT_PHY_LE_* values)
 * @return 0 on success, negative value on failure
 */
static int set_current_phy(uint16_t conn_handle, uint32_t phy_type)
{
    int dd;
    struct hci_request req;
    int ret = 0;
    
    if (!phy_type) {
        return 0; // No PHY configuration requested
    }
    
    if ((dd = hci_open_dev(g_hci_dev_id)) < 0) {
        PRLOG("Failed to open HCI device %d for PHY configuration: %s\n",
              g_hci_dev_id, strerror(errno));
        return -1;
    }
    
    // Calculate TX and RX PHY values
    uint8_t calc_tx_phys = (phy_type & MGMT_PHY_LE_CODED_TX) ? PHY_CODED_BIT :
                          (phy_type & MGMT_PHY_LE_2M_TX) ? PHY_2M_BIT : PHY_1M_BIT;
    uint8_t calc_rx_phys = (phy_type & MGMT_PHY_LE_CODED_RX) ? PHY_CODED_BIT :
                          (phy_type & MGMT_PHY_LE_2M_RX) ? PHY_2M_BIT : PHY_1M_BIT;
    
    // Prepare PHY configuration parameters
    le_set_phy_cp phy_cp = {
        .handle = htobs(conn_handle),
        .all_phys = 0x00,
        .tx_phys = calc_tx_phys,
        .rx_phys = calc_rx_phys,
        .phy_opts = htobs(0x0000)  // auto mode for s=2(0x0001)/s=8(0x0002)
    };

    // LE Set PHY command uses Command Status response, not Command Complete
    evt_cmd_status cmd_status;
    memset(&req, 0, sizeof(req));
    req.ogf = OGF_LE_CTL;
    req.ocf = OCF_LE_SET_PHY;
    req.cparam = &phy_cp;
    req.clen = sizeof(le_set_phy_cp);
    req.rparam = &cmd_status;
    req.rlen = sizeof(cmd_status);
    req.event = EVT_CMD_STATUS;  // LE Set PHY returns Command Status, not Command Complete

    // Send HCI command with extended timeout for PHY operations
    PRLOG("Sending HCI LE Set PHY command (handle=0x%04x, tx_phys=0x%02x, rx_phys=0x%02x, phy_opts=0x%04x)\n", conn_handle, phy_cp.tx_phys, phy_cp.rx_phys, btohs(phy_cp.phy_opts));
    if (hci_send_req(dd, &req, 5000) < 0) {
        int err = errno;
        PRLOG("ERROR: failed to send HCI LE Set PHY command: %s (errno=%d)\n", strerror(err), err);
        ret = -2;
    } else if (cmd_status.status != 0) {
        const char *errstr;
        // Decode common HCI error codes
        switch (cmd_status.status) {
            case 0x01: errstr = "Unknown HCI Command"; break;
            case 0x02: errstr = "Unknown Connection Identifier"; break;
            case 0x0C: errstr = "Command Disallowed"; break;
            case 0x11: errstr = "Unsupported Feature or Parameter Value"; break;
            case 0x12: errstr = "Invalid HCI Command Parameters"; break;
            case 0x13: errstr = "Remote User Terminated Connection"; break;
            case 0x16: errstr = "Connection Terminated by Local Host"; break;
            case 0x3A: errstr = "Controller Busy"; break;
            default: errstr = "Unknown"; break;
        }
        PRLOG("HCI LE Set PHY command failed with status 0x%02x - %s\n",
              cmd_status.status, errstr);
        ret = -3;
    } else {
        // Show requested PHY configuration
        uint8_t requested_tx = (phy_type & MGMT_PHY_LE_CODED_TX) ? 0x03 :
                                (phy_type & MGMT_PHY_LE_2M_TX) ? 0x02 : 0x01;
        uint8_t requested_rx = (phy_type & MGMT_PHY_LE_CODED_RX) ? 0x03 :
                                (phy_type & MGMT_PHY_LE_2M_RX) ? 0x02 : 0x01;
        
        PRLOG("PHY change requested to: TX %s, RX %s\n",
                get_phy_str(requested_tx), get_phy_str(requested_rx));
        ret = 0;
    }

    hci_close_dev(dd);
    return ret;
}

/**
 * Check and configure PHY if connection is established but PHY not yet configured
 * This function should be called periodically to handle cases where
 * MGMT connected event occurs before HCI connection handle is available
 */
static void check_and_configure_phy(void)
{
    // Only proceed if we have an active connection but PHY not configured
    if (!g_config.is_connected || g_phy_configured || !g_config.phy_type) {
        return;
    }
    
    // Try to get connection handle if we don't have one yet
    if (g_conn_handle == 0) {
        uint16_t queried_handle = query_connection_handle_from_hci();
        if (queried_handle != 0) {
            g_conn_handle = queried_handle;
        } else {
            PRLOG("Connection handle not available yet, will retry later\n");
            return; // Try again later
        }
    }

    // Read current PHY settings before configuration
    uint8_t current_tx_phy = 0, current_rx_phy = 0;
    if (get_current_phy(g_conn_handle, &current_tx_phy, &current_rx_phy) == 0) {
        PRLOG("Current PHY before configuration:\n");
        PRLOG("  TX PHY: %s (0x%02x)\n", get_phy_str(current_tx_phy), current_tx_phy);
        PRLOG("  RX PHY: %s (0x%02x)\n", get_phy_str(current_rx_phy), current_rx_phy);
        
        // Check if PHY change is actually needed
        uint8_t requested_tx = (g_config.phy_type & MGMT_PHY_LE_CODED_TX) ? 0x03 :
                               (g_config.phy_type & MGMT_PHY_LE_2M_TX) ? 0x02 : 0x01;
        uint8_t requested_rx = (g_config.phy_type & MGMT_PHY_LE_CODED_RX) ? 0x03 :
                               (g_config.phy_type & MGMT_PHY_LE_2M_RX) ? 0x02 : 0x01;
        
        if (current_tx_phy == requested_tx && current_rx_phy == requested_rx) {
            PRLOG("PHY already at requested settings, no change needed\n");
            g_phy_configured = true;
            return;
        }
    } else {
        PRLOG("Failed to read current PHY settings, proceeding with configuration...\n");
    }
    
    // Configure PHY with the valid connection handle
    (void)set_current_phy(g_conn_handle, g_config.phy_type);
}

// Phy config thread function
static void *phy_config_thread_func(void *arg)
{
    UNUSED(arg);
    
    PRLOG("phy config thread started\n");
    
    while (g_phy_config_running) {
        // Sleep to avoid excessive CPU usage
        sleep(5);
        // Check and configure PHY if needed
        if (g_phy_config_running) {
            check_and_configure_phy();
        }
    }
    
    PRLOG("HCI management thread exiting\n");
    return NULL;
}

/**
 * Set default PHY preferences for future connections
 * 
 * This function sets the default PHY preferences that will be used for
 * future LE connections. Unlike set_current_phy, this doesn't require
 * an active connection handle.
 * 
 * @param phy_type PHY type to configure (bitmask of MGMT_PHY_LE_* values)
 * @return 0 on success, negative value on failure
 */
static int set_default_phy(uint32_t phy_type)
{
    int dd;
    uint8_t status;
    struct hci_request req;
    int ret = 0;
    
    if (!phy_type) {
        PRLOG("No default PHY configuration requested\n");
        return 0;
    }
    
    if ((dd = hci_open_dev(g_hci_dev_id)) < 0) {
        PRLOG("Failed to open HCI device %d for default PHY configuration: %s\n", g_hci_dev_id, strerror(errno));
        return -1;
    }
    
    // Prepare default PHY configuration parameters
    le_btgatt_hci_set_default_phy_cp phy_cp = {
        .all_phys = 0x00,  // No preference for all PHYs
        .tx_phys = (phy_type & MGMT_PHY_LE_CODED_TX) ? 0x04 :
                  (phy_type & MGMT_PHY_LE_2M_TX) ? 0x02 : 0x01,
        .rx_phys = (phy_type & MGMT_PHY_LE_CODED_RX) ? 0x04 :
                  (phy_type & MGMT_PHY_LE_2M_RX) ? 0x02 : 0x01
    };
    
    memset(&req, 0, sizeof(req));
    req.ogf = OGF_LE_CTL;
    req.ocf = OCF_LE_SET_DEFAULT_PHY;
    req.cparam = &phy_cp;
    req.clen = sizeof(le_btgatt_hci_set_default_phy_cp);
    req.rparam = &status;
    req.rlen = sizeof(status);
    
    PRLOG("Sending HCI LE Set Default PHY command...\n");
    PRLOG("  TX PHY: %s (0x%02x)\n", get_phy_str(phy_cp.tx_phys), phy_cp.tx_phys);
    PRLOG("  RX PHY: %s (0x%02x)\n", get_phy_str(phy_cp.rx_phys), phy_cp.rx_phys);
    
    if (hci_send_req(dd, &req, 2000) < 0) {
        int err = errno;
        PRLOG("Failed to send HCI LE Set Default PHY command: %s (errno=%d)\n", strerror(err), err);
        ret = -2;
    } else if (status != 0) {
        PRLOG("HCI LE Set Default PHY command failed with status 0x%02x\n", status);
        
        // Decode common HCI error codes
        switch (status) {
            case 0x01: PRLOG("Error: Unknown HCI Command\n"); break;
            case 0x0C: PRLOG("Error: Command Disallowed\n"); break;
            case 0x11: PRLOG("Error: Unsupported Feature or Parameter Value\n"); break;
            case 0x12: PRLOG("Error: Invalid HCI Command Parameters\n"); break;
            default: PRLOG("Error: Unknown status code 0x%02x\n", status); break;
        }
        
        ret = -3;
    } else {
        ret = 0;
    }
    
    hci_close_dev(dd);
    return ret;
}

static void hci_attach(const char *uart_dev)
{
    // Switch into ble netconf mode if it is not
    if (access("/sys/class/bluetooth/hci0", F_OK) != 0)
    {
        char cmd[256] = {0};
        PRLOG("switch to ble netconf mode...\n");
        system("killall rk_hciattach");
        system("killall btmcu_daemon");
        snprintf(cmd, sizeof(cmd), "echo -e -n \"\xFF\xFF\x02\x13\x01\" > %s", uart_dev);
        system(cmd);
        sleep(2);

        #ifdef BLUETOOTHD
        system("bt_start.sh 2");
        #else
        snprintf(cmd, sizeof(cmd), "rk_hciattach -n -s 115200 -m 1 %s rockchip 3000000 noflow nosleep 55:66:77:88:99:aa &", uart_dev);
        system(cmd);
        #endif
    }
}

static void hci_disattach(const char *uart_dev)
{
    PRLOG("resetting bt mcu to maskroom...\n");
    system("killall rk_hciattach");
    sleep(1);
    
    // Send HCI vendor specific reset command directly to UART
    // Command: 0x01 0x92 0xFC 0x20 followed by 32 bytes of 0x00
    unsigned char hci_reset_cmd[36] = {
        0x01, 0x92, 0xFC, 0x20,  // HCI command header
        // 32 bytes of 0x00 (automatically initialized to 0)
    };
    
    int fd = open(uart_dev, O_WRONLY);
    if (fd < 0) {
        PRLOG("ERROR: Failed to open UART device %s: %s\n", uart_dev, strerror(errno));
        return;
    }
    
    ssize_t written = write(fd, hci_reset_cmd, sizeof(hci_reset_cmd));
    if (written != sizeof(hci_reset_cmd)) {
        PRLOG("ERROR: Failed to write HCI reset command, written %zd bytes\n", written);
    } else {
        PRLOG("HCI reset command sent successfully\n");
    }
    
    close(fd);
}

static int hci_up(void)
{
    // Check if HCI device ID is valid
    if (g_hci_dev_id < 0) {
        PRLOG("ERROR: Invalid HCI device ID %d\n", g_hci_dev_id);
        return EXIT_FAILURE;
    }

    int dd = hci_open_dev(g_hci_dev_id);
    if (dd < 0) {
        PRLOG("ERROR: failed to open HCI device %d: %s\n", g_hci_dev_id, strerror(errno));
        return EXIT_FAILURE;
    }

    if (ioctl(dd, HCIDEVUP, 0) < 0 && errno != EALREADY) {
        PRLOG("ERROR: failed to bring up HCI device %d: %s\n", g_hci_dev_id, strerror(errno));
        hci_close_dev(dd);
        return EXIT_FAILURE;
    }
    hci_close_dev(dd);

    PRLOG("HCI device %d brought up successfully\n", g_hci_dev_id);
    return EXIT_SUCCESS;
}

static int hci_down(void)
{
    // Check if HCI device ID is valid
    if (g_hci_dev_id < 0) {
        PRLOG("ERROR: Invalid HCI device ID %d\n", g_hci_dev_id);
        return EXIT_FAILURE;
    }

    int dd = hci_open_dev(g_hci_dev_id);
    if (dd < 0) {
        PRLOG("ERROR: failed to open HCI device %d for shutdown: %s\n", g_hci_dev_id, strerror(errno));
        return EXIT_FAILURE;
    }

    if (ioctl(dd, HCIDEVDOWN, 0) < 0) {
        PRLOG("ERROR: failed to bring down HCI device %d: %s\n", g_hci_dev_id, strerror(errno));
        hci_close_dev(dd);
        return EXIT_FAILURE;
    }
    
    hci_close_dev(dd);
    PRLOG("HCI device %d brought down successfully\n", g_hci_dev_id);
    
    return EXIT_SUCCESS;
}

// Initialize HCI module with complete setup
int btgatt_hci_init(bdaddr_t *p_bdaddr)
{
    int ret;
    
    // Get and cache UART device
    g_uart_dev = getenv("RK_WIFI_BTMCU_TTYS");
    if (NULL == g_uart_dev || strlen(g_uart_dev) == 0) {
        PRLOG("ERROR: uart_dev is NULL\n");
        g_uart_dev = NULL;
        return -1;
    }
    PRLOG("uart_dev: '%s'\n", g_uart_dev);
    
    // Attach hci0 to btmcu
    PRLOG("Attaching hci0 to btmcu...\n");
    hci_attach(g_uart_dev);
    
    // Wait for hci0 appear
    PRLOG("Waiting for hci0 to appear...\n");
    int times = 10;
    while (times-- > 0 && access("/sys/class/bluetooth/hci0", F_OK)) {
        PRLOG("wait hci0 appear\n");
        sleep(1);
    }
    if (access("/sys/class/bluetooth/hci0", F_OK) != 0) {
        PRLOG("ERROR: hci0 failed to appear after waiting\n");
        return -1;
    }
    PRLOG("hci0 appear\n");
    
    // Set device ID to 0 (hci0) directly
    g_hci_dev_id = 0;
    PRLOG("Using HCI device ID %d (hci0)\n", g_hci_dev_id);
    
    // Up hci0 using the hci_up function
    if (hci_up() != EXIT_SUCCESS) {
        PRLOG("ERROR: Failed to bring up HCI device\n");
        return -1;
    }
    
#if HCI_MONITOR_ENABLED
    // Create HCI event monitor thread for packet capture
    g_hci_monitor_enabled = g_config.hci_monitor;
    if (g_hci_monitor_enabled) {
    ret = pthread_create(&hci_raw_monitor_thread, NULL, hci_raw_monitor_thread_func, NULL);
    if (ret != 0) {
        PRLOG("Failed to create HCI event monitor thread: %s\n", strerror(ret));
        PRLOG("Continuing without HCI packet monitoring...\n");
        g_hci_monitor_running = false;
        // Don't fail initialization if event monitoring fails
        }
    }
#else
    g_hci_monitor_enabled = false;
#endif
    PRLOG("HCI monitoring is %s\n", g_hci_monitor_enabled ? "enabled" : "disabled");

    // Configure adv interval
    PRLOG("set adv interval to 256ms...\n");
    system("echo 256 > /sys/kernel/debug/bluetooth/hci0/adv_min_interval");
    system("echo 256 > /sys/kernel/debug/bluetooth/hci0/adv_max_interval");
    
    // Get bdaddr
    PRLOG("Getting bdaddr...\n");
    if (hci_devba(g_hci_dev_id, p_bdaddr) < 0) {
        PRLOG("ERROR: get bdaddr failed for device %d\n", g_hci_dev_id);
        memset(p_bdaddr, 0, sizeof(bdaddr_t));
        return -1;
    }
    
    // Set default PHY preferences before accepting connections
    if (g_config.phy_type) {
        PRLOG("Setting default PHY preferences...\n");
        int phy_result = set_default_phy(g_config.phy_type);
        if (phy_result == 0) {
            PRLOG("Default PHY preferences configured successfully\n");
        } else {
            PRLOG("Warning: Failed to set default PHY preferences (error=%d)\n", phy_result);
            PRLOG("Continuing with connection attempt...\n");
        }
    } else {
        PRLOG("No PHY configuration specified, using controller defaults\n");
    }

    // Start PHY configuration thread
    g_phy_config_running = true;
    ret = pthread_create(&phy_config_thread, NULL, phy_config_thread_func, NULL);
    if (ret != 0) {
        PRLOG("Failed to create phy config thread: %s\n", strerror(ret));
        g_phy_config_running = false;
        return -1;
    }
    
    return 0;
}

void btgatt_hci_deinit(void)
{
    // Stop PHY configuration thread
    if (g_phy_config_running) {
        PRLOG("Stopping PHY configuration thread...\n");
        g_phy_config_running = false;
        
        // Give thread time to exit
        usleep(100000); // 100ms
    }
    
#if HCI_MONITOR_ENABLED
    // Stop HCI event monitor thread
    if (g_hci_monitor_running) {
        PRLOG("Stopping HCI event monitor thread...\n");
        g_hci_monitor_running = false;
        
        // Close monitor socket to unblock recv()
        if (g_hci_monitor_fd >= 0) {
            close(g_hci_monitor_fd);
            g_hci_monitor_fd = -1;
        }
        
        // Give thread time to exit
        usleep(100000); // 100ms
    }
#endif
    
    // Bring down HCI device before reset
    PRLOG("Bringing down HCI device...\n");
    hci_down();
    
    // Use cached UART device for disattach
    if (g_uart_dev != NULL) {
        // Reset bt mcu to maskroom
        PRLOG("Resetting BT MCU to maskroom...\n");
        hci_disattach(g_uart_dev);
        sleep(1);
    } else {
        PRLOG("Warning: uart_dev not available for disattach\n");
    }
    
    // Reset all HCI module state
    g_hci_dev_id = -1;
    g_phy_configured = false;
    g_conn_handle = 0;
    g_hci_monitor_enabled = true; // Reset to default
    PRLOG("HCI module deinitialized\n");
}

void btgatt_hci_clean_connection(void)
{
    // Reset PHY configuration state
    g_phy_configured = false;
    g_conn_handle = 0;
}