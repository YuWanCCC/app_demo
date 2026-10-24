/*********************************************************************************
* Copyright (C),2020-2030,  Rockchip Beijing Bansband Center
* All Rights Reserved
* V1.00
* FileName : rknet_common.c
* Author :  wangqunyang
* Description:   test common source file
* History :
*   <author>      <time>        <version>         <desc>
*   wangqunyang   2025/08/20      1.0               ORG
*
*
*********************************************************************************/
#include "rknet_common.h"

volatile sig_atomic_t stop_flag = 0;

void signal_handler(int sig) {
    stop_flag = 1;
    const char msg[] = "\nReceived signal, exiting ...\n";
    write(STDERR_FILENO, msg, sizeof(msg) - 1);
}

void setup_signal_handlers(void (*handler)(int)) {
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction(SIGINT)");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction(SIGTERM)");
        exit(EXIT_FAILURE);
    }
}

void print_local_ip() {
    struct ifaddrs *ifaddr, *ifa;
    char ip[INET_ADDRSTRLEN];
    int found = 0;

    if (getifaddrs(&ifaddr) == -1) {
        perror("*** getifaddrs failed");
        return;
    }

    // First pass: prefer Ethernet and WiFi interfaces
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET)
            continue;

        struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
        inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip));

        // Filter out loopback and virtual interfaces
        if (strcmp(ip, "127.0.0.1") != 0 &&
            (ifa->ifa_flags & IFF_UP) &&          // Interface is up
            (ifa->ifa_flags & IFF_RUNNING) &&     // Interface is running
            !(ifa->ifa_flags & IFF_LOOPBACK)) {   // Not loopback interface
            
            // Prefer common physical interfaces
            if (strncmp(ifa->ifa_name, "eth", 3) == 0 ||
                strncmp(ifa->ifa_name, "en", 2) == 0 ||
                strncmp(ifa->ifa_name, "wlan", 4) == 0 ||
                strncmp(ifa->ifa_name, "wl", 2) == 0) {
                
                printf("Local IP Address: %s (interface: %s)\n", ip, ifa->ifa_name);
                found = 1;
                break;
            }
        }
    }

    // Second pass: show other non-virtual interfaces if preferred not found
    if (!found) {
        for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET)
                continue;

            struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
            inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip));

            if (strcmp(ip, "127.0.0.1") != 0 &&
                (ifa->ifa_flags & IFF_UP) &&
                (ifa->ifa_flags & IFF_RUNNING) &&
                !(ifa->ifa_flags & IFF_LOOPBACK) &&
                strncmp(ifa->ifa_name, "docker", 6) != 0 &&
                strncmp(ifa->ifa_name, "veth", 4) != 0 &&
                strncmp(ifa->ifa_name, "br-", 3) != 0 &&
                strncmp(ifa->ifa_name, "virbr", 5) != 0) {
                
                printf("Local IP Address: %s (interface: %s)\n", ip, ifa->ifa_name);
                found = 1;
                break;
            }
        }
    }

    if (!found) {
        printf("No valid network interface IP address found\n");
    }

    freeifaddrs(ifaddr);
}

bool validate_ip(const char *ip) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip, &(sa.sin_addr)) != 0;
}

bool validate_port(int port) {
    return port > 0 && port < 65536;
}

long long get_current_time_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000 + tv.tv_usec;
}

void format_time(long long timestamp_us, char *buffer, size_t buffer_size) {
    time_t seconds = timestamp_us / 1000000;
    long microseconds = timestamp_us % 1000000;
    struct tm *tm_info = localtime(&seconds);
    
    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", tm_info);
    int len = strlen(buffer);
    snprintf(buffer + len, buffer_size - len, ".%06ld", microseconds);
}

void hex_dump(const char *tag, const void *data, size_t size) {
    const unsigned char *byte_data = (const unsigned char *)data;
    
    printf("[RK_NET] %s - Hex dump (%zu bytes):\n", tag, size);
    
    for (size_t i = 0; i < size; i += 16) {
        printf("[RK_NET] %04zx: ", i);
        
        // print hex
        for (size_t j = 0; j < 16; j++) {
            if (i + j < size) {
                printf("%02x ", byte_data[i + j]);
            } else {
                printf("   ");
            }
            
            if (j == 7) printf(" ");
        }
        
        printf(" ");
        
        // print ASCII
        for (size_t j = 0; j < 16; j++) {
            if (i + j < size) {
                unsigned char c = byte_data[i + j];
                printf("%c", (c >= 32 && c < 127) ? c : '.');
            } else {
                printf(" ");
            }
        }
        
        printf("\n");
    }
}
