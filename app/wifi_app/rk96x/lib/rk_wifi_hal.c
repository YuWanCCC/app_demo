#include <stddef.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h> // Add the missing header file for usleep function
#include <string.h>
#include <stdio.h>

#include "utility.h"
#include "rk_wifi.h"
#include "rk_wifi_hal.h"
#include "slog.h"

// Use wpa_wifi as default
struct Rk_wifi_driver_ops *rk_hal_ops = &wpa_driver_ops;
RK_wifi_state_callback m_wifi_cb;

// Registration status callback
bool RK_wifi_register_callback(RK_wifi_state_callback cb)
{
    m_wifi_cb = cb;
    pr_info("m_wifi_cb = 0x%p\n", m_wifi_cb);
    return true;
}

// open WiFi
bool RK_wifi_enable(bool enable)
{
    char buf[64];
    int retry = 150;    //150 * 100ms = 15s
    int ret = true;
    pr_info("rk_hal_ops = 0x%p\n", rk_hal_ops);

    if (rk_hal_ops == NULL)
    {
#ifdef ATBM6441
        printf("ATBM6441\n");
        util_capture_command("cat /sys/bus/sdio/devices/*/uevent | grep \"007A\:6011\"", buf, 64);
        if (buf[0])
        {
            //The atbm603x/6441 have some vid:pid
            util_capture_command("lsmod | grep atbm6041_wifi_sdio", buf, 64);
            if (buf[0])
                rk_hal_ops = &atbm_driver_ops;
            printf("The wifi is atbm6441\n");
        }
#endif

#ifdef HI3861L
        printf("HI3861L\n");
        util_capture_command("cat /sys/bus/sdio/devices/*/uevent | grep \"0296\:5347\"", buf, 64);
        if (buf[0])
        {
            rk_hal_ops = &hisi_driver_ops;
            printf("The wifi is hi3861l\n");
        }
#endif

#ifdef WPA_WIFI
        printf("WPA_WIFI\n");
        //default
        if (!rk_hal_ops)
        {
            printf("The wifi is wpa wifi.\n");
            rk_hal_ops = &wpa_driver_ops;
            printf("wpa_driver_ops name: %s\n", wpa_driver_ops.name);
        }
#endif
    }

    if (!rk_hal_ops)
    {
        printf("The wifi is unknow!!!.\n");
        return false;
    }

    pr_info("Wifi check status by [ifconfig -a | grep wlan0]...\n");

    do {
        util_capture_command("ifconfig -a | grep wlan0", buf, 64);
        if (buf[0] == 0 && retry > 0) {
            usleep(100 * 1000); //100ms
            retry--;
            printf("...");
        } else {
            break;
        }
    } while (1);
    printf("\n");

    if (buf[0] == 0)
    {
        pr_info("*** Wifi open failed as not found wlan0 ***\n");
        return false;
    }

    if (rk_hal_ops->wifi_enable)
    {
        //function: wpa_wifi_enable()
        ret = rk_hal_ops->wifi_enable(enable);
    }

    pr_info("exit\n");
    return ret;
}

// Define fuctions with no parameters
#define RK_WIFI_FUNC(func_name, ret_type, ret_fail) \
ret_type RK_wifi_##func_name(void) { \
    if (rk_hal_ops && rk_hal_ops->wifi_##func_name) { \
        return rk_hal_ops->wifi_##func_name(); \
    } \
    else { \
        pr_err("rk_hal_ops is NULL or func is not supported."); \
        return ret_fail; \
    } \
}

// Define fuctions with one parameter
#define RK_WIFI_FUNC_1p(func_name, ret_type, ret_fail, t1, p1) \
ret_type RK_wifi_##func_name(t1 p1) { \
    if (rk_hal_ops && rk_hal_ops->wifi_##func_name) { \
        return rk_hal_ops->wifi_##func_name(p1); \
    } \
    else { \
        pr_err("rk_hal_ops is NULL or func is not supported."); \
        return ret_fail; \
    } \
}

// Define fuctions with two parameters
#define RK_WIFI_FUNC_2p(func_name, ret_type, ret_fail, t1, p1, t2, p2) \
ret_type RK_wifi_##func_name(t1 p1, t2 p2) { \
    if (rk_hal_ops && rk_hal_ops->wifi_##func_name) { \
        return rk_hal_ops->wifi_##func_name(p1, p2); \
    } \
    else { \
        pr_err("rk_hal_ops is NULL or func is not supported."); \
        return ret_fail; \
    } \
}

// get current connection info
RK_WIFI_FUNC_1p(get_status, bool, false, RK_WIFI_INFO_Connection_s*, pInfo)
// check if wifi is connected
RK_WIFI_FUNC(is_connected, bool, false)
// scan
RK_WIFI_FUNC(scan, bool, false)
// get scan result 
RK_WIFI_FUNC(scan_r, char *, NULL)
// Initiate connection
RK_WIFI_FUNC_2p(connect, bool, false, char *, ssid, const char *, psk)
// Connect to WEP encrypted WIFI
RK_WIFI_FUNC_2p(connect_wep, bool, false, char *, ssid, const char *, psk)
// Disconnect current network
RK_WIFI_FUNC(disconnect, bool, false)
// Get the MAC address of the local WiFi
RK_WIFI_FUNC_1p(get_mac, bool, false, char *, wifi_mac)
// WiFi restores the previous configuration, or reconnects to the previous configuration
RK_WIFI_FUNC(recovery, bool, false)
// Removes the specified connected WiFi
RK_WIFI_FUNC_1p(forget_with_ssid, bool, false, char *, ssid)
// Cancel the ongoing distribution network
RK_WIFI_FUNC(cancel, bool, false)
// Get the configured WiFi information
RK_WIFI_FUNC_2p(list_network, bool, false, RK_WIFI_SAVED_INFO_s *, pInfo, int *, ap_cnt)
// Connect to the specified previously connected WiFi
RK_WIFI_FUNC_1p(connect_with_ssid, bool, false, char *, ssid)
// WiFi is restored to factory Settings
RK_WIFI_FUNC(reset, bool, false)
// Get the signal strength connected to the AP
RK_WIFI_FUNC(get_connected_ap_rssi, bool, false)
// Get current COUNTRY CODE
RK_WIFI_FUNC(get_ccode, bool, false)
// Config COUNTRY CODE
RK_WIFI_FUNC(set_ccode, bool, false)
// The WiFi exits the low power mode
RK_WIFI_FUNC(exit_sleep, bool, false)
// host Initiates the keepalive connection with the server
RK_WIFI_FUNC_2p(start_keepalive, bool, false, RK_WIFI_LOW_POWER_KEEPALIVE_s *, cfgs, int, sockfd)
// config WiFi's listenIerval
RK_WIFI_FUNC_1p(set_beacon_interval, bool, false, int, dtim)
// Special: Rockchip distribution demo used
RK_WIFI_FUNC(scan_for_softap, char *, NULL)

RK_WIFI_FUNC(get_wakeup_reason, bool, false)

RK_WIFI_FUNC(get_batlevel, bool, false)

RK_WIFI_FUNC_1p(get_utc, bool, false, unsigned long long *, utc)

RK_WIFI_FUNC_1p(get_pir, bool, false, unsigned int *, pir_ret)

RK_WIFI_FUNC_1p(set_pir, bool, false, bool, enable)

RK_WIFI_FUNC_1p(ota, bool, false, char *, path)

RK_WIFI_FUNC(reboot, bool, false)
