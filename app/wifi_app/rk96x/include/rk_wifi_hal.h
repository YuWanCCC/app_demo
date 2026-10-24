#ifndef __RK_WIFI_HAL_H__
#define __RK_WIFI_HAL_H__

#include "rk_wifi.h"
#include "rk_wifi_kpalive.h"

struct Rk_wifi_driver_ops
{
    const char *name;
    //注册状态回调
    bool (*wifi_register_callback)(RK_wifi_state_callback cb);
    //获取当前WiFi运行状态
    bool (*wifi_running_getState)(RK_WIFI_RUNNING_State_e *pState);
    //获取当前连接信息
    bool (*wifi_get_status)(RK_WIFI_INFO_Connection_s *pInfo);
    bool (*wifi_is_connected)(void);
    //打开WiFi
    bool (*wifi_enable)(int enable);
    //扫描
    bool (*wifi_scan)(void);
    //获取扫描结果
    char *(*wifi_scan_r)(void);
    char *(*wifi_scan_for_softap)(void);
    //发起连接
    bool (*wifi_connect)(char *ssid, const char *psk);
    //连接WEP加密的WIFI
    bool (*wifi_connect_wep)(char *ssid, const char *psk);
    //断开当前网络
    bool (*wifi_disconnect)(void);
    //AP模块设置路由信息
    bool (*wifi_set_hostname)(const char *name);
    bool (*wifi_get_hostname)(char *name, int len);
    //获取本机WiFi的MAC地址
    bool (*wifi_get_mac)(char *wifi_mac);
    bool (*wifi_ping)(char *address);
    //WiFi恢复之前的配置,或重连之前的配置
    bool (*wifi_recovery)(void);
    //移除连接过的WiFi
    bool (*wifi_forget_with_ssid)(char *ssid);
    //取消配网
    bool (*wifi_cancel)(void);
    //获取配置过的WiFi信息
    bool (*wifi_list_network)(RK_WIFI_SAVED_INFO_s *pInfo, int *ap_cnt);
    //连接之前连过的WiFi
    bool (*wifi_connect_with_ssid)(char *ssid);
    //WiFi恢复出厂设置
    bool (*wifi_reset)(void);
    //获取AP的信号强度
    int (*wifi_get_connected_ap_rssi)(void);
    //获取COUNTRY CODE
    int (*wifi_get_ccode)(void);
    //配置COUNTRY CODE
    bool (*wifi_set_ccode)(void);
    //wifi固件ota升级
    bool (*wifi_ota)(char *path);

    //wifi重启
    bool (*wifi_reboot)(void);
    //获取平台PIR电平值：1为触发PIR，0为无触发
    bool (*wifi_get_pir)(unsigned int *pir_ret);

    /*
     * 低功耗专用API
     */
    bool (*wifi_enter_sleep)(void);
    bool (*wifi_exit_sleep)(void);
    bool (*wifi_start_keepalive)(RK_WIFI_LOW_POWER_KEEPALIVE_s *cfgs, int socketid);    //add socketid, maybe the caller create tcp connection
    //int (*host_stop_keepalive)(int port);
    bool (*wifi_set_beacon_interval)(int dtim);
    bool (*wifi_set_pir)(int enable);
    int (*wifi_get_batlevel)(void);
    int (*wifi_get_wakeup_reason)(void);
    bool (*wifi_get_utc)(unsigned long long *utc);
};

extern struct Rk_wifi_driver_ops hisi_driver_ops;
extern struct Rk_wifi_driver_ops atbm_driver_ops;
extern struct Rk_wifi_driver_ops wpa_driver_ops;
extern RK_wifi_state_callback m_wifi_cb;

#endif
