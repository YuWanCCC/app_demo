// Copyright 2021 Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <net/if.h>
#include <errno.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <stdbool.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdatomic.h>
#include <poll.h>
#include <wpa_ctrl.h>

#include "slog.h"
#include "utility.h"
#include "rk_wifi_hal.h"
#include "rk_encode.h"
#include "rk_wifi_kpalive.h"

#define EXPORT
#define HOSTAPD         "hostapd"
#define WPA_SUPPLICANT  "wpa_supplicant"
#define DNSMASQ         "dnsmasq"
#define SIMPLE_CONFIG   "simple_config"
#define SMART_CONFIG    "smart_config"
#define UDHCPC          "udhcpc"

#define EVENT_BUF_SIZE          1024
#define PROPERTY_VALUE_MAX      32
#define PROPERTY_KEY_MAX        32
#define DBG_NETWORK             1
#define IFNAMELEN               (sizeof(IFNAME) - 1)
#define IPADDRESS_MAX_LENGTH    16
#define MAX_SAVED_NETWORK       20

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

DEF_THREAD_STACK_BASE;

typedef struct
{
    char ssid[SSID_BUF_LEN];
    char bssid[BSSID_BUF_LEN];
} RK_WIFI_CONNECT_INFO;

static RK_WIFI_CONNECT_INFO g_connect_info =
{
    "",
    "",
};

typedef struct RK_WIFI_encode_gbk
{
    char ori[128];
    char utf8[128];
    struct RK_WIFI_encode_gbk *next;
} RK_WIFI_encode_gbk_t;

static char *wifi_state[] =
{
    "IDLE",
    "CONNECTING",
    "CONNECTFAILED",
    "CONNECTFAILED_NOT_FOUND",
    "CONNECTFAILED_WRONG_KEY",
    "AUTH_SUCCESS",
    "AUTH_FAILED",
    "CONNECTED",
    "DISCONNECTED",
    "DISCONNECTED_BY_HOST",
    "DISCONNECTED_BY_ROUTER",
    "DISCONNECTED_LOST_BEACON",
    "OPEN",
    "OPEN_FAIL",
    "OFF",
    "SCAN",
    "SCAN_RESULTS",
    "DHCP_ERR",
    "DHCP_OK",
    "NOT_CFG_NETWORK",
    "LP_EVENT_TYPE",
    "UNKNOW_ERR",
};

#define WPA_EVENT_IGNORE        "CTRL-EVENT-IGNORE "
#define IFNAME                  "IFNAME="
#define IFACE_PATH              "/var/run/wpa_supplicant/wlan0"
#define LOCAL_LOOPBACK_IP       "127.0.0.1"
#define MAX_CONNECT_RETRY_COUNT 5

static volatile bool g_wifi_wrong_key = false;
static volatile bool g_wifi_cancel = false;
static volatile bool g_wifi_connect_lock = false;
static volatile bool g_wifi_is_exist = false;
static volatile bool g_is_wep = false;

atomic_int g_atomic_wifi_monitor_running = ATOMIC_VAR_INIT(0);
static struct wpa_ctrl *g_monitor_conn = NULL;
static int g_connection_id = -1;
static int g_wep_id = -1;

// Variables for connecting retry
static int g_connect_retry_count = 0;

// GBK code routing list
static RK_WIFI_encode_gbk_t *m_gbk_head = NULL;
// Password-less routing list
static RK_WIFI_encode_gbk_t *m_nonpsk_head = NULL;

static RK_WIFI_encode_gbk_t *encode_gbk_insert(RK_WIFI_encode_gbk_t *head, const char *ori, const char *utf8)
{
    if ((ori == NULL || strlen(ori) == 0) || (utf8 == NULL || strlen(utf8) == 0))
        return head;

    RK_WIFI_encode_gbk_t *gbk = (RK_WIFI_encode_gbk_t *) malloc(sizeof(RK_WIFI_encode_gbk_t));
    memset(gbk->ori, 0, sizeof(gbk->ori));
    memset(gbk->utf8, 0, sizeof(gbk->utf8));

    if (strlen(ori) >= sizeof(gbk->ori) || strlen(utf8) >= sizeof(gbk->utf8))
    {
        free(gbk);
        return head;
    }

    strcat(gbk->ori, ori);
    strcat(gbk->utf8, utf8);
    //pr_info("encode_gbk_insert gbk->ori: %s, gbk->utf8: %s\n", gbk->ori, gbk->utf8);

    gbk->next = head;
    head = gbk;

    return head;
}

static RK_WIFI_encode_gbk_t *encode_gbk_reset(RK_WIFI_encode_gbk_t *head)
{
    RK_WIFI_encode_gbk_t *gbk, *gbk_next;

    gbk = head;
    while (gbk)
    {
        gbk_next = gbk->next;
        free(gbk);

        gbk = gbk_next;
    }
    head = NULL;

    return head;
}

static char *get_encode_gbk_ori(RK_WIFI_encode_gbk_t *head, const char *str, char *dst, int dst_len)
{
    int is_gbk = 0;
    RK_WIFI_encode_gbk_t *gbk = head;

    while (gbk)
    {
        //pr_info("get_encode_gbk_ori gbk->ori: %s, gbk->utf8: %s, str: %s\n", gbk->ori, gbk->utf8, str);
        if (strcmp(gbk->utf8, str) == 0)
        {
            is_gbk = 1;
            snprintf(dst, dst_len, "%s", gbk->ori);
            break;
        }
        gbk = gbk->next;
    }
    if (!is_gbk)
    {
        snprintf(dst, dst_len, "%s", str);
    }

    return dst;
}

static char *get_encode_gbk_utf8(RK_WIFI_encode_gbk_t *head, const char *str, char *dst, int dst_len)
{
    int is_gbk = 0;
    RK_WIFI_encode_gbk_t *gbk = head;

    while (gbk)
    {
        //pr_info("get_encode_gbk_utf8 gbk->ori: %s, gbk->utf8: %s, str: %s\n", gbk->ori, gbk->utf8, str);
        if (strcmp(gbk->ori, str) == 0)
        {
            is_gbk = 1;
            snprintf(dst, dst_len, "%s", gbk->utf8);
            break;
        }
        gbk = gbk->next;
    }
    if (!is_gbk)
    {
        snprintf(dst, dst_len, "%s", str);
    }

    return dst;
}

static char *remove_escape_character(const char *buf, char *dst)
{
    int i = 0;
    while (*buf != '\0') {
        if (*buf == '\\' && (*(buf + 1) == '\\' || *(buf + 1) == '\"')) {
            dst[i] = *(buf + 1);
            buf = buf + 2;
        } else {
            dst[i] = *buf;
            buf++;
        }
        i++;
    }
    dst[i] = '\0';
    return dst;
}

static char *spec_char_convers(const char *buf, char *dst)
{
    int i = 0;
    char temp[5];

    while (*buf != '\0') {
        if (*buf == '\\' && *(buf + 1) == 'x') {
            strncpy(temp, buf, 4);
            temp[0] = '0';
            temp[4] = '\0';
            dst[i] = (char)strtoul(temp, NULL, 16);
            buf += 4;
        } else {
            dst[i] = *buf;
            buf++;
        }
        i++;
    }
    dst[i] = '\0';
    return dst;
}

/**
 * List all configured networks from wpa_supplicant
 * 
 * @param pInfo: Pointer to array of RK_WIFI_SAVED_INFO_s provided by caller
 * @param ap_cnt: Input as buffer size, output as actual network count
 * @return true on success, false on failure
 */
EXPORT bool wpa_wifi_list_network(RK_WIFI_SAVED_INFO_s *pInfo, int *ap_cnt)
{
    char buffer[2048];
    int max_networks = *ap_cnt;  // Store input buffer size
    int valid_count = 0;         // Count of valid networks

    DEBUG_STACK_USAGE();
    //pr_info("Enter, max APs: %d\n", max_networks);

    // Get network list from wpa_cli
    if (!util_capture_command("wpa_cli -i wlan0 list_networks", buffer, sizeof(buffer))) {
        pr_err("Failed to get network list from wpa_cli\n");
        *ap_cnt = 0;
        return false;
    }
    
    if (strlen(buffer) == 0) {
        pr_err("Empty network list from wpa_cli\n");
        *ap_cnt = 0;
        return false;
    }

    pr_info("Raw network list:\n%s\n", buffer);

    // Parse network list
    char *saveptr = NULL;
    char *line = strtok_r(buffer, "\n", &saveptr);
    line = strtok_r(NULL, "\n", &saveptr); // Skip header line
    
    while (line && valid_count < max_networks) {
        char *field_saveptr = NULL;
        char *fields[4] = {0};
        
        //pr_info("Parsing line: %s\n", line);

        // Split line into fields using tab as delimiter
        fields[0] = strtok_r(line, "\t", &field_saveptr);
        for (int i = 1; i < 4; i++) {
            fields[i] = strtok_r(NULL, "\t", &field_saveptr);
        }

        // Check network_id, ssid, and bssid
        if (!fields[0] || !fields[1] || !fields[2]) {
            pr_err("Invalid network entry: %s\n", line);
            line = strtok_r(NULL, "\n", &saveptr);
            continue;
        }

        // Fill network info into current slot
        RK_WIFI_SAVED_INFO_s *current = &pInfo[valid_count];
        
        // Convert and validate network id
        char *endptr;
        long id = strtol(fields[0], &endptr, 10);
        if (*endptr != '\0' || id < 0) {
            pr_err("Invalid network id: %s\n", fields[0]);
            line = strtok_r(NULL, "\n", &saveptr);
            continue;
        }
        current->id = (int)id;

        // Copy strings with length checks
        strlcpy(current->ssid, fields[1], SSID_BUF_LEN);
        
        // Handle special case for bssid
        if (strcmp(fields[2], "any") == 0) {
            strlcpy(current->bssid, "00:00:00:00:00:00", BSSID_BUF_LEN);
        } else {
            strlcpy(current->bssid, fields[2], BSSID_BUF_LEN);
        }
        
        // Handle state field, that might be not exist
        if (fields[3]) {
            strlcpy(current->state, fields[3], STATE_BUF_LEN);
        } else {
            strlcpy(current->state, "[UNKNOWN]", STATE_BUF_LEN);
        }

        /*pr_info("Network %d: id=%d, ssid=%.32s, bssid=%.17s, flags=%.16s\n",
                valid_count, current->id, current->ssid, 
                current->bssid, current->state);*/

        valid_count++;
        line = strtok_r(NULL, "\n", &saveptr);
    }

    if (line) {
        pr_info("Buffer full, some networks not included (filled %d of %d)\n", 
                valid_count, max_networks);
    }

    *ap_cnt = valid_count;  // Return actual number of networks
    return true;
}

static int wpa_wifi_search_with_ssid(char *ssid)
{
    RK_WIFI_SAVED_INFO_s wsi[MAX_SAVED_NETWORK];
    int ap_cnt = MAX_SAVED_NETWORK;
    int id, len;

    DEBUG_STACK_USAGE();
    pr_info("enter\n");

    wpa_wifi_list_network(wsi, &ap_cnt);
    if (ap_cnt <= 0)
        return -EINVAL;

    for (int i = 0; i < ap_cnt; i++)
    {
        if ((strlen(wsi[i].ssid) > 64) || (strlen(ssid) > 64))
            pr_err("ssid len error: %d, %d\n", strlen(wsi[i].ssid), strlen(ssid));

        if (strlen(wsi[i].ssid) > strlen(ssid))
            len = strlen(wsi[i].ssid);
        else
            len = strlen(ssid);

        if (strncmp(wsi[i].ssid, ssid, len) == 0)
        {
            id = wsi[i].id;
            return id;
        }
    }

    return -EINVAL;
}

static bool get_wifi_status(RK_WIFI_INFO_Connection_s *pInfo)
{
    char buffer[512];
    char *line, *value;

    DEBUG_STACK_USAGE();
    //pr_info("Enter\n");

    if (!pInfo) {
        pr_err("Invalid pInfo(%p)\n", pInfo);
        return false;
    }

    memset(pInfo, 0, sizeof(RK_WIFI_INFO_Connection_s));
    
    if (!util_capture_command("wpa_cli -i wlan0 status", buffer, sizeof(buffer))) {
        pr_err("Failed to get status from wpa_cli\n");
        return false;
    }
    
    if (strlen(buffer) == 0) {
        pr_err("Empty status from wpa_cli\n");
        return false;
    }
    //pr_info("Raw status:\n%s\n\n", buffer);

    line = strtok(buffer, "\n");
    while (line) {
        if ((value = strchr(line, '='))) {
            *value++ = '\0';
            
            if (!strcmp(line, "bssid")) {
                strlcpy(pInfo->bssid, value, sizeof(pInfo->bssid));
            } else if (!strcmp(line, "freq")) {
                pInfo->freq = atoi(value);
            } else if (!strcmp(line, "ssid")) {
                char sname[256], sname1[256], utf8[256];
                spec_char_convers(value, sname);
                remove_escape_character(sname, sname1);
                get_encode_gbk_utf8(m_gbk_head, sname1, utf8, sizeof(utf8));
                strlcpy(pInfo->ssid, utf8, sizeof(pInfo->ssid));
            } else if (!strcmp(line, "id")) {
                pInfo->id = atoi(value);
            } else if (!strcmp(line, "mode")) {
                strlcpy(pInfo->mode, value, sizeof(pInfo->mode));
            } else if (!strcmp(line, "wpa_state")) {
                strlcpy(pInfo->wpa_state, value, sizeof(pInfo->wpa_state));
            } else if (!strcmp(line, "ip_address")) {
                strlcpy(pInfo->ip_address, value, sizeof(pInfo->ip_address));
            } else if (!strcmp(line, "address")) {
                strlcpy(pInfo->mac_address, value, sizeof(pInfo->mac_address));
            }
        }
        line = strtok(NULL, "\n");
    }
    return true;
}

EXPORT bool wpa_wifi_get_status(RK_WIFI_INFO_Connection_s *pInfo)
{
    if (pInfo == NULL)
    {
        pr_err("Invalid pInfo(%p)\n", pInfo);
        return false;
    }

    if (!get_wifi_status(pInfo)) {
        pr_err("Failed to get wifi status\n");
        return false;
    }

    // Add gw and dns to info
    memset(pInfo->gw, 0, sizeof(pInfo->gw));
    memset(pInfo->dns, 0, sizeof(pInfo->dns));
    util_network_gw_get(pInfo->gw);
    util_network_dns_get(pInfo->dns, NULL);

    return true;
}

static void wifi_state_send(RK_WIFI_RUNNING_State_e state, RK_WIFI_INFO_Connection_s *info)
{
    DEBUG_STACK_USAGE();

    if (state >= sizeof(wifi_state) / sizeof(wifi_state[0]))
    {
        pr_err("State '%d' is out of range\n", state);
        return;
    }
    else
    {
        pr_info("[RKWIFI]: State %s\n", wifi_state[state]);
    }

    if (m_wifi_cb == NULL)
    {
        pr_info("ERROR: m_wifi_cb is NULL\n");
        return;
    }

    if (state == RK_WIFI_State_CONNECTFAILED || state == RK_WIFI_State_CONNECTING)
    {
        RK_WIFI_INFO_Connection_s cndinfo;

        get_wifi_status(&cndinfo);
        strlcpy(cndinfo.ssid, g_connect_info.ssid, sizeof(cndinfo.ssid));
        strlcpy(cndinfo.bssid, g_connect_info.bssid, sizeof(cndinfo.bssid));
        m_wifi_cb(state, &cndinfo);
    }
    else
    {
        if (info)
        {
            info->ssid[SSID_BUF_LEN - 1] = '\0';
            info->bssid[BSSID_BUF_LEN - 1] = '\0';
        }
        m_wifi_cb(state, info);
    }
}

static int get_wifi_fw_hotboot_flag(void)
{
    int ret = -1;
    char buffer[4];
    int fd;
    ssize_t bytes;
    char *endptr, *newline;
    unsigned int value;

    DEBUG_STACK_USAGE();
    #if 0
    util_capture_command("cat /sys/module/rk96x/parameters/rk962_fw_dwst_hotboot", buffer, sizeof(buffer));
    #else
    fd = open("/sys/kernel/rkfs/rk962_fw_dwst_hotboot", O_RDONLY);
    if ((bytes = read(fd, buffer, sizeof(buffer) - 1)) < 0) {
        pr_info("read failed");
        close(fd);
        return -1;
    }
    close(fd);
    buffer[bytes] = '\0';
    pr_info("rk962_fw_dwst_hotboot: %s,bytes=%d", buffer,bytes);

    newline = strchr(buffer, '\n');
    if (newline)
        *newline = '\0';

    value = strtoul(buffer, &endptr, 16);

    if (endptr == buffer || *endptr != '\0') {
        pr_info("Invalid hex format: '%s'", buffer);
        return -1;
    }
    pr_info(" 0x%X", value);
    #endif
    ret = value & 0xff;
    pr_info("ret=%d.", ret);
    return ret;
}

static void wifi_close_wpa()
{
    DEBUG_STACK_USAGE();
    //pr_info("enter\n");

    if (g_monitor_conn != NULL)
    {
        wpa_ctrl_close(g_monitor_conn);
        g_monitor_conn = NULL;
    }

    /*
    if (exit_sockets[0] >= 0) {
        close(exit_sockets[0]);
        exit_sockets[0] = -1;
    }

    if (exit_sockets[1] >= 0) {
        close(exit_sockets[1]);
        exit_sockets[1] = -1;
    }
    */
}

static bool disable_no_connected_ssid(void)
{
    RK_WIFI_INFO_Connection_s info;
    RK_WIFI_SAVED_INFO_s saved_info[MAX_SAVED_NETWORK];
    int ap_cnt = MAX_SAVED_NETWORK;

    DEBUG_STACK_USAGE();

    if (get_wifi_status(&info) == false)
        return false;

    if (!wpa_wifi_list_network(saved_info, &ap_cnt) || ap_cnt <= 0) {
        pr_err("Failed to get saved networks\n");
        return false;
    }

    for (int i = 0; i < ap_cnt; i++) {
        if (saved_info[i].id != info.id) {
            char cmd[64];
            snprintf(cmd, sizeof(cmd), "wpa_cli -i wlan0 disable_network %d", saved_info[i].id);
            util_exec_command(cmd);
        }
    }

    return true;
}

static bool str_starts_with(const char *str, const char *search_str)
{
    if ((str == NULL) || (search_str == NULL))
        return false;
    return (strstr(str, search_str) == str);
}

static void get_wifi_info_by_event(const char *event, RK_WIFI_RUNNING_State_e state, RK_WIFI_INFO_Connection_s *info)
{
    int len = 0;
    char *start_tag = NULL, *id_tag = NULL, *reason_tag = NULL;

    DEBUG_STACK_USAGE();

    if (event == NULL)
        return;

    get_wifi_status(info);

    switch (state)
    {
    case RK_WIFI_State_DISCONNECTED:
        start_tag = strstr(event, "bssid=");
        if (start_tag)
        {
            strlcpy(info->bssid, start_tag + strlen("bssid="), 17+1);
        }
        reason_tag =  strstr(event, "reason=");
        if (reason_tag)
        {
            strlcpy(info->reason, reason_tag + strlen("reason="), 1+1);
        }

        break;

    case RK_WIFI_State_CONNECTFAILED_WRONG_KEY:
        start_tag = strstr(event, "ssid=\"");
        if (start_tag)
        {
            char *ssid_start = start_tag + strlen("ssid=\"");
            char *ssid_end = strchr(ssid_start, '\"');
            if (!ssid_end || ssid_end >= event + strlen(event)) {
                pr_err("SSID format error: missing closing quote\n");
                break;
            }
            len = ssid_end - ssid_start;

            char buf1[256] = {0};
            char buf2[256] = {0};
            char utf8[256] = {0};
            strlcpy(buf1, ssid_start, len + 1);
            //pr_info("check len value: %d\n", strlen(value));
            spec_char_convers(buf1, buf2);
            remove_escape_character(buf2, buf1);
            get_encode_gbk_utf8(m_gbk_head, buf1, utf8, sizeof(utf8));
            //pr_info("convers str: %s, sname: %s, ori: %s\n", value, sname1, utf8);
            strlcpy(info->ssid, utf8, sizeof(info->ssid));
        }

        id_tag =  strstr(event, "id=");
        if (id_tag)
        {
            char buf[32] = {0};
            len = strlen(id_tag) - strlen("id=") - strlen(start_tag) - 1;
            memset(buf, 0, sizeof(buf));
            strlcpy(buf, id_tag + strlen("id="), len + 1);
            info->id = atoi(buf);
        }

        strlcpy(info->bssid, g_connect_info.bssid, sizeof(info->bssid));
        strlcpy(info->reason, "WRONG_KEY", sizeof(info->reason));
        break;
    
    default:
        pr_info("unsupported state for getting info: %d\n", state);
        break;
    }
}

static bool wpa_supplicant_is_running()
{
    DEBUG_STACK_USAGE();

    if (util_find_pid_by_name(WPA_SUPPLICANT) > 0)
    {
        return true;
    }

    pr_info("wpa is not running.\n");
    return false;
}

static bool wifi_ctrl_recv(char *reply, size_t *reply_len)
{
    int res = -1;
    int ctrlfd = -1;
    struct pollfd rfds[1];

    DEBUG_STACK_USAGE();
    ctrlfd = wpa_ctrl_get_fd(g_monitor_conn);
    if (ctrlfd < 0) {
        pr_err("Invalid control fd: %d\n", ctrlfd);
        return false;
    }

    while (1)
    {
        memset(rfds, 0, 1 * sizeof(struct pollfd));
        rfds[0].fd = ctrlfd;
        rfds[0].events |= POLLIN;
        rfds[0].revents = 0;
        res = poll(rfds, 1, 1000);

        if (res < 0)    // error
        {
            if (errno == EINTR) {
                pr_info("poll interrupted by signal, retrying...\n");
                continue;
            }
            pr_err("poll error = %s\n", strerror(errno));
            return false;
        }
        else if (res == 0)  // time out
        {
            if (!wpa_supplicant_is_running())
            {
                pr_err("wpa supplicant is not running\n");
                return false;
            }
            continue;
        }
        else    // res > 0, received something
        {
            if (rfds[0].revents & POLLIN)
            {
                return (wpa_ctrl_recv(g_monitor_conn, reply, reply_len) == 0);
            }        
        }
    }

    pr_err("imposible to be here\n");
    return false;
}

static bool wifi_wait_on_wpa(char *buf, size_t buflen)
{
    size_t nread = buflen - 1;
    int result;
    char *match, *match2;

    DEBUG_STACK_USAGE();

    if (g_monitor_conn == NULL)
    {
        pr_err("g_monitor_conn is NULL\n");
        return false;
    }

    result = wifi_ctrl_recv(buf, &nread);
    if (!result || nread == 0)
    {
        if (0 == nread)
            pr_info("Received EOF on supplicant socket\n");
        return false;
    }

    buf[nread] = '\0';

    if (strncmp(buf, IFNAME, IFNAMELEN) == 0)
    {
        match = strchr(buf, ' ');
        if (match != NULL)
        {
            if (match[1] == '<')
            {
                match2 = strchr(match + 2, '>');
                if (match2 != NULL)
                {
                    nread -= (match2 - match);
                    memmove(match + 1, match2 + 1, nread - (match - buf) + 1);
                }
            }
        }
        else
        {
            return snprintf(buf, buflen, "%s", WPA_EVENT_IGNORE);
        }
    }
    else if (buf[0] == '<')
    {
        match = strchr(buf, '>');
        if (match != NULL)
        {
            nread -= (match + 1 - buf);
            memmove(buf, match + 1, nread + 1);
        }
    }
    else
    {
    }

    return nread;
}

/* Establishes the control and monitor socket connections on the interface */
static bool wifi_connect_to_wpa()
{
    int count = 30;

    DEBUG_STACK_USAGE();
    //pr_info("enter\n");

    while (count-- > 0)
    {
        if (access(IFACE_PATH, F_OK) == 0)
            break;
        pr_info("wait wpa_supplicant ready...\n");
        usleep(500000);
    }

    if (!wpa_supplicant_is_running())
    {
        pr_info("wpa_supplicant is not ready\n");
        return false;
    }

    g_monitor_conn = wpa_ctrl_open(IFACE_PATH);
    if (g_monitor_conn == NULL)
    {
        pr_info("g_monitor_conn Unable to open connection to supplicant on \"%s\": %s\n", IFACE_PATH, strerror(errno));
        return false;
    }

    if (wpa_ctrl_attach(g_monitor_conn) != 0)
    {
        pr_info("wpa_ctrl_attach Unable to open connection to supplicant on \"%s\": %s\n", IFACE_PATH, strerror(errno));
        wpa_ctrl_close(g_monitor_conn);
        g_monitor_conn = NULL;
        return false;
    }

    return true;
}

static bool parse_config_value(const char *buffer, const char *target_key, char *output, size_t output_len)
{
    const char *p = buffer;
    size_t key_len = strlen(target_key);

    while (*p) {
        if (strncmp(p, target_key, key_len) == 0 && p[key_len] == '=') {
            const char *value_start = p + key_len + 1;
            const char *value_end = strchr(value_start, '\n');
            size_t value_len = value_end ? (size_t)(value_end - value_start) : strlen(value_start);

            if (value_len >= output_len) {
                pr_err("Buffer too small for key %s\n", target_key);
                return false;
            }

            strlcpy(output, value_start, value_len + 1);
            return true;
        }
        p = strchr(p, '\n');
        if (!p++) break;
    }
    return false;
}

static bool load_kpalive_ip_config(char *ip, char *gw)
{
    char content[512];
    size_t bytes_read = 0;
    DEBUG_STACK_USAGE();

    FILE *fp = fopen(KEEPALIVE_CONF_PATH, "r");
    if (!fp) {
        pr_err("Failed to open keepalive config file\n");
        return false;
    }

    memset(content, 0, sizeof(content));
    bytes_read = fread(content, 1, sizeof(content)-1, fp);
    fclose(fp);
    if (bytes_read == 0) {
        pr_err("Failed to read keepalive config file or file is empty\n");
        return false;
    }
    content[bytes_read] = '\0';

    if (!parse_config_value(content, "ip", ip, IPADDRESS_MAX_LENGTH)) {
        pr_err("Failed to parse IP address from file\n");
        return false;
    }
    
    if (!parse_config_value(content, "gw", gw, IPADDRESS_MAX_LENGTH)) {
        pr_err("Failed to parse Gateway address from file\n");
        return false;
    }

    pr_info("IP: [%s], GW: [%s]\n", ip, gw);
    return true;
}

EXPORT bool wpa_wifi_is_connected_with_ip(void)
{
    bool ret = false;
    RK_WIFI_INFO_Connection_s wifiinfo;

    DEBUG_STACK_USAGE();

    get_wifi_status(&wifiinfo);
    if (strncmp(wifiinfo.wpa_state, "COMPLETED", strlen("COMPLETED")) == 0)
    {
        if (strlen(wifiinfo.ip_address) != 0 &&
            strncmp(wifiinfo.ip_address, LOCAL_LOOPBACK_IP, strlen(LOCAL_LOOPBACK_IP)) != 0)
        {
            ret = true;
        }
    }

    //pr_info("%s\n", ret ? "CONNECTED" : "NOT CONNECTED");
    return ret;
}

static bool config_ip_network(void)
{
    char ip[32];
    char gw[32];
    char command[128];
    const int retry_count = 15;
    int i;
    DEBUG_STACK_USAGE();

    if (load_kpalive_ip_config(ip, gw))
    {
        pr_info("get ip/gw from keepalive config file...\n");

        //ip address
        sprintf(command, "ifconfig wlan0 %s netmask 255.255.255.0", ip);
        util_exec_command(command);

        //gateway
        sprintf(command, "route add default gw %s", gw);
        util_exec_command(command);

        //dns
        util_network_dns_recovery();
    }
    else
    {
        pr_info("get ip/gw by udhcpc...\n");
        util_kill_process_by_name("udhcpc");
        usleep(300000);
        util_exec_command("udhcpc -i wlan0 -T 1 -A 0 -b -q &");
        sleep(1);
    }

    for (i = 0; i < retry_count; i++)
    {
        if (wpa_wifi_is_connected_with_ip())
        {
            pr_info("Success to configure IP.\n");
            return true;
        }
        pr_info("Waiting %d/%d...\n", i + 1, retry_count);
        sleep(1);
    }
    if (i == retry_count)
    {
        pr_info("Network recovery not completed after timeout\n");
        util_kill_process_by_name("udhcpc");
    }

    return false;
}

static void wpa_save_config(void)
{
    util_exec_command("wpa_cli -i wlan0 save_config");
    util_exec_command("sync");

    return;
}

static void wpa_handle_connect_fail(int id)
{
    char cmd[128] = {0};

    DEBUG_STACK_USAGE();
    pr_info("Enter\n");

    if (g_wifi_is_exist == true)
    {
        snprintf(cmd, sizeof(cmd), "wpa_cli -iwlan0 disable_network %d", id);
        util_exec_command(cmd);
    }
    else
    {
        snprintf(cmd, sizeof(cmd), "wpa_cli -iwlan0 remove_network %d", id);
        util_exec_command(cmd);
        wpa_save_config();
    }

    return;
}

static bool dispatch_event(const char *event)
{
    RK_WIFI_INFO_Connection_s info;
    DEBUG_STACK_USAGE();

    if (!strstr(event, "CTRL-EVENT-BSS"))  //too many BSS-ADDED and BSS-REMOVED events
        pr_info("[Event] %s\n", event);

    if (strstr(event, "CTRL-EVENT-BSS") /*|| strstr(event, "CTRL-EVENT-TERMINATING")*/)
        return true;

    if (str_starts_with(event, WPA_EVENT_DISCONNECTED))
    {
        //util_exec_command("ip addr flush dev wlan0");    //ip command is not supported by busybox
        util_kill_process_by_name("udhcpc");
        util_exec_command("ifconfig wlan0 0.0.0.0");
        get_wifi_info_by_event(event, RK_WIFI_State_DISCONNECTED, &info);
		g_wifi_connect_lock = false;
        wifi_state_send(RK_WIFI_State_DISCONNECTED, &info);

        // Reconnect
        /*if (g_wifi_connect_lock == false)
        {
            pr_info("reconnecting...\n");
            util_exec_command("wpa_cli -i wlan0 enable_network all");
            util_exec_command("wpa_cli -i wlan0 reconnect");
        }*/
    }
    else if (str_starts_with(event, WPA_EVENT_CONNECTED))
    {
        wpa_save_config();
        wifi_state_send(RK_WIFI_State_CONNECTED, NULL);
        disable_no_connected_ssid();
        bool dhcp_ok = config_ip_network();
        util_kill_process_by_name("udhcpc");
        g_wifi_connect_lock = false;
        if (dhcp_ok)
        {
            wpa_wifi_get_status(&info); //include gw and dns
            wifi_state_send(RK_WIFI_State_DHCP_OK, &info);
        }
        else
        {
            wifi_state_send(RK_WIFI_State_DHCP_ERR, NULL);
        }
    }
    else if (str_starts_with(event, WPA_EVENT_SCAN_RESULTS))
    {
        wifi_state_send(RK_WIFI_State_SCAN_RESULTS, NULL);
    }
    else if (strstr(event, "reason=WRONG_KEY"))
    {
        g_wifi_wrong_key = true;
        if (g_connection_id >= 0) {
            wpa_handle_connect_fail(g_connection_id);
        }
        g_wifi_connect_lock = false;
        get_wifi_info_by_event(event, RK_WIFI_State_CONNECTFAILED_WRONG_KEY, &info);
        wifi_state_send(RK_WIFI_State_CONNECTFAILED_WRONG_KEY, &info);
    }
    else if (str_starts_with(event, WPA_EVENT_NETWORK_NOT_FOUND))
    {
        if (g_wifi_connect_lock)
        {
            if ((g_connect_retry_count < MAX_CONNECT_RETRY_COUNT) && g_wifi_connect_lock)
            {
                g_connect_retry_count++;
                pr_info("Retrying connection, attempt %d/%d\n", g_connect_retry_count, MAX_CONNECT_RETRY_COUNT);
                util_exec_command("wpa_cli -i wlan0 scan");
                sleep(2);
                util_exec_command("wpa_cli -i wlan0 reconnect");
            }
            else    // finally failed
            {
                pr_info("Max retry count reached or not in connecting state, giving up\n");
                g_connect_retry_count = 0;
                if (g_connection_id >= 0) {
                    wpa_handle_connect_fail(g_connection_id);
                }
                g_wifi_connect_lock = false;
                wifi_state_send(RK_WIFI_State_CONNECTFAILED, NULL);
            }
        }
    }
    else if (str_starts_with(event, WPA_EVENT_TERMINATING))
    {
        wifi_close_wpa();
		pr_info("WPA terminated! unlock g_wifi_connect_lock\n");
		g_wifi_connect_lock = false;
        return false;
    }

    if (g_is_wep && strstr(event, "CTRL-EVENT-ASSOC-REJECT") && strstr(event, "status_code=13"))
    {
        char cmd[128];
        memset(cmd, 0, 128);
        snprintf(cmd, sizeof(cmd), "wpa_cli -i wlan0 set_network %d auth_alg OPEN", g_wep_id);
        util_exec_command(cmd);
        memset(cmd, 0, 128);
        snprintf(cmd, sizeof(cmd), "wpa_cli -i wlan0 select_network %d", g_wep_id);
        util_exec_command(cmd);
    }

    return true;
}

static void *RK_wifi_monitor_thread_cb(void *arg)
{
    UNUSED(arg);
    SET_THREAD_STACK_BASE();

    char eventStr[EVENT_BUF_SIZE] = {0};

    DEBUG_STACK_USAGE();

    atomic_store_explicit(&g_atomic_wifi_monitor_running, 1, memory_order_release);

    pr_info("Enter\n");

    prctl(PR_SET_NAME, "RK_wifi_monitor");

    for (;;)
    {
        memset(eventStr, 0, EVENT_BUF_SIZE);
        if (!wifi_wait_on_wpa(eventStr, EVENT_BUF_SIZE))
            continue;

        if (!dispatch_event(eventStr))
        {
            pr_info("disconnecting from the supplicant, no more events\n");
            break;
        }
    }

    pr_info("------ exit RK_wifi_monitor ------$\n");

    atomic_store_explicit(&g_atomic_wifi_monitor_running, 0, memory_order_release);
    return NULL;
}

static bool wpa_wifi_enable(int enable)
{
    static pthread_t wifi_monitor_thread_id = 0;
    void *retval;
    int s;

    DEBUG_STACK_USAGE();
    pr_info("+++++ wifi version: V1.4 [%s]+++++\n", enable ? "open" : "close");
    pr_info("Init, monitor_id: 0x%lx\n",wifi_monitor_thread_id);

    //Open WIFI
    if (enable)
    {
        //wifi is off status
        if (!wpa_supplicant_is_running())
        {
            pr_info("[WiFi is OFF], enable = 1, OPEN wifi...\n");

            pr_info("bring up wlan0\n");
            util_kill_process_by_name("udhcpc");
            //util_exec_command("ifconfig wlan0 down");
            util_exec_command("ifconfig wlan0 up");
            usleep(100000);
            util_exec_command("ifconfig wlan0 0.0.0.0");
            usleep(100000);

            pr_info("check hotboot or cold boot\n");
            if (0 == get_wifi_fw_hotboot_flag())
            {
                if (access("/data/fastlink_snonce.bin", F_OK) == 0)
                {
                    pr_info("Cold boot, fastlink_snonce.bin exists, delete it.\n");
                    util_exec_command("rm /data/fastlink_snonce.bin");
                    util_exec_command("sync");
                }
                else
                {
                    pr_info("Cold boot, fastlink_snonce.bin do not exist.\n");
                }
            }

            pr_info("run wpa_supplicant\n");
            //check: /data/cfg/wpa conf
            if (access("/data/cfg/wpa_supplicant.conf", F_OK) == 0)
            {
                pr_info("001 start wpa\n");
                util_exec_command("wpa_supplicant -B -i wlan0 -c /data/cfg/wpa_supplicant.conf -d");
            }
            else
            {
                char *rk96x_conf_path = getenv("RK96X_CONF_PATH");
                char wpa_conf_path[128] = {0};
                char cmd[256] = {0};

                snprintf(wpa_conf_path, sizeof(wpa_conf_path), "%s/wpa_supplicant.conf", rk96x_conf_path);
                pr_info("wpa_conf_path: '%s'\n", wpa_conf_path);

                //check: /etc/wpa conf
                if (access("/etc/wpa_supplicant.conf", F_OK) == 0)
                {
                    //check: /data/wpa conf 01
                    if (access(wpa_conf_path, F_OK) == 0)
                    {
                        pr_info("002 start wpa\n");
                        snprintf(cmd, sizeof(cmd), "wpa_supplicant -B -i wlan0 -c %s -d", wpa_conf_path);
                        pr_info("cmd: '%s'\n", cmd);
                        util_exec_command(cmd);
                    }
                    else
                    {
                        //check data not exist, copy from etc
                        snprintf(cmd, sizeof(cmd), "cp /etc/wpa_supplicant.conf %s", wpa_conf_path);
                        pr_info("cmd: '%s'\n", cmd);
                        if (util_exec_command(cmd))
                        {
                            pr_info("003 start wpa\n");
                            snprintf(cmd, sizeof(cmd), "wpa_supplicant -B -i wlan0 -c %s -d", wpa_conf_path);
                            pr_info("cmd: '%s'\n", cmd);
                            util_exec_command(cmd);
                        }
                        else
                        {
                            pr_info("004 start wpa\n");
                            util_exec_command("wpa_supplicant -B -i wlan0 -c /etc/wpa_supplicant.conf -d");
                        }
                    }
                }
                else
                {
                    //check: /data/wpa conf 02
                    if (access(wpa_conf_path, F_OK) == 0)
                    {
                        pr_info("005 start wpa\n");
                        snprintf(cmd, sizeof(cmd), "wpa_supplicant -B -i wlan0 -c %s -d", wpa_conf_path);
                        pr_info("cmd: '%s'\n", cmd);
                        util_exec_command(cmd);
                   }
                    else
                    {
                        //check data not exist, failure
                        pr_info("Don't found wpa_supplicant.conf so RK_wifi_enable failed!\n");
                        wifi_state_send(RK_WIFI_State_OFF, NULL);
                    }
                }
            }

            if (wifi_connect_to_wpa() != true)
            {
                pr_info("Connect to supplicant failed.\n");
                return false;
            }
        
            usleep(200000);  //100,150ms is ok, 50ms ping take more time.

            pr_info("check wifi monitor thread\n");
            if (wifi_monitor_thread_id != 0)
            {
                // check thread is active
                if (atomic_load_explicit(&g_atomic_wifi_monitor_running, memory_order_acquire))
                {
                    s = pthread_cancel(wifi_monitor_thread_id);
                    if (s != 0)
                    {
                        pr_info("pthread_cancel failed: %s\n", strerror(s));
                    }
                    s = pthread_join(wifi_monitor_thread_id, &retval);
                    if (s != 0)
                    {
                        pr_info("pthread_join failed: %s\n", strerror(s));
                    }
                    if (retval == PTHREAD_CANCELED)
                    {
                        pr_info("Enable = open, monitor thread caceled OK.\n");
                    }
                    else
                    {
                        pr_info("Enable = open, monitor thread caceled FAILURE.\n");
                    }
                }

                // reset pid
                wifi_monitor_thread_id = 0;
            }

            pr_info("send wifi state\n");
            wifi_state_send(RK_WIFI_State_OPEN, NULL);

            pr_info("start wifi monitor thread\n");
            s = pthread_create(&wifi_monitor_thread_id, NULL,
                               RK_wifi_monitor_thread_cb, NULL);
            if (s != 0)
            {
                errno = s;
                perror("RK_wifi_enable enable failed!");
                pr_info("RK_wifi_enable failed!\n");
                wifi_state_send(RK_WIFI_State_OFF, NULL);
                return false;
            }
            pthread_detach(wifi_monitor_thread_id);

            pr_info("RK_wifi_enable enable ok!\n");
        }
        else
        {
            pr_info("[WiFi is ON], enable = 1, do nothing\n");
            wifi_state_send(RK_WIFI_State_OPEN, NULL);
        }
    }
    else //Close WIFI
    {
        //wifi is ON now
        if (wpa_supplicant_is_running())
        {
            pr_info("[WiFi is ON], enable = 0, CLOSE wifi ... \n");

            util_exec_command("wpa_cli -i wlan0 enable_network all");
            wpa_save_config();
            usleep(200000);

            util_exec_command("ifconfig wlan0 down");
            usleep(300000);

            util_kill_process_by_name("wpa_supplicant");
            usleep(300000);
            util_kill_process_by_name("udhcpc");
            usleep(300000);

            if (wifi_monitor_thread_id != 0)
            {
                // check thread is active
                if (atomic_load_explicit(&g_atomic_wifi_monitor_running, memory_order_acquire))
                {
                    s = pthread_cancel(wifi_monitor_thread_id);
                    if (s != 0)
                    {
                        pr_info("pthread_cancel failed: %s\n", strerror(s));
                    }
                    s = pthread_join(wifi_monitor_thread_id, &retval);
                    if (s != 0)
                    {
                        pr_info("pthread_join failed: %s\n", strerror(s));
                    }
                    if (retval == PTHREAD_CANCELED)
                    {
                        pr_info("Enable = open, monitor thread caceled.\n");
                    }
                    else
                    {
                        pr_info("Enable = open, cancel monitor thread failed.\n");
                    }
                }

                // reset pid
                wifi_monitor_thread_id = 0;
            }

            wifi_state_send(RK_WIFI_State_OFF, NULL);
            wifi_close_wpa();

            util_kill_process_by_name("wpa_supplicant");
            util_kill_process_by_name("udhcpc");

            pr_info("Disable done.\n");
        }
        else
        {
            pr_info("[WiFi is OFF], enable = 0, do nothing\n");
            wifi_state_send(RK_WIFI_State_OFF, NULL);
        }
    }
    return true;
}

EXPORT bool wpa_wifi_scan(void)
{
    char str[32];

    DEBUG_STACK_USAGE();
    pr_info("Enter\n");

    memset(str, 0, sizeof(str));
    util_capture_command("wpa_cli -iwlan0 scan", str, 32);

    if (0 != strncmp(str, "OK", 2) &&  0 != strncmp(str, "ok", 2))
    {
        pr_info("scan error: %s\n", str);
        return -2;
    }

    return true;
}

static void convert_and_cache_ssid_encoding(const char *src, char *utf8_out, 
    int is_nonpsk, size_t out_size) 
{
    char dst[strlen(src) + 1];
    char utf8_noescape[out_size];
    char dst_noescape[out_size];

    memset(dst, 0, sizeof(dst));
    spec_char_convers(src, dst);

    int is_utf8 = RK_encode_is_utf8(dst, strlen(dst));

    memset(utf8_noescape, 0, sizeof(utf8_noescape));
    memset(dst_noescape, 0, sizeof(dst_noescape));

    if (!is_utf8) {
        RK_encode_gbk_to_utf8(dst, strlen(dst), utf8_out);
        remove_escape_character(dst, dst_noescape);
        remove_escape_character(utf8_out, utf8_noescape);
        m_gbk_head = encode_gbk_insert(m_gbk_head, dst_noescape, utf8_noescape);

        if (!RK_encode_is_utf8(utf8_out, strlen(utf8_out))) {
            *utf8_out = '\0';
        }
    } else {
        strlcpy(utf8_out, dst, out_size);
        remove_escape_character(dst, dst_noescape);
        remove_escape_character(utf8_out, utf8_noescape);
    }

    if (is_nonpsk) {
        m_nonpsk_head = encode_gbk_insert(m_nonpsk_head, dst_noescape, utf8_noescape);
    }
}

static char *wpa_wifi_scan_internal(const unsigned int cols, bool format_output)
{
    // Buffer for command output
    char *scan_r = NULL;
    char item[384];
    char col[256];
    char *p_strtok;
    size_t size = 0;
    size_t output_size = 4096;
    FILE *temp_file = NULL;
    char temp_path[] = "/tmp/wifi_scan_results.XXXXXX";

    // Return empty array if no columns are selected
    if (!(cols & 0x1F)) {
        scan_r = (char *) malloc(3 * sizeof(char));
        if (!scan_r) return NULL;
        strcpy(scan_r, "[]");
        return scan_r;
    }

    // Execute wpa_cli command and capture output directly
    char cmd[128] = {0};
    snprintf(cmd, sizeof(cmd), "wpa_cli -i wlan0 scan_results > %s", temp_path);
    if (!util_exec_command(cmd)) {
        pr_err("Failed to execute scan_results command\n");
        return NULL;
    }
    
    // Open the temporary file for reading
    temp_file = fopen(temp_path, "r");
    if (!temp_file) {
        pr_err("Failed to open temporary file: %s\n", strerror(errno));
        //unlink(temp_path);
        return NULL;
    }
    
    // Get file size
    fseek(temp_file, 0, SEEK_END);
    long file_size = ftell(temp_file);
    fseek(temp_file, 0, SEEK_SET);
    
    pr_info("Scan results file size: %ld bytes\n", file_size);
    if (file_size <= 0) {
        pr_err("Empty scan results\n");
        fclose(temp_file);
        //unlink(temp_path);
        scan_r = (char *) malloc(3 * sizeof(char));
        if (!scan_r) return NULL;
        strcpy(scan_r, "[]");
        return scan_r;
    }

    // Initialize result buffer
    size = output_size;
    scan_r = (char *) malloc(size * sizeof(char));
    if (!scan_r) {
        fclose(temp_file);
        //unlink(temp_path);
        return NULL;
    }
    memset(scan_r, 0, size);
    strcat(scan_r, "[");

    // Reset encoding lists
    m_gbk_head = encode_gbk_reset(m_gbk_head);
    m_nonpsk_head = encode_gbk_reset(m_nonpsk_head);

    // Skip header lines until we find actual BSSID data
    // Different wpa versions may have different header formats
    char line_buffer[512];
    bool found_bssid_line = false;

    while (fgets(line_buffer, sizeof(line_buffer), temp_file)) {
        // Remove trailing newline for checking
        size_t line_len = strlen(line_buffer);
        if (line_len > 0 && line_buffer[line_len-1] == '\n') {
            line_buffer[line_len-1] = '\0';
            line_len--;
        }

        // Check if this line contains BSSID data (MAC address format: xx:xx:xx:xx:xx:xx)
        if (line_len > 17 &&
            line_buffer[2] == ':' && line_buffer[5] == ':' && line_buffer[8] == ':' &&
            line_buffer[11] == ':' && line_buffer[14] == ':' &&
            isxdigit(line_buffer[0]) && isxdigit(line_buffer[1]) &&
            isxdigit(line_buffer[3]) && isxdigit(line_buffer[4])) {
            // This looks like a BSSID line, seek back to beginning of this line
            long current_pos = ftell(temp_file);
            fseek(temp_file, current_pos - strlen(line_buffer) - 1, SEEK_SET);
            found_bssid_line = true;
            break;
        }
    }

    if (!found_bssid_line) {
        pr_err("No valid BSSID data found in scan results\n");
        fclose(temp_file);
        //unlink(temp_path);
        free(scan_r);
        return NULL;
    }

    // Process all lines
    while (fgets(line_buffer, sizeof(line_buffer), temp_file)) {
        // Remove trailing newline if present
        size_t line_len = strlen(line_buffer);
        if (line_len > 0 && line_buffer[line_len-1] == '\n') {
            line_buffer[line_len-1] = '\0';
            line_len--;
        }
        
        if (line_len == 0) {
            continue;  // Skip empty lines
        }

        // Process the line
        char *saveptr = NULL;
        p_strtok = strtok_r(line_buffer, "\t", &saveptr);
        int index = 0;
        int is_nonpsk = 0;

        memset(item, 0, sizeof(item));
        strcat(item, "{");

        // Process each column in the line
        while (p_strtok) {
            if ((cols & (1 << index)) || 3 == index) {
                memset(col, 0, sizeof(col));
                if (0 == index) {           // bssid
                    snprintf(col, sizeof(col), "\"bssid\":\"%s\",", p_strtok);
                } else if (1 == index) {    // frequency
                    snprintf(col, sizeof(col), "\"frequency\":%d,", atoi(p_strtok));
                } else if (2 == index) {    // signal level
                    snprintf(col, sizeof(col), "\"%s\":%d,", 
                            format_output ? "signalLevel" : "rssi", 
                            atoi(p_strtok));
                } else if (3 == index) {    // flags
                    if (cols & (1 << index)) {
                        snprintf(col, sizeof(col), "\"flags\":\"%s\",", p_strtok);
                    }
                    if (!strstr(p_strtok, "WPA") && !strstr(p_strtok, "WEP")) {
                        is_nonpsk = 1;
                    }
                } else if (4 == index) {    // ssid
                    char utf8[168] = {0};
                    if (strlen(p_strtok) > 0) {
                        convert_and_cache_ssid_encoding(p_strtok, utf8, is_nonpsk, sizeof(utf8));
                    }
                    snprintf(col, sizeof(col), "\"ssid\":\"%s\",", utf8);
                }
                strcat(item, col);
            }
            p_strtok = strtok_r(NULL, "\t", &saveptr);
            index++;
        }

        // Remove trailing comma and close the object
        if (item[strlen(item) - 1] == ',') {
            item[strlen(item) - 1] = '\0';
        }
        strcat(item, "},");

        // Expand buffer if needed
        if (size <= (strlen(scan_r) + strlen(item)) + 3) {
            size_t new_size = size + output_size;
            pr_info("Expanding buffer from %zu to %zu bytes\n", size, new_size);
            
            char *new_buf = (char *) realloc(scan_r, new_size);
            if (!new_buf) {
                pr_err("Failed to expand buffer\n");
                fclose(temp_file);
                //unlink(temp_path);
                free(scan_r);
                return NULL;
            }
            scan_r = new_buf;
            size = new_size;
        }

        // Add formatted item
        strcat(scan_r, item);
        
        // Add newline if requested
        if (format_output && strlen(scan_r) > 1) {
            strcat(scan_r, "\n");
        }
    }

    // Close and remove temporary file
    fclose(temp_file);
    //unlink(temp_path);

    // Remove trailing ",\n" if has any record in scan_r
    int len = strlen(scan_r);
    if (len < 2) {
        strcpy(scan_r, "[]");
    } else {
        scan_r[len - 2] = ']';
        scan_r[len - 1] = '\0';
    }

    //pr_info("Scan results:\n%s\n", scan_r);

    return scan_r;
}

static char * wpa_wifi_scan_for_softap(void)
{
    DEBUG_STACK_USAGE();
    pr_info("Enter\n");
    return wpa_wifi_scan_internal(0x1F, false);
}

EXPORT char * wpa_wifi_scan_r(void)
{
    DEBUG_STACK_USAGE();
    pr_info("Enter\n");
    return wpa_wifi_scan_internal(0x1F, true);
}

static int add_network()
{
    char ret[8];

    DEBUG_STACK_USAGE();
    pr_info("Enter\n");

    memset(ret, 0, sizeof(ret));
    util_capture_command("wpa_cli -iwlan0 add_network", ret, 8);

    if (0 == strlen(ret) || !isdigit(ret[0]))
    {
        pr_err("ret is '%s'\n", ret);
        return -EINVAL;
    }

    return atoi(ret);
}

static void format_wifiinfo(int flag, char *info)
{
    char temp[1024];
    unsigned int i;
    int j = 0;

    DEBUG_STACK_USAGE();

    if (flag == 0)
    {
        for (i = 0; i < strlen(info); i++)
        {
            sprintf(temp + 2 * i, "%02x", info[i]);
        }
        temp[strlen(info) * 2] = '\0';
        strcpy(info, temp);
        //pr_info("format_wifiinfo ssid: %s\n", info);
    }
    else if (flag == 1)
    {
        for (i = 0; i < strlen(info); i++)
        {
            temp[j++] = '\\';
            temp[j++] = info[i];
        }
        temp[j] = '\0';
        strcpy(info, temp);
        //pr_info("format_wifiinfo password: %s\n", info);
    }
}

static bool set_network(const int id, const char *ssid, const char *psk, const RK_WIFI_CONNECTION_Encryp_e encryp)
{
    char str[8];
    char cmd[512];
    char wifi_ssid[256];
    char wifi_psk[256];

    DEBUG_STACK_USAGE();
    pr_info("ssid: %s, psk: %s\n", ssid, psk);

    strcpy(wifi_ssid, ssid);
    strcpy(wifi_psk, psk);

    // 1. set network ssid
    memset(str, 0, sizeof(str));
    memset(cmd, 0, sizeof(cmd));
    format_wifiinfo(0, wifi_ssid);
    snprintf(cmd, sizeof(cmd), "wpa_cli -iwlan0 set_network %d ssid %s", id, wifi_ssid);
    util_capture_command(cmd, str, 8);
    if (0 == strlen(str) || 0 != strncmp(str, "OK", 2))
        return false;

    // 2. set network psk
    memset(str, 0, sizeof(str));
    memset(cmd, 0, sizeof(cmd));
    if (strlen(psk) == 0 && encryp == NONE)
    {
        snprintf(cmd, sizeof(cmd), "wpa_cli -iwlan0 set_network %d key_mgmt NONE", id);
        util_capture_command(cmd, str, 8);
        if (0 == strlen(str) || 0 != strncmp(str, "OK", 2))
            return false;
    }
    else if (encryp == WEP)
    {
        snprintf(cmd, sizeof(cmd), "wpa_cli -iwlan0 set_network %d key_mgmt NONE", id);
        util_capture_command(cmd, str, 8);
        if (0 == strlen(str) || 0 != strncmp(str, "OK", 2))
            return false;

        memset(str, 0, sizeof(str));
        memset(cmd, 0, sizeof(cmd));
        g_wep_id = id;

        if ((sizeof(psk) == 5) || (sizeof(psk) == 13) || (sizeof(psk) == 16))
            snprintf(cmd, sizeof(cmd), "wpa_cli -iwlan0 set_network %d wep_key0 \\\"%s\\\"", id, psk);
        else
            snprintf(cmd, sizeof(cmd), "wpa_cli -iwlan0 set_network %d wep_key0 %s", id, psk);
        util_capture_command(cmd, str, 8);
        if (0 == strlen(str) || 0 != strncmp(str, "OK", 2))
            return false;

        memset(str, 0, sizeof(str));
        memset(cmd, 0, sizeof(cmd));
        snprintf(cmd, sizeof(cmd), "wpa_cli -iwlan0 set_network %d auth_alg SHARED", id);
        util_capture_command(cmd, str, 8);
        if (0 == strlen(str) || 0 != strncmp(str, "OK", 2))
            return false;
    }
    else if (strlen(psk) || encryp == WPA)
    {
        format_wifiinfo(1, wifi_psk);
        snprintf(cmd, sizeof(cmd), "wpa_cli -iwlan0 set_network %d psk \\\"%s\\\"", id, wifi_psk);
        util_capture_command(cmd, str, 8);
        if (0 == strlen(str) || 0 != strncmp(str, "OK", 2))
            return false;
    }

    return true;
}

static bool set_hide_network(const int id)
{
    char str[8];
    char cmd[128];

    DEBUG_STACK_USAGE();
    pr_info("Enter\n");

    memset(str, 0, sizeof(str));
    memset(cmd, 0, sizeof(cmd));
    snprintf(cmd, sizeof(cmd), "wpa_cli -iwlan0 set_network %d scan_ssid %d", id, 1);
    util_capture_command(cmd, str, 8);

    if (0 == strlen(str) || 0 != strncmp(str, "OK", 2))
        return false;

    return true;
}

static bool clear_bssid_network(const int id)
{
    char str[8];
    char cmd[128];

    DEBUG_STACK_USAGE();

    memset(str, 0, sizeof(str));
    memset(cmd, 0, sizeof(cmd));
    snprintf(cmd, sizeof(cmd), "wpa_cli -iwlan0 bssid %d 00:00:00:00:00:00", id);
    util_capture_command(cmd, str, 8);

    if (0 == strlen(str) || 0 != strncmp(str, "OK", 2))
    {
        pr_err("clear_bssid_network fail!\n");
        return false;
    }

    return true;
}

static bool select_network(const int id)
{
    char str[8];
    char cmd[128];

    DEBUG_STACK_USAGE();
    pr_info("Enter\n");

    clear_bssid_network(id);

    memset(str, 0, sizeof(str));
    memset(cmd, 0, sizeof(cmd));
    snprintf(cmd, sizeof(cmd), "wpa_cli -iwlan0 select_network %d", id);
    util_capture_command(cmd, str, 8);

    if (0 == strlen(str) || 0 != strncmp(str, "OK", 2))
        return false;

    return true;
}

static bool enable_network(const int id)
{
    char str[8];
    char cmd[128];

    DEBUG_STACK_USAGE();
    pr_info("Enter\n");

    memset(str, 0, sizeof(str));
    memset(cmd, 0, sizeof(cmd));
    snprintf(cmd, sizeof(cmd), "wpa_cli -iwlan0 enable_network %d", id);
    util_capture_command(cmd, str, 8);

    if (0 == strlen(str) || 0 != strncmp(str, "OK", 2))
        return false;

    return true;
}

static bool set_priority_network(const int id, int priority)
{
    char str[8];
    char cmd[128];

    DEBUG_STACK_USAGE();

    memset(str, 0, sizeof(str));
    memset(cmd, 0, sizeof(cmd));
    snprintf(cmd, sizeof(cmd), "wpa_cli -iwlan0 set_network %d priority %d", id, priority);
    util_capture_command(cmd, str, 8);

    if (0 == strlen(str) || 0 != strncmp(str, "OK", 2))
        return false;

    return true;
}

static void set_network_highest_priority(const int id)
{
    RK_WIFI_SAVED_INFO_s saved_info[MAX_SAVED_NETWORK];
    int ap_cnt = MAX_SAVED_NETWORK;

    DEBUG_STACK_USAGE();
    pr_info("Enter\n");

    if (!wpa_wifi_list_network(saved_info, &ap_cnt) || ap_cnt <= 0) {
        pr_err("Failed to get network list\n");
        return;
    }

    for (int i = 0; i < ap_cnt; i++) {
        const int current_id = saved_info[i].id;
        const int priority = (current_id == id) ? ap_cnt : 1;
        set_priority_network(current_id, priority);
    }

    return;
}

static void save_connect_info(char *ssid, char *bssid)
{
    DEBUG_STACK_USAGE();

    memset(&g_connect_info, 0, sizeof(RK_WIFI_CONNECT_INFO));
    if (ssid)
    {
        strlcpy(g_connect_info.ssid, ssid, sizeof(g_connect_info.ssid));
    }
    if (bssid)
    {
        strlcpy(g_connect_info.bssid, bssid, sizeof(g_connect_info.bssid));
    }
}

static bool is_non_psk(const char *str)
{
    RK_WIFI_encode_gbk_t *nonpsk = m_nonpsk_head;
    while (nonpsk)
    {
        if (strcmp(nonpsk->utf8, str) == 0)
        {
            return true;
        }
        nonpsk = nonpsk->next;
    }

    return false;
}

/****************************
    ssid: Pointer to a character array containing the SSID of the WiFi network.
    psk: Pointer to a constant character array containing the pre-shared key (password) for the WiFi network.
    encryp: An enumeration indicating the encryption type of the WiFi network.
    Returns: true upon successful initiation of connection, otherwise returns false
****************************/
static bool wpa_wifi_connect1(char *ssid, const char *psk,
                        RK_WIFI_CONNECTION_Encryp_e encryp)
{
    int id;
    char ori[256];
    RK_WIFI_SAVED_INFO_s saved_info[MAX_SAVED_NETWORK];
    int ap_cnt = MAX_SAVED_NETWORK;

    DEBUG_STACK_USAGE();

    if (g_wifi_connect_lock == true)
    {
        pr_err("Connection in progress");
        return false;
    }
    pr_info("ssid: %s, psk: %s\n", ssid, psk);

    // Handle case when ssid is NULL - connect to first saved network
    if (NULL == ssid)
    {
        if (!wpa_wifi_list_network(saved_info, &ap_cnt) || ap_cnt <= 0) {
            pr_err("No saved networks found\n");
            return false;
        }
        ssid = saved_info[0].ssid;
        psk = NULL;
        encryp = AUTO;
        pr_info("Connecting to first saved network: %s\n", ssid);
    }

    // Initialize connection state
    g_wifi_connect_lock = true;
    g_wifi_wrong_key = false;
    g_wifi_is_exist = false;

    //save connection information
    save_connect_info(ssid, NULL);
    //sends a WIFI state indicating connecting
    wifi_state_send(RK_WIFI_State_CONNECTING, NULL);
    //Executes system commands to manage WiFi connections (wpa_cli commands).
    util_exec_command("wpa_cli -iwlan0 disable_network all");
    usleep(60000);

    // Search for existing network with the given SSID
    id = wpa_wifi_search_with_ssid(ssid);

    if (id >= 0)
    {
        pr_info("Found saved network for '%s', connecting to it\n", ssid);
        g_wifi_is_exist = true;
        if (encryp == AUTO)
        {
            encryp = is_non_psk(ssid) ? NONE : WPA;
        }
    }
    else
    {
        pr_info("No saved network found for '%s', adding it\n", ssid);
        if (encryp == AUTO)
        {
            encryp = (psk == NULL) ? NONE : WPA;
        }

        id = add_network();
        if (id < 0)
        {
            pr_err("add_network id: %d failed!\n", id);
            goto fail;
        }
    }

    g_connection_id = id;
    g_is_wep = (encryp == WEP);

    //Sets network configurations based on whether a password (psk) is provided or not (is_non_psk).
    if (!g_wifi_is_exist)
    {
        if (encryp == NONE)
        {
            get_encode_gbk_ori(m_nonpsk_head, ssid, ori, sizeof(ori));
            if (!set_network(id, ori, "", NONE))
            {
                pr_info("set_network failed. ssid:\"%s\"\n", ssid);
                goto fail;
            }
        }
        else
        {
            get_encode_gbk_ori(m_gbk_head, ssid, ori, sizeof(ori));
            if (!set_network(id, ori, psk, encryp))
            {
                pr_info("set_network failed. ssid:\"%s\", psk:\"%s\"\n", ssid, psk);
                goto fail;
            }
        }
        //pr_info("ori:\"%s\" ori_len:%u\n", ori, strlen(ori));
    }

    set_network_highest_priority(id);

    if (!set_hide_network(id)) {
        pr_err("set_hide_network failed for id: %d", id);
        goto fail;
    }

    if (!select_network(id)) {
        pr_err("select_network failed for id: %d", id);
        goto fail;
    }

    if (!enable_network(id)) {
        pr_err("enable_network failed for id: %d", id);
        goto fail;
    }

	if (g_wifi_connect_lock == true)
    {
        pr_info("Connection sucess! unlock connection lock");
		g_wifi_connect_lock = false;
    }
    
    return true;

fail:
    wifi_state_send(RK_WIFI_State_CONNECTFAILED, NULL);
    wpa_handle_connect_fail(id);
    g_wifi_connect_lock = false;
    g_wifi_wrong_key = false;
    g_is_wep = false;
    return false;
}

static bool wpa_wifi_connect(char *ssid, const char *psk)
{
    DEBUG_STACK_USAGE();
    return wpa_wifi_connect1(ssid, psk, AUTO);
}

static bool wpa_wifi_connect_with_ssid(char *ssid)
{
    DEBUG_STACK_USAGE();
    if (!ssid) {
        pr_err("ssid is NULL\n");
        return false;
    }
    return wpa_wifi_connect1(ssid, NULL, AUTO);
}

static bool wpa_wifi_connect_wep(char *ssid, const char *psk)
{
    DEBUG_STACK_USAGE();
    if (!ssid || !psk) {
        pr_err("ssid or psk is NULL\n");
        return false;
    }
    return wpa_wifi_connect1(ssid, psk, WEP);
}

static bool wpa_wifi_cancel(void)
{
    int timeout = 8;

    DEBUG_STACK_USAGE();
    pr_info("Enter\n");

    if (g_wifi_connect_lock == false)
    {
        pr_info("wifi dont connecting!\n");
        return false;
    }

    g_wifi_cancel = true;

    while (timeout--)
    {
        if (g_wifi_cancel == false)
            break;
        sleep(1);
    }

    if (g_wifi_cancel == false)
        return true;
    else
        return false;
}

EXPORT bool wpa_wifi_forget_with_ssid(char *ssid)
{
    int id;
    char cmd[64];
    char str[8];

    DEBUG_STACK_USAGE();
    pr_info("ssid %s\n", ssid);

    if (!ssid)
    {
        pr_err("ssid is null\n");
        return false;
    }

    id = wpa_wifi_search_with_ssid(ssid);
    if (id == -EINVAL)
    {
        pr_err("RK_wifi_forget_with_ssid [%s] not found!\n", ssid);
        return false;
    }

    // Added by yanle, if this id is being connected, cancel it
    if (g_wifi_connect_lock && id == g_connection_id)
    {
        pr_info("ssid %s is connecting, cancel it first.\n", ssid);
        wpa_wifi_cancel();
    }

    snprintf(cmd, sizeof(cmd), "wpa_cli -iwlan0 remove_network %d", id);
    util_capture_command(cmd, str, 8);
    if (0 == strlen(str) || 0 != strncmp(str, "OK", 2))
        return false;

    util_exec_command("wpa_cli -iwlan0 save");

    return true;
}

EXPORT bool wpa_wifi_disconnect(void)
{
    DEBUG_STACK_USAGE();
    pr_info("Enter\n");

    util_exec_command("wpa_cli -iwlan0 disconnect");
    return true;
}

EXPORT bool wpa_wifi_reset(void)
{
    DEBUG_STACK_USAGE();
    pr_info("Enter\n");

    if (util_find_pid_by_name("wpa_supplicant") > 0)
    {
        util_exec_command("wpa_cli -i wlan0 flush");
        util_exec_command("wpa_cli -i wlan0 save");
    }
    else
    {
        util_exec_command("rm /data/cfg/wpa_supplicant.conf");
        util_exec_command("cp /etc/wpa_supplicant.conf /data/cfg/wpa_supplicant.conf");
    }
    util_exec_command("sync");

    return true;
}

EXPORT bool wpa_wifi_recovery(void)
{
    RK_WIFI_INFO_Connection_s info;
    DEBUG_STACK_USAGE();

    // If wpa status is not DISCONNECTED, no need recovery, just wait it
    if (wpa_wifi_get_status(&info) &&
        strncmp(info.wpa_state, "DISCONNECTED", strlen("DISCONNECTED"))) {
        pr_info("wifi is %s, ignore recovery request.\n", info.wpa_state);
        return true;
    }

    pr_info("wifi is disconnected, recovery it.\n");
    util_exec_command("wpa_cli -i wlan0 flush");
    util_exec_command("wpa_cli -i wlan0 reconfigure");
    util_exec_command("wpa_cli -i wlan0 reconnect");

    return true;
}

EXPORT bool wpa_wifi_get_mac(char *wifi_mac)
{
    int sock_mac;
    struct ifreq ifr_mac;
    char mac_addr[18] = {0};

    DEBUG_STACK_USAGE();
    pr_info("Enter\n");

    if (!wifi_mac)
    {
        pr_err("wifi_mac is null\n");
        return false;
    }

    sock_mac = socket(AF_INET, SOCK_STREAM, 0);

    if (sock_mac == -1)
    {
        pr_info("create mac socket failed.\n");
        return false;
    }

    memset(&ifr_mac, 0, sizeof(ifr_mac));
    strlcpy(ifr_mac.ifr_name, "wlan0", sizeof(ifr_mac.ifr_name));

    if ((ioctl(sock_mac, SIOCGIFHWADDR, &ifr_mac)) < 0)
    {
        pr_info("Mac socket ioctl failed.\n");
        return false;
    }

    sprintf(mac_addr, "%02X:%02X:%02X:%02X:%02X:%02X",
            (unsigned char)ifr_mac.ifr_hwaddr.sa_data[0],
            (unsigned char)ifr_mac.ifr_hwaddr.sa_data[1],
            (unsigned char)ifr_mac.ifr_hwaddr.sa_data[2],
            (unsigned char)ifr_mac.ifr_hwaddr.sa_data[3],
            (unsigned char)ifr_mac.ifr_hwaddr.sa_data[4],
            (unsigned char)ifr_mac.ifr_hwaddr.sa_data[5]);

    close(sock_mac);
    strlcpy(wifi_mac, mac_addr, 17 + 1);
    pr_info("the wifi mac : %s\r\n", wifi_mac);

    return true;
}

struct Rk_wifi_driver_ops wpa_driver_ops =
{
    .name = "wpa_supplicant standard",
    .wifi_enable = wpa_wifi_enable,
    .wifi_disconnect = wpa_wifi_disconnect,
    .wifi_connect = wpa_wifi_connect,
    .wifi_get_status = wpa_wifi_get_status,
    .wifi_is_connected = wpa_wifi_is_connected_with_ip,
    .wifi_cancel = wpa_wifi_cancel,
    .wifi_connect_wep = wpa_wifi_connect_wep,
    .wifi_connect_with_ssid = wpa_wifi_connect_with_ssid,
    .wifi_forget_with_ssid = wpa_wifi_forget_with_ssid,
    .wifi_recovery = wpa_wifi_recovery,
    .wifi_list_network = wpa_wifi_list_network,
    .wifi_get_mac = wpa_wifi_get_mac,
    .wifi_reset = wpa_wifi_reset,
    .wifi_scan = wpa_wifi_scan,
    .wifi_scan_r = wpa_wifi_scan_r,
    .wifi_scan_for_softap = wpa_wifi_scan_for_softap,
    .wifi_start_keepalive = wpa_wifi_start_keepalive,
};
