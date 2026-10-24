/*
 * ble_netconf_demo.c - Test program for communication with transfer service
 * Receives data forwarded from transfer service, waits 1 second then replies "OK"
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <getopt.h>

#define BTGATT_TRANS_SOCK_PATH "/tmp/rk_btgatt_trans_sock"
#define BTGATT_QUERY_SOCK_PATH "/tmp/rk_btgatt_query_sock"
#define BUFFER_SIZE 8192

static volatile int g_gatt_sock_running = 1;
static pthread_t socket_thread;
static const char *phy_type = NULL;
static struct option options[] = {
    { "phy", required_argument, 0, 'p' },
    { NULL, 0, 0, 0 }
};

static int receive_callback(int sock, const char *data, size_t data_len);


//
// Get Bluetooth address ===> implemented by API holder
//
int get_bluetooth_address(char *bdaddr, size_t size)
{
    int sock_fd;
    struct sockaddr_un addr;
    char buffer[256];
    ssize_t bytes_read;
    int ret = -1;

    // Create socket
    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        fprintf(stderr, "[ble_netconf] Failed to create socket: %s\n", strerror(errno));
        return -1;
    }

    // Set up server address
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, BTGATT_QUERY_SOCK_PATH, sizeof(addr.sun_path) - 1);

    // Connect to server
    if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "[ble_netconf] Failed to connect to query socket: %s\n", strerror(errno));
        goto cleanup;
    }

    // Send bdaddr command
    if (write(sock_fd, "bdaddr", 6) != 6) {
        fprintf(stderr, "[ble_netconf] Failed to send command: %s\n", strerror(errno));
        goto cleanup;
    }

    // Read response
    bytes_read = read(sock_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        fprintf(stderr, "[ble_netconf] Failed to read response: %s\n", strerror(errno));
        goto cleanup;
    }

    // Null terminate the response
    buffer[bytes_read] = '\0';

    // Check if response starts with "Error:"
    if (strncmp(buffer, "Error:", 6) == 0) {
        fprintf(stderr, "[ble_netconf] Server returned error: %s", buffer);
        goto cleanup;
    }

    // Remove trailing newline if present
    if (bytes_read > 0 && buffer[bytes_read - 1] == '\n') {
        buffer[bytes_read - 1] = '\0';
    }

    // Copy response to output buffer
    if (strlen(buffer) >= size) {
        fprintf(stderr, "[ble_netconf] Buffer too small for Bluetooth address\n");
        goto cleanup;
    }

    strncpy(bdaddr, buffer, size);
    ret = 0;

cleanup:
    close(sock_fd);
    return ret;
}

//
// Wait for GATT server to be ready ===> implemented by API holder
//
static int wait_gatt_server_ready(void)
{
    char bdaddr[18];
    const int MAX_RETRIES = 10;    // Maximum number of retries
    const int RETRY_INTERVAL = 1;  // 1s between retries
    int retries = 0;

    printf("[ble_netconf] Waiting for GATT server to be ready...\n");

    while (retries < MAX_RETRIES) {
        if (get_bluetooth_address(bdaddr, sizeof(bdaddr)) == 0) {
            printf("[ble_netconf] GATT server is ready (BD Address: %s)\n", bdaddr);
            return 0;
        }
        sleep(RETRY_INTERVAL);
        retries++;
    }

    fprintf(stderr, "[ble_netconf] Timeout waiting for GATT server\n");
    return -1;
}

//
// Wait for GATT server to quit ===> implemented by API holder
//
static int wait_gatt_server_quit(void)
{
    const int MAX_RETRIES = 10;    // Maximum number of retries
    const int RETRY_INTERVAL = 1;  // 1s between retries
    int retries = 0;

    printf("[ble_netconf] Waiting for GATT server to quit...\n");

    while (retries < MAX_RETRIES) {
        if (access(BTGATT_QUERY_SOCK_PATH, F_OK) != 0) {
            // Socket file doesn't exist - server has quit
            printf("[ble_netconf] GATT server has quit successfully\n");
            return 0;
        }
        sleep(RETRY_INTERVAL);
        retries++;
    }

    fprintf(stderr, "[ble_netconf] Timeout waiting for GATT server to quit\n");
    return -1;
}

//
// Send message function ===> implemented by API holder
//
static int send_message(int sock, const char *message)
{
    // Send message
    if (send(sock, message, strlen(message), 0) < 0) {
        fprintf(stderr, "[ble_netconf] Failed to send message: %s\n", strerror(errno));
        return -1;
    }
    
    return 0;
}

//
// Socket communication thread ===> implemented by API holder
//
static void *socket_thread_func(void *arg)
{
    (void)arg;
    struct sockaddr_un addr;
    int sock;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    
    printf("[ble_netconf] Wait connecting...\n");

    while (g_gatt_sock_running) {
        sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock < 0) {
            fprintf(stderr, "[ble_netconf] failed to create socket - %s\n", strerror(errno));
            sleep(1);
            continue;
        }
        
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, BTGATT_TRANS_SOCK_PATH, sizeof(addr.sun_path) - 1);
        
        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            //fprintf(stderr, "[ble_netconf] Connection failed: %s\n", strerror(errno));
            close(sock);
            sleep(1);
            continue;
        }
        
        printf("[ble_netconf] Connected to transfer service\n");
        
        // Set socket to non-blocking mode
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);

        fd_set readfds;
        struct timeval tv;
        
        while (g_gatt_sock_running) {
            FD_ZERO(&readfds);
            FD_SET(sock, &readfds);
            
            // Set select timeout to 100ms
            tv.tv_sec = 0;
            tv.tv_usec = 100000;
            
            int activity = select(sock + 1, &readfds, NULL, NULL, &tv);
            
            if (activity < 0) {
                if (errno == EINTR) {
                    // Interrupted by signal, check g_gatt_sock_running
                    continue;
                }
                fprintf(stderr, "[ble_netconf] Select error: %s\n", strerror(errno));
                break;
            }
            
            if (activity == 0) {
                // Timeout, continue to check g_gatt_sock_running
                continue;
            }
            
            if (FD_ISSET(sock, &readfds)) {
                bytes_read = recv(sock, buffer, sizeof(buffer) - 1, 0);
                
                if (bytes_read <= 0) {
                    if (bytes_read < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                        // No data available to read in non-blocking mode
                        continue;
                    }
                    if (bytes_read < 0) {
                        fprintf(stderr, "[ble_netconf] Data receive error: %s\n", strerror(errno));
                    } else {
                        printf("[ble_netconf] Server closed connection\n");
                    }
                    break;
                }
                
                buffer[bytes_read] = '\0';
                
                if (receive_callback(sock, buffer, (size_t)bytes_read) < 0) {
                    break;
                }
            }
        }
        
        if (g_gatt_sock_running) {
            sleep(1);
        }
        send_message(sock, "quit");
        sleep(2);
        close(sock);
    }
    
    printf("[ble_netconf] Communication thread exited\n");
    return NULL;
}

//
// Data processing callback ===> implemented by USER
//
static int receive_callback(int sock, const char *data, size_t data_len) 
{
    char response[BUFFER_SIZE];
    int ret = 0;
    
    // Validate input parameters
    if (!data || data_len <= 0) {
        fprintf(stderr, "[ERROR] Invalid input parameters\n");
        return -1;
    }

    // Log received data (safe print with length limitation)
    printf("[ble_netconf] Received %zd bytes: %.*s\n", data_len, 
          (int)(data_len > 32 ? 32 : data_len), data);

    // Check for quit command (case insensitive)
    if (data_len == 4 && strncasecmp(data, "quit", 4) == 0) {
        printf("[ble_netconf] Quit command received\n");
        g_gatt_sock_running = 0;
        return -2;
    } else {
        // Prepare normal response: "OK <received data>"
        // Calculate safe copy length (reserve 4 bytes for "OK " + null terminator)
        size_t max_copy = sizeof(response) - 4;
        size_t copy_len = data_len < max_copy ? data_len : max_copy;
        
        // Format response safely
        int written = snprintf(response, sizeof(response), "OK %.*s",
                             (int)copy_len, data);
        
        // Handle potential truncation
        if (written >= (int)sizeof(response)) {
            response[sizeof(response)-1] = '\0';
            printf("[WARNING] Response truncated to %zu bytes\n", sizeof(response)-1);
        }
    }

    // Send response back to client
    if (send_message(sock, response) < 0) {
        fprintf(stderr, "[ERROR] Failed to send response\n");
        return -1;
    }

    printf("[ble_netconf] Sent response: %s\n", response);
    
    return ret; // 0=success, -1=error, -2=quit
}

//
// Signal handler ===> implemented by USER
//
static void signal_handler(int sig)
{
    printf("[ble_netconf] Received signal %d, preparing to exit\n", sig);
    g_gatt_sock_running = 0;
}

//
// Main function ===> implemented by USER
//
int main(int argc, char *argv[])
{
    int opt;

    // Parse command line arguments
    while ((opt = getopt_long(argc, argv, "p:", options, NULL)) != -1) {
        switch (opt) {
            case 'p':
                phy_type = optarg;
                break;
            default:
                break;
        }
    }

    // Validate PHY type if provided
    if (phy_type && strcmp(phy_type, "1m") && 
        strcmp(phy_type, "2m") && 
        strcmp(phy_type, "coded")) {
        fprintf(stderr, "Invalid PHY type. Supported: 1m, 2m, coded\n");
        return 1;
    }

    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Start GATT server with optional PHY parameter
    char cmd[512];
    snprintf(cmd, sizeof(cmd), 
            "rkbt_gatt_server -n RK-BLE-NETCONF -a on "
            "-u 22222222-4444-4444-8888-888800000001 "
            "--uuid-write 22222222-4444-4444-8888-888800000002 "
            "--uuid-notify 22222222-4444-4444-8888-888800000003 "
            "--hci-monitor "
            "%s%s &", phy_type ? "-p " : "", phy_type ? phy_type : "");
    system(cmd);

    // Wait for GATT server to be ready
    if (wait_gatt_server_ready() < 0) {
        fprintf(stderr, "[ble_netconf] Failed to wait for GATT server ready\n");
        return 1;
    }

    // Create socket communication thread
    if (pthread_create(&socket_thread, NULL, socket_thread_func, NULL) != 0) {
        fprintf(stderr, "[ble_netconf] Thread creation failed: %s\n", strerror(errno));
        return 1;
    }
    
    // Example of getting Bluetooth address
    char bdaddr[18];  // Bluetooth addresses are 17 chars + null terminator
    if (get_bluetooth_address(bdaddr, sizeof(bdaddr)) == 0) {
        printf("[ble_netconf] Bluetooth Address: %s\n", bdaddr);
    }
    else {
        fprintf(stderr, "[ble_netconf] Failed to get Bluetooth address.\n");
    }

    printf("[ble_netconf] Main thread g_gatt_sock_running, press Ctrl+C to exit...\n");
    while (g_gatt_sock_running) {
        sleep(1);
    }
    
    pthread_join(socket_thread, NULL);
    printf("[ble_netconf] Program exited\n");

    // Example of waiting for GATT server to quit and switch back to normal mode
    g_gatt_sock_running = 0;
    if (wait_gatt_server_quit() == 0) {
        // Switch back to normal mode
        system("sh ${RK96X_TARGET_PATH}/script/bt_start.sh");
    } else {
        fprintf(stderr, "[ble_netconf] Failed to wait for GATT server quit\n");
    }

    return 0;
}