/*********************************************************************************
* Copyright (C),2020-2030,  Rockchip Beijing Bansband Center
* All Rights Reserved
* V1.00
* FileName : rknet_tcp_server.c
* Author :  wangqunyang
* Description:   tcp server test
* History :
*   <author>      <time>        <version>         <desc>
*   wangqunyang   2025/08/20      1.0               ORG
*
*
*********************************************************************************/
#include "rknet_common.h"

void run_tcp_server() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[MAX_BUFFER_SIZE];
    
    printf("Start TCP Server, Port: %d\n", TCP_DEFAULT_PORT);
    
    // Create tcp socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("*** socket create failure!");
        return;
    }
    
    // set socket option
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("*** setsockopt failure");
        close(server_fd);
        return;
    }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(TCP_DEFAULT_PORT);
    
    // bind
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("*** bind failure!");
        close(server_fd);
        return;
    }
    
    // listen
    if (listen(server_fd, 3) < 0) {
        perror("*** listen failure!");
        close(server_fd);
        return;
    }
    
    printf("TCP Server Started, Wait TCP Client Connect ...\n");
    
    while (!stop_flag) {
        // recv client connect
        if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len)) < 0) {
            if (!stop_flag) perror("*** connect tcp server");
            break;
        }
        
        printf("Connect Client From: %s:%d\n\n", 
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        while (!stop_flag) {
            // recv data
            ssize_t bytes_read = recv(client_fd, buffer, MAX_BUFFER_SIZE, 0);
            
            if (bytes_read <= 0) {
                if (bytes_read == 0) {
                    printf("*** tcp client disconnected.\n");
                } else if (!stop_flag) {
                    perror("*** tcp server read data");
                }
                break;
            }
            
            // print data
            printf("[RK_NET] TCP Server Recv TCP data, size: %zd bytes\n", bytes_read);
            hex_dump("TCP Server Recv", buffer, bytes_read);
            
            // send RK_TCP_APP_ACK
            const char *ack_msg = "RK_TCP_APP_ACK";
            if (send(client_fd, ack_msg, strlen(ack_msg), 0) < 0) {
                if (!stop_flag) perror("*** tcp server send ack");
                break;
            }
            
            printf("[RK_NET] TCP Server Sent ack message: RK_TCP_APP_ACK\n\n");
        }
        
        close(client_fd);
    }
    
    close(server_fd);
    printf("TCP sever closed.\n");
}

