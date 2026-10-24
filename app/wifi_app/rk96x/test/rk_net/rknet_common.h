/*********************************************************************************
* Copyright (C),2020-2030,  Rockchip Beijing Bansband Center
* All Rights Reserved
* V1.00
* FileName : rknet_common.h
* Author :  wangqunyang
* Description:   rk tcpip test header file
* History :
*   <author>      <time>        <version>         <desc>
*   wangqunyang   2025/08/20      1.0               ORG
*
*
*********************************************************************************/
#ifndef RKNET_COMMON_H
#define RKNET_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <stdbool.h>
#include <signal.h>
#include <ctype.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <net/if.h>

#define MAX_BUFFER_SIZE 4096
#define TCP_DEFAULT_PORT 8888
#define UDP_DEFAULT_PORT 9999
#define MAX_PACKET_SIZE 1024
#define MAX_REPEAT_TIMES 10

// Functions declare
void print_local_ip();
bool validate_ip(const char *ip);
bool validate_port(int port);
long long get_current_time_us();
void format_time(long long timestamp_us, char *buffer, size_t buffer_size);
void hex_dump(const char *tag, const void *data, size_t size);
void setup_signal_handlers(void (*handler)(int));

// TCP function
void run_tcp_server();
void run_tcp_client();

// UDP function
void run_udp_server();
void run_udp_client();

extern volatile sig_atomic_t stop_flag;
void signal_handler(int sig);

#endif
