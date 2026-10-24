#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <termios.h>
#include <ctype.h>

#include <sys/prctl.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "rk_wifi.h"
#include "rk_softap.h"

static volatile int wifi_onff_cnt = 0;
static pthread_t rkwifi_init_thread = 0;
static pthread_mutex_t wifi_test_mutex = PTHREAD_MUTEX_INITIALIZER;  // use to sync
static RK_WIFI_RUNNING_State_e g_wifi_state = RK_WIFI_State_OFF;

static void printf_wifi_link_info(RK_WIFI_INFO_Connection_s *info)
{
    DEBUG_STACK_USAGE();

    if (!info)
        return;

    printf("[= WiFi link info ===================]\n");
    printf(" id: %d\n", info->id);
    printf(" bssid: %s\n", info->bssid);
    printf(" ssid: %s\n", info->ssid);
    printf(" freq: %d\n", info->freq);
    printf(" mode: %s\n", info->mode);
    printf(" wpa_state: %s\n", info->wpa_state);
    printf(" ip_address: %s\n", info->ip_address);
    printf(" mac_address: %s\n", info->mac_address);
    printf(" gw_address: %s\n", info->gw);
    printf(" dns_address: %s\n", info->dns);
    printf("[===================================]\n");
}

/* SOFTAP CONFIG TEST */
static int rk_wifi_softap_state_callback(RK_SOFTAP_STATE state, const char *data)
{
    UNUSED(data);
    DEBUG_STACK_USAGE();

    switch (state)
    {
    case RK_SOFTAP_STATE_CONNECTING:
        pr_info("RK_SOFTAP_STATE_CONNECTING\n");
        break;
    case RK_SOFTAP_STATE_DISCONNECT:
        pr_info("RK_SOFTAP_STATE_DISCONNECT\n");
        break;
    case RK_SOFTAP_STATE_FAIL:
        pr_info("RK_SOFTAP_STATE_FAIL\n");
        break;
    case RK_SOFTAP_STATE_SUCCESS:
        pr_info("RK_SOFTAP_STATE_SUCCESS\n");
        break;
    default:
        break;
    }

    return 0;
}

static bool rk_wifi_softap_start(int argc, char *argv[])
{
    DEBUG_STACK_USAGE();
    char *ssid = "Rockchip-SoftAp";
    char *psk = NULL;
    int channel = 0;  // 0 means auto select best channel

    pr_info("argc = %d\n", argc);
    for (int i = 0; i < argc; i++) {
        pr_info("argv[%d] = %s\n", i, argv[i]);
    }

    // Parse arguments with more flexibility
    for (int i = 0; i < argc; i++) {
        // First argument is always SSID
        if (i == 0) {
            ssid = argv[i];
            pr_info("using ssid = %s\n", ssid);
            continue;
        }
        
        // Check if argument is a channel number (1-13)
        char *endptr;
        long val = strtol(argv[i], &endptr, 10);
        if (*endptr == '\0' && val > 0 && val <= 13) {
            channel = (int)val;
            pr_info("using channel = %d\n", channel);
        }
        // Otherwise, treat as password
        else {
            // Check password length
            if (strlen(argv[i]) < 8) {
                pr_err("password too short (len < 8) - '%s'\n", argv[i]);
                return false;
            }
            psk = argv[i];
            pr_info("using psk = %s\n", psk);
        }
    }

    if (argc == 0) {
        pr_info("using default ssid = %s\n", ssid);
    }
    
    // If no password was provided, it's an open network
    if (psk == NULL) {
        pr_info("using open network (no password)\n");
    }
    
    if (channel == 0) {
        pr_info("using auto select channel\n");
    }

    RK_softap_register_callback(rk_wifi_softap_state_callback);
    return (0 == RK_softap_start(ssid, psk, channel));
}

static bool rk_wifi_softap_stop(int argc, char *argv[])
{
    UNUSED(argc); UNUSED(argv);
    DEBUG_STACK_USAGE();

    return (0 == RK_softap_stop());
}
/* SOFTAP TEST END */

static void rk_wifi_state_callback(RK_WIFI_RUNNING_State_e state, RK_WIFI_INFO_Connection_s *info)
{
    DEBUG_STACK_USAGE();

    g_wifi_state = state;
    if (state == RK_WIFI_State_CONNECTED)
    {
        pr_info("RK_WIFI_State_CONNECTED\n");
    }
    if (state == RK_WIFI_State_DHCP_OK)
    {
        pr_info("RK_WIFI_State_DHCP_OK\n");
        printf_wifi_link_info(info);
    }
    else if (state == RK_WIFI_State_CONNECTFAILED)
    {
        pr_info("RK_WIFI_State_CONNECTFAILED\n");
        printf_wifi_link_info(info);
    }
    else if (state == RK_WIFI_State_CONNECTFAILED_WRONG_KEY)
    {
        pr_info("RK_WIFI_State_CONNECTFAILED_WRONG_KEY\n");
        printf_wifi_link_info(info);
    }
    else if (state == RK_WIFI_State_OPEN)
    {
        pr_info("RK_WIFI_State_OPEN\n");
    }
    else if (state == RK_WIFI_State_OFF)
    {
        pr_info("RK_WIFI_State_OFF\n");
    }
    else if (state == RK_WIFI_State_DISCONNECTED)
    {
        pr_info("RK_WIFI_State_DISCONNECTED\n");
        printf_wifi_link_info(info);
    }
    else if (state == RK_WIFI_State_SCAN_RESULTS)
    {
        char *scan_r = NULL;
        pr_info("RK_WIFI_State_SCAN_RESULTS\n");
        scan_r = RK_wifi_scan_r();

        if(scan_r != NULL)
        {
            free(scan_r);
            scan_r = NULL;
        }
    }
    else if (state == RK_WIFI_State_LP_EVENT_TYPE)
    {
        if (info->lp_event_type == RK_WIFI_LP_BAT_LEVEL)
        {
            pr_info("RK_WIFI_MCU_BAT_LEVEL: %d\n", info->lp_data[0]);
        }
        else if (info->lp_event_type == RK_WIFI_LP_PIR_DETECT)
        {
            pr_info("RK_WIFI_MCU_PIR_DETECT: %d\n", info->lp_data[0]);
        }
        else if (info->lp_event_type == RK_WIFI_LP_LONG_PRESS)
        {
            pr_info("RK_WIFI_MCU_LONG_PRESS_KEY\n");
        }
        else if (info->lp_event_type == RK_WIFI_LP_DOORBELL)
        {
            pr_info("RK_WIFI_MCU_DOORBELL_PRESS_KEY\n");
        }
        else if (info->lp_event_type == RK_WIFI_LP_SHORT_PRESS)
        {
            pr_info("RK_WIFI_MCU_SHORT_PRESS_KEY\n");
        }
        else if (info->lp_event_type == RK_WIFI_LP_KEEPALIVE_OK)
        {
            pr_info("RK_WIFI_MCU_KEEPALIVE_OK\n");
        }
        else if (info->lp_event_type == RK_WIFI_LP_KEEPALIVE_FAILED)
        {
            pr_info("RK_WIFI_MCU_KEEPALIVE_FAILED\n");
        }
        else if (info->lp_event_type == RK_WIFI_LP_WAKEUP_REASON)
        {
            pr_info("RK_WIFI_LP_WAKEUP_REASON: %d\n", info->lp_data[0]);
        }
    }

    return;
}

static bool rk_wifi_open(void)
{
    int timeout = 200; //200 * 10ms == 2s
    RK_WIFI_INFO_Connection_s Info;

    DEBUG_STACK_USAGE();
    pr_info("enter\n");

    RK_wifi_register_callback(rk_wifi_state_callback);

    if (!RK_wifi_enable(true))
    {
        pr_info("RK_wifi_enable(true) fail!\n");
        goto fail;
    }

    while ((timeout--) && (g_wifi_state == RK_WIFI_State_OFF))
    {
        usleep(10 * 1000);
        pr_info("TURNING ON ...\n");
    }

    if (g_wifi_state == RK_WIFI_State_OFF)
    {
        goto fail;
    }

    if (RK_wifi_get_status(&Info))
    {
        printf_wifi_link_info(&Info);
    }

    return true;

fail:
    pr_info("Failed end\n");
    return false;
}

static bool rk_wifi_close(int argc, char *argv[])
{
    UNUSED(argc); UNUSED(argv);
    DEBUG_STACK_USAGE();

    if (!RK_wifi_enable(false))
        pr_info("RK_wifi_enable(false) fail!\n");

    return true;
}

//9 input fish1:rk12345678
static bool rk_wifi_connect(int argc, char *argv[])
{
    char *ssid = NULL;
    char *psk = NULL;
    DEBUG_STACK_USAGE();

    if (argc > 0)
    {
        ssid = argv[0];
        if (argc > 1)
        {
            psk = argv[1];
        }
    }

    if (!RK_wifi_connect(ssid, psk))
        pr_info("rk_wifi_connect fail!\n");

    return true;
}

static bool rk_wifi_is_connected(int argc, char *argv[])
{
    UNUSED(argc); UNUSED(argv);
    DEBUG_STACK_USAGE();
    return RK_wifi_is_connected();
}

static bool rk_wifi_connect_wep(int argc, char *argv[])
{
    DEBUG_STACK_USAGE();

    if (argc != 2)
    {
        printf("[Usage:] rkwifi_server connect_wep <ssid> <psk>\n");
        return false;
    }

    if (!RK_wifi_connect_wep(argv[0], argv[1]))
        pr_info("rk_wifi_connect_wep fail!\n");

    return true;
}


static bool rk_wifi_scan(int argc, char *argv[])
{
    UNUSED(argc); UNUSED(argv);
    DEBUG_STACK_USAGE();

    if (!RK_wifi_scan())
        pr_info("RK_wifi_scan fail!\n");

    return true;
}

static bool rk_wifi_scan_r(int argc, char *argv[])
{
    UNUSED(argc); UNUSED(argv);
    char *formatted_scan_result = RK_wifi_scan_r();

    DEBUG_STACK_USAGE();

    if (formatted_scan_result != NULL)
    {
        // print scan result 
        printf("\n--- Scan Result --------------------------------\n%s\n%s\n\n",
               formatted_scan_result,
               "------------------------------------------------");

        // Note: Since formatted_scan_result is allocated by malloc or realloc,
        // After use, it needs to be released through the free function
        free(formatted_scan_result);
        formatted_scan_result = NULL;
    }
    else
    {
        pr_info("Failed to get scan result.\n");
    }

    return true;
}


static bool rk_wifi_list_saved_aps(int argc, char *argv[])
{
    UNUSED(argc); UNUSED(argv);
    RK_WIFI_SAVED_INFO_s wsi[20];
    int ap_cnt = sizeof(wsi) / sizeof(RK_WIFI_SAVED_INFO_s);

    DEBUG_STACK_USAGE();

    if (!RK_wifi_list_network(wsi, &ap_cnt))
    {
        pr_info("Failed to get saved network list\n");
        return false;
    }

    printf("\n--- Network List -----------------------------\n");
    for (int i = 0; i < ap_cnt; i++)
    {
        printf("id: %d, \tname: %s, \tbssid: %s, \tstate: %s\n",
               wsi[i].id, wsi[i].ssid,
               wsi[i].bssid, wsi[i].state);
    }
    printf("----------------------------------------------\n\n");

    return true;
}

static bool rk_wifi_status(int argc, char *argv[])
{
    UNUSED(argc); UNUSED(argv);
    DEBUG_STACK_USAGE();

    RK_WIFI_INFO_Connection_s info;

    if (RK_wifi_get_status(&info))
        printf_wifi_link_info(&info);

    return true;
}

#define WIFI_MAC_LENGTH 18

static bool rk_wifi_get_mac(int argc, char *argv[])
{
    UNUSED(argc); UNUSED(argv);
    char *wifi_mac = (char *)malloc(sizeof(char) * WIFI_MAC_LENGTH);

    DEBUG_STACK_USAGE();

    if (!wifi_mac)
    {
        pr_info("Failed to allocate memory for wifi_mac\n");
        return false;
    }
    if (!RK_wifi_get_mac(wifi_mac))
    {
        pr_info("Failed to get MAC address\n");
        free(wifi_mac); // free mem
        return false;
    }

    pr_info("MAC address: %s\n", wifi_mac);

    free(wifi_mac); // free mem
    return true;
}

static bool rk_wifi_connect_with_ssid(int argc, char *argv[])
{
    UNUSED(argc);
    DEBUG_STACK_USAGE();

    if (!RK_wifi_connect_with_ssid(argv[0]))
        pr_info("RK_wifi_connect_with_ssid fail!\n");

    return true;
}

static bool rk_wifi_recovery(int argc, char *argv[])
{
    UNUSED(argc); UNUSED(argv);
    DEBUG_STACK_USAGE();

    if (!RK_wifi_recovery())
        pr_info("RK_wifi_recovery fail!\n");

    return true;
}

static bool rk_wifi_cancel(int argc, char *argv[])
{
    UNUSED(argc); UNUSED(argv);
    DEBUG_STACK_USAGE();

    if (!RK_wifi_cancel())
        pr_info("RK_wifi_cancel fail!\n");

    return true;
}

static bool rk_wifi_forget_with_ssid(int argc, char *argv[])
{
    UNUSED(argc);
    DEBUG_STACK_USAGE();

    if (!RK_wifi_forget_with_ssid(argv[0]))
    {
        pr_info("rk_wifi_forget_with_ssid fail!\n");
    }

    return true;
}

static bool rk_wifi_disconnect(int argc, char *argv[])
{
    UNUSED(argc); UNUSED(argv);
    DEBUG_STACK_USAGE();

    RK_wifi_disconnect();

    return true;
}

#if 0
static bool rk_wifi_set_pir(int argc, char *argv[])
{
    UNUSED(argc);
    int enable = atoi(argv[0]);

    DEBUG_STACK_USAGE();

    pr_info("%s\n", enable ? "enable" : "disable");
    if (!RK_wifi_set_pir(enable))
        pr_info("rk_wifi_set_pir fail!\n");

    return true;
}
#endif

// use to sync Wi-Fi state changed
static pthread_cond_t wifi_state_cond = PTHREAD_COND_INITIALIZER;  // conditional variable
static pthread_mutex_t wifi_state_mutex = PTHREAD_MUTEX_INITIALIZER;  // state lock

/* WIFI ON/OFF LOOP TEST */
static void *wifi_test_onoff_thread(void *arg)
{
    int *test_count = (int *)arg;
    int successful_tests = 0;
    char gateway[64] = {0};
    int gateway_found = 0;

    DEBUG_STACK_USAGE();
    pr_info("Expected test count: %d\n", *test_count);

    // Get the gateway address (only need to get it once at the start of the function)
    if (util_network_dns_get(gateway, NULL) > 0)
    {
        pr_info("Gateway found: %s\n", gateway);
        gateway_found = 1;  // Gateway obtained successfully
    }
    else
    {
        pr_info("Failed to get gateway address.\n");
        gateway_found = 0;  // Gateway obtained failure
    }

    RK_wifi_register_callback(rk_wifi_state_callback);

    while (wifi_onff_cnt < *test_count)
    {
        bool wifi_enable_success = false;
        bool wifi_disable_success = false;

        // open WiFi
        printf("\n-----------------------------------\n");
        printf(" RKWIFI ON\n");
        printf("-----------------------------------\n\n");        
        if (!RK_wifi_enable(true))
        {
            pr_err("RK_wifi_enable(true) fail!\n");
        }
        else
        {
            // wait WiFi enable
            pthread_mutex_lock(&wifi_state_mutex);
            while (g_wifi_state == RK_WIFI_State_OFF)
            {
                pthread_cond_wait(&wifi_state_cond, &wifi_state_mutex);  // Wait with condition variables
            }
            pthread_mutex_unlock(&wifi_state_mutex);

            wifi_enable_success = true;  // Wi-Fi enabled
        }

        sleep(2);

        if (wifi_enable_success)
        {
            util_exec_command("rkwifi_server scan");
            sleep(2);
            util_exec_command("rkwifi_server scanr");
            sleep(2);

            //util_exec_command("top -n 1");

            // If the gateway is successfully obtained, ping the gateway
            if (gateway_found && RK_wifi_is_connected())
            {
                if (util_ping_gateway(gateway))
                {
                    pr_info("Ping gateway %s successful!\n", gateway);
                }
                else
                {
                    pr_info("Ping gateway %s failed!\n", gateway);
                }
            }
            else
            {
                pr_info("Skipping ping test due to failed gateway retrieval or wifi disconnected.\n");
            }

            // disable Wi-Fi
            printf("\n-----------------------------------\n");
            printf(" RKWIFI OFF\n");
            printf("-----------------------------------\n\n");

            if (!RK_wifi_enable(false))
            {
                pr_info("RK_wifi_enable(false) fail!\n");
            }
            else
            {
                // wait WiFi disabled
                pthread_mutex_lock(&wifi_state_mutex);
                while (g_wifi_state == RK_WIFI_State_OPEN)
                {
                    pthread_cond_wait(&wifi_state_cond, &wifi_state_mutex);  // Wait with condition variables
                }
                pthread_mutex_unlock(&wifi_state_mutex);

                wifi_disable_success = true;  // Wi-Fi disabled
            }
        }

        // sucess counter
        if (wifi_enable_success && wifi_disable_success)
        {
            successful_tests++;
        }

        pthread_mutex_lock(&wifi_test_mutex);
        wifi_onff_cnt++;
        pthread_mutex_unlock(&wifi_test_mutex);

        printf("\n----------------------------------------------------------\n");
        printf(" RKWIFI TURN ONOFF CNT: [%d] ON: [%d] OFF: [%d]\n", wifi_onff_cnt, wifi_enable_success, wifi_disable_success);
        printf("----------------------------------------------------------\n\n");
        sleep(2);
    }

    printf("\n----------------------------------------------------------\n");
    printf(" Test complete, successful tests: %d/%d\n", successful_tests, *test_count);
    printf("----------------------------------------------------------\n\n");

    free(test_count);  // release memory

    // clean state and thread variables
    pthread_mutex_lock(&wifi_test_mutex);
    rkwifi_init_thread = 0;  // reset thread flag
    wifi_onff_cnt = 0;       //reset counter
    pthread_mutex_unlock(&wifi_test_mutex);
    sleep(2);

    // reboot wifi before exit thread
    pr_info("Wait for system restart ... \n\n");
    sleep(3);
    util_exec_command("reboot");

    pr_info("exit wifi_onoff_test thread\n");
    pthread_exit(NULL);
}

static bool rk_wifi_openoff_test(int argc, char *argv[])
{
    DEBUG_STACK_USAGE();

    if (argc < 1)
    {
        printf("Usage: %s <test_count>\n", argv[0]);
        return false;
    }

    // malloc test_count
    int *test_count = malloc(sizeof(int));
    if (test_count == NULL)
    {
        pr_info("Memory allocation failed\n");
        return false;
    }

    *test_count = atoi(argv[0]);

    // Print the argument for test count
    pr_info("Test count is %d\n", *test_count);

    pthread_mutex_lock(&wifi_test_mutex);
    if (rkwifi_init_thread)
    {
        pr_info("rkwifi_init_thread already exists\n");
        pthread_mutex_unlock(&wifi_test_mutex);
        free(test_count);
        return false;
    }

    //pthread_attr_init(&attr);
    //pthread_attr_setstacksize(&attr, 16777216); // set thread stack size

    if (pthread_create(&rkwifi_init_thread, NULL, wifi_test_onoff_thread, test_count) != 0)
    {
        pr_info("Create rkwifi_init_thread failed\n");
        pthread_mutex_unlock(&wifi_test_mutex);
        free(test_count);
        return false;
    }

    //pthread_attr_destroy(&attr);
    pthread_mutex_unlock(&wifi_test_mutex);
    return true;
}

#if 0
static bool rk_wifi_ota(int argc, char *argv[])
{
    DEBUG_STACK_USAGE();

    pr_info("arg nums is %d \n", argc);
    if (argc < 1)
    {
        pr_info("no path,pls enter correct path \n");
        return false;
    }

    return Rk_wifi_ota(argv[0]);
}
#endif

#define CLI_SOCKET_PATH "/tmp/rkserver_socket"
#define RK_CMD_LINE_LEN 1600
#define MAX_ARGS 10

typedef struct
{
    const char *cmd;
    const char *desc;
    bool (*exec)(int argc, char *argv[]);
} command_t;

static command_t socket_cmd_table[] =
{
    {"close", "rkwifi_server close", rk_wifi_close},
    {"scan", "rkwifi_server scan", rk_wifi_scan},
    {"scanr", "rkwifi_server scanr", rk_wifi_scan_r},
    {"connect", "rkwifi_server connect [<ssid> [password]]", rk_wifi_connect},
    {"is_connected", "rkwifi_server is_connected", rk_wifi_is_connected},
    {"connect_wep", "rkwifi_server connect_wep <ssid> <password>", rk_wifi_connect_wep},
    {"disconnect", "rkwifi_server disconnect", rk_wifi_disconnect},
    {"status", "rkwifi_server status", rk_wifi_status},
    //{"deepsleep", "rkwifi_server deepsleep [ONLY LOW POWER]", RK_wifi_enter_sleep},
    {"reconnect", "rkwifi_server reconnect <ssid>", rk_wifi_connect_with_ssid},
    {"forget", "rkwifi_server forget <ssid>", rk_wifi_forget_with_ssid},
    {"list", "rkwifi_server list", rk_wifi_list_saved_aps},
    {"getmac", "rkwifi_server getmac", rk_wifi_get_mac},
    {"cancel", "rkwifi_server cancel", rk_wifi_cancel},
    {"recovery", "rkwifi_server recovery", rk_wifi_recovery},
    {"onoff_test", "rkwifi_server onoff_test", rk_wifi_openoff_test},
    {"ap_cfg", "rkwifi_server ap_cfg <ssid> [password] [channel]\n\t\tpassword: optional, minimum 8 characters (omit for open network)\n\t\tchannel: optional, 1-13 (auto-select if not specified)", rk_wifi_softap_start},
    {"ap_cfg_clr", "rkwifi_server ap_cfg_clr", rk_wifi_softap_stop},
    //{"setpir", "rkwifi_server setpir <0/1> = disable/enable PIR", rk_wifi_set_pir},
    //{"ota","rkwifi_server ota", rk_wifi_ota},
};

static bool rk_cmd_parse(char *arg)
{
    int i = 0;
    int cnt = sizeof(socket_cmd_table) / sizeof(socket_cmd_table[0]);
    char *argv[MAX_ARGS];
    int argc;
    int ret = false;

    DEBUG_STACK_USAGE();

    argc = util_tokenize_cmd(arg, argv, MAX_ARGS);
    pr_info("[%s %d]: %s %s\n", arg, argc, argv[0], argv[1]);
    if (argc)
    {
        for (i = 0; i < cnt; i++)
        {
            if (!strcmp(socket_cmd_table[i].cmd, argv[0]))
            {
                ret = socket_cmd_table[i].exec(argc - 1, &argv[1]);
                break;
            }
        }
    }

    return ret;
}

static void *rkwifi_server_command_func(void *arg)
{
    SET_THREAD_STACK_BASE();

    UNUSED(arg);
    struct sockaddr_un ser_un;
    int socket_fd = 0;
    int connect_fd = 0;
    int ret = 0;
    char *call_ret;
    char cmd_line[RK_CMD_LINE_LEN];

    DEBUG_STACK_USAGE();

    socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd <= 0)
    {
        pr_info("open socket err\n");
        return NULL;
    }

    unlink(CLI_SOCKET_PATH);

    memset(&ser_un, 0, sizeof(ser_un));
    ser_un.sun_family = AF_UNIX;
    strcpy(ser_un.sun_path, CLI_SOCKET_PATH);
    ret = bind(socket_fd, (struct sockaddr *)&ser_un, sizeof(struct sockaddr_un));
    if (ret < 0)
    {
        pr_info("bind err %s %s\n", CLI_SOCKET_PATH, strerror(errno));
        close(socket_fd);
        return NULL;
    }

    ret = listen(socket_fd, 5);
    if (ret < 0)
    {
        pr_info("listen err\n");
        close(socket_fd);
        return NULL;
    }

    while (1)
    {
        pr_info("rkwifi_server_command: accept ...\n");

        //system resource list
        //util_exec_command("top -n 1");
        //util_exec_command("wait $!");
        //util_exec_command("lsof");
        //util_exec_command("wait $!");
        //util_exec_command("netstat -a");
        //util_exec_command("wait $!");
        //usleep(200000);

        // Receive command
        connect_fd = accept(socket_fd, NULL, NULL);
        if (connect_fd < 0) {
            continue;
        }
        memset(cmd_line, 0, sizeof(cmd_line));
        ret = read(connect_fd, cmd_line, sizeof(cmd_line));
        if (ret <= 0) {
            pr_info("cmd_line error!!!\n");
        }

        // Execute command
        pr_info("cmd_line: %s [%d]\n", cmd_line, strlen(cmd_line));
        if (!strncmp(cmd_line, "quit", strlen("quit"))) {
            close(socket_fd);  // close socket
            unlink(CLI_SOCKET_PATH);  // unband address
            break;  // exit thread
        }
        if (rk_cmd_parse(cmd_line)) {
            call_ret = "OK";
        }
        else {
            call_ret = "FAILED";
        }

        // Send response
        pr_info("call_ret: %s\n", call_ret);
        ssize_t write_ret = write(connect_fd, call_ret, strlen(call_ret) + 1);
        if (write_ret < 0) {
            pr_info("Failed to write to socket: %s\n", strerror(errno));
        }
        else if (write_ret != (ssize_t)(strlen(call_ret) + 1)) {
            pr_info("Incomplete write: %zd bytes of %zu bytes\n", write_ret, strlen(call_ret) + 1);
        }
        close(connect_fd);
    }

    // It should not be executed here, but return NULL is added to avoid compiler warnings
    return NULL;
}

static void show_help(void)
{
    unsigned int i;
    printf("\n[Usage]:\n\n");
    for (i = 0; i < sizeof(socket_cmd_table) / sizeof(command_t); i++)
        printf("  %s\n", socket_cmd_table[i].desc);
    printf("\n\n");
}

static int rkwifi_cli(int argc, char *argv[])
{
    char command[RK_CMD_LINE_LEN];
    struct sockaddr_un ser_un;
    int tmp_argc = 1;
    int socket_fd = 0;
    int ret = 0;

    DEBUG_STACK_USAGE();

    memset(command, 0, sizeof(command));
    sprintf(command, "%s", argv[tmp_argc++]);

    while (tmp_argc < argc)
    {
        strcat(command, " ");
        strcat(command, argv[tmp_argc++]);
    }

    socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd <= 0)
    {
        pr_info("open socket err\n");
        return -1;
    }

    memset(&ser_un, 0, sizeof(ser_un));
    ser_un.sun_family = AF_UNIX;
    strcpy(ser_un.sun_path, CLI_SOCKET_PATH);

    ret = connect(socket_fd, (struct sockaddr *)&ser_un, sizeof(struct sockaddr_un));
    if (ret < 0)
    {
        pr_info("connect err\n");
        return -1;
    }

    pr_info("rkwifi cmd: %s\n", command);
    ssize_t write_ret = write(socket_fd, command, strlen(command) + 1);
    if (write_ret < 0) {
        pr_info("Failed to write to socket: %s\n", strerror(errno));
        return -1;
    } else if (write_ret != (ssize_t)(strlen(command) + 1)) {
        pr_info("Incomplete write: %zd bytes of %zu bytes\n", write_ret, strlen(command) + 1);
        return -1;
    }
    
    ssize_t read_ret = read(socket_fd, command, sizeof(command));
    if (read_ret <= 0) {
        pr_info("Failed to read from socket: %s\n", strerror(errno));
        return -1;
    }
    
    pr_info("Ret str: '%s'\n", command);
    if (strcmp(command, "OK")) {
        return -1;
    }
    else {
        return 0;
    }
}

// rkwifi_server main
int main(int argc, char *argv[])
{
    pthread_t sock_tid;

    SET_THREAD_STACK_BASE();
    DEBUG_STACK_USAGE();

    if (argc <= 1) {
        show_help();
        return true;
    }

    if (!strcmp("-h", argv[1])) {
        show_help();
        return true;
    }

    if (!strcmp("start", argv[1])) {
        pr_info("rkwifi_server: start\n");

        if (util_find_pid_by_name("rkwifi_server") > 0)
        {
            pr_info("rkwifi_server: already running\n");
            return 0;
        }

        if (!rk_wifi_open())
        {
            pr_err("============ wlan start failed ============\n");
            return 1;
        }

        if (pthread_create(&sock_tid, NULL, rkwifi_server_command_func, NULL) != 0)
        {
            pr_info("rkwifi_server: *** error creating thread ***\n");
            fprintf(stderr, "Error creating thread\n");
            return 1;
        }
        pr_info("rkwifi_server thread is new created\n");

        pthread_join(sock_tid, NULL);
        pr_info("rkwifi_server: end\n");
    }
    else {
        return rkwifi_cli(argc, argv);
    }

    pr_info("end main\n");
    return 0;
}
