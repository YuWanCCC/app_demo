/*********************************************************************************
* Copyright (C),2020-2030,  Rockchip Beijing Bansband Center
* All Rights Reserved
* V1.00
* FileName : rknet_udp_client.c
* Author :  wangqunyang
* Description:   udp client test
* History :
*   <author>      <time>        <version>         <desc>
*   wangqunyang   2025/08/20      1.0               ORG
*
*
*********************************************************************************/
#include "rknet_common.h"

void udp_client_menu(int sockfd, struct sockaddr_in *server_addr) {
    char choice;
    int packet_size;
    int interval_ms = 2000;
	int repeat_times = MAX_REPEAT_TIMES;
    char buffer[MAX_BUFFER_SIZE];
    socklen_t server_len = sizeof(*server_addr);
    
    while (!stop_flag) {
        printf("\n=== UDP Client Menu ===\n");
        printf("A: UDP Single Packet\n");
        printf("B: UDP Multi Packetes\n");
        printf("X: Return Main Menu\n");
        printf("Please Select: ");
        
        scanf(" %c", &choice);
        choice = toupper(choice);
        
        if (choice == 'X') break;
        
        if (choice != 'A' && choice != 'B') {
            printf("*** Invalid selection, try again\n");
            continue;
        }
        
        printf("Please input packet size (1-%d): ", MAX_PACKET_SIZE);
        scanf("%d", &packet_size);
        
        if (packet_size < 1 || packet_size > MAX_PACKET_SIZE) {
            printf("*** Invalid size, try again\n");
            continue;
        }
        
        if (choice == 'B') {
            printf("UDP Client: Input Multi-Packet Repeat Interval (ms, default: 2000): ");
            scanf("%d", &interval_ms);
            if (interval_ms <= 0) interval_ms = 2000;

            printf("UDP Client: Input Multi-Packet Repeat Times (default: 10): ");
            scanf("%d", &repeat_times);
            if (repeat_times <= 0) repeat_times = MAX_REPEAT_TIMES;
        }
        
        // build data
        memset(buffer, 'U', packet_size); // fill with U
        printf("\n");

        if (choice == 'A') {
            // single packet
            long long t3 = get_current_time_us();
            
            if (sendto(sockfd, buffer, packet_size, 0,
                      (struct sockaddr *)server_addr, server_len) < 0) {
                perror("*** udp client send data");
                continue;
            }
            
            printf("[RK_NET] UDP Send Single, size: %d bytes\n", packet_size);
            hex_dump("UDP SEND", buffer, packet_size);
            
            // wait ack
            ssize_t ack_len = recvfrom(sockfd, buffer, MAX_BUFFER_SIZE, 0,
                                      (struct sockaddr *)server_addr, &server_len);
            long long t4 = get_current_time_us();
            
            if (ack_len > 0) {
                printf("[RK_NET] UDP Recv Single ACK, size: %zd bytes\n", ack_len);
                hex_dump("UDP ACK", buffer, ack_len);
                
                long long time_diff = t4 - t3;
                printf("[RK_NET] --> UDP Client, Packet Round-Trip Time: %lld us (%.3f ms)\n\n", 
                       time_diff, time_diff / 1000.0);
            }
            
        } else if (choice == 'B') {
            // multi packet
            int packet_count = 1;
            
            while ((!stop_flag) && (repeat_times)){
                long long t3 = get_current_time_us();
                
                if (sendto(sockfd, buffer, packet_size, 0,
                          (struct sockaddr *)server_addr, server_len) < 0) {
                    perror("*** udp client send data");
                    break;
                }
                
                printf("[RK_NET] UDP Send Multi #%d, size: %d bytes\n", packet_count, packet_size);
                hex_dump("UDP SEND", buffer, packet_size);
                
                // wait ack
                ssize_t ack_len = recvfrom(sockfd, buffer, MAX_BUFFER_SIZE, 0,
                                          (struct sockaddr *)server_addr, &server_len);
                long long t4 = get_current_time_us();
                
                if (ack_len > 0) {
                    printf("[RK_NET] UDP Recv Multi ACK, size: %zd bytes\n", ack_len);
                    hex_dump("UDP ACK", buffer, ack_len);
                    
                    long long time_diff = t4 - t3;
                    printf("[RK_NET] --> UDP Client, Packet Round-Trip Time Multi #%d: %lld us (%.3f ms)\n\n", 
                           packet_count, time_diff, time_diff / 1000.0);
                }
                
                usleep(interval_ms * 1000);
				repeat_times--;
				packet_count++;
            }
        }
    }
}

void run_udp_client() {
    int sockfd;
    struct sockaddr_in server_addr;
    char ip_input[16];
    int port = UDP_DEFAULT_PORT;
    
    // gain server info
    while (!stop_flag) {
        printf("Input Server IP: ");
        scanf("%15s", ip_input);
        
        printf("Please Input Port (Default, %d): ", UDP_DEFAULT_PORT);
        char port_input[10];
        scanf("%s", port_input);
        
        if (strlen(port_input) > 0) {
            port = atoi(port_input);
        }
        
        if (!validate_ip(ip_input)) {
            printf("*** Invalid IP, try again\n");
            continue;
        }
        
        if (!validate_port(port)) {
            printf("*** Invalid port, try again\n");
            continue;
        }
        
        break;
    }
    
    // UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("*** socket failure");
        return;
    }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip_input);
    
    printf("UDP client connect to server: %s:%d\n", ip_input, port);
    
    // test connect
    const char *test_msg = "TEST UDP";
    if (sendto(sockfd, test_msg, strlen(test_msg), 0,
              (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("*** test udp server");
        close(sockfd);
        return;
    }
    
    char buffer[10];
    socklen_t server_len = sizeof(server_addr);
    ssize_t ack_len = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                              (struct sockaddr *)&server_addr, &server_len);
    
    if (ack_len > 0) {
        printf("Test UDP Server Communication Success!\n");
        
        // udp client menu
        udp_client_menu(sockfd, &server_addr);

    } else {
        printf("Test UDP Server Failure!\n");
    }
    
    close(sockfd);
    printf("UDP Service Close.\n");
}

