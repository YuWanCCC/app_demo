#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <paths.h>
#include <signal.h>
#include <dirent.h>
#include <sys/eventfd.h>
#include <fcntl.h>
#include <poll.h>
#include "slog.h"
#include "utility.h"
#include "../utility/uart/uart.h"

#define LOG_NONE    0
#define LOG_ERROR   1
#define LOG_WARN    2
#define LOG_INF     3
#define LOG_DBG     4
#define LOG_LEVEL   LOG_INF
#define LOG_DISPLAY(level, fmt, args...) do { \
            if (LOG_LEVEL >= (level)) { \
                if (LOG_LEVEL == LOG_ERROR) { \
                    pr_err(fmt, ##args); \
                } else { \
                    pr_info(fmt, ##args); \
                } \
            } \
        } while (0)
#define LOG_E(args...) LOG_DISPLAY(LOG_ERROR, ##args)
#define LOG_W(args...) LOG_DISPLAY(LOG_WARN, ##args)
#define LOG_I(args...) LOG_DISPLAY(LOG_INF, ##args)
#define LOG_D(args...) LOG_DISPLAY(LOG_DBG, ##args)

#define NETCONF_PROCESS  "./oem/usr/bin/rkwifibt_app_test Network"
#define RX_BUFFER_SIZE 32
#define HEAD_CODE 0xFFFFul
#define HEAD_LEN 2
#define LENGTH_LEN 1
#define CMD_OFFSET 0
#define CMD_LEN 1

#define UART_CMD_WAKEUP             0x00
#define UART_CMD_HEARTBEAT          0x01
#define UART_CMD_GET_VERSION        0x03
#define UART_CMD_GET_PIRTH          0x04
#define UART_CMD_SET_PIRTH          0x05
#define UART_CMD_GET_BAT            0x06
#define UART_CMD_SET_RTC            0x10
#define UART_CMD_GET_RTC            0x11
#define UART_CMD_SET_CHARGE_STATE   0x14
#define UART_CMD_DEEPSLEEP          0x18

#define UART_EVENT_ACK              0x00
#define UART_EVENT_KEY              0x01
#define UART_EVENT_VERSION          0x03
#define UART_EVENT_PIRTH            0x04
#define UART_EVENT_PIR              0x05
#define UART_EVENT_BAT              0x06
#define UART_EVENT_WAKEUP_REASON    0x07
#define UART_EVENT_BREAK_REASON     0x08
#define UART_EVENT_USB              0x09
#define UART_EVENT_NET_ERROR        0x0A
#define UART_EVENT_DISCOVER_AP      0x0B
#define UART_EVENT_RTC              0x11
#define UART_EVENT_RECOVER_WIFI     0x16
#define UART_EVENT_SLEEP_TIMEOUT    0x17
#define UART_EVENT_EXCEPTION        0xFF

#define KEY_NET_PUSH        (1)
#define KEY_RESET_PUSH      (2)
#define KEY_PROTECT_PUSH    (3)
#define KEY_DOORBELL_PUSH   (4)
#define KEY_USER_PUSH       (5)
#define RK962_RECOVERY_EVENTFD_VALUE  0x5A5A5B5B

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define WAKEUP_REASON_NORMAL                    0x00
#define WAKEUP_REASON_REACH_MAX_COUNTS          0x01
#define WAKEUP_REASON_SERVER_DISCONNECT         0x02
#define WAKEUP_REASON_TCP_KA_KEY_RENEW          0x03
#define WAKEUP_REASON_AP_KA_KEY_RENEW           0x05
#define WAKEUP_REASON_IPADDR_CHANGED            0x06
#define WAKEUP_REASON_DEVICEASSOC_FAILURE       0x08
#define WAKEUP_REASON_DEEPSLEEP_SCAN_FOUND      0x09
#define WAKEUP_REASON_TCP_KA_BCN_LOST           0x0A
#define WAKEUP_REASON_CONNECT_SERVER_FAILED     0x0B
#define WAKEUP_REASON_AP_KA_BCN_LOST            0x0C

//exception return value
enum{
    EXCEPTION_OTHER         = -1,
    EXCEPTION_NORMAL        = 0,
    EXCEPTION_RST_KPAL      = 1,
    EXCEPTION_RST_ACT       = 2,
    EXCEPTION_ACCERT        = 3,
    EXCEPTION_STACK         = 4,
    EXCEPTION_HARD_FAULT    = 5,
    EXCEPTION_WIFI_WRITE    = 6,
};

// Host wakeup reason description strings
static const char *wakeup_reason_desc[] = {
    [WAKEUP_REASON_NORMAL]                  = "0x00 - wakeup by server",
    [WAKEUP_REASON_REACH_MAX_COUNTS]        = "0x01 - retransmission failure",
    [WAKEUP_REASON_SERVER_DISCONNECT]       = "0x02 - server released connection",
    [WAKEUP_REASON_TCP_KA_KEY_RENEW]        = "0x03 - key renew when TCP keep-alive",
    [4]                                     = "0x04 - unknown",
    [WAKEUP_REASON_AP_KA_KEY_RENEW]         = "0x05 - key renew when AP keep-alive",
    [WAKEUP_REASON_IPADDR_CHANGED]          = "0x06 - IP address changed",
    [7]                                     = "0x07 - unknown",
    [WAKEUP_REASON_DEVICEASSOC_FAILURE]     = "0x08 - association failure",
    [WAKEUP_REASON_DEEPSLEEP_SCAN_FOUND]    = "0x09 - discovered AP",
    [WAKEUP_REASON_TCP_KA_BCN_LOST]         = "0x0A - beacon lost when TCP keep-alive",
    [WAKEUP_REASON_CONNECT_SERVER_FAILED]   = "0x0B - connect to server failed",
    [WAKEUP_REASON_AP_KA_BCN_LOST]          = "0x0C - beacon lost when AP keep-alive",
};

/**
 * Get host wakeup reason description string
 * @param reason: wakeup reason code
 * @return: description string, or "Unknown reason" if not found
 */
static const char *get_wakeup_reason_desc(uint8_t reason)
{
    if (reason < ARRAY_SIZE(wakeup_reason_desc) && wakeup_reason_desc[reason] != NULL) {
        return wakeup_reason_desc[reason];
    }
    else {
        LOG_E("Unknown wakeup reason: 0x%02x", reason);
        return "Unknown wakeup reason";
    }
}

#define UART_SOCKET_PATH "/tmp/rkuart_socket"
#define RK_CMD_LINE_LEN 1600
#define MAX_ARGS 10

// Command to device
typedef struct
{
    uint16_t code;
    uint8_t len;
    uint8_t type;       // command type
    uint8_t data[10];   // parameters
} host_comm_t;

// Event structure
struct event_desc
{
    uint32_t event; //event type
    void *cb;   //callback function for event
};

// Response type enumeration
typedef enum {
    RESP_NONE,    // No response needed (e.g., heartbeat)
    RESP_ACK,     // Only ACK response needed (e.g., setpirth)
    RESP_INFO     // Detailed information response needed (e.g., getversion)
} response_type_t;

// Command structure
typedef struct {
    const char *cmd;        // name
    const char *desc;       // description
    bool (*exec)(int argc, char *argv[]);  // handler
    response_type_t resp_type;  // type of response expected
} command_t;

// Global variables for response handling
static struct {
    char message[512];      // Response message
    bool is_ready;          // Flag to indicate response is ready
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    uint8_t expect_evt;     // Expected event code for response
} g_response = {
    .message = "",
    .is_ready = false,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .cond = PTHREAD_COND_INITIALIZER,
    .expect_evt = 0xFF      // 0xFF means not expecting any event
};

static pthread_t btmcu_daemon_thread = 0;
static pthread_t btmcu_heartbeat_thread = 0;
static pthread_t uart_rx_thread = 0;
static pthread_t wifi_event_thread = 0;

static uint8_t rx_buf[RX_BUFFER_SIZE];
static bool g_ap_kpalive = false;
static const char *g_uart_dev = NULL;
static int g_uart_fd = -1;


//-------------------------------------------------------------------------
// UART functions
//-------------------------------------------------------------------------

static void msg_tx_type(uint8_t type)
{
    switch (type)
    {
    case UART_CMD_GET_VERSION:
        LOG_I("Tx UART_CMD_GET_VERSION, type=0x%X.\n", type);
        break;
    case UART_CMD_HEARTBEAT:
        LOG_D("Tx UART_CMD_HEARTBEAT, type=0x%X.\n", type);
        break;
    case UART_CMD_GET_PIRTH:
        LOG_I("Tx UART_CMD_GET_PIRTH, type=0x%X.\n", type);
        break;
    case UART_CMD_SET_PIRTH:
        LOG_I("Tx UART_CMD_SET_PIRTH, type=0x%X.\n", type);
        break;
    case UART_CMD_GET_BAT:
        LOG_I("Tx UART_CMD_GET_BAT, type=0x%X.\n", type);
        break;
    case UART_CMD_SET_RTC:
        LOG_I("Tx UART_CMD_SET_RTC, type=0x%X.\n", type);
        break;
    case UART_CMD_GET_RTC:
        LOG_I("Tx UART_CMD_GET_RTC, type=0x%X.\n", type);
        break;
    case UART_CMD_SET_CHARGE_STATE:
        LOG_I("Tx UART_CMD_SET_CHARGE_STATE, type=0x%X.\n", type);
        break;
    case UART_CMD_DEEPSLEEP:
        LOG_I("Tx UART_CMD_DEEPSLEEP, type=0x%X.\n", type);
        break;
    default:
        LOG_E("Invalid uart command 0x%X.\n", type);
        break;
    }
}

static void msg_rx_type(uint8_t type)
{
    switch (type)
    {
    case UART_EVENT_ACK:
        LOG_I("UART_EVENT_ACK, type=0x%X.\n", type);
        break;
    case UART_EVENT_KEY:
        LOG_I("UART_EVENT_KEY, type=0x%X.\n", type);
        break;
    case UART_EVENT_VERSION:
        LOG_I("UART_EVENT_VERSION, type=0x%X.\n", type);
        break;
    case UART_EVENT_PIRTH:
        LOG_I("UART_EVENT_PIRTH, type=0x%X.\n", type);
        break;
    case UART_EVENT_PIR:
        LOG_I("UART_EVENT_PIR, type=0x%X.\n", type);
        break;
    case UART_EVENT_BAT:
        LOG_I("UART_EVENT_BAT, type=0x%X.\n", type);
        break;
    case UART_EVENT_WAKEUP_REASON:
        LOG_I("UART_EVENT_WAKEUP_REASON, type=0x%X.\n", type);
        break;
    case UART_EVENT_BREAK_REASON:
        LOG_I("UART_EVENT_BREAK_REASON, type=0x%X.\n", type);
        break;
    case UART_EVENT_USB:
        LOG_I("UART_EVENT_USB, type=0x%X.\n", type);
        break;
    case UART_EVENT_NET_ERROR:
        LOG_I("UART_EVENT_NET_ERROR, type=0x%X.\n", type);
        break;
    case UART_EVENT_RTC:
        LOG_I("UART_EVENT_RTC, type=0x%X.\n", type);
        break;
    case UART_EVENT_EXCEPTION:
        LOG_I("UART_EVENT_EXCEPTION, type=0x%X.\n", type);
        break;
    case UART_EVENT_DISCOVER_AP:
        LOG_I("UART_EVENT_DISCOVER_AP, type=0x%X.\n", type);
        break;
    case UART_EVENT_RECOVER_WIFI:
        LOG_I("UART_EVENT_RECOVER_WIFI, type=0x%X.\n", type);
        break;
    case UART_EVENT_SLEEP_TIMEOUT:
        LOG_I("UART_EVENT_SLEEP_TIMEOUT, type=0x%X.\n", type);
        break;
    default:
        LOG_E("Unkown uart event 0x%X.\n", type);
        break;
    }
}

static int rk_uart_init(int fd, struct uart_t *u, struct termios *ti)
{
    UNUSED(u); UNUSED(ti);
    LOG_D("Enter\n");

    //uart_set_hw_fctrl(fd, 1);
    uart_set_hw_fctrl(fd, 0);
    usleep(20 * 1000);

    tcflush(fd, TCIFLUSH);
    tcflush(fd, TCOFLUSH);
    tcflush(fd, TCIOFLUSH);

    return 0;
}

static void uart_read_exact(int fd, uint8_t *buf, int len)
{
    if (uart_readn(fd, buf, len) < len)
    {
        LOG_E("ERROR: uart_readn failed\n");
    }   
}

/*read a frame(frame head and frame data) from uart.
save frame type and data into rx_buf only if frame head is readed.
 return type+data length*/
static int frame_read(int fd)
{
    uint8_t len;
    uint16_t code;

    // Nothing read
    if ((size_t)uart_readn(fd, (uint8_t*)&code, sizeof(code)) < sizeof(code))
    {
        return 0;
    }

    // Received code
    while (code != HEAD_CODE)
    {
        //LOG_I("in while uart fd=%d\n", fd);
        uint8_t next_byte;
        code >>= 8; //remove first byte
        uart_read_exact(fd, &next_byte, sizeof(next_byte));
        code |= next_byte << (sizeof(code) - 1) * 8; //add highest byte
        //LOG_I("code 0x%x\n",code);
    }

    uart_read_exact(fd, &len, sizeof(len));
    uart_read_exact(fd, rx_buf, len);

    LOG_I("uart get frame, len=%d.\n", len);
    return len;
}

static int frame_write(int fd, uint8_t type, uint8_t *data, uint8_t data_len)
{
    uint32_t i;
    int ret;
    host_comm_t host_comm_msg;
#ifdef RK962_SLEEP_RUNTIME_AUTO
    host_comm_t host_wakeup_msg;
    memset(&host_wakeup_msg, 0, sizeof(host_wakeup_msg));
    // Prepare the wake-up message
    host_wakeup_msg.code = HEAD_CODE;
    host_wakeup_msg.len = CMD_LEN;
    host_wakeup_msg.type = UART_CMD_WAKEUP;
#endif
    memset(&host_comm_msg, 0, sizeof(host_comm_msg));

    // Prepare the message
    host_comm_msg.code = HEAD_CODE;
    host_comm_msg.len = CMD_LEN + data_len;
    host_comm_msg.type = type;

    // Copy data if available
    if ((0 != data_len) && (NULL != data))
    {
        memcpy(host_comm_msg.data, data, data_len);
        LOG_D("\nSend data: ");
        for (i = 0; i < data_len; i++)
            LOG_D("0x%x ", host_comm_msg.data[i]);
        LOG_D("\n");
    }

    // Print debug information
    LOG_D("uart fd=%d, addr=%p, cmd=0x%x, size=%d\n",
          fd, &host_comm_msg, host_comm_msg, sizeof(host_comm_msg));

#ifdef RK962_SLEEP_RUNTIME_AUTO
    LOG_D("write frame head to wakeup device:%x\n", host_comm_msg.code);
    // Write HEAD_CODE first
    ret = uart_write(fd, &host_wakeup_msg, host_wakeup_msg.len + HEAD_LEN + CMD_LEN);
    if (ret < 0)
    {
        fprintf(stderr, "Failed to write HEAD_CODE to UART\n");
        return -1;
    }

    // Wait for 10 milliseconds
    LOG_D("wait for 10ms\n");
    usleep(10000);  // 10000 microseconds = 10 milliseconds
#endif

    // Write the rest of the message
    ret = uart_write(fd, &host_comm_msg, host_comm_msg.len + HEAD_LEN + CMD_LEN);
    if (ret < 0)
    {
        fprintf(stderr, "Failed to write host_comm_msg to UART\n");
        return -1;
    }

    msg_tx_type(host_comm_msg.type);

    return 0;
}


//-------------------------------------------------------------------------
// Internal tools
//-------------------------------------------------------------------------

#if 0
/* update network connect info */
static void update_network_connect_status(void)
{
    char command[128] = {0};
    char ip[32] = {0};
    char netmask[32] = {0};
    const char *ip_interface = "wlan0";

    LOG_I("Update network status, kill udhcpc if exist -->\n");
    util_kill_process_by_name("udhcpc");

    // update networks
    if (util_ipaddr_is_exist(ip, netmask))
    {
        // Two methods are available
#if 1
        memset(command, 0, sizeof(command));
        sprintf(command, "ifconfig %s %s netmask %s", ip_interface, ip, netmask);
        util_exec_command(command);
        usleep(100000);

        memset(command, 0, sizeof(command));
        sprintf(command, "ifconfig %s up", ip_interface);
        util_exec_command(command);
        usleep(100000);
#else
        sprintf(command, "udhcpc -i %s -q -T 1 -t 2 -A 0", ip_interface);
#endif
    }
    else //regain ip address
    {
        util_exec_command("udhcpc -i wlan0 -T 1 -A 0 -b -q");
        for (int i = 0; i < 5; i++) {
            sleep(1);
            if (util_ipaddr_is_exist(ip, netmask)) {
                break;
            }
        }
    }

    return;
}
#endif

static void getSystemTime(uint8_t time_array[6])
{
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    // Stores the year, month, day, hour, and second in an array
    time_array[0] = timeinfo->tm_year % 100; // The last two digits of the year
    time_array[1] = timeinfo->tm_mon + 1;    // Month plus 1, tm mon ranges from 0 to 11
    time_array[2] = timeinfo->tm_mday;       // date
    time_array[3] = timeinfo->tm_hour;       // hour
    time_array[4] = timeinfo->tm_min;        // min
    time_array[5] = timeinfo->tm_sec;        // second

    // Add the base year and print the time
    LOG_I("System Time: %02d-%02d-%02d %02d:%02d:%02d\n",
          time_array[0] + 1900, time_array[1], time_array[2],
          time_array[3], time_array[4], time_array[5]);
}

static void recovery_wifi_mcu(void)
{
    char cmd[128] = {0};
    
    if (g_uart_dev != NULL) {
        snprintf(cmd, sizeof(cmd), "echo -e -n \"\xFF\xFF\x01\x16\" > %s", g_uart_dev);
        util_exec_command(cmd);
    } else {
        LOG_E("g_uart_dev is NULL, cannot recover wifi mcu\n");
    }
}

static void response_expect(uint8_t expect_evt)
{
    pthread_mutex_lock(&g_response.mutex);
    g_response.expect_evt = expect_evt;
    g_response.is_ready = false;
    pthread_mutex_unlock(&g_response.mutex);
}

/**
 * Wait for response with timeout
 * 
 * @param timeout_ms: Timeout in milliseconds
 * @return true if response received, false if timeout
 */
static bool response_wait(int timeout_ms)
{
    bool result = false;
    struct timespec timeout;
    
    // Calculate absolute timeout time
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += timeout_ms / 1000;
    timeout.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (timeout.tv_nsec >= 1000000000) {
        timeout.tv_sec += 1;
        timeout.tv_nsec -= 1000000000;
    }

    pthread_mutex_lock(&g_response.mutex);
    int ret = pthread_cond_timedwait(&g_response.cond, &g_response.mutex, &timeout);
    if (ret != ETIMEDOUT && g_response.is_ready) {
        g_response.is_ready = false;  // Reset ready flag after reading
        g_response.expect_evt = 0xFF;
        result = true;
    }
    
    pthread_mutex_unlock(&g_response.mutex);
    return result;
}

// Function to set response data
static void response_set(const char *format, ...)
{
    pthread_mutex_lock(&g_response.mutex);

    // Check if any command is waiting for response
    if (g_response.expect_evt == 0xFF || g_response.expect_evt != rx_buf[CMD_OFFSET]) {
        LOG_D("No command is waiting for response or event doesn't match "
              "(expecting: 0x%02x, received: 0x%02x)\n",
              g_response.expect_evt, rx_buf[CMD_OFFSET]);
        pthread_mutex_unlock(&g_response.mutex);
        return;
    }

    if (g_response.is_ready) {
        LOG_E("The last response is not removed.\n");
        pthread_mutex_unlock(&g_response.mutex);
        return;
    }

    // Format the response message
    if (!format) {
        g_response.message[0] = '\0';
        LOG_D("Response set: null\n");
    } else {
        va_list args;
        va_start(args, format);
        vsnprintf(g_response.message, sizeof(g_response.message), format, args);
        va_end(args);
        LOG_D("Response set: %s\n", g_response.message);
    }

    // Set response ready and clear expected event
    g_response.is_ready = true;
    g_response.expect_evt = 0xFF;  // Clear expected event after setting response
    
    // Signal waiting thread
    pthread_cond_signal(&g_response.cond);
    LOG_D("Response set with event type: 0x%02x\n", rx_buf[CMD_OFFSET]);

    pthread_mutex_unlock(&g_response.mutex);
}

static void ensure_serial_flush()
{
    int fd = -1;

    fflush(stdout);
    fflush(stderr);

    if (isatty(STDOUT_FILENO)) {
        fd = STDOUT_FILENO;
    }
    else if (isatty(STDERR_FILENO)) {
        fd = STDERR_FILENO;
    }
    else {
        fd = open("/dev/console", O_WRONLY | O_NOCTTY);
    }

    if (fd >= 0) {
        if (isatty(fd)) {
            tcdrain(fd);
        }

        if (fd != STDOUT_FILENO && fd != STDERR_FILENO) {
            close(fd);
        }
    }

    usleep(500000);
}

/**
 * Send command to MCU and optionally set up response handling
 * 
 * @param cmd_code: Command code to send
 * @param data: Command data payload
 * @param len: Length of data payload
 * @param expect_evt: Expected event code for response (0xFF if no response expected)
 * @return true on success, false on failure
 */
static bool send_command(uint8_t cmd_code, uint8_t *data, size_t len,
                        response_type_t response_type, uint8_t expect_evt)
{
    if (g_uart_fd <= 0) {
        LOG_E("Error: Invalid UART file descriptor\n");
        return false;
    }

    // Set up response handling if an event is expected
    if (response_type != RESP_NONE) {
        response_expect(expect_evt);
    }

    // Send the command frame
    if (frame_write(g_uart_fd, cmd_code, data, len) < 0) {
        LOG_E("Failed to send command 0x%02x\n", cmd_code);
        // Clear expected event on send failure
        if (response_type != RESP_NONE) {
            response_expect(0xFF);
        }
        return false;
    }

    return true;
}

static bool run_into_deepsleep(int mode, int seconds)
{
    uint8_t cmd_data[5] = {0};
    size_t cmd_len = 0;

    // Check sleep type
    switch (mode)
    {
    case 1:
        LOG_I("System will run into deep sleep mode 1 - lowest power.\n");
        break;
    case 2:
        LOG_I("System will run into deep sleep mode 2 - retry AP connection per 5 minutes.\n");
        //wpa_fwcr_save();   //deelsleep mode should not follow fast wifi link
        break;
    case 3:
        LOG_I("System will run into deep sleep mode 3 - keep AP connection.\n");
        g_ap_kpalive = true;
        //wpa_fwcr_save();   //deelsleep mode should not follow fast wifi link
        break;
    default:
        LOG_E("Invalid sleep type: must be 1, 2 or 3\n");
        return false;
    }

    // Prepare 5-byte command data
    cmd_data[0] = (uint8_t)mode;

    if (seconds > 0)
    {
        // Validate sleep seconds
        if (seconds < 1 || seconds > 21474835)  // max 0xffffffff/2/100-1
        {
            LOG_E("Invalid sleep seconds: %d\n", seconds);
            return false;
        }

        // Store sleep_seconds in little-endian format
        cmd_data[1] = (uint8_t)(seconds & 0xFF);
        cmd_data[2] = (uint8_t)((seconds >> 8) & 0xFF);
        cmd_data[3] = (uint8_t)((seconds >> 16) & 0xFF);
        cmd_data[4] = (uint8_t)((seconds >> 24) & 0xFF);
        cmd_len = 5;
    }
    else
    {
        cmd_len = 1;
    }

    ensure_serial_flush();

    bool ret = send_command(UART_CMD_DEEPSLEEP, cmd_data, cmd_len, RESP_NONE, 0);
    if (ret) {
        while(1) {
            LOG_I("Wating for host shutdown...\n");
            sleep(1);
        }
        // never to be here
    }
    else {
        LOG_E("send command to host failed.\n");
    }

    return ret;
}

static void resumekp()
{
    if (!util_get_kp_status()) {
        LOG_E("not in keepalive status.\n");
        return;
    }

    LOG_I("resume keepalive...\n");

    // Check and recovery wifi
    if (!util_check_and_recovery_wifi(true)) {
        LOG_I("recovery wifi failed, run to deepsleep mode 2\n");
        run_into_deepsleep(2, 0);
    }

    // Resume keepalive
    if (g_ap_kpalive) {
        LOG_I("wifi is connected, resume AP keepalive\n");
        run_into_deepsleep(3, 0);
    }
    else {
        LOG_I("resume server keepalive fastly\n");
        if (!util_exec_command("rkwifi_kpalive")) {
            LOG_I("recovery keepalive failed, run into deepsleep mode 1 for 5 minutes");
            run_into_deepsleep(1, 300);
            // LOG_I("recovery server failed, run to deepsleep mode 2\n");
            // run_into_deepsleep(2, 0);
        }
    }
    return;
}


//-------------------------------------------------------------------------
// Handle events
//-------------------------------------------------------------------------

static void uart_event_ack(uint8_t *data, uint8_t len)
{
    if (data == NULL) {
        LOG_E("data is null.\n");
        goto err;
    }
    if (len != 1) {
        LOG_E("command ack value is invalid.\n");
        goto err;
    }

    uint8_t ack_val = data[0];
    LOG_D("get cmd ack [%d]\n", ack_val);
    
    response_set("%s", ack_val == 0 ? "OK" : "Failed");
    return;

err:
    response_set("Failed");
    return;
}

static void uart_event_version(uint8_t *data, uint8_t len)
{
    if (data == NULL) {
        LOG_E("data is null.\n");
        goto err;
    }
    if (len != 2)
    {
        LOG_E("version is invalid: major version and minor version\n");
        goto err;
    }

    LOG_D("MCU version: %u.%u\n", *(data), *(data + 1));
    response_set("%u.%u", *(data), *(data + 1));
    return;

err:
    response_set("Failed");
    return;
}

static void uart_event_pirth(uint8_t *data, uint8_t len)
{
    if (data == NULL) {
        LOG_E("data is null.\n");
        goto err;
    }
    if (len != 1) {
        LOG_E("PIR threshold is invalid: Threshold length is 1 byte\n");
        goto err;
    }

    LOG_D("PIR threshold: %u\n", *data);
    response_set("%u", *data);
    return;

err:
    response_set("Failed");
    return;
}

static void uart_event_bat(uint8_t *data, uint8_t len)
{
    if (data == NULL) {
        LOG_E("data is null.\n");
        goto err;
    }
    if (len != 1) {
        LOG_E("Battery level is only one\n");
        goto err;
    }

    if (*data > 100) {
        LOG_E("Battery level outside: 0~100\n");
        goto err;
    }

    LOG_D("Battery level: %u\n", *data);
    response_set("%u", *data);
    return;

err:
    response_set("Failed");
    return;
}

static void uart_event_rtc(uint8_t *data, uint8_t len)
{
    if (data == NULL) {
        LOG_E("data is null.\n");
        goto err;
    }
    if (len != 6) {
        LOG_E("RTC data must be 6 bytes\n");
        goto err;
    }

    // Validate RTC data
    if (data[0] > 99 || 
        data[1] < 1 || data[1] > 12 ||
        data[2] < 1 || data[2] > 31 ||
        data[3] > 23 ||
        data[4] > 59 ||
        data[5] > 59) {
        LOG_E("Invalid RTC data values\n");
        goto err;
    }

    LOG_D("RTC: %02d-%02d-%02d %02d:%02d:%02d\n",
          data[0] + 2000, data[1], data[2],
          data[3], data[4], data[5]);

    response_set("%02d-%02d-%02d %02d:%02d:%02d",
                data[0] + 2000, data[1], data[2],
                data[3], data[4], data[5]);
    return;

err:
    response_set("Failed");
    return;
}

static void uart_event_key(uint8_t *data, uint8_t len)
{
    if (len != 1)
        LOG_I("key value is invalid\n");

    uint8_t key_val = data[0];
    switch (key_val)
    {
    case KEY_NET_PUSH:
        LOG_I("get netconf key pressed [%d]\n", key_val);
        util_set_kp_status(false);   //bt restart abnormally when netconf mode exited
        LOG_I("NETCONF_PROCESS is parent proccess, btmcu_daemon exit\n");
        exit(EXIT_SUCCESS);
        break;
    case KEY_RESET_PUSH:
        LOG_I("get reset key pressed\n");
        break;
    case KEY_PROTECT_PUSH:
        LOG_I("get antidisassembly key pressed\n");
        break;
    case KEY_DOORBELL_PUSH:
        LOG_I("get doorbell key pressed\n");
        break;
    case KEY_USER_PUSH:
        LOG_I("get user key pressed\n");
        break;
    default:
        LOG_I("get unknown key pressed [%d]\n", key_val);
        break;
    }
}

static void uart_event_pir_trigger(uint8_t *data, uint8_t len)
{
    UNUSED(data); UNUSED(len);

    LOG_I("PIR trigger\n");

}

static void uart_event_wakeup(uint8_t *data, uint8_t len)
{
    if (len != 1)
    {
        LOG_E("KP wake up reason data is only one byte\n");
        return;
    }

    LOG_I("Wake up reason: %s\n", get_wakeup_reason_desc(data[0]));

    switch (data[0])
    {
    case WAKEUP_REASON_NORMAL:
    case WAKEUP_REASON_SERVER_DISCONNECT:
        util_set_kp_status(false);
        break;

    case WAKEUP_REASON_REACH_MAX_COUNTS:
        LOG_I("Run into deepsleep mode 1 for 5 minutes and then recovery keep-alive\n");
        run_into_deepsleep(1, 300);
        break;

    case WAKEUP_REASON_TCP_KA_KEY_RENEW:
    case WAKEUP_REASON_TCP_KA_BCN_LOST:
    case WAKEUP_REASON_IPADDR_CHANGED:
    case WAKEUP_REASON_DEVICEASSOC_FAILURE:
    case WAKEUP_REASON_CONNECT_SERVER_FAILED:
    case WAKEUP_REASON_DEEPSLEEP_SCAN_FOUND:
        resumekp();
        break;

    case WAKEUP_REASON_AP_KA_KEY_RENEW:
    case WAKEUP_REASON_AP_KA_BCN_LOST:
        g_ap_kpalive = true;
        resumekp();
        break;

    default:
        LOG_I("Unknown wakeup reason, we don't know how to handle it!\n");
        util_set_kp_status(false);
        break;
    }
}

static void uart_event_wifi_break(uint8_t *data, uint8_t len)
{
    LOG_I("get wifi connection break reason: %u\n", *(data));

    if (len != 1)
    {
        LOG_E("wifi connection break reason data is only one byte\n");
        return;
    }
}

static void uart_event_usb_plugin(uint8_t *data, uint8_t len)
{
    LOG_I("get USB event\n");

    if (len != 1)
    {
        LOG_E("USB status is only one: plugin or pullout\n");
        return;
    }

    switch (*data)
    {
    case 0x01:
        LOG_I("USB plug in\n");
        break;
    case 0x00:
        LOG_I("USB pull out\n");
        break;
    default:
        LOG_I("USB status error\n");
        break;
    }

}

static void uart_event_wifi_recovery(uint8_t *data, uint8_t len)
{
    UNUSED(data); UNUSED(len);
    (void)util_check_and_recovery_wifi(true);
}

static void uart_event_discover_ap(uint8_t *data, uint8_t len)
{
    UNUSED(data); UNUSED(len);
    LOG_I("The lost AP has been recovered.\n");
    resumekp();
}

static void uart_event_sleep_timeout(uint8_t *data, uint8_t len)
{
    UNUSED(data); UNUSED(len);
    LOG_I("Deepsleep timeout.\n");
    resumekp();
}

/*
    Check the UART_EVENT_EXCEPTION event data reported after the system restarts and return the restart cause
    00 00 00 00 00 00 00 00：Normal restart, return 0
    00 00 00 00 01 00 00 00：If host is restarted abnormally during the keepalive period, 1 is returned
    00 00 00 00 02 00 00 00：If host is restarted abnormally during the activity, return 2

    01 00 00 00 : ASSERT exception ,return 3    //EXCEPTION_TYPE_ASSERT = 0x00000001
    02 00 00 00 : stack overflow ,return 4      //EXCEPTION_TYPE_STACK  = 0x00000002
    03 00 00 00 : HardFault ,return 5           //EXCEPTION_TYPE_HARD_FAULT = 0x00000003
    01 00 01 00 : wifi write in ,return 6       //EXCEPTION_TYPE_WIFI_WRITE = 0x00010001

    Otherwise, return -1
*/

static int check_exception_data(uint8_t *data, uint8_t len)
{
    // If the length is not 8, return -1
    
    // if (len != 8)
    // {
    //     return -1;
    // }

    // Determine whether all of them are 0x00
    for (uint8_t i = 0; i < len; i++)
    {
        if (data[i] != 0x00)
        {
            break;
        }
        // If all values are 0x00, 0 is returned
        if (i == len - 1)
        {
            return 0;
        }
    }

    // Check if it is 00 00 00 00 01 00 00 00 00
    if (data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x00 &&
            data[3] == 0x00 && data[4] == 0x01 && data[5] == 0x00 &&
            data[6] == 0x00 && data[7] == 0x00)
    {
        return EXCEPTION_RST_KPAL;
    }

    // Check if it is 00 00 00 00 02 00 00 00
    if (data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x00 &&
            data[3] == 0x00 && data[4] == 0x02 && data[5] == 0x00 &&
            data[6] == 0x00 && data[7] == 0x00)
    {
        return EXCEPTION_RST_ACT;
    }

    // Check if it is 01 00 00 00  ASSERT exception ,return 3   EXCEPTION_TYPE_ASSERT = 0x00000001
    if (data[0] == 0x01 && data[1] == 0x00 && data[2] == 0x00 && data[3] == 0x00)
    {
        return EXCEPTION_ACCERT;
    }
    
    // Check if it is 02 00 00 00  stack overflow ,return 4     EXCEPTION_TYPE_STACK = 0x00000002
    if (data[0] == 0x02 && data[1] == 0x00 && data[2] == 0x00 && data[3] == 0x00)
    {
        return EXCEPTION_STACK;
    }
    
    // Check if it is 03 00 00 00  HardFault ,return 5          EXCEPTION_TYPE_HARD_FAULT = 0x00000003
    if (data[0] == 0x03 && data[1] == 0x00 && data[2] == 0x00 && data[3] == 0x00)
    {
        return EXCEPTION_HARD_FAULT;
    }
    
    // Check if it is 01 00 01 00  wifi write in ,return 6      EXCEPTION_TYPE_WIFI_WRITE = 0x00010001
    if (data[0] == 0x01 && data[1] == 0x00 && data[2] == 0x01 && data[3] == 0x00)
    {
        return EXCEPTION_WIFI_WRITE;
    }
    
    // others, return -1
    return -1;
}

static void uart_event_exception(uint8_t *data, uint8_t len)
{
    int why_restart;

    LOG_I("get exception %u\n", *(data));

    // Print the contents of data in hexadecimal format
    if (LOG_LEVEL >= LOG_DBG)
    {
        printf("Exception data (%d): ", len);
        for (int i = 0; i < len; i++)
        {
            printf("%02X ", data[i]);
        }
        printf("\n");
    }

    why_restart = check_exception_data(data, len);
    if (EXCEPTION_NORMAL == why_restart)
    {
        LOG_I("system restart normally.\n");
        util_set_kp_status(false);
    }
    else if(EXCEPTION_RST_ACT == why_restart)
    {
        LOG_I("System restart abnormally!\n");
        LOG_I("system restart in activity\n");
        util_set_kp_status(false);
    }
    else
    {
        LOG_I("System restart abnormally!\n");
        if (EXCEPTION_RST_KPAL == why_restart)
        {
            LOG_I("system restart in keepalive, set kp_status to true\n");
            util_set_kp_status(true);
        }
        else if(EXCEPTION_ACCERT == why_restart)
        {
            LOG_I("exception for ASSERT\n");
        }
        else if(EXCEPTION_STACK == why_restart)
        {
            LOG_I("exception for stack overflow\n");
        }
        else if(EXCEPTION_HARD_FAULT == why_restart)
        {
            LOG_I("exception for hard fault\n");
        }
        else if(EXCEPTION_WIFI_WRITE == why_restart)
        {
            LOG_I("exception for wifi write in\n");
        }
        resumekp();
    }

    return;
}

static const struct event_desc *event_desc_get(uint32_t event, const struct event_desc *event_tab, uint32_t event_tab_size)
{
    uint32_t i;
    for (i = 0; i < event_tab_size; i++)
    {
        if (event_tab[i].event == event)
        {
            return &event_tab[i];
        }
    }
    return NULL;
}

static const struct event_desc g_event_tab[] =
{
    {.event = UART_EVENT_ACK,               .cb = uart_event_ack},
    {.event = UART_EVENT_KEY,               .cb = uart_event_key},
    {.event = UART_EVENT_VERSION,           .cb = uart_event_version},
    {.event = UART_EVENT_PIRTH,             .cb = uart_event_pirth},
    {.event = UART_EVENT_PIR,               .cb = uart_event_pir_trigger},
    {.event = UART_EVENT_BAT,               .cb = uart_event_bat},
    {.event = UART_EVENT_WAKEUP_REASON,     .cb = uart_event_wakeup},
    {.event = UART_EVENT_BREAK_REASON,      .cb = uart_event_wifi_break},
    {.event = UART_EVENT_RTC,               .cb = uart_event_rtc},
    {.event = UART_EVENT_USB,               .cb = uart_event_usb_plugin},
    {.event = UART_EVENT_EXCEPTION,         .cb = uart_event_exception},
    {.event = UART_EVENT_RECOVER_WIFI,      .cb = uart_event_wifi_recovery},
    {.event = UART_EVENT_DISCOVER_AP,       .cb = uart_event_discover_ap},
    {.event = UART_EVENT_SLEEP_TIMEOUT,     .cb = uart_event_sleep_timeout},
};


static void *uart_rx_thread_func(void *arg)
{
    UNUSED(arg);
    uint8_t len;
    uint8_t event;
    const struct event_desc *desc = NULL;
    void (*cb)(uint8_t * data, uint8_t len);

    LOG_D("Enter\n");
    while (1)
    {
        if (g_uart_fd <= 0)
        {
            pr_err("Uart fd is invalid\n");
            sleep(1); continue;
        }

        memset(rx_buf, 0, sizeof(rx_buf));
        len = frame_read(g_uart_fd);
        if (len <= 0)
        {
            sleep(1); continue;
        }

        if (LOG_LEVEL >= LOG_DBG)
        {
            printf("Received: ");
            for (int i = 1; i <= len; i++)
            {
                printf("0x%02x ", rx_buf[i - 1]);
                if (i % 8 == 0)
                    printf("\n");
            }
            printf("\n");
        }

        event = rx_buf[CMD_OFFSET];
        desc = event_desc_get(event, g_event_tab, ARRAY_SIZE(g_event_tab));
        if (desc == NULL)
        {
            LOG_E("Invalid uart event!\n");
            sleep(1); continue;
        }
        LOG_D("recieve Event(type) = %d:\n", event);
        msg_rx_type(event);

        cb = desc->cb;
        if (cb)
        {
            /* if (_is_send_task_suspend())
             {
                 rt_timer_start(reset_host_timer);
                 _send_task_resume();
             }*/
            len = len - CMD_LEN;
            if (len > 0)
            {
                cb(&rx_buf[CMD_OFFSET + CMD_LEN], len);
            }
            else
            {
                cb(NULL, 0);
            }
        }
    }

    return NULL;
}


//-------------------------------------------------------------------------
// Handle commands
//-------------------------------------------------------------------------

static bool rk_cmd_heartbeat(int argc, char *argv[])
{
    UNUSED(argv);
    
    // Check parameters
    if (argc != 0) {
        LOG_E("Usage: btmcu_daemon heartbeat\n");
        return false;
    }
    
    LOG_I("Heartbeat\n");

    return send_command(UART_CMD_HEARTBEAT, NULL, 0, RESP_NONE, 0);
}

static bool rk_cmd_deepsleep(int argc, char *argv[])
{
    if (argc != 1 && argc != 2)
    {
        LOG_E("Usage: btmcu_daemon deepsleep <sleeptype:1,2,3> [sleep_seconds]\n");
        LOG_E("       - sleeptype: 1,2,3 for different sleep modes\n");
        LOG_E("       - sleep_seconds: optional, time to sleep in seconds (1 ~ 21474835)\n");
        return false;
    }

    LOG_I("Deep sleep command: type=%s, seconds=%s\n", 
          argv[0], argc == 2 ? argv[1] : "unlimited");

    // Parse sleep type
    int sleep_type = (int)strtol(argv[0], NULL, 10);
    
    // Parse sleep seconds if provided
    int sleep_seconds = 0;
    if (argc == 2)
    {
        sleep_seconds = (int)strtoul(argv[1], NULL, 10);
        // Check sleep_seconds range (1 ~ 21474835)
        if (sleep_seconds < 1 || sleep_seconds > 21474835)
        {
            LOG_E("Invalid sleep_seconds: %d, valid range is 1 ~ 21474835\n", sleep_seconds);
            return false;
        }
    }

    return run_into_deepsleep(sleep_type, sleep_seconds);
}

static bool rk_cmd_getversion(int argc, char *argv[])
{
    UNUSED(argv);
    
    // Check parameters
    if (argc != 0) {
        LOG_E("Usage: btmcu_daemon getversion\n");
        return false;
    }
    
    LOG_I("Get Version\n");
    return send_command(UART_CMD_GET_VERSION, NULL, 0, RESP_INFO, UART_EVENT_VERSION);
}

static bool rk_cmd_getpirth(int argc, char *argv[])
{
    UNUSED(argv);
    
    // Check parameters
    if (argc != 0) {
        LOG_E("Usage: btmcu_daemon getpirth\n");
        return false;
    }
    
    LOG_I("Get PIR threshold\n");
    return send_command(UART_CMD_GET_PIRTH, NULL, 0, RESP_INFO, UART_EVENT_PIRTH);
}

static bool rk_cmd_getbat(int argc, char *argv[])
{
    UNUSED(argv);
    
    // Check parameters
    if (argc != 0) {
        LOG_E("Usage: btmcu_daemon getbat\n");
        return false;
    }
    
    LOG_I("Get BAT\n");
    return send_command(UART_CMD_GET_BAT, NULL, 0, RESP_INFO, UART_EVENT_BAT);
}

static bool rk_cmd_getrtc(int argc, char *argv[])
{
    UNUSED(argv);
    
    // Check parameters
    if (argc != 0) {
        LOG_E("Usage: btmcu_daemon getrtc\n");
        return false;
    }
    
    LOG_I("Get RTC\n");
    return send_command(UART_CMD_GET_RTC, NULL, 0, RESP_INFO, UART_EVENT_RTC);
}

static bool rk_cmd_setpirth(int argc, char *argv[])
{
    if (argc != 1) {
        LOG_E("Usage: btmcu_daemon setpirth <threshold>\n");
        return false;
    }
    
    LOG_I("Set PIR threshold\n");

    uint8_t pir_value = atoi(argv[0]);
    LOG_I("pir value: %d\n", pir_value);

    return send_command(UART_CMD_SET_PIRTH, &pir_value, 1, RESP_ACK, UART_EVENT_ACK);
}

static bool rk_cmd_setrtc(int argc, char *argv[])
{
    uint8_t rtc_data[6];
    bool valid = true;

    // Check if arguments count is valid (must be either 0 or 6)
    if (argc != 0 && argc != 6) {
        LOG_E("Usage: btmcu_daemon setrtc [y m d h m s]\n");
        LOG_E("       - With 6 parameters: set RTC to specified year, month, day, hour, minute, second\n");
        LOG_E("       - With 0 parameters: set RTC to current system time\n");
        return false;
    }

    if (argc == 6)    // Check if exactly 6 time arguments are provided
    {
        LOG_I("Received parameters: %s %s %s %s %s %s\n",
               argv[0], argv[1], argv[2], argv[3], argv[4], argv[5]);

        for (int i = 0; i < 6; ++i)
        {
            int val = (int)strtol(argv[i], NULL, 10);  // Convert argument to integer
            switch (i)
            {
            case 0:  // Year (0~99, take last two digits)
                if (val >= 0 && val <= 9999)
                {
                    rtc_data[i] = val % 100;
                }
                else
                {
                    valid = false;
                    LOG_I("Invalid year: 0000~9999\n");
                }
                break;
            case 1:  // Month (1~12)
                if (val >= 1 && val <= 12)
                {
                    rtc_data[i] = (uint8_t)val;
                }
                else
                {
                    valid = false;
                    LOG_I("Invalid month: 01~12\n");
                }
                break;
            case 2:  // Day (1~31)
                if (val >= 1 && val <= 31)
                {
                    rtc_data[i] = (uint8_t)val;
                }
                else
                {
                    valid = false;
                    LOG_I("Invalid day: 01~31\n");
                }
                break;
            case 3:  // Hour (0~23)
                if (val >= 0 && val <= 23)
                {
                    rtc_data[i] = (uint8_t)val;
                }
                else
                {
                    valid = false;
                    LOG_I("Invalid hour: 00~23\n");
                }
                break;
            case 4:  // Minute (0~59)
            case 5:  // Second (0~59)
                if (val >= 0 && val <= 59)
                {
                    rtc_data[i] = (uint8_t)val;
                }
                else
                {
                    valid = false;
                    LOG_I("Invalid minute or second: 00~59\n");
                }
                break;
            default:
                valid = false;
                break;
            }
        }
    }
    else
    {
        // Use current system time if arguments are not provided or invalid
        getSystemTime(rtc_data);
    }

    if (valid)
    {
        LOG_I("Setting RTC: %02d-%02d-%02d %02d:%02d:%02d\n",
              rtc_data[0], rtc_data[1], rtc_data[2],
              rtc_data[3], rtc_data[4], rtc_data[5]);

        return send_command(UART_CMD_SET_RTC, rtc_data, 6, RESP_ACK, UART_EVENT_ACK);
    }
    else
    {
        LOG_E("Invalid time parameters!\n");
        return false;
    }
}

static bool rk_cmd_notcharge(int argc, char *argv[])
{
    UNUSED(argv);
    
    // Check parameters
    if (argc != 0) {
        LOG_E("Usage: btmcu_daemon notcharge\n");
        return false;
    }

    uint8_t data = 0;
    LOG_I("Stop charge!\n");

    return send_command(UART_CMD_SET_CHARGE_STATE, &data, 1, RESP_ACK, UART_EVENT_ACK);
}

static command_t cmd_table[] =
{
    // command no need to reply
    {"heartbeat",   "btmcu_daemon heartbeat           #send a heartbeat", rk_cmd_heartbeat, RESP_NONE},
    {"deepsleep",   "btmcu_daemon deepsleep <1|2|3>   #enter deep sleep mode 1|2|3", rk_cmd_deepsleep, RESP_NONE},
    // command need to reply ACK
    {"setpirth",    "btmcu_daemon setpirth (0 ~ 255)  #set PIR threshold", rk_cmd_setpirth, RESP_ACK},
    {"setrtc",      "btmcu_daemon setrtc y m d h m s  #set RTC value", rk_cmd_setrtc, RESP_ACK},
    {"notcharge",   "btmcu_daemon notcharge           #disable charge", rk_cmd_notcharge, RESP_ACK},
    // command need to reply more info
    {"getversion",  "btmcu_daemon getversion          #get bt mcu version info", rk_cmd_getversion, RESP_INFO},
    {"getpirth",    "btmcu_daemon getpirth            #get PIR threshold", rk_cmd_getpirth, RESP_INFO},
    {"getbat",      "btmcu_daemon getbat              #get battery level", rk_cmd_getbat, RESP_INFO},
    {"getrtc",      "btmcu_daemon getrtc              #get RTC value", rk_cmd_getrtc, RESP_INFO}
};


//-------------------------------------------------------------------------
// Threads
//-------------------------------------------------------------------------

//
// Thread to send heartbeat
//
static void *btmcu_heartbeat_thread_func(void *arg)
{
    UNUSED(arg);
    /*
    uint8_t pir_data[1] = {25};
    uint8_t rtc_data[6];
    struct _msg
    {
        uint8_t year;
        uint8_t month;
        uint8_t mday;
        uint8_t hour;
        uint8_t min;
        uint8_t sec;
    } rtc_msg;
    rtc_msg.year = 13;
    rtc_msg.month = 9;
    rtc_msg.mday = 28;
    rtc_msg.hour = 11;
    rtc_msg.min = 22;
    rtc_msg.sec = 55;
    */
    LOG_D("Enter\n");
    int loop_count = 1;

    while (loop_count < 2)
    {
        LOG_D("Tx CMD loop: %d\n", loop_count);
        loop_count++;
        frame_write(g_uart_fd, UART_CMD_HEARTBEAT, NULL, 0);
        sleep(1);
        frame_write(g_uart_fd, UART_CMD_GET_VERSION, NULL, 0);
        /*     sleep(5);
             frame_write(g_uart_fd, UART_CMD_GET_PIRTH, NULL, 0);
             sleep(5);
             frame_write(g_uart_fd, UART_CMD_SET_PIRTH, pir_data, 1);
             sleep(5);
             frame_write(g_uart_fd, UART_CMD_GET_BAT, NULL, 0);
             sleep(5);
             frame_write(g_uart_fd, UART_CMD_SET_RTC, &rtc_msg, 6);
             sleep(5);
             frame_write(g_uart_fd, UART_CMD_GET_RTC, NULL, 0);
             sleep(5);
         getSystemTime(rtc_data);
         LOG_I("Current System Time: %02d-%02d-%02d %02d:%02d:%02d\n",
                 rtc_data[0], rtc_data[1], rtc_data[2],
                 rtc_data[3], rtc_data[4], rtc_data[5]);
         frame_write(g_uart_fd, UART_CMD_SET_RTC, rtc_data, 6);
             sleep(5);
         frame_write(g_uart_fd, UART_CMD_GET_RTC, NULL, 0);
             sleep(5);
        */
    }

    while (1)
    {
        sleep(1);
        frame_write(g_uart_fd, UART_CMD_HEARTBEAT, NULL, 0);
    }

    return NULL;
}

//
// Thread to monitor events from rk96x wifi driver
//
void *wifi_event_thread_func(void *arg)
{
    UNUSED(arg);
    int recovery_efd, recovery_sysfs_fd = 0;
    char buf[32];
    uint64_t counter;
    int ret = -1;
    volatile int running = 1;

    LOG_E("%s: Starting eventfd thread\n", __func__);

    // create eventfd
    recovery_efd = eventfd(0, EFD_CLOEXEC);
    if (recovery_efd == -1) {
        LOG_E("%s: Failed to create eventfd: %s\n", __func__, strerror(errno));
        goto cleanup;
    }

    // open sysfs file
    recovery_sysfs_fd = open("/sys/kernel/rkfs/recovery_efd", O_WRONLY);
    if (recovery_sysfs_fd == -1) {
        LOG_E("%s: Failed to open sysfs: %s\n", __func__, strerror(errno));
        goto cleanup;
    }

    LOG_I("%s, recovery_efd = %d\n", __func__, recovery_efd);
    // write eventfd fd to sysfs
    int len = snprintf(buf, sizeof(buf), "%d", recovery_efd);
    if (len < 0 || (unsigned int)len >= sizeof(buf)) {
        LOG_E("%s: Buffer overflow\n", __func__);
        goto cleanup;
    }
    if (write(recovery_sysfs_fd, buf, len) != len) {
        LOG_E("%s: Failed to write to sysfs: %s\n", __func__, strerror(errno));
        goto cleanup;
    }

    close(recovery_sysfs_fd);
    recovery_sysfs_fd = -1;

    LOG_I("Waiting for events...\n");
    while (running) {
        struct pollfd pfd = {
            .fd = recovery_efd,
            .events = POLLIN,
        };

        // Waiting for events
        ret = poll(&pfd, 1, 1000);
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            LOG_E("%s: Poll failed: %s\n", __func__, strerror(errno));
            break;
        }
        if (ret > 0 && (pfd.revents & POLLIN)) {
            ssize_t bytes = read(recovery_efd, &counter, sizeof(counter));
            if (bytes != sizeof(counter)) {
                LOG_E("%s: Read failed: %s\n", __func__, strerror(errno));
                break;
            }
            LOG_I("%s: Received event counter = %lu\n", __func__, counter);
            if(RK962_RECOVERY_EVENTFD_VALUE == counter)
            {
                LOG_I("Recovery WIFI MCU.\n");
                recovery_wifi_mcu();
            }
            else
            {
                LOG_I("Counter value is not WIFI recovery eventfd value.\n");
            }
        }
    }
cleanup:
    if (recovery_sysfs_fd >= 0)
        close(recovery_sysfs_fd);
    if (recovery_efd >= 0)
        close(recovery_efd);

    LOG_I("%s: Thread exiting\n", __func__);
    return NULL;
}

//
// Thread to monitor commands from btmcu_daemon client
//
static void *btmcu_daemon_thread_func(void *arg)
{
    UNUSED(arg);
    struct sockaddr_un ser_un;
    int socket_fd = -1;
    int client_fd = -1;
    int ret = 0;
    char cmd_line[RK_CMD_LINE_LEN];

    socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd <= 0)
    {
        LOG_E("open socket err\n");
        return NULL;
    }

    unlink(UART_SOCKET_PATH);

    memset(&ser_un, 0, sizeof(ser_un));
    ser_un.sun_family = AF_UNIX;
    strcpy(ser_un.sun_path, UART_SOCKET_PATH);
    ret = bind(socket_fd, (struct sockaddr *)&ser_un, sizeof(struct sockaddr_un));
    if (ret < 0)
    {
        LOG_E("bind err %s %s\n", UART_SOCKET_PATH, strerror(errno));
        return NULL;
    }

    ret = listen(socket_fd, 5);
    if (ret < 0)
    {
        LOG_E("listen err\n");
        return NULL;
    }

    while (1)
    {
        LOG_I("Waiting for client connection...\n");
        
        client_fd = accept(socket_fd, NULL, NULL);
        
        if (client_fd < 0)
        {
            LOG_E("accept failed: %s\n", strerror(errno));
            continue;
        }

        memset(cmd_line, 0, sizeof(cmd_line));
        ret = read(client_fd, cmd_line, sizeof(cmd_line));
        if (ret <= 0)
        {
            LOG_E("read cmd_line failed: %s\n", strerror(errno));
            close(client_fd);
            client_fd = -1;
            continue;
        }

        LOG_I("Received command: '%s'\n", cmd_line);

        // Parse command line into arguments
        char *argv[MAX_ARGS];
        int argc = util_tokenize_cmd(cmd_line, argv, MAX_ARGS);
        if (argc <= 0) {
            LOG_E("Failed to parse command line.\n");
            close(client_fd);
            client_fd = -1;
            continue;
        }
        
        // Find command in table
        const command_t *cmd = NULL;
        size_t i;
        for (i = 0; i < ARRAY_SIZE(cmd_table); i++) {
            if (!strcmp(cmd_table[i].cmd, argv[0])) {
                cmd = &cmd_table[i];
                break;
            }
        }

        const char *ret_str = NULL;
        if (i >= ARRAY_SIZE(cmd_table)) {           // command not found
            ret_str = "Wrong command";
        }
        else if (!cmd->exec(argc - 1, &argv[1])) {  // wrong arguments or send failed
            ret_str = "Wrong arguments or send failed";
        }
        else if (cmd->resp_type == RESP_NONE) {     // no response needed
            ret_str = "OK";
        }
        else if (response_wait(1000)) {             // get response successfully
            ret_str = (const char *)g_response.message;
        }
        else {                                      // get response timeout
            ret_str = "Command timeout";
        }

        // Send back response to client
        ssize_t written = write(client_fd, ret_str, strlen(ret_str));
        if (written < 0) {
            LOG_E("Failed to send response: %s\n", strerror(errno));
        }

        // Close client connection
        close(client_fd);
        client_fd = -1;
    }

    LOG_E("********** Never be here!\n");

    close(socket_fd);
    return NULL;
}


//-----------------------------------------------------------------------
// Main function and btmcu daemon client
//-----------------------------------------------------------------------

static int btmcu_daemon_client(int argc, char *argv[])
{
    char command[RK_CMD_LINE_LEN] = {0};
    char response[RK_CMD_LINE_LEN] = {0};
    struct sockaddr_un ser_un;
    int tmp_argc = 1;
    int socket_fd = 0;
    int ret = 0;
    size_t pos = 0;

    if (argc < 2) {
        LOG_E("Missing command\n");
        return -1;
    }

    // Concatenate command and parameters
    for (; tmp_argc < argc; tmp_argc++) {
        const char* arg = argv[tmp_argc];
        int available = sizeof(command) - pos;

        if (pos > 0) {
            if (available > 1) {
                command[pos++] = ' ';
                available--;
            } else {
                break;
            }
        }

        int written = snprintf(command + pos, available, "%s", arg);
        if (written < 0 || written >= available) {
            break;
        }
        pos += written;
    }

    command[sizeof(command)-1] = '\0';

    if (tmp_argc < argc) {
        LOG_E("Command truncated: %s...\n", command);
        return -1;
    }

    socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd <= 0)
    {
        LOG_E("open socket err\n");
        return -1;
    }

    memset(&ser_un, 0, sizeof(ser_un));
    ser_un.sun_family = AF_UNIX;
    strcpy(ser_un.sun_path, UART_SOCKET_PATH);

    ret = connect(socket_fd, (struct sockaddr *)&ser_un, sizeof(struct sockaddr_un));
    if (ret < 0)
    {
        LOG_E("connect err\n");
        return -1;
    }

    write(socket_fd, command, strlen(command) + 1);
    
    // Read response from server
    ret = read(socket_fd, response, sizeof(response) - 2);
    if (ret <= 0) {
        LOG_E("Failed to read response: %s\n", strerror(errno));
        close(socket_fd);
        return -1;
    }
    
    // Ensure terminating with '\n' and '\0'
    if (response[ret-1] != '\n') {
        response[ret] = '\n';
        response[ret+1] = '\0';
    }
    else {
        response[ret] = '\0';
    }
    
    // Print the raw response
    printf(response);
    
    close(socket_fd);
    return 0;
}

extern int pthread_kill(pthread_t thread, int sig);

static int is_thread_alive(pthread_t thread)
{
    return pthread_kill(thread, 0) == 0 ? 1 : 0;
}

static void show_help(void)
{
    unsigned int i;
    printf("[Usage]:\n\n");
    for (i = 0; i < sizeof(cmd_table) / sizeof(command_t); i++) {
        printf("  %s\n", cmd_table[i].desc);
    }
    printf("\n");
}

int main(int argc, char *argv[])
{
    if (argc <= 1)
    {
        show_help();
        return -1;
    }

    if (!strncmp("-h", argv[1], sizeof("-h")))
    {
        show_help();
        return -1;
    }

    if (!strncmp("start", argv[1], sizeof("start")))
    {
        if (argc < 3) {
            LOG_E("Missing device path parameter!\n");
            LOG_E("Usage: %s start <device_path>\n", argv[0]);
            return -1;
        }
        g_uart_dev = argv[2];
        LOG_I("g_uart_dev: %s\n", g_uart_dev);

        struct uart_t uart;
        uart.type = "UART";
        uart.m_id = 0;
        uart.p_id = 0;
        uart.proto = HCI_UART_H4;
        uart.init_speed = 115200; // init baud
        uart.speed = 115200; // work baud
        uart.flags = 0; //FLOW_CTL;
        uart.pm = DISABLE_PM;
        uart.bdaddr = NULL;
        uart.init = rk_uart_init;
        uart.post = NULL;
    
        LOG_E("btmcu_daemon: start\n");

        // Check if the process is already running and kill it
        util_kill_process_by_name("btmcu_daemon");

        pthread_create(&btmcu_daemon_thread, NULL, btmcu_daemon_thread_func, NULL);

        LOG_D("Init UART with speed %d\n", uart.init_speed);
        g_uart_fd = uart_init(g_uart_dev, &uart, 0, 0); // init com port
        if (g_uart_fd < 0)
        {
            fprintf(stderr, "Failed to initialize UART\n");
            return -1;
        }

        while (1) {
            // Create and mointer RX thread
            if (!uart_rx_thread || !is_thread_alive(uart_rx_thread)) {
                if (uart_rx_thread) {
                    LOG_E("rx_thread is not running, creating...\n");
                    pthread_join(uart_rx_thread, NULL);  // clean up thread resources
                }
                if (pthread_create(&uart_rx_thread, NULL, uart_rx_thread_func, NULL)) {
                    LOG_E("Create rx_thread failed[%d]: %s\n", errno, strerror(errno));
                    sleep(1);
                } else {
                    LOG_I("rx_thread created successfully\n");
                }
            }
    
            // Create and mointer TX thread
            if (!btmcu_heartbeat_thread || !is_thread_alive(btmcu_heartbeat_thread)) {
                if (btmcu_heartbeat_thread) {
                    LOG_E("tx_thread is not running, creating...\n");
                    pthread_join(btmcu_heartbeat_thread, NULL);  // clean up thread resources
                }
                if (pthread_create(&btmcu_heartbeat_thread, NULL, btmcu_heartbeat_thread_func, NULL)) {
                    LOG_E("Create tx_thread failed[%d]: %s\n", errno, strerror(errno));
                    sleep(1);
                } else {
                    LOG_I("tx_thread created successfully\n");
                }
                if (pthread_create(&wifi_event_thread, NULL, wifi_event_thread_func, NULL))
                {
                    LOG_E("Create wifi_event_thread failed\n");
                    return -1;
                } else {
                    LOG_I("tx_thread created successfully\n");
                }
            }
    
            sleep(1);
        }
    }

    //LOG_I("UART initialized successfully.fd=%d\n", g_uart_fd);

    if (argc >= 2)
        return btmcu_daemon_client(argc, argv);
    //close(g_uart_fd); // close com port

    return 0;
}
