/*********************************************************************************
* Copyright (C),2020-2030,  Rockchip Beijing Bansband Center
* All Rights Reserved
* V1.00
* FileName : rknet_udp_server.c
* Author :  wangqunyang
* Description:   udp server test
* History :
*   <author>      <time>        <version>         <desc>
*   wangqunyang   2025/08/20      1.0               ORG
*
*
*********************************************************************************/
#include "rknet_common.h"

void run_udp_server() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[MAX_BUFFER_SIZE];
    
    printf("Start UPD Server, Port: %d\n", UDP_DEFAULT_PORT);
    
    // create UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("*** socket failure");
        return;
    }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(UDP_DEFAULT_PORT);
    
    // bind
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("*** bind");
        close(sockfd);
        return;
    }
    
    printf("UDP Server Started, Wait UDP Client Connect ...\n\n");
    
    while (!stop_flag) {
        // Recv Data
        ssize_t bytes_read = recvfrom(sockfd, buffer, MAX_BUFFER_SIZE, 0,
                                     (struct sockaddr *)&client_addr, &client_len);
        
        if (bytes_read <= 0) {
            if (!stop_flag) perror("*** udp server read data");
            break;
        }
        
        printf("[RK_NET] UDP Server Recv UDP data from %s:%d, size: %zd bytes\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), bytes_read);
        hex_dump("UDP Server Recv", buffer, bytes_read);
        
        // send RK_UDP_APP_ACK
        const char *ack_msg = "RK_UDP_APP_ACK";
        if (sendto(sockfd, ack_msg, strlen(ack_msg), 0,
                  (struct sockaddr *)&client_addr, client_len) < 0) {
            if (!stop_flag) perror("*** udp server send ack");
            break;
        }
        
        printf("[RK_NET] UDP Server Send message: RK_UDP_APP_ACK\n\n");
    }
    
    close(sockfd);
    printf("UDP Server Closed.\n");
}

