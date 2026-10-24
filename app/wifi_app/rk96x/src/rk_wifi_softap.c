#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>

#include <rk_softap.h>
#include <rk_wifi.h>


#define UDHCPD_CONF_DIR "/data/udhcpd.conf"
int create_udhcpd_file(const char *interface)
{
    FILE *fp;
    int ret = 0;
    char cmd[256] = {0};

    // Parameter validation
    if (!interface) {
        pr_err("Invalid interface: NULL pointer\n");
        return -1;
    }

    if (strlen(interface) == 0) {
        pr_err("Invalid interface: empty string\n");
        return -1;
    }

    // Open configuration file
    fp = fopen(UDHCPD_CONF_DIR, "w");
    if (fp == NULL) {
        pr_err("Failed to create udhcpd configuration file: %s\n", strerror(errno));
        return -1;
    }

    // Write configuration with error checking
    ret = fprintf(fp, "# udhcpd.conf configuration\n\n") >= 0;

    ret &= fprintf(fp, "# Define network interface to use\n") >= 0;
    ret &= fprintf(fp, "interface %s\n\n", interface) >= 0;

    ret &= fprintf(fp, "# Define IP address range\n") >= 0;
    ret &= fprintf(fp, "start 10.201.126.2\n") >= 0;
    ret &= fprintf(fp, "end 10.201.126.100\n\n") >= 0;

    ret &= fprintf(fp, "# Define maximum number of leases\n") >= 0;
    ret &= fprintf(fp, "max_leases 20\n\n") >= 0;

    ret &= fprintf(fp, "# Lease time configurations\n") >= 0;
    ret &= fprintf(fp, "remaining yes\n") >= 0;
    ret &= fprintf(fp, "auto_time 7200\n") >= 0;
    ret &= fprintf(fp, "decline_time 3600\n") >= 0;
    ret &= fprintf(fp, "conflict_time 3600\n") >= 0;
    ret &= fprintf(fp, "offer_time 60\n") >= 0;
    ret &= fprintf(fp, "min_lease 60\n\n") >= 0;

    ret &= fprintf(fp, "# Lease file location\n") >= 0;
    ret &= fprintf(fp, "lease_file /data/udhcpd.leases\n\n") >= 0;

    ret &= fprintf(fp, "# Network options\n") >= 0;
    ret &= fprintf(fp, "option subnet 255.255.255.0\n") >= 0;
    ret &= fprintf(fp, "option router 10.201.126.1\n") >= 0;
    ret &= fprintf(fp, "option dns 10.221.0.11 8.8.8.8\n") >= 0;

    if (!ret) {
        pr_err("Failed to write udhcpd configuration: %s\n", strerror(errno));
        fclose(fp);
        return -1;
    }

    // Ensure all data is written to disk
    if (fflush(fp) != 0) {
        pr_err("Failed to flush file buffer: %s\n", strerror(errno));
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

// Find the least congested channel
static int find_best_channel(void)
{
    char scan_results[8192] = {0};
    int channel_load[14] = {0}; // Load for channels 1-13, index 0 not used
    int ret, i;
    int best_channel = 6; // Default channel
    int min_load = INT_MAX;
    int max_load = 0;
    char *line, *next_line;
    
    pr_info("Scanning for best channel...\n");
    
    // Execute wpa_cli scan command
    if (!util_exec_command("wpa_cli scan")) {
        pr_err("Scan command failed\n");
        return best_channel; // Default to channel 6 if scan fails
    }
    
    // Wait for scan completion by checking status
    pr_info("Waiting for scan to complete...\n");
    int wait_count = 0;
    const int max_wait_times = 10;
    int scan_completed = 0;
    
    while (wait_count < max_wait_times && !scan_completed) {
        msleep(500);
        wait_count++;
        
        // Check scan status
        char status_output[512] = {0};
        if (util_capture_command("wpa_cli status", status_output, sizeof(status_output) - 1) > 0) {
            if (strstr(status_output, "wpa_state=SCANNING") == NULL) {
                scan_completed = 1;
            }
        }
    }
    
    if (!scan_completed) {
        pr_info("Scan may not have completed within %d seconds, proceeding anyway\n",
                max_wait_times / 2);
    } else {
        pr_info("Scan completed successfully in %d.%d seconds\n",
                wait_count / 2, (wait_count % 2) * 5);
    }
    
    // Get final scan results
    ret = util_capture_command("wpa_cli scan_results", scan_results, sizeof(scan_results) - 1);
    if (ret < 0) {
        pr_err("Failed to get scan results\n");
        return best_channel; // Return default channel
    }
    
    // Validate scan results content
    if (strlen(scan_results) < 30) {
        pr_err("Scan results appear empty or incomplete, using default channel\n");
        return best_channel;
    }
    
    // Skip header lines until we find actual BSSID data
    // Different wpa versions may have different header formats
    line = scan_results;
    while (line && *line) {
        // Find the end of current line
        char *line_end = strchr(line, '\n');
        if (line_end) {
            *line_end = '\0'; // Temporarily null-terminate the line
        }

        // Check if this line contains BSSID data (MAC address format: xx:xx:xx:xx:xx:xx)
        // A valid BSSID line should start with a MAC address pattern
        if (strlen(line) > 17 &&
            line[2] == ':' && line[5] == ':' && line[8] == ':' &&
            line[11] == ':' && line[14] == ':' &&
            isxdigit(line[0]) && isxdigit(line[1]) &&
            isxdigit(line[3]) && isxdigit(line[4])) {
            // This looks like a BSSID line, restore newline and break
            if (line_end) {
                *line_end = '\n';
            }
            break;
        }

        // Restore newline and move to next line
        if (line_end) {
            *line_end = '\n';
            line = line_end + 1;
        } else {
            // No more lines
            line = NULL;
            break;
        }
    }

    if (!line || !*line) {
        pr_err("No valid BSSID data found in scan results\n");
        return best_channel;
    }

    // Parse each line of scan results starting from the first BSSID line
    while (line && *line) {
        char bssid[18] = {0};
        int frequency = 0;
        int signal = 0;
        int channel;

        // Find the beginning of the next line
        next_line = strchr(line, '\n');
        if (next_line) {
            *next_line = '\0'; // Temporarily replace newline with string terminator
            next_line++; // Move to the beginning of the next line
        }

        // Parse current line
        if (sscanf(line, "%17s %d %d", bssid, &frequency, &signal) >= 3) {
            // Convert frequency to channel number
            if (frequency >= 2412 && frequency <= 2472) {
                channel = (frequency - 2407) / 5;
                if (channel >= 1 && channel <= 13) {
                    // Calculate load based on signal strength
                    // signal is typically between -20 (strong) and -80 (weak)
                    int load = (signal / 10) + 10;

                    // Ensure load is positive (in case of extremely weak signals)
                    if (load < 1) load = 1;

                    // Update load for this channel
                    channel_load[channel] += load;

                    // Calculate adjacent channel loads
                    int load_adj1 = (load * 3) / 4;  // 3/4 of main load
                    int load_adj2 = load / 2;        // 1/2 of main load
                    int load_adj3 = load / 4;        // 1/4 of main load

                    // Add load to adjacent channels based on distance
                    if (channel > 1) {
                        channel_load[channel-1] += load_adj1;  // Adjacent -1
                        if (channel > 2) {
                            channel_load[channel-2] += load_adj2;  // Adjacent -2
                            if (channel > 3) {
                                channel_load[channel-3] += load_adj3;  // Adjacent -3
                            }
                        }
                    }

                    if (channel < 13) {
                        channel_load[channel+1] += load_adj1;  // Adjacent +1
                        if (channel < 12) {
                            channel_load[channel+2] += load_adj2;  // Adjacent +2
                            if (channel < 11) {
                                channel_load[channel+3] += load_adj3;  // Adjacent +3
                            }
                        }
                    }

                    pr_info("Found network on channel %d (frequency %d MHz, signal %d dBm, load %d)\n", 
                            channel, frequency, signal, load);
                }
            }
        }

        // Move to the next line
        line = next_line;
    }

    // Find channel with minimum load and prepare for visualization
    for (i = 1; i <= 13; i++) {
        if (channel_load[i] > max_load) {
            max_load = channel_load[i];
        }

        // Only consider channels 1-11 for best channel selection (softapd limitation)
        if (i <= 11 && channel_load[i] < min_load) {
            min_load = channel_load[i];
            best_channel = i;
        }
    }

    // Print channel load visualization
    pr_info("\nChannel Load Visualization:\n");
    for (i = 1; i <= 13; i++) {
        char visual[51] = {0}; // Max 50 '|' characters + null terminator
        int bar_count;
        
        if (max_load <= 50) {
            // If max_load <= 50, show one '|' per load unit
            bar_count = channel_load[i];
        } else {
            // If max_load > 50, scale to 50 bars maximum
            bar_count = (channel_load[i] * 50) / max_load;
        }

        // Ensure we don't exceed buffer size
        if (bar_count > 50) bar_count = 50;
        
        // Fill the visual bar
        for (int j = 0; j < bar_count; j++) {
            visual[j] = '|';
        }

        printf("CH %2d [%3d]: %s %s\n", i, channel_load[i], visual,
               (i == best_channel) ? "(BEST)" : (i > 11) ? "(FORBIDDEN)" : "");
    }

    pr_info("\nSelected best channel: %d with load %d (max load: %d)\n", 
            best_channel, min_load, max_load);
    return best_channel;
}

static int create_hostapd_file(char *ap, char *ssid, char *psk, int channel)
{
    FILE *fp;
    char cmdline[256] = {0};
    static char HOSTAPD_CONF_DIR[] = "/data/hostapd.conf";
    int ret = 0;
    
    // Parameter validation
    if (!ap || !ssid) {
        pr_err("Invalid parameters: NULL pointer(s)\n");
        return -1;
    }

    if (strlen(ap) == 0 || strlen(ssid) == 0) {
        pr_err("Invalid parameters: empty string(s)\n");
        return -1;
    }

    if (channel < 1 || channel > 11) {
        pr_err("Invalid channel number: %d (must be 1-11)\n", channel);
        return -1;
    }
    
    pr_info("ap=%s, ssid=%s, psk=%s, channel=%d\n", ap, ssid, psk ? psk : "NULL", channel);

    // Open configuration file
    fp = fopen(HOSTAPD_CONF_DIR, "wt+");
    if (NULL == fp) {
        pr_err("Failed to create hostapd configuration file: %s\n", strerror(errno));
        return -1;
    }

    // Write basic configuration with error checking
    ret = fprintf(fp, "interface=%s\n", ap);
    ret &= fputs("ctrl_interface=/var/run/hostapd\n", fp) >= 0;
    ret &= fputs("driver=nl80211\n", fp) >= 0;
    ret &= fprintf(fp, "ssid=%s\n", ssid) >= 0;
    ret &= fprintf(fp, "channel=%d\n", channel) >= 0;
    ret &= fputs("hw_mode=g\n", fp) >= 0;
    ret &= fputs("ieee80211n=1\n", fp) >= 0;
    ret &= fputs("ignore_broadcast_ssid=0\n", fp) >= 0;

    if (ret < 0) {
        pr_err("Failed to write basic configuration: %s\n", strerror(errno));
        fclose(fp);
        return -1;
    }

    // Add WPA configuration if password is provided
    if (psk && strlen(psk) > 0)
    {
        ret = fputs("auth_algs=1\n", fp) >= 0;
        ret &= fputs("wpa=3\n", fp) >= 0;
        ret &= fprintf(fp, "wpa_passphrase=%s\n", psk) >= 0;
        ret &= fputs("wpa_key_mgmt=WPA-PSK\n", fp) >= 0;
        ret &= fputs("wpa_pairwise=TKIP\n", fp) >= 0;
        ret &= fputs("rsn_pairwise=CCMP\n", fp) >= 0;

        if (ret < 0) {
            pr_err("Failed to write WPA configuration: %s\n", strerror(errno));
            fclose(fp);
            return -1;
        }
    }

    // Ensure all data is written to disk
    if (fflush(fp) != 0) {
        pr_err("Failed to flush file buffer: %s\n", strerror(errno));
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

static int start_hostapd(char *ap, char *ssid, char *psk, int channel)
{
    char cmdline[256] = {0};
    static char HOSTAPD_CONF_DIR[] = "/data/hostapd.conf";
    int ret;
    
    // Parameter validation
    if (!ap || !ssid) {
        pr_err("Invalid parameters: NULL pointer(s)\n");
        return -1;
    }
    
    pr_info("ap=%s, ssid=%s, psk=%s, channel=%d\n", ap, ssid, psk ? psk : "NULL", channel);
    
    // Create hostapd configuration file
    ret = create_hostapd_file(ap, ssid, psk, channel);
    if (ret != 0) {
        pr_err("Failed to create hostapd configuration file\n");
        return -1;
    }

    // Configure network interface
    if (snprintf(cmdline, sizeof(cmdline), "ifconfig %s up", ap) >= sizeof(cmdline)) {
        pr_err("Command buffer too small\n");
        return -1;
    }
    
    if (!util_exec_command(cmdline)) {
        pr_err("Failed to bring up interface %s\n", ap);
        return -1;
    }
    
    sleep(1);
    
    memset(cmdline, 0, sizeof(cmdline));
    if (snprintf(cmdline, sizeof(cmdline), "ifconfig %s 10.201.126.1 netmask 255.255.255.0", ap) >= sizeof(cmdline)) {
        pr_err("Command buffer too small\n");
        return -1;
    }
    
    if (!util_exec_command(cmdline)) {
        pr_err("Failed to configure IP address for interface %s\n", ap);
        return -1;
    }

    // Create DHCP server configuration
    ret = create_udhcpd_file(ap);
    if (ret != 0) {
        pr_err("Failed to create udhcpd configuration file\n");
        return -1;
    }
    
    // Create empty leases file
    if (!util_exec_command("touch /data/udhcpd.leases")) {
        pr_err("Failed to create udhcpd leases file\n");
        return -1;
    }
    
    // Start DHCP server
    memset(cmdline, 0, sizeof(cmdline));
    if (snprintf(cmdline, sizeof(cmdline), "/usr/sbin/udhcpd -f %s &", UDHCPD_CONF_DIR) >= sizeof(cmdline)) {
        pr_err("Command buffer too small\n");
        return -1;
    }
    
    if (!util_exec_command(cmdline)) {
        pr_err("Failed to start udhcpd\n");
        return -1;
    }

    // Start hostapd
    memset(cmdline, 0, sizeof(cmdline));
    if (snprintf(cmdline, sizeof(cmdline), "hostapd %s &", HOSTAPD_CONF_DIR) >= sizeof(cmdline)) {
        pr_err("Command buffer too small\n");
        return -1;
    }
    
    if (!util_exec_command(cmdline)) {
        pr_err("Failed to start hostapd\n");
        return -1;
    }

    // Wait for hostapd to initialize
    int time = 100;
    while (time-- > 0 && access("/var/run/hostapd", F_OK))
    {
        usleep(100 * 1000);
    }
    
    if (time <= 0) {
        pr_err("Timeout waiting for hostapd to initialize\n");
        return -1;
    }
    
    return 0;
}

int wifi_start_hostapd(char *ssid, char *psk, int channel)
{
    char wifi_type[128] = {0};
    char ap[64] = {0};
    int ret;

    // Parameter validation
    if (!ssid) {
        pr_err("Invalid SSID: NULL pointer\n");
        return -1;
    }

    pr_info("ssid=%s, psk=%s, channel=%d\n", ssid, psk ? psk : "NULL", channel);

    util_exec_command("killall hostapd");
    util_exec_command("killall udhcpd");

    // Check for p2p0 interface
    ret = util_capture_command("ifconfig -a | grep p2p0", wifi_type, sizeof(wifi_type) - 1);
    if (ret < 0) {
        pr_err("Failed to check for p2p0 interface\n");
        // Continue to check for wlan1
    }

    if (strstr(wifi_type, "p2p0"))
    {
        strcpy(ap, "p2p0");
        
        if (!util_exec_command("ifconfig p2p0 up")) {
            pr_err("Failed to bring up p2p0 interface\n");
            return -1;
        }
        pr_info("using interface p2p0\n");
    }
    else
    {
        // Check for wlan1 interface
        memset(wifi_type, 0, sizeof(wifi_type));
        ret = util_capture_command("ifconfig -a | grep wlan1", wifi_type, sizeof(wifi_type) - 1);
        if (ret < 0 || !strstr(wifi_type, "wlan1")) {
            pr_err("Neither p2p0 nor wlan1 interface found\n");
            return -1;
        }
        
        strcpy(ap, "wlan1");
        
        if (!util_exec_command("ifconfig wlan1 up")) {
            pr_err("Failed to bring up wlan1 interface\n");
            return -1;
        }
        pr_info("using interface wlan1\n");
    }

    // Start hostapd with the selected interface
    ret = start_hostapd(ap, ssid, psk, channel);
    if (ret != 0) {
        pr_err("Failed to start hostapd\n");
        return -1;
    }

    return 0;
}

int wifi_stop_hostapd(void)
{
    char wifi_type[128] = {0};
    int ret;
    bool interface_found = false;
    
    pr_info("Stopping hostapd...\n");
    
    util_exec_command("killall hostapd");
    util_exec_command("killall udhcpd");
    
    // Check for p2p0 interface
    ret = util_capture_command("ifconfig -a | grep p2p0", wifi_type, sizeof(wifi_type) - 1);
    if (ret < 0) {
        pr_err("Failed to check for p2p0 interface\n");
        // Continue to check for wlan1
    }
    
    if (strstr(wifi_type, "p2p0"))
    {
        pr_info("Bringing down p2p0 interface\n");
        if (!util_exec_command("ifconfig p2p0 down")) {
            pr_err("Failed to bring down p2p0 interface\n");
            // Continue anyway
        }
    }
    else
    {
        // Check for wlan1 interface
        memset(wifi_type, 0, sizeof(wifi_type));
        ret = util_capture_command("ifconfig -a | grep wlan1", wifi_type, sizeof(wifi_type) - 1);
        if (ret >= 0 && strstr(wifi_type, "wlan1")) {
            pr_info("Bringing down wlan1 interface\n");
            if (!util_exec_command("ifconfig wlan1 down")) {
                pr_err("Failed to bring down wlan1 interface\n");
                // Continue anyway
            }
        } else {
            pr_info("Neither p2p0 nor wlan1 interface found\n");
            // Continue with cleanup anyway
        }
    }
    
    // Wait for hostapd to fully terminate
    int time = 100;
    while (time-- > 0 && !access("/var/run/hostapd", F_OK))
    {
        usleep(10 * 1000);
    }
    
    if (time <= 0) {
        pr_err("Timeout waiting for hostapd to terminate\n");
        // Continue anyway
    } else {
        pr_info("Hostapd successfully stopped\n");
    }
    
    // Remove configuration files
    if (unlink(UDHCPD_CONF_DIR) != 0 && errno != ENOENT) {
        pr_err("Failed to remove udhcpd configuration file: %s\n", strerror(errno));
        // Continue anyway
    }
    
    if (unlink("/data/hostapd.conf") != 0 && errno != ENOENT) {
        pr_err("Failed to remove hostapd configuration file: %s\n", strerror(errno));
        // Continue anyway
    }
    
    if (unlink("/data/udhcpd.leases") != 0 && errno != ENOENT) {
        pr_err("Failed to remove udhcpd leases file: %s\n", strerror(errno));
        // Continue anyway
    }
    
    return 0;
}

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <pthread.h>

typedef struct
{
    RK_SOFTAP_STATE_CALLBACK callback;
    RK_SOFTAP_SERVER_TYPE server_type;
} RkSoftAp;

#define REQUEST_WIFI_LIST                   "/provision/wifiListInfo"
#define REQUEST_WIFI_SET_UP                 "/provision/wifiSetup"
#define REQUEST_IS_WIFI_CONNECTED           "/provision/wifiState"
#define REQUEST_POST_CONNECT_RESULT         "/provision/connectResult"

#define MSG_BUFF_LEN 8888
static char HTTP_RESPOSE_MESSAGE[] = "HTTP/1.1 200 OK\r\nContent-Type:text/html\r\nContent-Length:%d\r\n\r\n%s";

RkSoftAp m_softap;

#define SOCKET_PORT 8443

pthread_t m_thread;
bool m_isConnecting = false;
static int fd_server = -1;
static int m_port;

static RK_SOFTAP_STATE_CALLBACK m_cb = NULL;
static RK_SOFTAP_STATE m_state = RK_SOFTAP_STATE_IDLE;

static void sendState(RK_SOFTAP_STATE state, const char *data)
{
    if (m_cb != NULL)
        m_cb(state, data);
}

static int initSocket(const unsigned int port)
{
    int ret, fd_socket, val = 1;
    struct sockaddr_in server_addr;

    /* create a socket */
    fd_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_socket < 0)
    {
        printf("%s: create socket failed\n", __func__);
        return -1;
    }

    /* set socket non-blocking */
    //int flags;
    //flags = fcntl(fd_socket, F_GETFL, 0);
    //fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    ret = setsockopt(fd_socket, SOL_SOCKET, SO_REUSEADDR, (void *)&val, sizeof(int));
    if (ret < 0)
    {
        printf("%s: setsockopt failed, ret: %d\n", __func__, ret);
        return -2;
    }

    /* initialize server address */
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    /* bind with the local file */
    ret = bind(fd_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret < 0)
    {
        printf("%s: bind failed, ret: %d\n", __func__, ret);
        close(fd_socket);
        return -3;
    }

    /* listen */
    ret = listen(fd_socket, 1);
    if (ret < 0)
    {
        printf("%s: listen failed, ret: %d\n", __func__, ret);
        close(fd_socket);
        return -4;
    }

    return fd_socket;
}

static int sendWifiList(const int fd)
{
    char msg[MSG_BUFF_LEN] = {0};
    char send_msg[MSG_BUFF_LEN] = {0};
    char *wifilist;

scan_retry:
    printf("[RK] RK_wifi_scan ...\n");
    RK_wifi_scan();
    usleep(800000);

    printf("[RK] RK_wifi_scan_r_sec ...\n");
    wifilist = RK_wifi_scan_for_softap();
    printf("[RK] RK_wifi_scan_r_sec end wifilist: %p\n", wifilist);

    if (wifilist == NULL)
        goto scan_retry;
    if (wifilist && (strlen(wifilist) < 3))
    {
        free(wifilist);
        goto scan_retry;
    }

    strcpy(msg, "{\"type\":\"WifiList\", \"content\":");
    strcat(msg, wifilist);
    strcat(msg, "}");

    snprintf(send_msg, sizeof(msg), HTTP_RESPOSE_MESSAGE, strlen(msg), msg);

    printf("scan_r: %s\n", send_msg);

    if (send(fd, send_msg, sizeof(send_msg), 0) < 0)
    {
        return false;
    }

    free(wifilist);
    return true;
}

static int isWifiConnected(const int fd)
{
    char msg[MSG_BUFF_LEN] = {0};
    bool isConn = 0;

    RK_WIFI_INFO_Connection_s info;

    printf("%s: enter info: %p\n", __func__, &info);
    if (!RK_wifi_get_status(&info))
    {
        printf("%s: info.wpa_state: %s\n", __func__, info.wpa_state);
        if (strncmp(info.wpa_state, "COMPLETED", 9) == 0)
            isConn = 1;
    }

    memset(msg, 0, sizeof(msg));
    snprintf(msg, sizeof(msg), HTTP_RESPOSE_MESSAGE, 1, isConn ? "1" : "0");

    printf("%s: msg: %s\n", __func__, msg);

    m_isConnecting = false;

    if (send(fd, msg, sizeof(msg), 0) < 0)
    {
        return false;
    }

    return true;
}

static char softap_ssid[128];
static char softap_psk[128];

static bool wifiSetup(const int fd, const char *buf)
{
    char msg[MSG_BUFF_LEN] = {0};
    char *str1, *str2;
    int ret;

    printf("enter %s\n", __func__);

    memset(msg, 0, sizeof(msg));
    snprintf(msg, sizeof(msg), HTTP_RESPOSE_MESSAGE, 0, "");
    if (send(fd, msg, sizeof(msg), 0) < 0)
    {
        return false;
    }

    if (m_isConnecting == true)
        return false;

    memset(softap_ssid, 0, 128);
    memset(softap_psk, 0, 128);

    //{"ssid":"YHX","pwd":"rk123456789"}
    str1 = strstr(buf, "\"ssid\"");
    str2 = strstr(buf, "\",\"pwd\"");
    strncpy(softap_ssid, str1 + 8, str2 - str1 - 8);

    str1 = strstr(buf, "\"pwd\"");
    str2 = strstr(str1, "\"}");
    strncpy(softap_psk, str1 + 7, str2 - str1 - 7);

    printf("do connect ssid:\"%s\", psk:\"%s\", isConnecting:%d\n", softap_ssid, softap_psk, m_isConnecting);

    sendState(RK_SOFTAP_STATE_CONNECTING, NULL);
    m_state = RK_SOFTAP_STATE_CONNECTING;

    system("wpa_cli -i wlan0 remove_network all");

    ret = RK_wifi_connect(softap_ssid, softap_psk);
    if (ret < 0)
    {
        sendState(RK_SOFTAP_STATE_FAIL, NULL);
        m_state = RK_SOFTAP_STATE_FAIL;
        m_isConnecting = false;
        return false;
    }

    m_isConnecting = true;

    printf("exit %s\n", __func__);
    return true;
}

static bool doConnectResult(const int fd, const char *buf)
{
    char msg[MSG_BUFF_LEN] = {0};
    char result[2];
    char *str;

    memset(msg, 0, sizeof(msg));
    snprintf(msg, sizeof(msg), HTTP_RESPOSE_MESSAGE, 0, "");
    if (send(fd, msg, sizeof(msg), 0) < 0)
    {
        return false;
    }

    //{"result":"0"}
    str = strstr(buf, "\"result\"");
    strncpy(result, str + 10, 1);
    printf("%s: %s", __func__, result);

    if (strncmp(result, "1", 1) == 0)   // connect success, disable ap
    {
        sendState(RK_SOFTAP_STATE_SUCCESS, NULL);
        m_state = RK_SOFTAP_STATE_SUCCESS;
        wifi_stop_hostapd();
    }
    else
    {
        sendState(RK_SOFTAP_STATE_FAIL, NULL);
        m_state = RK_SOFTAP_STATE_FAIL;
    }

    return true;
}

static void handleRequest(const int fd_client)
{
    size_t n;
    char buf[2048] = {0};

    n = recv(fd_client, buf, sizeof(buf), 0);
    if (n <= 0)
    {
        close(fd_client);
        return;
    }
    buf[n] = '\0';
    printf("TcpServer recv buf:\n[%s]\n", buf);

    if (strstr(buf, REQUEST_WIFI_LIST))
    {
        sendWifiList(fd_client);
    }
    else if (strstr(buf, REQUEST_WIFI_SET_UP))
    {
        wifiSetup(fd_client, buf);
    }
    else if (strstr(buf, REQUEST_IS_WIFI_CONNECTED))
    {
        isWifiConnected(fd_client);
    }
    else if (strstr(buf, REQUEST_POST_CONNECT_RESULT))
    {
        doConnectResult(fd_client, buf);
    }

    close(fd_client);
}

static void *threadAccept(void *arg)
{
    int fd_client, port;
    struct sockaddr_in addr_client;
    socklen_t len_addr_client;
    len_addr_client = sizeof(addr_client);

    prctl(PR_SET_NAME, "threadAccept");

    port = *(int *)arg;

    printf("threadAccept port = %d\n", port);
    fd_server = initSocket(port);
    if (fd_server < 0)
    {
        printf("TcpServer::threadAccept init tcp socket port %d fail. error:%d\n", port, fd_server);
        goto end;
    }

    /* Accept connection all time */
    while (1)
    {
        printf("accept...\n");
        fd_client = accept(fd_server, (struct sockaddr *)&addr_client, &len_addr_client);
        printf("accept fd_client = %d\n", fd_client);
        if (fd_client < 0)
            goto end;

        handleRequest(fd_client);

        if (m_state == RK_SOFTAP_STATE_SUCCESS)
            break;
    }

end:
    printf("Exit Tcp accept thread\n");

    shutdown(fd_server, SHUT_RDWR);
    close(fd_server);
    fd_server = -1;

    return NULL;
}

static int startTcpServer(void)
{
    int ret;

    m_port = SOCKET_PORT;
    m_state = RK_SOFTAP_STATE_IDLE;
    printf("startTcpServer m_port = %d\n", m_port);
    ret = pthread_create(&m_thread, NULL, threadAccept, &m_port);

    return ret;
}

static int stopTcpServer()
{
    if (m_thread <= 0)
        return 0;

    if (fd_server >= 0)
    {
        shutdown(fd_server, SHUT_RDWR);
        close(fd_server);
        fd_server = -1;
    }

    if (0 != pthread_join(m_thread, NULL))
    {
        printf("stopTcpServer fail\n");
        return -1;
    }

    m_thread = 0;
    printf("stopTcpServer success\n");
    return 0;
}

/**
 * Get the channel number of the current station connection
 * @return Channel number (1-11) if connected as station, 0 if not connected or invalid
 */
static int get_station_channel(void)
{
    RK_WIFI_INFO_Connection_s station_info;
    int station_channel = 0;
    
    if (!RK_wifi_is_connected() || !RK_wifi_get_status(&station_info)) {
        return 0;
    }
    
    // Extract channel number from station connection info
    if (strlen(station_info.channel) > 0) {
        station_channel = atoi(station_info.channel);
        pr_info("Station connected on channel: %d\n", station_channel);
    } else if (station_info.freq > 0) {
        // Convert frequency to channel if channel field is empty
        // 2.4GHz: channel = (freq - 2412) / 5 + 1
        if (station_info.freq >= 2412 && station_info.freq <= 2484) {
            station_channel = (station_info.freq - 2412) / 5 + 1;
            pr_info("Station connected on frequency %d MHz (channel %d)\n", 
                   station_info.freq, station_channel);
        }
    }
    
    // Validate channel range
    if (station_channel < 1 || station_channel > 11) {
        pr_info("Invalid station channel: %d, ignoring\n", station_channel);
        return 0;
    }
    
    return station_channel;
}

int RK_softap_start(char *name, char *psk, int channel)
{
    int ret;

    // Parameter validation
    if (!name) {
        pr_err("Invalid SSID: NULL pointer\n");
        return -1;
    }

    if (strlen(name) == 0) {
        pr_err("Invalid SSID: empty string\n");
        return -1;
    }

    if (psk && strlen(psk) > 0 && strlen(psk) < 8) {
        pr_err("Invalid PSK: at least 8 characters\n");
        return -1;
    }

    pr_info("name=%s, psk=%s, channel=%d\n", name, psk ? psk : "NULL", channel);

    // Enable WiFi
    if (!RK_wifi_enable(1)) {
        pr_err("Failed to enable WiFi\n");
        return -1;
    }

    // Check if already connected as station
    int station_channel = get_station_channel();
    
    // Channel selection logic
    if (channel <= 0) {
        if (station_channel > 0 && station_channel <= 11) {
            // Use the same channel as station connection
            channel = station_channel;
            pr_info("Using station channel: %d\n", channel);
        } else {
            // No station connection or invalid channel, select the least congested one
            channel = find_best_channel();
            pr_info("Auto-selected channel: %d\n", channel);
        }
    } else if (channel > 11) {
        pr_err("Invalid channel number: %d (must be 1-11)\n", channel);
        return -1;
    } else if (station_channel > 0 && channel != station_channel) {
        // Warn if specified channel differs from station channel
         pr_info("Warning: Specified channel %d differs from station channel %d. "
                "SoftAP may not work properly.\n", channel, station_channel);
    }

    // Start hostapd
    ret = wifi_start_hostapd(name, psk, channel);
    if (ret != 0) {
        pr_err("Failed to start hostapd\n");
        return -1;
    }

    // Start TCP server
    ret = startTcpServer();
    if (ret != 0) {
        pr_err("Failed to start TCP server\n");
        wifi_stop_hostapd();
        return -1;
    }

    return 0;
}

int RK_softap_stop(void)
{
    //RK_wifi_enable(0);
    wifi_stop_hostapd();
    stopTcpServer();
    return 0;
}


int RK_softap_register_callback(RK_SOFTAP_STATE_CALLBACK cb)
{
    m_cb = cb;
    return 0;
}

int RK_softap_getState(RK_SOFTAP_STATE *pState)
{
    *pState = m_state;
    return 0;
}
