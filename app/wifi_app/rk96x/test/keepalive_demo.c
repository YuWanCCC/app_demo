/*
 * keepalive_demo.c - Demo program for testing WiFi keepalive functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>

/* Validate IP address format */
static int is_valid_ip(const char *ip)
{
    int segments = 0;
    int digits = 0;
    int segment_value = 0;
    
    while (*ip) {
        if (*ip == '.') {
            if (digits == 0 || segment_value > 255) {
                return 0; // Invalid segment
            }
            segments++;
            digits = 0;
            segment_value = 0;
        } else if (isdigit(*ip)) {
            segment_value = segment_value * 10 + (*ip - '0');
            digits++;
            if (digits > 3) {
                return 0; // Maximum 3 digits per segment
            }
        } else {
            return 0; // Invalid character
        }
        ip++;
    }
    
    // Check last segment
    if (digits == 0 || segment_value > 255) {
        return 0;
    }
    
    // IP should have 3 dots for 4 segments
    return segments == 3;
}

/* Validate positive integer */
static int is_positive_integer(const char *str)
{
    char *endptr;
    long num = strtol(str, &endptr, 10);
    return *endptr == '\0' && num > 0;
}

/* Print usage instructions */
static void print_usage(const char *program_name)
{
    printf("Usage: %s <ip> <port> <period>\n\n", program_name);
    printf("Arguments:\n");
    printf("  ip     - Server IP address\n");
    printf("  port   - Server port\n");
    printf("  period - Keepalive interval in seconds\n");
    printf("\nExample:\n");
    printf("  %s 192.168.1.100 8080 60\n", program_name);
}

int main(int argc, char *argv[])
{
    char domain_str[64] = "";
    char port_str[8] = "";
    char period_str[4] = "";
    char cmd[1024];
    int ret;
    
    // Parameter validation
    if (argc < 4) {
        fprintf(stderr, "[kp_demo]: ERROR: Insufficient arguments!\n");
        print_usage(argv[0]);
        return 1;
    }

    // Check IP address
    if (!is_valid_ip(argv[1])) {  // a string like "192.168.0.2"
        fprintf(stderr, "[kp_demo]: ERROR: Invalid IP address format\n");
        return 1;
    }
    strncpy(domain_str, argv[1], sizeof(domain_str)-1);

    // Check port number    // a string like "8080"
    if (!is_positive_integer(argv[2])) {
        fprintf(stderr, "[kp_demo]: ERROR: Port must be positive integer\n");
        return 1;
    }
    strncpy(port_str, argv[2], sizeof(port_str)-1);

    // Check period
    if (!is_positive_integer(argv[3])) {    // a string like "60"
        fprintf(stderr, "[kp_demo]: ERROR: Period must be positive integer\n");
        return 1;
    }
    strncpy(period_str, argv[3], sizeof(period_str)-1);

    //
    // Keepalive example
    //

    // login_str, heartbeet_str and wake_str can be normal strings or hex strings
    // char *wake_str = "rk962wakeup";  // example of normal string
    // char *wake_str = "0xAB,0xCD,0xEF,0x12,0x34,0x56"; // example of hex string

    //char *domain_str = "192.168.1.2";
    //char *port_str = "8080";
    //char *period_str = "60";
    char *login_str = "please replace me with your login string";
    char *heartbeat_str = "please replace me with your heartbeat string";
    char *wake_str = "rk962wakeup";  // please replace me with your wake string

    // Start keepalive process
    snprintf(cmd, sizeof(cmd), "rkwifi_kpalive '%s' '%s' '%s' '%s' '%s' '%s'",
             domain_str, port_str, period_str, login_str, heartbeat_str, wake_str);
    printf("[kp_demo]: cmd: '%s'\n", cmd);

    ret = system(cmd);  // run command
    printf("[kp_demo]: ret: %d\n", ret);

    if (ret != -1 && WIFEXITED(ret)) {
        if (WEXITSTATUS(ret) != 0) {
            fprintf(stderr, "[kp_demo]: ERROR: Failed to start keepalive, enter deepsleep mode 1 for 5 minutes\n");
            system("btmcu_daemon deepsleep 1 300");
            while (1) {
                sleep(1);
            }
            // Should not reach here
        }
    }
    
    return 0;
}