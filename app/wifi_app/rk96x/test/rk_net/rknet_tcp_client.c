/*********************************************************************************
* Copyright (C),2020-2030,  Rockchip Beijing Bansband Center
* All Rights Reserved
* V1.00
* FileName : rknet_tcp_client.c
* Author :  wangqunyang
* Description:   tcp client test
* History :
*   <author>      <time>        <version>         <desc>
*   wangqunyang   2025/08/20      1.0               ORG
*
*
*********************************************************************************/
#include "rknet_common.h"

void tcp_client_menu(int sockfd, struct sockaddr_in *server_addr) {
    char choice;
    int packet_size;
    int interval_ms = 2000;
	int repeat_times = MAX_REPEAT_TIMES;
    char buffer[MAX_BUFFER_SIZE];
    
    while (!stop_flag) {
        printf("\n=== TCP Client Menu ===\n");
        printf("A: TCP Single Packet\n");
        printf("B: TCP Multi Packetes\n");
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
            printf("TCP Client: Input Multi-Packet Repeat Interval (ms, default: 2000): ");
            scanf("%d", &interval_ms);
            if (interval_ms <= 0) interval_ms = 2000;

            printf("TCP Client: Input Multi-Packet Repeat Times (default: 10): ");
            scanf("%d", &repeat_times);
            if (repeat_times <= 0) repeat_times = MAX_REPEAT_TIMES;
        }
        
        // build data 
        memset(buffer, 'T', packet_size); // fill with T
        printf("\n");

        if (choice == 'A') {
            // single packet
            long long t1 = get_current_time_us();
            
            if (send(sockfd, buffer, packet_size, 0) < 0) {
                perror("*** tcp client send data");
                continue;
            }
            
            printf("[RK_NET] TCP Send Single, Size: %d bytes\n", packet_size);
            hex_dump("TCP SEND", buffer, packet_size);
            
            // waiting for ack
            ssize_t ack_len = recv(sockfd, buffer, MAX_BUFFER_SIZE, 0);
            long long t2 = get_current_time_us();
            
            if (ack_len > 0) {
                printf("[RK_NET] TCP Recv Single ACK, Size: %zd bytes\n", ack_len);
                hex_dump("TCP ACK", buffer, ack_len);
                
                long long time_diff = t2 - t1;
                printf("[RK_NET] --> TCP Client, Packet Round-Trip Time: %lld us (%.3f ms)\n\n", 
                       time_diff, time_diff / 1000.0);
            }
            
        } else if (choice == 'B') {
            // multi
            int packet_count = 1;
            
            while ((!stop_flag) && (repeat_times)) {
                long long t1 = get_current_time_us();
                
                if (send(sockfd, buffer, packet_size, 0) < 0) {
                    perror("*** tcp client send data");
                    break;
                }
                
                printf("[RK_NET] TCP Send Multi #%d, Size: %d bytes\n", packet_count, packet_size);
                hex_dump("TCP SEND", buffer, packet_size);
                
                // waiting for ACK
                ssize_t ack_len = recv(sockfd, buffer, MAX_BUFFER_SIZE, 0);
                long long t2 = get_current_time_us();
                
                if (ack_len > 0) {
                    printf("[RK_NET] TCP Recv Multi ACK, Size: %zd bytes\n", ack_len);
                    hex_dump("TCP ACK", buffer, ack_len);
                    
                    long long time_diff = t2 - t1;
                    printf("[RK_NET] --> TCP Client, Packet Round-Trip Time Multi #%d: %lld us (%.3f ms)\n\n", 
                           packet_count, time_diff, time_diff / 1000.0);
                }
                
                usleep(interval_ms * 1000);
				repeat_times--;
				packet_count++;
            }
        }
    }
}

void run_tcp_client() {
    int sockfd;
    struct sockaddr_in server_addr;
    char ip_input[16];
    int port = TCP_DEFAULT_PORT;
    
    // gain server information
    while (!stop_flag) {
        printf("Input Server IP: ");
        scanf("%15s", ip_input);
        
        printf("Port (Default, %d): ", TCP_DEFAULT_PORT);
        char port_input[10];
        scanf("%s", port_input);
        
        if (strlen(port_input) > 0) {
            port = atoi(port_input);
        }
        
        if (!validate_ip(ip_input)) {
            printf("Invalid ip, try\n");
            continue;
        }
        
        if (!validate_port(port)) {
            printf("Invalid port, try\n");
            continue;
        }
        
        break;
    }
    
    // create socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("*** socket failure");
        return;
    }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip_input);
    
    printf("Connect To Tcp Server %s:%d ...\n", ip_input, port);
    
    // connect server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("*** connect tcp server");
        close(sockfd);
        return;
    }
    
    printf("Connect Tcp Server OK!\n");
    
    // display menu
    tcp_client_menu(sockfd, &server_addr);
    
    close(sockfd);
    printf("TCP Service Closed.\n");
}

