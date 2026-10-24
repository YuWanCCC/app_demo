// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  BlueZ - Bluetooth protocol stack for Linux
 *  Transfer Service and Local Socket Handler
 */

#include "btgatt-server.h"
#include "btgatt-server-transfer.h"

// Buffer size for data transfer
#define BUFFER_SIZE 8192

// Global variables
extern bool g_running;

/*
static void transfer_read_cb(struct gatt_db_attribute *attrib,
                    unsigned int id, uint16_t offset,
                    uint8_t opcode, struct bt_att *att,
                    void *user_data)
{
    UNUSED(att); UNUSED(opcode);
    //struct server *server = user_data;
    uint8_t error = 0;
    const uint8_t *value = (const uint8_t *)"No data to read";
    size_t len = strlen((const char *)value);

    PRLOG("Transfer Read called, offset: %d\n", offset);
    gatt_db_attribute_read_result(attrib, id, error, value, len);
}
*/

static void transfer_write_cb(struct gatt_db_attribute *attrib,
                    unsigned int id, uint16_t offset,
                    const uint8_t *value, size_t len,
                    uint8_t opcode, struct bt_att *att,
                    void *user_data)
{
    UNUSED(att); UNUSED(opcode);
    struct server *server = user_data;
    uint8_t error = 0;
    ssize_t bytes_sent = 0;
    const char *no_provider_msg = "No service provider found";
    
    PRLOG("-------------------------------------\n");
    PRLOG("Transfer Write called, offset: %d, len: %d\n", offset, len);

    // Forward data to local program if connected
    if (server->client_socket > 0) {
        bytes_sent = write(server->client_socket, value, len);
        if (bytes_sent < 0) {
            PRLOG("Failed to write to local socket: %s\n", strerror(errno));
            error = BT_ATT_ERROR_UNLIKELY;
        } else if ((size_t)bytes_sent != len) {
            PRLOG("Partial write to local socket: %zd/%zu bytes\n", bytes_sent, len);
        } else {
            PRLOG("Data forwarded to local program: %zu bytes\n", len);
        }
    } else {
        PRLOG("No local connection, data discarded\n");
        error = BT_ATT_ERROR_UNLIKELY;
        
        // Send notification to BT client about missing local client
        if (server->transfer_ccc & 0x01) { // If notifications are enabled
            bool success = bt_gatt_server_send_notification(server->gatt,
                server->trans_notify_handle,
                (uint8_t *)no_provider_msg,
                strlen(no_provider_msg),
                false);
            
            if (!success) {
                PRLOG("Failed to send no-provider notification\n");
            } else {
                PRLOG("Sent no-provider notification\n");
            }
        }
    }

    gatt_db_attribute_write_result(attrib, id, error);
}

void transfer_ccc_read_cb(struct gatt_db_attribute *attrib,
                    unsigned int id, uint16_t offset,
                    uint8_t opcode, struct bt_att *att,
                    void *user_data)
{
    UNUSED(offset); UNUSED(att); UNUSED(opcode);
    struct server *server = user_data;
    uint8_t value[2];

    PRLOG("Transfer: CCC Read called\n");

    value[0] = server->transfer_ccc;
    value[1] = 0x00;

    gatt_db_attribute_read_result(attrib, id, 0, value, sizeof(value));
}

void transfer_ccc_write_cb(struct gatt_db_attribute *attrib,
                    unsigned int id, uint16_t offset,
                    const uint8_t *value, size_t len,
                    uint8_t opcode, struct bt_att *att,
                    void *user_data)
{
    UNUSED(opcode); UNUSED(att);
    struct server *server = user_data;
    uint8_t ecode = 0;

    PRLOG("Transfer: CCC Write called [%d:%d]\n", value[0], value[1]);

    if (!value || len != 2) {
        PRLOG("Invalid CCC value length: %zu (expected 2)\n", len);
        ecode = BT_ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LEN;
        goto done;
    }

    if (offset) {
        PRLOG("Invalid CCC offset: %u\n", offset);
        ecode = BT_ATT_ERROR_INVALID_OFFSET;
        goto done;
    }
    
    uint8_t old_ccc = server->transfer_ccc;
    server->transfer_ccc = value[0];

    PRLOG("Transfer: CCC changed from 0x%02x to 0x%02x\n", old_ccc, server->transfer_ccc);
    
    if (server->transfer_ccc == 0x00)
        PRLOG("Client disabled both notifications and indications\n");
    else if (server->transfer_ccc == 0x01)
        PRLOG("Client enabled notifications\n");
    else if (server->transfer_ccc == 0x02)
        PRLOG("Client enabled indications\n");
    else if (server->transfer_ccc == 0x03)
        PRLOG("Client enabled both notifications and indications\n");
    else
        PRLOG("Client set invalid CCC value: 0x%02x\n", server->transfer_ccc);

done:
    gatt_db_attribute_write_result(attrib, id, ecode);
}

static void socket_data_callback(int fd, uint32_t events, void *user_data)
{
    struct server *server = user_data;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    if ((events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) || !g_running) {
        PRLOG("Socket closed or error, or server is quiting...\n");
        if (server->client_socket > 0) {
            close(server->client_socket);
            server->client_socket = -1;
        }
        server->socket_watch_id = 0;
        return;
    }

    if (events & EPOLLIN) {
        bytes_read = read(fd, buffer, sizeof(buffer) - 1);
        if (bytes_read <= 0) {
            if (bytes_read < 0) {
                PRLOG("Error reading from local socket: %s\n", strerror(errno));
            }
            if (server->client_socket > 0) {
                close(server->client_socket);
                server->client_socket = -1;
            }
            server->socket_watch_id = 0;
            return;
        }

        buffer[bytes_read] = '\0';
        PRLOG("Received %zd bytes from local program: '%.*s'\n", 
                bytes_read, (int)bytes_read, buffer);

        // Check for quit command
        if (bytes_read == 4 && strncmp(buffer, "quit", 4) == 0) {
            PRLOG("Received quit command, disconnecting...\n");
            g_running = false;
            close(server->fd);  // Close the underlying socket first
            return;
        }

        // Send notifications/indications if enabled
        if (server->transfer_ccc == 0x01) { // Notification
            bool success = bt_gatt_server_send_notification(server->gatt,
                server->trans_notify_handle,
                (uint8_t *)buffer, bytes_read, false);

            if (!success) {
                PRLOG("Notification send failed.\n");
            } else {
                PRLOG("Notification sent successfully.\n");
            }
        } else if (server->transfer_ccc == 0x02) { // Indication
            bt_gatt_server_send_indication(server->gatt,
                server->trans_notify_handle,
                (uint8_t *)buffer, bytes_read,
                NULL, NULL, NULL);
        } else {
            // Store data in buffer for read requests
            //memcpy(server->transfer_buffer, buffer, bytes_read);
        }
    }

    return;
}

static void socket_accept_callback(int fd, uint32_t events, void *user_data)
{
    struct server *server = user_data;
    int client_sock;

    if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
        PRLOG("Listen socket error\n");
        return;
    }

    if (events & EPOLLIN) {
        client_sock = accept(fd, NULL, NULL);
        if (client_sock < 0) {
            PRLOG("Failed to accept connection: %s\n", strerror(errno));
            return;
        }

        // Close existing client connection if any
        if (server->client_socket > 0) {
            close(server->client_socket);
            if (server->socket_watch_id > 0) {
                mainloop_remove_fd(server->socket_watch_id);
                server->socket_watch_id = 0;
            }
        }

        PRLOG("New local program connected\n");
        server->client_socket = client_sock;

        // Add watch for client socket
        server->socket_watch_id = mainloop_add_fd(client_sock,
                                               EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR,
                                               socket_data_callback,
                                               server,
                                               NULL);
    }

    return;
}

// Create unix socket for local program to connect
static int create_unix_socket(struct server *server)
{
    int sock_fd = -1;
    struct sockaddr_un addr;
    int watch_id;
    
    // Create unix socket
    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        PRLOG("Failed to create socket: %s\n", strerror(errno));
        return -1;
    }
    
    // Set unix socket path
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, BTGATT_TRANS_SOCK_PATH, sizeof(addr.sun_path) - 1);
    
    // Remove socket file if already exists
    unlink(BTGATT_TRANS_SOCK_PATH);
    
    // Set socket to non-blocking mode
    int flags = fcntl(sock_fd, F_GETFL, 0);
    if (flags != -1) {
        flags |= O_NONBLOCK;
        fcntl(sock_fd, F_SETFL, flags);
        // Also clear FD_CLOEXEC
        flags = fcntl(sock_fd, F_GETFD);
        if (flags != -1) {
            fcntl(sock_fd, F_SETFD, flags & ~FD_CLOEXEC);
        }
    }
    
    // Bind socket to unix socket path
    if (bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        PRLOG("Failed to bind socket: %s (errno=%d)\n", strerror(errno), errno);
        close(sock_fd);
        unlink(BTGATT_TRANS_SOCK_PATH);
        return -1;
    }
    
    // Set socket permissions to 0666
    if (chmod(BTGATT_TRANS_SOCK_PATH, 0666) < 0) {
        PRLOG("Failed to set socket permissions: %s\n", strerror(errno));
        close(sock_fd);
        return -1;
    }
    PRLOG("Socket permissions set successfully (0666)\n");
    
    // Listen on socket
    if (listen(sock_fd, 5) < 0) {
        PRLOG("Failed to listen on socket: %s\n", strerror(errno));
        close(sock_fd);
        unlink(BTGATT_TRANS_SOCK_PATH);
        return -1;
    }
    
    // Add socket to mainloop
    watch_id = mainloop_add_fd(sock_fd,
                            EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR,
                            socket_accept_callback,
                            server,
                            NULL);
    if (watch_id < 0) {
        PRLOG("Failed to add socket to main loop\n");
        close(sock_fd);
        unlink(BTGATT_TRANS_SOCK_PATH);
        return -1;
    }
    
    PRLOG("Unix socket created and listening on %s (permissions: 0666)\n", BTGATT_TRANS_SOCK_PATH);
    system("ls -la /tmp/rk_btgatt_trans_sock");
    
    server->local_socket = sock_fd;
    server->client_socket = -1;
    server->socket_watch_id = watch_id;
    
    return sock_fd;
}

void close_unix_socket(int unix_sock)
{
    if (unix_sock >= 0) {
        close(unix_sock);
        unlink(BTGATT_TRANS_SOCK_PATH);
    }
}

static void transfer_chngd_ccc_read_cb(struct gatt_db_attribute *attrib,
                    unsigned int id, uint16_t offset,
                    uint8_t opcode, struct bt_att *att,
                    void *user_data)
{
    UNUSED(att); UNUSED(opcode); UNUSED(offset);
    struct server *server = user_data;
    uint8_t value[2];

    PRLOG("Transfer: CCC Read called\n");

    value[0] = server->transfer_ccc;
    value[1] = 0x00;

    gatt_db_attribute_read_result(attrib, id, 0, value, sizeof(value));
}

static void transfer_chngd_ccc_write_cb(struct gatt_db_attribute *attrib,
                    unsigned int id, uint16_t offset,
                    const uint8_t *value, size_t len,
                    uint8_t opcode, struct bt_att *att,
                    void *user_data)
{
    UNUSED(att); UNUSED(opcode);
    struct server *server = user_data;
    uint8_t ecode = 0;

    PRLOG("Transfer: CCC Write called [%d:%d]\n", value[0], value[1]);

    if (!value || len != 2) {
        PRLOG("Invalid CCC value length: %zu (expected 2)\n", len);
        ecode = BT_ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LEN;
        goto done;
    }

    if (offset) {
        PRLOG("Invalid CCC offset: %u\n", offset);
        ecode = BT_ATT_ERROR_INVALID_OFFSET;
        goto done;
    }
    
    uint8_t old_ccc = server->transfer_ccc;
    server->transfer_ccc = value[0];

    PRLOG("Transfer: CCC changed from 0x%02x to 0x%02x\n", old_ccc, server->transfer_ccc);
    
    if (server->transfer_ccc == 0x00)
        PRLOG("Client disabled both notifications and indications\n");
    else if (server->transfer_ccc == 0x01)
        PRLOG("Client enabled notifications\n");
    else if (server->transfer_ccc == 0x02)
        PRLOG("Client enabled indications\n");
    else if (server->transfer_ccc == 0x03)
        PRLOG("Client enabled both notifications and indications\n");
    else
        PRLOG("Client set invalid CCC value: 0x%02x\n", server->transfer_ccc);

done:
    gatt_db_attribute_write_result(attrib, id, ecode);
}

void populate_transfer_service(struct server *server)
{
    bt_uuid_t uuid;
    struct gatt_db_attribute *service, *write_char, *notify_char, *desc;

    PRLOG("populate_transfer_service\n");

    /* Set default UUIDs if not provided */
    if (!g_config.trans_service_uuid) {
        g_config.trans_service_uuid = strdup(DEFAULT_TRANSFER_SERVICE_UUID);
        if (!g_config.trans_service_uuid) {
            PRLOG("Failed to allocate memory for default service UUID\n");
            return;
        }
    }
    PRLOG("Transfer service UUID: %s\n", g_config.trans_service_uuid);

    if (!g_config.trans_write_uuid) {
        g_config.trans_write_uuid = strdup(DEFAULT_TRANSFER_WRITE_UUID);
        if (!g_config.trans_write_uuid) {
            PRLOG("Failed to allocate memory for default write UUID\n");
            return;
        }
    }
    PRLOG("Transfer write characteristic UUID: %s\n", g_config.trans_write_uuid);

    if (!g_config.trans_notify_uuid) {
        g_config.trans_notify_uuid = strdup(DEFAULT_TRANSFER_NOTIFY_UUID);
        if (!g_config.trans_notify_uuid) {
            PRLOG("Failed to allocate memory for default notify UUID\n");
            return;
        }
    }
    PRLOG("Transfer notify characteristic UUID: %s\n", g_config.trans_notify_uuid);

    /* Add Transfer Service with name */
    if (bt_string_to_uuid(&uuid, g_config.trans_service_uuid) < 0) {
        PRLOG("Failed to parse service UUID: %s\n", g_config.trans_service_uuid);
        return;
    }
    service = gatt_db_add_service(server->db, &uuid, true, 10);

    /* Add service name descriptor */
    bt_uuid16_create(&uuid, GATT_CHARAC_USER_DESC_UUID);
    desc = gatt_db_service_add_descriptor(service, &uuid, 
                    BT_ATT_PERM_READ,
                    NULL, NULL, NULL);
    gatt_db_attribute_write(desc, 0, (void *)"Transfer", 
                          strlen("Transfer"), 
                          BT_ATT_OP_WRITE_REQ, NULL, confirm_write, NULL);

    /* Add Write Characteristic with name */
    if (bt_string_to_uuid(&uuid, g_config.trans_write_uuid) < 0) {
        PRLOG("Failed to parse write characteristic UUID: %s\n", g_config.trans_write_uuid);
        return;
    }
    write_char = gatt_db_service_add_characteristic(service, &uuid,
                        BT_ATT_PERM_WRITE,
                        BT_GATT_CHRC_PROP_WRITE,
                        NULL, transfer_write_cb, server);
    server->trans_notify_handle = gatt_db_attribute_get_handle(write_char);

    /* Add write characteristic name descriptor */
    bt_uuid16_create(&uuid, GATT_CHARAC_USER_DESC_UUID);
    desc = gatt_db_service_add_descriptor(service, &uuid,
                    BT_ATT_PERM_READ,
                    NULL, NULL, NULL);
    gatt_db_attribute_write(desc, 0, (void *)"Write", 
                          strlen("Write"), 
                          BT_ATT_OP_WRITE_REQ, NULL, confirm_write, NULL);

    /* Add Notify Characteristic with name */
    if (bt_string_to_uuid(&uuid, g_config.trans_notify_uuid) < 0) {
        PRLOG("Failed to parse notify characteristic UUID: %s\n", g_config.trans_notify_uuid);
        return;
    }
    notify_char = gatt_db_service_add_characteristic(service, &uuid,
                        BT_ATT_PERM_READ,
                        BT_GATT_CHRC_PROP_NOTIFY,
                        NULL, NULL, server);
    server->trans_notify_handle = gatt_db_attribute_get_handle(notify_char);

    /* Add notify characteristic name descriptor */
    bt_uuid16_create(&uuid, GATT_CHARAC_USER_DESC_UUID);
    desc = gatt_db_service_add_descriptor(service, &uuid,
                    BT_ATT_PERM_READ,
                    NULL, NULL, NULL);
    gatt_db_attribute_write(desc, 0, (void *)"Notify", 
                          strlen("Notify"), 
                          BT_ATT_OP_WRITE_REQ, NULL, confirm_write, NULL);

    /* Add CCC descriptor for notify characteristic */
    bt_uuid16_create(&uuid, GATT_CLIENT_CHARAC_CFG_UUID);
    gatt_db_service_add_descriptor(service, &uuid,
                    BT_ATT_PERM_READ | BT_ATT_PERM_WRITE,
                    transfer_chngd_ccc_read_cb,
                    transfer_chngd_ccc_write_cb, server);

    gatt_db_service_set_active(service, true);
    
    // Create local socket for transfer service and integrate with GLib main loop
    PRLOG("Creating transfer service local socket and adding to main loop...\n");
    if (create_unix_socket(server) < 0) {
        PRLOG("Failed to create Unix socket\n");
    } else {
        PRLOG("Unix socket successfully added to main loop\n");
    }
}