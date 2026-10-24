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

static int parse_hex_str(const char *hex_str, uint8_t *out_buf, int out_buf_size)
{
    int in_len = (hex_str != NULL) ? strlen((char *)hex_str) : 0;
    int i, j = 0;

    for (i = 0; i < in_len; i++) {
        if (j >= out_buf_size){
            pr_err("buffer overflow detected\n");
            return -1;
        }

        if (hex_str[i] == ',') {
            continue;
        }
        else if (hex_str[i] == '0' && (hex_str[i + 1] == 'x' || hex_str[i + 1] == 'X')) {
            i += 2;
            if (i >= in_len || !isxdigit(hex_str[i])) {
                pr_err("invalid hex format at position %d\n", i);
                return -1;
            }
            unsigned int temp;
            if (sscanf(hex_str + i, "%02x", &temp) != 1 || temp > 0xFF) {
                pr_err("hex parsing failed at position %d (value=0x%02x)\n", i, temp);
                return -1;
            }
            out_buf[j++] = (uint8_t)temp;
            i++;    // skip second hex digit
        } else {
            pr_err("invalid char at [->%s]\n", &hex_str[i]);
            return -1;
        }
    }
    return j;
}

static int parse_str(const char *in_str, uint8_t *out_buf, int out_buf_size)
{
    int out_len = -1;

    if (in_str == NULL || strlen(in_str) == 0) {
        pr_err("input string is empty.\n");
        return -1;
    }

    memset(out_buf, 0, out_buf_size);
    if (in_str[0] == '0' && (in_str[1] == 'x' || in_str[1] == 'X')) { // hex string
        out_len = parse_hex_str(in_str, out_buf, out_buf_size);
        if (out_len <= 0) {
            pr_err("failed to parse in_str.\n");
            return -1;
        }
    }
    else {  // normal string
        strncpy((char *)out_buf, in_str, out_buf_size - 1);
        out_len = strlen((char *)out_buf);
    }

    return out_len;
} 

// create TCP socket to server, return sockid sucessfully or -1
static int create_and_connect_socket(const char *domain, const char *port, int max_retries)
{
    int sockfd;
    struct sockaddr_in serv_addr;
    int retrycnt = 0;

    DEBUG_STACK_USAGE();

    pr_info("domain: %s, port: %s, max_retries: %d\n", domain, port, max_retries);

    while (retrycnt++ <= max_retries) {  // try 1 more time
        pr_info("connecting to server... %d/%d \n", retrycnt, max_retries + 1);

        // create socket
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd == -1) {
            pr_info("socket creation failed!\n");
            return -1;
        }

        // Set socket timeout
        struct timeval timeout;
        timeout.tv_sec = 15;
        timeout.tv_usec = 0;
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
            pr_info("setsockopt failed for SO_RCVTIMEO\n");
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
            pr_info("setsockopt failed for SO_SNDTIMEO\n");
        }

        // connect to server
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = inet_addr(domain);
        serv_addr.sin_port = htons(atoi(port));
        if (0 == connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) {
            pr_info("connected successfully!\n");
            return sockfd;
        }

        // connect failed
        pr_info("failed to connect to server - %s\n", strerror(errno));
        close(sockfd); // close socket

        // Restart wifi because the wifi connection might be fake after key renew failed
        pr_info("restarting wifi...\n");
        if (!util_check_and_recovery_wifi(true)) {
            pr_err("timeout when wait wifi restarting.\n");
            return -1;
        }
    }

    pr_err("failed to connect to server after %d retries.\n", max_retries);
    return -1;
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

int main(int argc, char *argv[])
{
    RK_WIFI_LOW_POWER_KEEPALIVE_s kp;
    int sockfd = -1;
    char *sockfd_str = NULL;
    char *login_str = NULL;
    char *heartbeat_str = NULL;
    char *wake_str = NULL;

    DEBUG_STACK_USAGE();

    // Print arguments
    pr_info("argc = %d, argv = [\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("'%s' ", argv[i]);
    }
    printf("\n]\n");

    // Proccess 'stop' command
    if (argc > 1 && strcmp(argv[1], "stop") == 0) {
        pr_info("stop keepalive\n");
        util_set_kp_status(false);
        return 0;
    }

    // Init 'kp' struct
    memset(&kp, 0, sizeof(RK_WIFI_LOW_POWER_KEEPALIVE_s));

    // Try to get sock_fd and wake_str from enviaronment variables
    sockfd_str = getenv("KEEPALIVE_SOCK_FD");
    if (sockfd_str != NULL) {  // use the sockfd from user
        pr_info("get arguments from env-var, sock_fd = %s.\n", sockfd_str);
        sockfd = atoi(sockfd_str);
        if (sockfd <= 0) {
            pr_err("invalid sockfd value: %s\n", sockfd_str);
            return -1;
        }

        // Get domain and port
        struct sockaddr_in peer_addr;
        socklen_t peer_addr_len = sizeof(peer_addr);
        if (getpeername(sockfd, (struct sockaddr *)&peer_addr, &peer_addr_len) < 0) {
            pr_err("failed to get peer name: %s\n", strerror(errno));
            return -1;
        }
        // Set kp.domain and kp.port
        memset(kp.domain, 0, sizeof(kp.domain));
        memset(kp.port, 0, sizeof(kp.port));
        inet_ntop(AF_INET, &peer_addr.sin_addr, kp.domain, sizeof(kp.domain));
        snprintf(kp.port, sizeof(kp.port), "%d", ntohs(peer_addr.sin_port));
        pr_info("domain: %s, port: %s\n", kp.domain, kp.port);

        // Get period from environment variable
        char *period_str = getenv("KEEPALIVE_PERIOD");
        if (period_str == NULL || strlen(period_str) == 0) {
            pr_err("KEEPALIVE_PERIOD is not set.\n");
            return -1;
        }
        // Set kp.period
        strncpy(kp.period, period_str, sizeof(kp.period) - 1);
        pr_info("period: %s\n", kp.period);

        // get login_str, heartbeat_str and wake_str from environment variable
        login_str = getenv("KEEPALIVE_LOGIN_STR");
        heartbeat_str = getenv("KEEPALIVE_HEARTBEAT_STR");
        wake_str = getenv("KEEPALIVE_WAKE_STR");
    }
    else {  // create socket int by ourself
        if (argc < 7) {
            pr_info("%d arguments are insufficient, read from file...\n", argc - 1);
            if (!load_kpalive_conf(&kp)) {
                pr_err("failed to read configuration from file.\n");
                return -1;
            }
        }
        else {
            pr_info("get arguments from command line...\n");
            if (!is_valid_domain(argv[1]) ||
                !is_valid_port(argv[2]) ||
                !is_valid_period(argv[3])) {
                pr_err("wrong arguments: domain '%s', port '%s', period '%s'\n",
                       argv[1], argv[2], argv[3]);
                return -1;
            }

            strncpy(kp.domain, argv[1], sizeof(kp.domain) - 1);
            strncpy(kp.port, argv[2], sizeof(kp.port) - 1);
            strncpy(kp.period, argv[3], sizeof(kp.period) - 1);
            login_str = argv[4];
            heartbeat_str = argv[5];
            wake_str = argv[6];
        }
    }

    if (NULL != wake_str) {
        kp.login_bytes_len = parse_str(login_str, kp.login_bytes, sizeof(kp.login_bytes));
        if (kp.login_bytes_len <= 0) {
            pr_err("failed to parse login_str.\n");
            return -1;
        }
        kp.heartbeat_bytes_len = parse_str(heartbeat_str, kp.heartbeat_bytes, sizeof(kp.heartbeat_bytes));
        if (kp.heartbeat_bytes_len <= 0) {
            pr_err("failed to parse heartbeat_str.\n");
            return -1;
        }
        kp.wake_bytes_len = parse_str(wake_str, kp.wake_bytes, sizeof(kp.wake_bytes));
        if (kp.wake_bytes_len <= 0) {
            pr_err("failed to parse wake_str.\n");
            return -1;
        }
    }

    // Create socket if sockfd is not set
    if (sockfd <= 0) {
        sockfd = create_and_connect_socket(kp.domain, kp.port, 1);
        if (sockfd <= 0) {
            pr_err("failed to create and connect socket.\n");
            //return -1;
        }
    }

    // start keepalive
    util_set_kp_status(true);
    if (!wpa_wifi_start_keepalive(&kp, sockfd))
    {
        pr_info("wpa_wifi_start_keepalive fail!\n");
        util_set_kp_status(false);
        return -1;
    }
    else
    {
        pr_info("wpa_wifi_start_keepalive sucess!\n");
    }

    return 0;
}
