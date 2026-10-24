#ifndef RKBT_UTILITY_H
#define RKBT_UTILITY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define UNUSED(x) (void)(x)

//
// For debugging of stack usage
//
#if _DEBUG_STACK
    #define DEF_THREAD_STACK_BASE       __thread void *thread_stack_base = NULL

    #define SET_THREAD_STACK_BASE() \
        do { \
            thread_stack_base = __builtin_frame_address(0); \
            char thread_name[16]; \
            pthread_getname_np(pthread_self(), thread_name, sizeof(thread_name)); \
            printf("[%s] %s(): Thread %s Stack Base Address: %p\n", __FILE__, __func__, thread_name, thread_stack_base); \
        } while (0)

    #define DEBUG_STACK_USAGE() \
        do { \
            void *sp = __builtin_frame_address(0); \
            if (thread_stack_base) { \
                ptrdiff_t stack_usage = (char *)thread_stack_base - (char *)sp; \
                char thread_name[16]; \
                pthread_getname_np(pthread_self(), thread_name, sizeof(thread_name)); \
                printf("[%s] %s(): Thread %s Stack Usage: %d bytes\n", __FILE__, __func__, thread_name, stack_usage); \
            } else { \
                char thread_name[16]; \
                pthread_getname_np(pthread_self(), thread_name, sizeof(thread_name)); \
                printf("[%s] %s(): Thread %s Stack unmonitored\n", __FILE__, __func__, thread_name); \
            } \
        } while (0)

    extern __thread void *thread_stack_base;
#else
    #define DEF_THREAD_STACK_BASE
    #define EXTERN_THREAD_STACK_BASE
    #define SET_THREAD_STACK_BASE()
    #define DEBUG_STACK_USAGE()
#endif  // _DEBUG_STACK


#define KEEPALIVE_BIN_PATH "/data/rk960_fwcr.bin"   //write by rk96x driver
#define KEEPALIVE_CONF_PATH "/data/keepalive.conf"
#define KEEPALIVE_STATUS_PATH "/data/keepalive_status.txt"
#define BT_FW_STATUS_PATH "/data/emode/fw_status.txt"
#define TEMP_FILE_PATH "/data/keepalive_temp.conf"

bool util_capture_command(const char *cmd, char *buffer, size_t buf_size);
bool util_exec_command(const char *cmd);
int util_find_pid_by_name(const char* process_name);
int util_kill_process_by_name(const char *process_name);
bool util_is_hex_string(const char *str);
bool util_ipaddr_is_exist(char *ip, char *netmask);
bool util_network_dns_recovery(void);
bool util_network_gw_get(char *gateway);
int util_network_dns_get(char *ethernet_v4_dns, char *ethernet_v4_dns2);
bool util_ping_gateway(const char *gateway);
int util_tokenize_cmd(char *cmd, char *argv[], int max_args);
int util_bytes_to_hex_string(const uint8_t *bytes, size_t bytes_len,
    char *hex_str_buf, size_t hex_str_buf_len);
int util_hex_string_to_bytes(const char *hex_str, uint8_t *bytes_buf,
    size_t bytes_buf_len);

bool util_check_and_recovery_wifi(bool force_recovery);
void util_set_kp_status(bool status);
bool util_get_kp_status();

#define msleep(x) usleep(x * 1000)

#ifdef __cplusplus
}
#endif

#endif //RKBT_UTILITY_H

