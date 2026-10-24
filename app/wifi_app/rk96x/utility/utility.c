#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <paths.h>
#include <sys/wait.h>

#include <sys/types.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

#include "slog.h"
#include "utility.h"
#include "rk_wifi.h"

#define DEBUG 1

bool util_exec_command(const char *cmd)
{
    int status;
    //pr_info("Execute sys command: '%s'\n", cmd);

    status = system(cmd);
    if (status == -1)
    {
        pr_info("run '%s' failed - system() call failed\n", cmd);
        return false;
    }
    
    // Check if command executed successfully (exit code 0)
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
    {
        return true;
    }
    else
    {
        if (WIFEXITED(status))
        {
            pr_info("run '%s' failed - exit code %d\n", cmd, WEXITSTATUS(status));
        }
        else
        {
            pr_info("run '%s' failed - abnormal termination\n", cmd);
        }
        return false;
    }
}

/**
 * @brief Safely execute command and capture output
 * @param cmd Command to execute (null-terminated)
 * @param buffer Output buffer (will be null-terminated)
 * @param buf_size Total capacity of buffer including null terminator
 * @return Number of bytes written to buffer (excluding null terminator)
 */
bool util_capture_command(const char *cmd, char *buffer, size_t buf_size)
{
    if (!buffer || buf_size < 1)
    {
        pr_info("Invalid parameter: buffer or buf_size is NULL\n");
        *buffer = '\0';
        return false;
    }
    //pr_info("Capture sys command: '%s'\n", cmd);

    FILE *pipe = popen(cmd, "r");
    if (!pipe) {
        *buffer = '\0';
        return false;
    }

    size_t total = 0;
    *buffer = '\0';

    while (fgets(buffer + total, buf_size - total, pipe)) {
        size_t added = strlen(buffer + total);
        total += added;
        
        if (buf_size - total <= 1) {
            // Preserve last character for null terminator
            buffer[buf_size-1] = '\0';
            break;
        }
    }

    //pr_info("Capture sys command output: '%s'\n", buffer);
    pclose(pipe);
    return true;
}

// check main-string is included sub-string, with text or binary
static int b_contains(const unsigned char *main_str, size_t main_len,
                      const unsigned char *sub_str, size_t sub_len)
{
    if (sub_len > main_len) return 0;

    for (size_t i = 0; i <= main_len - sub_len; ++i)
    {
        if (memcmp(main_str + i, sub_str, sub_len) == 0)
        {
            return 1; // include
        }
    }

    return 0; // not include
}

/**
 * @brief Unified function to find or kill processes by name
 * @param process_name Target process name to search for
 * @param kill_it true to kill matching processes, false to only find
 * @return >0 : PID found (when kill_it=false) or kill count (kill_it=true)
 *         =0 : No matching process found
 *         <0 : Error occurred
 */
static int find_pid_by_name(const char *process_name, bool kill_it)
{
    const int self_pid = getpid();
    int result = 0;
    struct dirent *entry = NULL;
    DIR *dir = opendir("/proc");
    FILE *fp = NULL;

    if (!dir) {
        pr_err("Failed to open /proc");
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        char cmdline_path[512] = {0};
        char cmdline[256] = {0};

        // Filter process directories (numerical names)
        if (!isdigit(entry->d_name[0])) continue;

        // Read process command line
        snprintf(cmdline_path, sizeof(cmdline_path), "/proc/%s/cmdline", entry->d_name);
        
        fp = fopen(cmdline_path, "r");
        if (!fp) continue;

        // if (fgets(cmdline, sizeof(cmdline), fp) && strstr(cmdline, process_name)) {
        if (fgets(cmdline, sizeof(cmdline), fp) &&
            b_contains((uint8_t *)cmdline, sizeof(cmdline),
                       (uint8_t *)process_name, strlen(process_name))) {
            const int pid = atoi(entry->d_name);
            
            // Always skip current process
            if (pid == self_pid) {
                fclose(fp);
                continue;
            }

            // Execute specified action
            if (kill_it) {
                kill(pid, SIGTERM);
                result++;
            } else {
                // Return first matching PID immediately
                closedir(dir);
                fclose(fp);
                return pid;
            }
        }
        fclose(fp);
    }
    
    closedir(dir);

    if(kill_it)
    {
        if(result)
        {
            printf("PS: %s killed !!!\n", process_name);
        }
        else
        {
            printf("PS: %s no found !!!\n", process_name);
        }
    }

    return kill_it ? result : -1;
}

// Original interface for finding PID
int util_find_pid_by_name(const char *process_name)
{
    return find_pid_by_name(process_name, false);
}

// Interface for killing existing instances
int util_kill_process_by_name(const char *process_name)
{
    return find_pid_by_name(process_name, true);
}

bool util_ipaddr_is_exist(char *ip, char *netmask)
{
    FILE *fp = NULL;
    char line_buffer[256] = {0};
    char *token = NULL;
    const char *ip_pattern = "inet addr:";
    const char *mask_pattern = "Mask:";

    fp = popen("ifconfig -a", "r");
    if (fp == NULL)
    {
        pr_err("popen failed");
        return false;
    }

    while (fgets(line_buffer, sizeof(line_buffer), fp) != NULL)
    {
        token = strstr(line_buffer, ip_pattern);
        if (token != NULL)
        {
            token += strlen(ip_pattern);
            strcpy(ip, token);
            token = strstr(ip, " ");
            if (token != NULL)
            {
                *token = '\0';
            }
        }

        token = strstr(line_buffer, mask_pattern);
        if (token != NULL)
        {
            token += strlen(mask_pattern);
            strcpy(netmask, token);
            token = strstr(netmask, " ");
            if (token != NULL)
            {
                *token = '\0';
            }
        }
    }

    if (ip[0] != '\0' && netmask[0] != '\0')
    {
        pr_info("IP Address: %s, Netmask: %s\n", ip, netmask);
        pclose(fp);
        return true;
    }

    pclose(fp);
    pr_info("No IP address is configured.\n");
    return false;
}

/*
static void delete_file(const char *filepath)
{
    if (access(filepath, F_OK) == 0)    // check file existence
    {
        if (remove(filepath) == 0)
        {
            pr_info("File %s deleted successfully.\n", filepath);
        }
        else
        {
            pr_err("Error deleting file");
        }
    }
    else
    {
        pr_info("File %s does not exist.\n", filepath);
    }
}
*/

static void clean_keepalive_config(void)
{
    char *filepath = KEEPALIVE_CONF_PATH;
    FILE *file = fopen(filepath, "r");
    if (file == NULL)
    {
        pr_info("File %s not found or failed to open.\n", filepath);
        return;
    }

    FILE *temp_file = fopen(TEMP_FILE_PATH, "w");
    if (temp_file == NULL)
    {
        pr_info("Failed to create temporary file.\n");
        fclose(file);
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file))
    {
        // check lines starting with ip or gw
        // Remove blank characters at the beginning and end of the line
        char *start = line;
        while (*start == ' ' || *start == '\t') start++;
        char *end = start + strlen(start) - 1;
        while (end > start && (*end == '\n' || *end == '\r' || *end == ' ' || *end == '\t'))
        {
            *end-- = '\0';
        }

        // If the row is empty or starts with ip or gw, skip it
        if (strlen(start) == 0 || strncmp(start, "ip", 2) == 0 || strncmp(start, "gw", 2) == 0)
        {
            continue;
        }

        // Write the retained content to a temporary file
        fprintf(temp_file, "%s\n", start);
    }

    fclose(file);
    fclose(temp_file);

    // replace original file
    if (remove(filepath) == 0 && rename(TEMP_FILE_PATH, filepath) == 0)
    {
        pr_info("File %s cleaned successfully.\n", filepath);
    }
    else
    {
        pr_err("Error replacing config file");
    }
}

/* function: Get the default gateway IP address
 * IN: gateway - buffer to store the gateway IP address
 * OUT: true - gateway found, false - gateway not found
 */
bool util_network_gw_get(char *gateway)
{
    FILE *fp;
    char buf[256];
    char iface[16];
    unsigned long dest, gw;
    int flags, refcnt, use, metric, mask, mtu, window, irtt;
    bool ret = false;

    if (NULL == gateway)
    {
        pr_info("Invalid parameter: gateway is NULL\n");
        return false;
    }

    fp = fopen("/proc/net/route", "r");
    if (NULL == fp)
    {
        pr_info("[/proc/net/route] fopen fail\n");
        return false;
    }

    // Skip the header line
    if (fgets(buf, sizeof(buf), fp) == NULL)
    {
        pr_info("Failed to read header from route file\n");
        fclose(fp);
        return false;
    }

    // Parse each line to find the default route (destination 0.0.0.0)
    while (fgets(buf, sizeof(buf), fp))
    {
        if (sscanf(buf, "%15s %lx %lx %d %d %d %d %x %d %d %d",
                   iface, &dest, &gw, &flags, &refcnt, &use, &metric,
                   &mask, &mtu, &window, &irtt) != 11)
        {
            continue;
        }

        // Default route has destination 0.0.0.0
        if (dest == 0)
        {
            // Convert gateway from hex network byte order to dotted decimal
            // In /proc/net/route, the bytes are stored in reverse order
            sprintf(gateway, "%d.%d.%d.%d",
                    (int)(gw & 0xFF),
                    (int)((gw >> 8) & 0xFF),
                    (int)((gw >> 16) & 0xFF),
                    (int)((gw >> 24) & 0xFF));
            
            pr_info("Default gateway: %s (via %s)\n", gateway, iface);
            ret = true;
            break;
        }
    }

    fclose(fp);
    
    if (!ret)
    {
        pr_info("No default gateway found\n");
    }
    
    return ret;
}

int util_network_dns_get(char *ethernet_v4_dns, char *ethernet_v4_dns2)
{
    char buf[128];
    int count = 0;
    FILE *pFile;

    pFile = fopen("/etc/resolv.conf", "r");

    if (NULL == pFile)
    {
        pr_info("[/etc/resolv.conf] fopen fail, erroe 02\n");
    }
    else
    {
        memset(buf, 0, sizeof(buf));
        while ((fgets(buf, sizeof(buf), pFile)))
        {
            if (strncmp(buf, "nameserver", 10) == 0)
            {
                int start = 0, end = 0;
                int i = 0;
                for (i = 1; i < 127; i++)
                {
                    if ((buf[i - 1] == 'e') && (buf[i] == 'r') && (buf[i + 1] == ' '))
                    {
                        start = i + 2;
                    }
                    if (buf[i] == '\n')
                    {
                        end = i - 1;
                        break;
                    }
                }
                if ((end - start) > 0)
                {
                    count++;
                    if (count == 1)
                    {
                        sscanf(buf, "%*s%s", ethernet_v4_dns);
                        // memcpy(ethernet_v4_dns, &(buf[start]), end - start + 1);
                    }
                    else if (count == 2 && ethernet_v4_dns2 != NULL)
                    {
                        sscanf(buf, "%*s%s", ethernet_v4_dns2);
                        // memcpy(ethernet_v4_dns2, &(buf[start]), end - start + 1);
                    }
                }
                else
                {
                    pr_info("[/etc/resolv.conf] erroe 01\n");
                }
                if (count == 2)
                {
                    break;
                }
            }
        }
        pr_info("DNS1:%s, DNS2:%s\n", ethernet_v4_dns,
                (ethernet_v4_dns2 != NULL) && (strlen(ethernet_v4_dns2) != 0) ?
                ethernet_v4_dns2 : "NULL");
        fclose(pFile);

        return count;
    }

    return 0;
}

bool util_network_dns_recovery(void)
{
    char buf[128];
    FILE *file_fd, *file_to_fd;

    char *FILE_NAME = "/data/resolv.conf";
    char *FILE_TO_NAME = "/etc/resolv.conf";

    file_fd = fopen(FILE_NAME, "r");
    if (NULL == file_fd)
    {
        pr_info("[/data/resolv.conf] fopen fail, erroe 01 \n");
        return false;
    }
    file_to_fd = fopen(FILE_TO_NAME, "w");
    if (NULL == file_to_fd)
    {
        pr_info("[/etc/resolv.conf] fopen fail, erroe 02 \n");
        return false;
    }

    memset(buf, 0, sizeof(buf));
    while (fgets(buf, sizeof(buf), file_fd) != NULL)
    {
        pr_info("buf:\n%s\n", buf);
        fwrite(buf, strlen(buf), 1, file_to_fd);
    }

    fclose(file_fd);
    fclose(file_to_fd);
    usleep(1000);

    return true;
}

int read_btfw_status(int *firmware, int *status)
{
    FILE *file = fopen(BT_FW_STATUS_PATH, "r");
    if (file == NULL)
    {
        return -1;  // 文件不存在或无法打开
    }

    char line[50];
    while (fgets(line, sizeof(line), file))
    {
        if (sscanf(line, "firmware=%d", firmware) == 1)
        {
            // 读取firmware值
        }
        else if (sscanf(line, "status=%d", status) == 1)
        {
            // 读取status值
        }
    }

    fclose(file);
    return 0;  // 成功读取
}

// 设置终端模式（启用或恢复）
void configure_terminal(bool enable)
{
    static struct termios oldt, newt;

    if (enable)
    {
        tcgetattr(STDIN_FILENO, &oldt); // 保存当前终端设置
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO); // 禁用行缓冲和回显
        tcsetattr(STDIN_FILENO, TCSANOW, &newt); // 应用新设置
        fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK); // 设置非阻塞模式
    }
    else
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // 恢复终端设置
        fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) & ~O_NONBLOCK); // 恢复阻塞模式
    }
}

bool util_ping_gateway(const char *gateway)
{
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ping -c 5 %s", gateway);
    return util_exec_command(cmd);
}

/* function：Check if a string is in hexadecimal format
   IN: str   string pointer
   OUT：0 - Non hexadecimal; 1 - hexadecimal
*/
bool util_is_hex_string(const char *str)
{
    while (*str)
    {
        if (!isxdigit((unsigned char)*str))
            return false;  // Non hexadecimal
        str++;
    }
    return true;  // All characters are in hexadecimal format
}

int util_bytes_to_hex_string(const uint8_t *bytes, size_t bytes_len,
                             char *hex_str_buf, size_t hex_str_buf_len)
{
    // Check if output buffer has enough space (each byte needs 2 chars + null terminator)
    if (hex_str_buf_len < (bytes_len * 2 + 1)) {
        pr_info("Output buffer too small for hex string conversion\n");
        return -1;
    }

    for (size_t i = 0; i < bytes_len; i++)
    {
        sprintf(hex_str_buf + 2 * i, "%02x", bytes[i]);
    }
    hex_str_buf[2 * bytes_len] = '\0';
    return 2 * bytes_len;
}

/* function：Convert hexadecimal string to byte array
   IN: hex_str   hexadecimal string pointer
       output    output buffer
   OUT：number of bytes If the conversion is successful, otherwise return -1
*/
int util_hex_string_to_bytes(const char *hex_str, uint8_t *bytes_buf,
                             size_t bytes_buf_len)
{
    // Check for NULL pointers
    if (!hex_str || !bytes_buf) {
        pr_info("Invalid parameter: hex_str or bytes_buf is NULL\n");
        return -1;
    }
    
    size_t len = strlen(hex_str);
    
    // Check if hex string length is even
    if (len % 2 != 0) {
        pr_info("Hexadecimal string length must be even. len=%d\n", len);
        return -1;  // Hexadecimal string length must be even
    }
    
    // Check if output buffer has enough space
    if (bytes_buf_len < (len / 2)) {
        pr_info("Output buffer too small for hex string conversion. Need %zu bytes, have %zu bytes\n", 
                len / 2, bytes_buf_len);
        return -1;
    }

    for (size_t i = 0; i < len / 2; i++)
    {
        if (sscanf(hex_str + 2 * i, "%2hhx", &bytes_buf[i]) != 1) {
            pr_info("i=%d convert fail\n", i);
            return -1;  // failure conversion
        }
    }
    return len / 2;  // return number of bytes
}

int util_tokenize_cmd(char *cmd, char *argv[], int max_args)
{
    char *pos;
    int argc = 0;

    pos = cmd;
    for (;;)
    {
        while (*pos == ' ')
            pos++;
        if (*pos == '\0')
            break;
        if (*pos == '"')
        {
            argv[argc] = pos + 1;
            char *pos2 = strrchr(pos, '"');
            if (pos2)
            {
                *pos2 = '\0';
                pos = pos2 + 1;
            }
        }
        else
        {
            argv[argc] = pos;
        }
        argc++;
        if (argc == max_args)
            break;
        while (*pos != '\0' && *pos != ' ')
            pos++;
        if (*pos == ' ')
            *pos++ = '\0';
    }

    return argc;
}

bool util_check_and_recovery_wifi(bool force_recovery)
{
    const int retries = 2;
    int i = 0;

    pr_info("enter\n");

    // Wait wps_supplicant start
    while (!util_exec_command("wpa_cli status | grep 'wpa_state='") && (i++ < retries)) {
        pr_info("waiting for wpa_supplicant start... %d/%d\n", i, retries);
        sleep(1);
    }
    if (i >= retries) {
        pr_err("timeout to get wpa_supplicant status.\n");
        return false;
    }
    if (i != 0) sleep(2);   // no need to wait if wpa was ready early

    // Restart wifi if it is disconnected
    if (force_recovery ||
        util_exec_command("wpa_cli status | grep 'wpa_state=DISCONNECTED' >/dev/null 2>&1")) {
        pr_info("recover wifi...\n");
        util_exec_command("wpa_cli -i wlan0 disconnect");
        usleep(300 * 1000);
        util_exec_command("wpa_cli -i wlan0 flush");
        util_exec_command("wpa_cli -i wlan0 reconfigure");
        util_exec_command("wpa_cli -i wlan0 reconnect");
        sleep(1);
    }
    else{
        pr_info("wifi state is NOT DISCONNECTED, just wait it ready...\n");
        util_exec_command("wpa_cli status | grep 'wpa_state='");
    }

    // Wait for wifi to be connected
    i = 0;
    while (!util_exec_command("wpa_cli status | grep 'wpa_state=COMPLETED' >/dev/null 2>&1") && (i++ < retries)) {
        pr_info("waiting for wifi to be connected... %d/%d\n", i, retries);
        util_exec_command("wpa_cli status | grep 'wpa_state='");
        sleep(1);
    }
    if (i >= retries) {
        pr_err("timeout to wait wifi connected.\n");
        return false;
    }
    if (i != 0) sleep(2);   // no need to wait if wifi was ready early

    // Run udhcpc if ip address is not ready
    if (!util_exec_command("wpa_cli status | grep 'ip_address=[^[:space:]]\\+' >/dev/null 2>&1")) {
        pr_info("ip address is not ready, run udhcpc...\n");
        util_exec_command("udhcpc -i wlan0 &");
        sleep(1);
    }
    else {
        return true;
    }

    // Wait for ip address ready
    i = 0;
    while (!util_exec_command("wpa_cli status | grep 'ip_address=[^[:space:]]\\+' >/dev/null 2>&1") && (i++ < retries)) {
        pr_info("wait udhcpc done... %d/%d\n", i, retries);
        sleep(1);
    }
    if (i >= retries) {
        pr_err("timeout to get ip address.\n");
        util_exec_command("killall udhcpc");
        return false;
    }
    util_exec_command("wpa_cli status");

    return true;
}

// Get keepalive_flag
bool util_get_kp_status()
{
    char buffer[16];
    bool result = false;

    FILE *file = fopen(KEEPALIVE_STATUS_PATH, "r");
    if (file == NULL)
    {
        pr_info("Failed to open keepalive_status.txt file\n");
        return false;
    }

    if (fgets(buffer, sizeof(buffer), file) == NULL)
    {
        pr_info("Failed to read from keepalive_status.txt\n");
    }
    else if (strncmp(buffer, "true", 4) == 0)
    {
        result = true;
    }

    fclose(file);
    return result;
}

// set keepalive_flag
void util_set_kp_status(bool status)
{
    FILE *file = fopen(KEEPALIVE_STATUS_PATH, "w");
    if (file == NULL)
    {
        pr_info("Failed to open keepalive_status.txt file for writing\n");
        return;
    }

    if (status)
    {
        fprintf(file, "true\n");
    }
    else
    {
        fprintf(file, "false\n");

        // delete /data/rk960_fwcr.bin
        //pr_info("find and delete ap connection info file!\n");
        //delete_file(KEEPALIVE_BIN_PATH);

        // clean config file, remove ip_address and namespace
        pr_info("remove lines starting with ip and gw in kp configure file!\n");
        clean_keepalive_config();
    }

    fclose(file);
    pr_info("keepalive_flag set to %s\n", status ? "true" : "false");
}
