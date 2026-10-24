#ifndef __RK_WIFI_RAW_H__
#define __RK_WIFI_RAW_H__

#include "rk_wifi.h"

bool wpa_wifi_list_network(RK_WIFI_SAVED_INFO_s *pInfo, int *ap_cnt);
bool wpa_wifi_get_status(RK_WIFI_INFO_Connection_s *pInfo);
bool wpa_wifi_is_connected_with_ip(void);
bool wpa_wifi_scan(void);
char * wpa_wifi_scan_r(void);
bool wpa_wifi_forget_with_ssid(char *ssid);
bool wpa_wifi_disconnect(void);
bool wpa_wifi_reset(void);
bool wpa_wifi_recovery(void);
bool wpa_wifi_get_mac(char *wifi_mac);

#endif
