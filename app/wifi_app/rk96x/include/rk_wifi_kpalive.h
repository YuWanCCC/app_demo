#ifndef KEEPALIVE_H
#define KEEPALIVE_H

#include <stdbool.h>

// Configuration for keepalive
typedef struct
{
    char domain[64];
    char port[8];
    char period[4];                 //period of keep alive packets in seconds
    uint8_t login_bytes[100];       //The login string
    uint8_t login_bytes_len;            //The length of login string
    uint8_t heartbeat_bytes[100];   //The heartbeat string
    uint8_t heartbeat_bytes_len;        //The length of heartbeat string
    uint8_t wake_bytes[128];         //The wake up string
    uint8_t wake_bytes_len;             //The length of wake up string
} RK_WIFI_LOW_POWER_KEEPALIVE_s;

// HI command to RK96x
typedef struct
{
    uint32_t tcp_keep_alive;            //The unit is seconds
    uint32_t tcp_keep_interval;         //The unit is seconds
    uint32_t tcp_keep_count;            //The max retry counts
    uint8_t is_enable_tcpkeepalive;    //whether to enable tco keepalive function. 1:enable;0:disable
    uint8_t heartbeat_len;
    uint8_t login_len;
    uint8_t wakeup_len;
    uint8_t heartbeat_data[100];
    uint8_t login_data[100];
    uint8_t wakeup_data[128];
} HI_TCP_KEEPALIVE_REQ_S;

bool wpa_wifi_start_keepalive(RK_WIFI_LOW_POWER_KEEPALIVE_s *cfgs, int sockfd);
bool save_kpalive_conf(RK_WIFI_LOW_POWER_KEEPALIVE_s *kp);
bool load_kpalive_conf(RK_WIFI_LOW_POWER_KEEPALIVE_s *kp);


#endif // KEEPALIVE_H
