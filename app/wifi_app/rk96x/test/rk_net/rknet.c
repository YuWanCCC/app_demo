/*********************************************************************************
* Copyright (C),2020-2030,  Rockchip Beijing Bansband Center
* All Rights Reserved
* V1.00
* FileName : rknet.c
* Author :  wangqunyang
* Description:   rk tcpip test main program
* History :
*   <author>      <time>        <version>         <desc>
*   wangqunyang   2025/08/20      1.0               ORG
*
*
*********************************************************************************/
#include "rknet_common.h"

int main() {
    char choice;
    
    setup_signal_handlers(signal_handler);
    
    printf("=== RK TCPIP TEST TOOLS ===\n\n");
    print_local_ip();
    
    while (!stop_flag) {
        printf("\n=== Main Menu ===\n");
        printf("1. rk_tcp_S - TCP SERVER\n");
        printf("2. rk_tcp_C - TCP CLIENT\n");
        printf("3. rk_udp_S - UDP SERVER\n");
        printf("4. rk_udp_C - UDP CLIENT\n");
        printf("5. exit - Close(CtrlC CtrlZ)\n");
        printf("Please Select: ");
        
        scanf(" %c", &choice);
        
        switch (choice) {
            case '1':
                printf("\n --- TCP SERVER Process ---\n\n");
                run_tcp_server();
                break;
                
            case '2':
                printf("\n --- TCP CLIENT Process ---\n\n");
                run_tcp_client();
                break;
                
            case '3':
                printf("\n --- UDP SERVER Process ---\n\n");
                run_udp_server();
                break;
                
            case '4':
                printf("\n --- UDP CLIENT Process ---\n\n");
                run_udp_client();
                break;
                
            case '5':
                printf("\n --- TCPIP TOOLS EXIT ---\n\n");
                stop_flag = 1;
                break;
                
            default:
                printf("\n *** Invalid Num, Try ***\n\n");
                break;
        }
        
        if (stop_flag) break;
    }
    
    printf("RK TCPIP TEST TOOLS STOP!\n");
    return 0;
}

