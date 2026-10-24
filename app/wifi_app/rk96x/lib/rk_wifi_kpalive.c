// Copyright 2021 Rockchip Electronics Co., Ltd. All rights reserved.
// *NOTE*: the APIs in this file may be called in a separate process than that of
// rf_wifi.c, so we need to ensure never using global variables or non-reentrant
// functions in rk_wifi.c.

#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include "slog.h"
#include "utility.h"
#include "rk_wifi_raw.h"
#include "rk_wifi_kpalive.h"

/* Read system parameter value from sysfs interface */
static bool get_sys_parameter(const char *param_path, char *buffer, size_t buffer_size)
{
    int fd;
    ssize_t bytes_read;

    // Open file for reading
    fd = open(param_path, O_RDONLY);
    if (fd < 0) {
        pr_info("Failed to open %s for reading\n", param_path);
        return false;
    }

    // Read parameter value
    bytes_read = read(fd, buffer, buffer_size - 1);
    close(fd);

    if (bytes_read <= 0) {
        pr_info("Read %s failed\n", param_path);
        return false;
    }

    // Null terminate and remove trailing newline if present
    buffer[bytes_read] = '\0';
    if (bytes_read > 0 && buffer[bytes_read - 1] == '\n') {
        buffer[bytes_read - 1] = '\0';
    }

    return true;
}

/* Enhanced parameter setter for rkfs with validation */
static bool set_sys_parameter(const char *param_path, const char *value_fmt, ...)
{
    char expected_value[640] = {0};
    char actual_value[640] = {0};
    va_list args;
    int fd;
    int validation_retry = 3;

    // Generate expected value
    va_start(args, value_fmt);
    int value_len = vsnprintf(expected_value, sizeof(expected_value), value_fmt, args);
    va_end(args);

    // Check value buffer overflow
    if (value_len >= (int)sizeof(expected_value)) {
        pr_err("SET %s ABORT: Value requires %d bytes (buffer size=%zu)\n",
               param_path, value_len + 1, sizeof(expected_value));
        return false;
    }

    // Write to rkfs file
    fd = open(param_path, O_WRONLY);
    if (fd < 0) {
        pr_err("Failed to open %s for writing\n", param_path);
        return false;
    }

    ssize_t written = write(fd, expected_value, strlen(expected_value));
    close(fd);

    if (written != (ssize_t)strlen(expected_value)) {
        pr_err("SET value %s to path %s failed\n", expected_value, param_path);
        return false;
    }

    // Validation with retry mechanism
    do {
        usleep(1000);  // Short delay for system synchronization

        // Read actual parameter value
        if (!get_sys_parameter(param_path, actual_value, sizeof(actual_value)) ||
            strlen(actual_value) == 0) {
            continue;
        }
        else {
            // Compare actual and expected values
            // TODO: the read value may not equal to set value even if it is right,
            // so we can't strictly compare them here.
            pr_info("%s = %s\n", param_path, actual_value);
            return true;  // Exit on successful validation
        }
    } while (--validation_retry > 0);

    // Final validation failure handling
    pr_info("PARAM SET FAIL: %s (Expected '%s'(%d) != Actual '%s'(%d))\n",
            param_path,
            expected_value, strlen(expected_value),
            actual_value, strlen(actual_value));

    return false;
}

static int serialize_byte_array(char *buffer,
                                size_t buffer_size,
                                const uint8_t *data,
                                int len,
                                const char *element_fmt)
{
    int pos = 0;
    for (int i = 0; i < len; i++) {
        if (i > 0) {
            int n = snprintf(buffer + pos, buffer_size - pos, ",");
            if (n < 0 || (pos + n) >= (int)buffer_size) return -1;
            pos += n;
        }
        int n = snprintf(buffer + pos, buffer_size - pos, element_fmt, data[i]);
        if (n < 0 || (pos + n) >= (int)buffer_size) return -1;
        pos += n;
    }
    buffer[pos] = '\0';
    return pos;
}

static bool driver_set_keepalive(RK_WIFI_LOW_POWER_KEEPALIVE_s *kp,
                                 HI_TCP_KEEPALIVE_REQ_S *requst)
{
    uint32_t s_addr =  inet_addr((const char *)kp->domain); //server address

    pr_info("domain: %s, port: %s, period: %s\n", kp->domain, kp->port, kp->period);

    // Set keepalive_enable
    if (!set_sys_parameter("/sys/kernel/rkfs/keepalive_enable", "%s", "1")) {
        return false;
    }

    // Set keepalive port
    if (!set_sys_parameter("/sys/kernel/rkfs/keepalive_port", "%s", kp->port)) {
        return false;
    }

    // Set keepalive ip
    if (!set_sys_parameter("/sys/kernel/rkfs/keepalive_ip", "0x%x", s_addr)) {
        return false;
    }

    // Set keepalive parameters
    if (!set_sys_parameter("/sys/kernel/rkfs/keepalive_parameter",
                           "%d,%d,%d,%d,%d,%d,%d",
                           requst->tcp_keep_alive,
                           requst->tcp_keep_interval,
                           requst->tcp_keep_count,
                           requst->is_enable_tcpkeepalive,
                           requst->login_len,
                           requst->wakeup_len,
                           requst->heartbeat_len)) {
        return false;
    }

    // Set heartbeat string
    char heartbeat_str[500];
    pr_info("requst->heartbeat_data=%s,requst->heartbeat_len=%d\n", requst->heartbeat_data, requst->heartbeat_len);
    int heartbeat_str_len = serialize_byte_array(heartbeat_str, sizeof(heartbeat_str),
                         requst->heartbeat_data, requst->heartbeat_len, "0x%x");
    if (heartbeat_str_len < 0) {
        pr_info("Failed to format login string, login_str_len=%d\n", heartbeat_str_len);
        return false;
    } else {
        if (heartbeat_str_len >= (int)sizeof(heartbeat_str)) {
            pr_info("heartbeat string buffer overflow!!!\n");
            return false;
        }
        if (!set_sys_parameter("/sys/kernel/rkfs/keepalive_heartbeatstring",
                               "%s", heartbeat_str)) {
            return false;
        }
    }

    // Set wakeup string
    char wakeup_str[640];
    int wakeup_str_len = serialize_byte_array(wakeup_str, sizeof(wakeup_str),
                         requst->wakeup_data, requst->wakeup_len, "0x%x");
    if (wakeup_str_len < 0) {
        pr_info("Failed to format wakeup string\n");
        return false;
    } else {
        if (wakeup_str_len >= (int)sizeof(wakeup_str)) {
            pr_info("wakeup string buffer overflow!!!\n");
            return false;
        }
        if (!set_sys_parameter("/sys/kernel/rkfs/keepalive_wakeupstring",
                               "%s", wakeup_str)) {
            return false;
        }
    }

    // Set login string
    char login_str[500];
    pr_info("requst->login_data=%s,requst->login_len=%d\n", requst->login_data, requst->login_len);
    int login_str_len = serialize_byte_array(login_str, sizeof(login_str),
                         requst->login_data, requst->login_len, "0x%x");
    if (login_str_len < 0) {
        pr_info("Failed to format login string, login_str_len=%d\n", login_str_len);
        return false;
    } else {
        if (login_str_len >= (int)sizeof(login_str)) {
            pr_info("login string buffer overflow!!!\n");
            return false;
        }
        if (!set_sys_parameter("/sys/kernel/rkfs/keepalive_loginstring",
                               "%s", login_str)) {
            return false;
        }
    }

    return true;
}

static void wpa_fwcr_save(void)
{
    // Mount debug node
    util_exec_command("mount -t debugfs none /sys/kernel/debug");
    usleep(50);
    // Triger saving
    util_exec_command("cat /sys/kernel/rkfs/fwcr_save");
    usleep(50);
    // Sync file system
    util_exec_command("sync");
    usleep(50);
    return;
}

static HI_TCP_KEEPALIVE_REQ_S gererate_keepalive_request(RK_WIFI_LOW_POWER_KEEPALIVE_s *kp)
{
    // Create struct variables
    HI_TCP_KEEPALIVE_REQ_S tcp_keepalive_req;

    memset(&tcp_keepalive_req, 0, sizeof(HI_TCP_KEEPALIVE_REQ_S));
    // Initializes the struct field
    tcp_keepalive_req.tcp_keep_alive = (uint32_t)atoi((const char *)kp->period);
    tcp_keepalive_req.tcp_keep_interval = 3;
    tcp_keepalive_req.tcp_keep_count = 10;
    tcp_keepalive_req.is_enable_tcpkeepalive = 1;
    tcp_keepalive_req.heartbeat_len = kp->heartbeat_bytes_len;
    tcp_keepalive_req.login_len = kp->login_bytes_len;
    tcp_keepalive_req.wakeup_len = kp->wake_bytes_len;

    memcpy(tcp_keepalive_req.heartbeat_data, kp->heartbeat_bytes, kp->heartbeat_bytes_len);
    memcpy(tcp_keepalive_req.login_data, kp->login_bytes, kp->login_bytes_len);
    memcpy(tcp_keepalive_req.wakeup_data, kp->wake_bytes, kp->wake_bytes_len);

    return tcp_keepalive_req;
}

static void set_nagle(int sockfd, int enable)
{
    const int val = enable ? 0 : 1; // 0: enable Nagle, 1: disable Nagle

    if (0 == enable)
        pr_info("Enable Nagle algorithm\n");
    else
        pr_info("Disable Nagle algorithm\n");

    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
}

static int is_valid_domain(const char *ip)
{
    struct in_addr addr;
    return inet_pton(AF_INET, ip, &addr) == 1;
}

static int is_valid_port(const char *port_str)
{
    int port; char extra;
    return isdigit(port_str[0]) && sscanf(port_str, "%d%c", &port, &extra) == 1 && port > 0 && port < 65535;
}

static int is_valid_period(const char *period_str)
{
    int period; char extra;
    return isdigit(period_str[0]) && sscanf(period_str, "%d%c", &period, &extra) == 1 && period > 0 && period < 7200;
}

static bool check_paremeters(RK_WIFI_LOW_POWER_KEEPALIVE_s *kp)
{
    if (NULL == kp) {
        pr_err("kp can't be NULL.\n");
        return false;
    }

    if (!is_valid_domain(kp->domain)) {
        pr_err("Invalid domain string - %s.\n", kp->domain);
        return false;
    }

    if (!is_valid_port(kp->port)) {
        pr_err("Invalid port string - %s.\n", kp->port);
        return false;
    }

    if (!is_valid_period(kp->period)) {
        pr_err("Invalid period string - %s.\n", kp->period);
        return false;
    }

    return true;
}

static bool wpa_wait_wifi_keepalive_confirm(void)
{
    char buffer[4];
    struct timeval start, current;
    int confirm1_timeout = 10;
    char *endptr;
    unsigned int value;

    pr_info("enter\n");

    // Gets the current time as the starting point for timing
    gettimeofday(&start, NULL);

    while (1)
    {
        if (get_sys_parameter("/sys/kernel/rkfs/keepalive_confirm", buffer, sizeof(buffer))) {
            pr_info("keepalive confirm: %s!\n", buffer);
            value = strtoul(buffer, &endptr, 16);
            if (endptr == buffer || *endptr != '\0') {
                pr_info("Invalid hex format: '%s'\n", buffer);
                return false;
            }
            pr_info("confirm value: 0x%X\n", value);
            if (0x1 == value) {
                return true;
            }
        }

        sleep(1);

        // Check whether more than 10 seconds, two ACK time
        gettimeofday(&current, NULL);
        if ((current.tv_sec - start.tv_sec) >= confirm1_timeout) {
            pr_info("Timeout: Did not receive keepalive confirmation within %d seconds.\n", confirm1_timeout);
            return false;
        }
    }

    return false;
}

bool save_kpalive_conf(RK_WIFI_LOW_POWER_KEEPALIVE_s *kp)
{
    RK_WIFI_INFO_Connection_s info;

    DEBUG_STACK_USAGE();
    pr_info("enter\n");

    FILE *fp = fopen(KEEPALIVE_CONF_PATH, "w");
    if (fp == NULL) {
        pr_info("Failed to open file for writing\n");
        return false;
    }

    // Save domain, port, period to config file
    fprintf(fp, "domain=%s\nport=%s\nperiod=%s\n",
                kp->domain, kp->port, kp->period);

    // Save login_bytes, heartbeat_bytes, and wake_bytes to config file
    char hex_str[512];
    util_bytes_to_hex_string(kp->login_bytes, kp->login_bytes_len, hex_str, sizeof(hex_str));
    fprintf(fp, "login_bytes=%s\n", hex_str);
    util_bytes_to_hex_string(kp->heartbeat_bytes, kp->heartbeat_bytes_len, hex_str, sizeof(hex_str));
    fprintf(fp, "heartbeat_bytes=%s\n", hex_str);
    util_bytes_to_hex_string(kp->wake_bytes, kp->wake_bytes_len, hex_str, sizeof(hex_str));
    fprintf(fp, "wake_bytes=%s\n", hex_str);

    wpa_wifi_get_status(&info);

    // Check and save network parameters from info structure
    if (strlen(info.ip_address) == 0 || strlen(info.gw) == 0) {
        pr_err("Empty network parameters: ip='%s' gw='%s'\n",
               info.ip_address, info.gw);
        fclose(fp);
        return false;
    }
    pr_info("ip=%s, gw=%s\n", info.ip_address, info.gw);
    fprintf(fp, "ip=%s\ngw=%s\n", info.ip_address, info.gw);
    fclose(fp);

    return true;
}

bool load_kpalive_conf(RK_WIFI_LOW_POWER_KEEPALIVE_s *kp)
{
    char line[300];
    struct {
        const char *key; char *dest; size_t dest_buf_len; size_t *p_dest_len;
    } config_map[] = {
        {"domain", kp->domain, sizeof(kp->domain), NULL},
        {"port", kp->port, sizeof(kp->port), NULL},
        {"period", kp->period, sizeof(kp->period), NULL},
        {"login_bytes", (char *)kp->login_bytes, sizeof(kp->login_bytes), (size_t *)&kp->login_bytes_len},
        {"heartbeat_bytes", (char *)kp->heartbeat_bytes, sizeof(kp->heartbeat_bytes), (size_t *)&kp->heartbeat_bytes_len},
        {"wake_bytes", (char *)kp->wake_bytes, sizeof(kp->wake_bytes), (size_t *)&kp->wake_bytes_len},
    };
    const int config_map_size = sizeof(config_map) / sizeof(config_map[0]);

    DEBUG_STACK_USAGE();

    FILE *fp = fopen(KEEPALIVE_CONF_PATH, "r");
    if (fp == NULL)
    {
        pr_info("Failed to open config file: %s\n", KEEPALIVE_CONF_PATH);
        return false;
    }

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        // Remove newline character
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
        {
            line[len - 1] = '\0';
        }

        char *key = strtok(line, "=");
        char *value = strtok(NULL, "=");
        if (key != NULL && value != NULL)
        {
            for (int i = 0; i < config_map_size; i++)
            {
                if (strcmp(key, config_map[i].key) == 0)
                {
                    if (config_map[i].p_dest_len == NULL) {
                        strncpy(config_map[i].dest, value, config_map[i].dest_buf_len);
                    }
                    else {
                        *(config_map[i].p_dest_len) = util_hex_string_to_bytes(value, (uint8_t *)config_map[i].dest, config_map[i].dest_buf_len);
                    }
                    pr_info("%s='%s'\n", config_map[i].key, config_map[i].dest);
                    break;
                }
            }
        }
    }

    fclose(fp);
    return true;
}

bool wpa_wifi_start_keepalive(RK_WIFI_LOW_POWER_KEEPALIVE_s *kp, int sockfd)
{
    int ret = -1;
    int cnt;
    char dns[32];
    int statu_flag = 0;
    DEBUG_STACK_USAGE();
    pr_info("Enter\n");

    if (!check_paremeters(kp)) {
       return false;
    }

    pr_info("domain: '%s'\n", kp->domain);
    pr_info("port: '%s'\n", kp->port);
    pr_info("period: '%s'\n", kp->period);
    pr_info("login_bytes: '%s'\n", kp->login_bytes);
    pr_info("heartbeat_bytes: '%s'\n", kp->heartbeat_bytes);
    pr_info("wake_bytes: '%s', HEX(%d bytes): \n", kp->wake_bytes, kp->wake_bytes_len);
    for (int i = 0; i < kp->wake_bytes_len; i++) {
        printf("%02X ", kp->wake_bytes[i]);
    }
    printf("\n");

    // Save keepalive config
    if (!save_kpalive_conf(kp)) {
        pr_err("save_kpalive_conf failed\n");
        //return false;
        statu_flag = 1;
    }

    // Save dns
    util_exec_command("cp /etc/resolv.conf /data/resolv.conf");
    cnt = util_network_dns_get(dns, NULL);
    pr_info("dns=%s, cnt=%d\n", dns, cnt);

    // Init keepalive package payload
    //initialize_heartbeat_payload();
    // Save wifi quick start infomation
    if(statu_flag != 1)
        wpa_fwcr_save();

    // Generate request to wifi device
    HI_TCP_KEEPALIVE_REQ_S requst = gererate_keepalive_request(kp);

    // Send login package
    ret = write(sockfd, kp->login_bytes, kp->login_bytes_len);
    if (ret < 0) {
        pr_info("write login_bytes error: %s\n", strerror(errno));
        //close(sockfd);
        //return false;
        requst.is_enable_tcpkeepalive = 0;
        pr_info("is_enable_tcpkeepalive: %d\n", requst.is_enable_tcpkeepalive);
    }
    pr_info("sent login_bytes.\n");
    usleep(500*1000);

    // Notify ko to start keepalive
    if (!driver_set_keepalive(kp, &requst)) {
        pr_err("driver_set_keepalive failed\n");
        return false;
    }
    pr_info("driver_set_keepalive success.\n");
    usleep(500*1000);

    // Send keepalive package
    ret = write(sockfd, kp->heartbeat_bytes, kp->heartbeat_bytes_len);
    if (ret < 0) {
        pr_info("write heartbeat_bytes error: %s\n", strerror(errno));
        //close(sockfd);
        //return false;
    }
    pr_info("sent haerteat_bytes.\n");

    // Wait for wifi device to confirm
    if (wpa_wait_wifi_keepalive_confirm()) {
        // Enable Nagle algorithm
        set_nagle(sockfd, 0);
        return true;
    }
	
	return false;
}
