// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  BlueZ - Bluetooth protocol stack for Linux
 *  GATT Server Main Program
 */

#include "btgatt-server.h"

#define UUID_GAP        0x1800
#define UUID_GATT       0x1801

#define BTGATT_QUERY_SOCK_PATH "/tmp/rk_btgatt_query_sock"

// Query thread data
struct query_data {
    int query_socket;      // Listening socket for queries
    bool query_thread_running;
    pthread_t query_thread;
    pthread_mutex_t query_mutex;
};

// Global query data
static struct query_data g_query_data;

// Global variables and constants
static const char *test_device_name = "RK-BTGATT-SERVER";
struct mgmt *g_mgmt = NULL;
uint16_t mgmt_index = MGMT_INDEX_NONE;

volatile bool g_running = true;
volatile bool g_recon = false;

struct server_config g_config = {
    .mtu = 512,
    .sec_level = BT_SECURITY_LOW,
    .src_type = BDADDR_LE_PUBLIC,
    .adv_name = "RK-BTGATT-SERVER",
    .trans_service_uuid = DEFAULT_TRANSFER_SERVICE_UUID,
    .trans_write_uuid = DEFAULT_TRANSFER_WRITE_UUID,
    .trans_notify_uuid = DEFAULT_TRANSFER_NOTIFY_UUID,
    .enable_advertising = true,
    .phy_type = MGMT_PHY_LE_1M_TX | MGMT_PHY_LE_1M_RX,
    .is_connected = false,
    .hci_monitor = false
};

static bdaddr_t g_src_addr = {{0}};


// Attribute set error callback
void confirm_write(struct gatt_db_attribute *attr, int err, void *user_data)
{
    UNUSED(user_data);
    if (err != 0) {
        PRLOG("ERROR: caching attribute %p - err: %d\n", attr, err);
        return;
    }
}

// Command line options
static struct option main_options[] = {
    { "name",           1, 0, 'n' },
    { "adv",            1, 0, 'a' },
    { "uuid",           1, 0, 'u' },
    { "uuid-write",     1, 0, 0   },
    { "uuid-notify",    1, 0, 0   },
    { "mtu",            1, 0, 'm' },
    { "security-level", 1, 0, 's' },
    { "type",           1, 0, 't' },
    { "phy",            1, 0, 'p' },
    { "hci-monitor",    0, 0, 'M' },
    { "help",           0, 0, 'h' },
    { }
};

static void usage(void)
{
    printf("Rockchip Bluetooth GATT Server\n");
    printf("Usage:\n\trkbt_gatt_server [options]\n");

    printf("Options:\n"
        "\t-n, --name <name>\t\tSet advertising name (default: RK-BTGATT-SERVER)\n"
        "\t-a, --adv <on|off>\t\tEnable or disable advertising (default: on)\n"
        "\t-u, --uuid <uuid>\t\tSet service UUID (long format)\n"
        "\t    --uuid-write <uuid>\t\tSet write characteristic UUID (long format)\n"
        "\t    --uuid-notify <uuid>\t\tSet notify characteristic UUID (long format)\n"
        "\t-m, --mtu <mtu>\t\t\tSet ATT MTU size (minimum 23, default 512)\n"
        "\t-s, --security-level <sec>\tSet security level:\n"
        "\t\t  low    - No encryption (default)\n"
        "\t\t  medium - Encryption without MITM protection\n"
        "\t\t  high   - Encryption with MITM protection\n"
        "\t-t, --type <type>\t\tSet source address type:\n"
        "\t\t  public - Public address (default)\n"
        "\t\t  random - Random address\n"
        "\t-p, --phy <phy>\t\t\tSet PHY type:\n"
        "\t\t  1m     - LE 1M PHY (default)\n"
        "\t\t  2m     - LE 2M PHY\n"
        "\t\t  coded  - LE Coded PHY (extended range)\n"
        "\t-M, --hci-monitor\t\tEnable HCI monitoring\n"
        "\t-h, --help\t\t\tDisplay this help message\n");

    printf("\nExamples:\n"
        "\trkbt_gatt_server -n MyDevice     # Set advertising name to MyDevice\n"
        "\trkbt_gatt_server -a off          # Disable advertising\n"
        "\trkbt_gatt_server --uuid xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx\n"
        "                                   # Set service UUID\n"
        "\trkbt_gatt_server --uuid-write xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx\n"
        "                                   # Set write characteristic UUID\n"
        "\trkbt_gatt_server --uuid-notify xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx\n"
        "                                   # Set notify characteristic UUID\n"
        "\trkbt_gatt_server -s high -m 512  # High security with MTU 512\n"
        "\trkbt_gatt_server -t random       # Random address\n"
        "\trkbt_gatt_server -p 1m           # Use LE 1M PHY for higher speed\n"
        "\trkbt_gatt_server -p 2m           # Use LE 2M PHY for higher speed\n"
        "\trkbt_gatt_server -p 125k         # Use LE Coded PHY S=8 for maximum range\n"
        "\trkbt_gatt_server -p 500k         # Use LE Coded PHY S=2 for extended range\n");
}

static void parse_options(int argc, char *argv[])
{
    int i;
    int opt;
    int option_index = 0;

    PRLOG("Parsing command line options:\n");
    PRLOG("  argc = %d\n", argc);
    for (i = 0; i < argc; i++) {
        PRLOG("  argv[%d] = %s\n", i, argv[i]);
    }

    while ((opt = getopt_long(argc, argv, "+h:n:a:u:m:s:t:p:M", main_options, &option_index)) != -1) {
        PRLOG("Processing option: %c (optarg: %s)\n", opt, optarg ? optarg : "NULL");
        switch (opt) {
        case 'n':
            g_config.adv_name = optarg;
            break;
        case 'a':
            if (!strcmp(optarg, "off"))
                g_config.enable_advertising = false;
            else if (!strcmp(optarg, "on"))
                g_config.enable_advertising = true;
            else {
                PRLOG("ERROR: Invalid advertising option: %s\n", optarg);
                exit(EXIT_FAILURE);
            }
            break;
        case 'u':
            if (strlen(optarg) != 36) {
                PRLOG("ERROR: UUID must be in format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx\n");
                exit(EXIT_FAILURE);
            }
            g_config.trans_service_uuid = strdup(optarg);
            break;
        case 0: // Handle long options without short equivalents
            if (strcmp(main_options[option_index].name, "uuid-write") == 0) {
                if (strlen(optarg) != 36) {
                    PRLOG("ERROR: Write UUID must be in format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx\n");
                    exit(EXIT_FAILURE);
                }
                g_config.trans_write_uuid = strdup(optarg);
                PRLOG("Set uuid-write: %s\n", g_config.trans_write_uuid);
            } else if (strcmp(main_options[option_index].name, "uuid-notify") == 0) {
                if (strlen(optarg) != 36) {
                    PRLOG("ERROR: Notify UUID must be in format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx\n");
                    exit(EXIT_FAILURE);
                }
                g_config.trans_notify_uuid = strdup(optarg);
                PRLOG("Set uuid-notify: %s\n", g_config.trans_notify_uuid);
            }
            break;
        case 'm': {
            int arg_mtu = atoi(optarg);
            if (arg_mtu < 23) {
                PRLOG("ERROR: Invalid MTU: %d\n", arg_mtu);
                exit(EXIT_FAILURE);
            }
            g_config.mtu = arg_mtu;
            break;
        }
        case 's':
            if (!strcmp(optarg, "low"))
                g_config.sec_level = BT_SECURITY_LOW;
            else if (!strcmp(optarg, "medium"))
                g_config.sec_level = BT_SECURITY_MEDIUM;
            else if (!strcmp(optarg, "high"))
                g_config.sec_level = BT_SECURITY_HIGH;
            else {
                PRLOG("ERROR: Invalid security level: %s\n", optarg);
                exit(EXIT_FAILURE);
            }
            break;
        case 't':
            if (!strcmp(optarg, "random"))
                g_config.src_type = BDADDR_LE_RANDOM;
            else if (!strcmp(optarg, "public"))
                g_config.src_type = BDADDR_LE_PUBLIC;
            else {
                PRLOG("ERROR: Invalid source address type: %s\n", optarg);
                exit(EXIT_FAILURE);
            }
            break;
        case 'p':
            if (!strcmp(optarg, "1m")) {
                g_config.phy_type = MGMT_PHY_LE_1M_TX | MGMT_PHY_LE_1M_RX;
                PRLOG("Set PHY type: 1M (0x%08x)\n", g_config.phy_type);
            } else if (!strcmp(optarg, "2m")) {
                g_config.phy_type = MGMT_PHY_LE_2M_TX | MGMT_PHY_LE_2M_RX;
                PRLOG("Set PHY type: 2M (0x%08x)\n", g_config.phy_type);
            } else if (!strcmp(optarg, "coded")) {
                g_config.phy_type = MGMT_PHY_LE_CODED_TX | MGMT_PHY_LE_CODED_RX;
                PRLOG("Set PHY type: Coded (extended range) (0x%08x)\n", g_config.phy_type);
            } else {
                 PRLOG("ERROR: Invalid PHY type: %s (supported: 1m, 2m, coded)\n", optarg);
                exit(EXIT_FAILURE);
            }
            break;
        case 'M':
            g_config.hci_monitor = true;
            break;
        case 'h':
            usage();
            exit(EXIT_SUCCESS);
        default:
            usage();
            exit(EXIT_FAILURE);
        }
    }

    return;
}

static void cleanup_query_socket(void)
{
    // Stop query thread
    if (g_query_data.query_thread_running) {
        g_query_data.query_thread_running = false;
        pthread_cancel(g_query_data.query_thread);
        pthread_join(g_query_data.query_thread, NULL);
    }

    // Close query socket and remove socket file
    if (g_query_data.query_socket >= 0) {
        close(g_query_data.query_socket);
        g_query_data.query_socket = -1;
        unlink(BTGATT_QUERY_SOCK_PATH);
    }

    // Destroy query mutex
    pthread_mutex_destroy(&g_query_data.query_mutex);
}

static int l2cap_le_att_listen_and_accept(const bdaddr_t *src, int sec_level, 
                                        uint8_t src_type)
{
    struct sockaddr_l2 srcaddr, addr;
    int sk, nsk;
    socklen_t optlen;
    struct bt_security btsec;
    char ba[18];
    fd_set rfds;
    struct timeval tv;
    int max_fd, ret;

    sk = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
    if (sk < 0) {
        PRLOG("Failed to create L2CAP socket: %s\n", strerror(errno));
        return -1;
    }

    /* Set up source address */
    memset(&srcaddr, 0, sizeof(srcaddr));
    srcaddr.l2_family = AF_BLUETOOTH;
    srcaddr.l2_cid = htobs(BT_ATT_CID);
    srcaddr.l2_bdaddr_type = src_type;
    bacpy(&srcaddr.l2_bdaddr, src);

    if (bind(sk, (struct sockaddr *) &srcaddr, sizeof(srcaddr)) < 0) {
        PRLOG("Failed to bind L2CAP socket: %s\n", strerror(errno));
        goto fail;
    }

    /* Set the security level */
    memset(&btsec, 0, sizeof(btsec));
    btsec.level = sec_level;
    PRLOG("btsec.level: %d\n", btsec.level);
    if (setsockopt(sk, SOL_BLUETOOTH, BT_SECURITY, &btsec,
                            sizeof(btsec)) != 0) {
        PRLOG("Failed to set L2CAP security level: %s\n", strerror(errno));
        goto fail;
    }

    if (listen(sk, 10) < 0) {
        PRLOG("Listening on socket failed: %s\n", strerror(errno));
        goto fail;
    }

    PRLOG("Started listening on ATT channel. Waiting for connections\n");

    // Set socket to non-blocking mode
    int flags = fcntl(sk, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(sk, F_SETFL, flags | O_NONBLOCK);
    }

    // Wait for connection (HCI monitoring removed)
    while (g_running) {
        FD_ZERO(&rfds);
        FD_SET(sk, &rfds);
        max_fd = sk;
        
        // Set timeout to 1 second
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        ret = select(max_fd + 1, &rfds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            PRLOG("Select failed: %s\n", strerror(errno));
            goto fail;
        }
        
        // Check for incoming connection
        if (FD_ISSET(sk, &rfds)) {
            memset(&addr, 0, sizeof(addr));
            optlen = sizeof(addr);
            nsk = accept(sk, (struct sockaddr *) &addr, &optlen);
            if (nsk < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    continue;
                PRLOG("Accept failed: %s\n", strerror(errno));
                goto fail;
            }
            
            ba2str(&addr.l2_bdaddr, ba);
            PRLOG("Connect from %s\n", ba);
            close(sk);
            return nsk;
        }
    }

    // If we get here, we're shutting down
    close(sk);
    return -1;

fail:
    close(sk);
    return -1;
}

static void signal_cb(int signum, void *user_data)
{
    UNUSED(user_data);
    PRLOG("enter signal_cb...\n");

    switch (signum) {
    case SIGINT:
    case SIGTERM:
        g_running = false;
        cmd_rm_adv();
        cleanup_query_socket();
        mainloop_quit();
        btgatt_hci_deinit();
        break;
    default:
        break;
    }
}

// Thread function for handling query socket connections
static void *query_thread_func(void *user_data)
{
    UNUSED(user_data);
    char buffer[256];
    char cmd_buffer[128];
    struct sockaddr_un client_addr;
    socklen_t client_addr_len;
    ssize_t bytes_read;

    while (g_query_data.query_thread_running) {
        client_addr_len = sizeof(client_addr);
        int client_fd = accept(g_query_data.query_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        
        if (client_fd < 0) {
            if (errno == EINTR)
                continue;
            PRLOG("Query socket accept failed: %s\n", strerror(errno));
            break;
        }

        // Read command from client
        bytes_read = read(client_fd, cmd_buffer, sizeof(cmd_buffer) - 1);
        if (bytes_read <= 0) {
            PRLOG("Failed to read from query client: %s\n", strerror(errno));
            close(client_fd);
            continue;
        }
        cmd_buffer[bytes_read] = '\0';

        // Remove trailing newline if present
        if (bytes_read > 0 && cmd_buffer[bytes_read - 1] == '\n') {
            cmd_buffer[bytes_read - 1] = '\0';
        }

        // Lock to safely access shared data
        pthread_mutex_lock(&g_query_data.query_mutex);
        
        // Handle commands
        if (strcmp(cmd_buffer, "bdaddr") == 0) {
            // Get local Bluetooth address
            char addr[18];
            bdaddr_t zero_addr = {{0}};
            if (memcmp(&g_src_addr, &zero_addr, sizeof(bdaddr_t)) != 0) {
                ba2str(&g_src_addr, addr);
                snprintf(buffer, sizeof(buffer), "%s\n", addr);
            } else {
                snprintf(buffer, sizeof(buffer), "Error: Failed to get Bluetooth address\n");
            }
        } else {
            // Invalid command
            snprintf(buffer, sizeof(buffer), "Error: Invalid command - '%s'.\n"
                     "Available commands: bdaddr\n", cmd_buffer);
        }
        
        pthread_mutex_unlock(&g_query_data.query_mutex);

        // Send response
        if (write(client_fd, buffer, strlen(buffer)) < 0) {
            PRLOG("Failed to write to query client: %s\n", strerror(errno));
        }
        close(client_fd);
    }

    return NULL;
}

// Initialize query socket for status requests
static int init_query_socket(void)
{
    struct sockaddr_un addr;
    int fd;

    // Initialize query data structure
    g_query_data.query_socket = -1;
    g_query_data.query_thread_running = false;

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        PRLOG("Failed to create query socket: %s\n", strerror(errno));
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, BTGATT_QUERY_SOCK_PATH, sizeof(addr.sun_path) - 1);

    unlink(BTGATT_QUERY_SOCK_PATH);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        PRLOG("Failed to bind query socket: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    if (listen(fd, 1) < 0) {
        PRLOG("Failed to listen on query socket: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    // Set socket permissions to allow other processes to connect
    if (chmod(BTGATT_QUERY_SOCK_PATH, 0666) < 0) {
        PRLOG("Failed to set query socket permissions: %s\n", strerror(errno));
        close(fd);
        unlink(BTGATT_QUERY_SOCK_PATH);
        return -1;
    }

    g_query_data.query_socket = fd;
    
    // Initialize mutex
    if (pthread_mutex_init(&g_query_data.query_mutex, NULL) != 0) {
        PRLOG("Failed to initialize query mutex\n");
        close(fd);
        unlink(BTGATT_QUERY_SOCK_PATH);
        return -1;
    }
    
    // Start query thread
    g_query_data.query_thread_running = true;
    if (pthread_create(&g_query_data.query_thread, NULL, query_thread_func, NULL) != 0) {
        PRLOG("Failed to create query thread\n");
        pthread_mutex_destroy(&g_query_data.query_mutex);
        close(fd);
        unlink(BTGATT_QUERY_SOCK_PATH);
        return -1;
    }

    PRLOG("Query socket created and listening on %s\n", BTGATT_QUERY_SOCK_PATH);
    return 0;
}

#include <sys/syscall.h>
int gettid(void)
{
    return syscall(SYS_gettid);
}

static void *mainloop_thread_func(void *data)
{
    UNUSED(data);

    PRLOG("mainloop_thread_func tid: %d\n", gettid());
    mainloop_run_with_signal(signal_cb, NULL);

    return NULL;
}

static void gap_device_name_read_cb(struct gatt_db_attribute *attrib,
                    unsigned int id, uint16_t offset,
                    uint8_t opcode, struct bt_att *att,
                    void *user_data)
{
    UNUSED(opcode); UNUSED(att);
    struct server *server = user_data;
    uint8_t error = 0;
    size_t len = 0;
    const uint8_t *value = NULL;

    PRLOG("GAP Device Name Read called\n");

    len = server->name_len;

    if (offset > len) {
        error = BT_ATT_ERROR_INVALID_OFFSET;
        goto done;
    }

    len -= offset;
    value = len ? &server->device_name[offset] : NULL;

done:
    gatt_db_attribute_read_result(attrib, id, error, value, len);
}

static void gap_device_name_write_cb(struct gatt_db_attribute *attrib,
                    unsigned int id, uint16_t offset,
                    const uint8_t *value, size_t len,
                    uint8_t opcode, struct bt_att *att,
                    void *user_data)
{
    UNUSED(opcode); UNUSED(att);
    struct server *server = user_data;
    uint8_t error = 0;

    PRLOG("GAP Device Name Write called\n");

    /* If the value is being completely truncated, clean up and return */
    if (!(offset + len)) {
        free(server->device_name);
        server->device_name = NULL;
        server->name_len = 0;
        goto done;
    }

    /* Implement this as a variable length attribute value. */
    if (offset > server->name_len) {
        error = BT_ATT_ERROR_INVALID_OFFSET;
        goto done;
    }

    if (offset + len != server->name_len) {
        uint8_t *name;

        name = realloc(server->device_name, offset + len);
        if (!name) {
            error = BT_ATT_ERROR_INSUFFICIENT_RESOURCES;
            goto done;
        }

        server->device_name = name;
        server->name_len = offset + len;
    }

    if (value)
        memcpy(server->device_name + offset, value, len);

done:
    gatt_db_attribute_write_result(attrib, id, error);
}

static void gap_device_name_ext_prop_read_cb(struct gatt_db_attribute *attrib,
                    unsigned int id, uint16_t offset,
                    uint8_t opcode, struct bt_att *att,
                    void *user_data)
{
    UNUSED(offset); UNUSED(opcode); UNUSED(att); UNUSED(user_data);
    uint8_t value[2];

    PRLOG("Device Name Extended Properties Read called\n");

    value[0] = BT_GATT_CHRC_EXT_PROP_RELIABLE_WRITE;
    value[1] = 0;

    gatt_db_attribute_read_result(attrib, id, 0, value, sizeof(value));
}

static void gatt_service_changed_cb(struct gatt_db_attribute *attrib,
                    unsigned int id, uint16_t offset,
                    uint8_t opcode, struct bt_att *att,
                    void *user_data)
{
    UNUSED(offset); UNUSED(opcode); UNUSED(att); UNUSED(user_data);

    PRLOG("Service Changed Read called\n");
    gatt_db_attribute_read_result(attrib, id, 0, NULL, 0);
}

static void gatt_svc_chngd_ccc_read_cb(struct gatt_db_attribute *attrib,
                    unsigned int id, uint16_t offset,
                    uint8_t opcode, struct bt_att *att,
                    void *user_data)
{
    UNUSED(offset); UNUSED(opcode); UNUSED(att);
    struct server *server = user_data;
    uint8_t value[2];

    PRLOG("Service Changed CCC Read called\n");

    value[0] = server->svc_chngd_enabled ? 0x02 : 0x00;
    value[1] = 0x00;

    gatt_db_attribute_read_result(attrib, id, 0, value, sizeof(value));
}

static void gatt_svc_chngd_ccc_write_cb(struct gatt_db_attribute *attrib,
                    unsigned int id, uint16_t offset,
                    const uint8_t *value, size_t len,
                    uint8_t opcode, struct bt_att *att,
                    void *user_data)
{
    UNUSED(opcode); UNUSED(att); UNUSED(user_data);
    struct server *server = user_data;
    uint8_t ecode = 0;

    PRLOG("Service Changed CCC Write called\n");

    if (!value || len != 2) {
        ecode = BT_ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LEN;
        goto done;
    }

    if (offset) {
        ecode = BT_ATT_ERROR_INVALID_OFFSET;
        goto done;
    }

    if (value[0] == 0x00)
        server->svc_chngd_enabled = false;
    else if (value[0] == 0x02)
        server->svc_chngd_enabled = true;
    else
        ecode = 0x80;

    PRLOG("Service Changed Enabled: %s\n",
                server->svc_chngd_enabled ? "true" : "false");

done:
    gatt_db_attribute_write_result(attrib, id, ecode);
}

static void populate_gap_service(struct server *server)
{
    bt_uuid_t uuid;
    struct gatt_db_attribute *service, *tmp;
    uint16_t appearance;

    /* Add the GAP service */
    bt_uuid16_create(&uuid, UUID_GAP);
    service = gatt_db_add_service(server->db, &uuid, true, 6);

    /*
     * Device Name characteristic. Make the value dynamically read and
     * written via callbacks.
     */
    bt_uuid16_create(&uuid, GATT_CHARAC_DEVICE_NAME);
    gatt_db_service_add_characteristic(service, &uuid,
                    BT_ATT_PERM_READ | BT_ATT_PERM_WRITE,
                    BT_GATT_CHRC_PROP_READ |
                    BT_GATT_CHRC_PROP_EXT_PROP,
                    gap_device_name_read_cb,
                    gap_device_name_write_cb,
                    server);

    bt_uuid16_create(&uuid, GATT_CHARAC_EXT_PROPER_UUID);
    gatt_db_service_add_descriptor(service, &uuid, BT_ATT_PERM_READ,
                    gap_device_name_ext_prop_read_cb,
                    NULL, server);

    /*
     * Appearance characteristic. Reads and writes should obtain the value
     * from the database.
     */
    bt_uuid16_create(&uuid, GATT_CHARAC_APPEARANCE);
    tmp = gatt_db_service_add_characteristic(service, &uuid,
                            BT_ATT_PERM_READ,
                            BT_GATT_CHRC_PROP_READ,
                            NULL, NULL, server);

    /*
     * Write the appearance value to the database, since we're not using a
     * callback.
     */
    put_le16(128, &appearance);
    gatt_db_attribute_write(tmp, 0, (void *) &appearance,
                            sizeof(appearance),
                            BT_ATT_OP_WRITE_REQ,
                            NULL, confirm_write,
                            NULL);

    gatt_db_service_set_active(service, true);
}

static void populate_gatt_service(struct server *server)
{
    bt_uuid_t uuid;
    struct gatt_db_attribute *service, *svc_chngd;

    /* Add the GATT service */
    bt_uuid16_create(&uuid, UUID_GATT);
    service = gatt_db_add_service(server->db, &uuid, true, 4);

    bt_uuid16_create(&uuid, GATT_CHARAC_SERVICE_CHANGED);
    svc_chngd = gatt_db_service_add_characteristic(service, &uuid,
            BT_ATT_PERM_READ,
            BT_GATT_CHRC_PROP_READ | BT_GATT_CHRC_PROP_INDICATE,
            gatt_service_changed_cb,
            NULL, server);
    server->gatt_svc_chngd_handle = gatt_db_attribute_get_handle(svc_chngd);

    bt_uuid16_create(&uuid, GATT_CLIENT_CHARAC_CFG_UUID);
    gatt_db_service_add_descriptor(service, &uuid,
                BT_ATT_PERM_READ | BT_ATT_PERM_WRITE,
                gatt_svc_chngd_ccc_read_cb,
                gatt_svc_chngd_ccc_write_cb, server);

    gatt_db_service_set_active(service, true);
}

static void populate_db(struct server *server)
{
    populate_gap_service(server);
    populate_gatt_service(server);
    populate_transfer_service(server);
}

static void server_destroy(struct server *server)
{
    // Remove socket watch from mainloop
    if (server->socket_watch_id > 0) {
        mainloop_remove_fd(server->socket_watch_id);
        server->socket_watch_id = 0;
    }
    
    // Close client socket if connected
    if (server->client_socket > 0) {
        close(server->client_socket);
        server->client_socket = -1;
    }
    
    // Close listening socket and remove socket file
    if (server->local_socket > 0) {
        close(server->local_socket);
        server->local_socket = -1;
        unlink(BTGATT_TRANS_SOCK_PATH);
    }
    
    bt_gatt_server_unref(server->gatt);
    gatt_db_unref(server->db);
    free(server->device_name);

    // Free UUID strings if they were dynamically allocated
    if (g_config.trans_service_uuid) {
        free((void *)g_config.trans_service_uuid);
        g_config.trans_service_uuid = NULL;
    }
    if (g_config.trans_write_uuid) {
        free((void *)g_config.trans_write_uuid);
        g_config.trans_write_uuid = NULL;
    }
    if (g_config.trans_notify_uuid) {
        free((void *)g_config.trans_notify_uuid);
        g_config.trans_notify_uuid = NULL;
    }
}

// Callback functions
static void att_disconnect_cb(int err, void *user_data)
{
    struct server *server = user_data;
    PRLOG("ATT DISCONNECT CALLBACK: Device disconnected with error: %s (errno=%d)\n", strerror(err), err);

    g_config.is_connected = false;
    server_destroy(server);

    if (!g_running) {
        PRLOG("Quitting...\n");
        mainloop_quit();
    } else {
        PRLOG("Waiting for new connection...\n");
        g_recon = true;
    }
}

static void debug_cb(const char *str, void *user_data)
{
    const char *prefix = user_data;
    printf("%s%s\n", prefix, str);
}

static struct server *server_create(int fd, uint16_t mtu)
{
    struct server *server;
    size_t name_len = strlen(test_device_name);

    PRLOG("Creating GATT server (fd=%d, mtu=%d)\n", fd, mtu);
    server = new0(struct server, 1);
    if (!server) {
        PRLOG("ERROR: Failed to allocate memory for server\n");
        return NULL;
    }

    server->att = bt_att_new(fd, false);
    if (!server->att) {
        PRLOG("ERROR: Failed to initialze ATT transport layer\n");
        goto fail;
    }

    if (!bt_att_set_close_on_unref(server->att, true)) {
        PRLOG("ERROR: Failed to set up ATT transport layer\n");
        goto fail;
    }

    if (!bt_att_register_disconnect(server->att, att_disconnect_cb, server,
                                    NULL)) {
        PRLOG("ERROR: Failed to set ATT disconnect handler\n");
        goto fail;
    }

    server->name_len = name_len + 1;
    server->device_name = malloc(name_len + 1);
    if (!server->device_name) {
        PRLOG("ERROR: Failed to allocate memory for device name\n");
        goto fail;
    }

    memcpy(server->device_name, test_device_name, name_len);
    server->device_name[name_len] = '\0';

    server->fd = fd;
    server->db = gatt_db_new();
    if (!server->db) {
        PRLOG("ERROR: Failed to create GATT database\n");
        goto fail;
    }

    server->gatt = bt_gatt_server_new(server->db, server->att, mtu, 0);
    if (!server->gatt) {
        PRLOG("ERROR: Failed to create GATT server\n");
        goto fail;
    }

    if (1) {
        bt_att_set_debug(server->att, BT_ATT_DEBUG_VERBOSE, debug_cb, "[ATT] ", NULL);
        bt_gatt_server_set_debug(server->gatt, debug_cb, "[GATT Server] ", NULL);
    }

    /* bt_gatt_server already holds a reference */
    populate_db(server);
    
    PRLOG("GATT server created successfully\n");
    return server;

fail:
    if (server->db)
        gatt_db_unref(server->db);
    if (server->device_name)
        free(server->device_name);
    if (server->att)
        bt_att_unref(server->att);
    free(server);

    return NULL;
}

int main(int argc, char *argv[])
{
    int fd = -1;
    struct server *server = NULL;

    // Parse command line options and store in global config
    parse_options(argc, argv);

    // Initialize HCI module with complete setup
    PRLOG("Initializing HCI module...\n");
    if (btgatt_hci_init(&g_src_addr) < 0) {
        PRLOG("ERROR: Failed to initialize HCI module\n");
        return EXIT_FAILURE;
    }

    // Debug tool
    #if 0
    PRLOG("start btmon...\n");
    system("btmon -i hci0 -w /data/btmon_debug.log >/data/btmon_debug.txt &");
    sleep(1);
    #endif

    // Init mainloop
    PRLOG("init mainloop...\n");
    mainloop_init();

    // Configure g_mgmt
    PRLOG("configure mgmt...\n");
    g_mgmt = mgmt_new_default();
    if (!g_mgmt) {
        PRLOG("ERROR: Unable to open mgmt_socket\n");
        return EXIT_FAILURE;
    }
    if (getenv("MGMT_DEBUG")) {
        mgmt_set_debug(g_mgmt, debug_cb, "[MGMT] ", NULL);
    }
    register_mgmt_callbacks(g_mgmt, mgmt_index);

    // Initialize query socket
    PRLOG("initializing query socket...\n");
    if (init_query_socket() < 0) {
        PRLOG("ERROR: Failed to initialize query socket\n");
        return EXIT_FAILURE;
    }

    // Create mainloop thread
    PRLOG("create mainloop thread...\n");
    pthread_t mainloop_thread;
    pthread_create(&mainloop_thread, NULL, mainloop_thread_func, NULL);

    // Add commands
    PRLOG("add commands...\n");
    cmd_setting(MGMT_OP_SET_POWERED, 0);
    cmd_privacy();
    cmd_setting(MGMT_OP_SET_POWERED, 1);
    cmd_setting(MGMT_OP_SET_BONDABLE, 1);
    cmd_io_cap();
    cmd_setting(MGMT_OP_SET_LE, 1);
    cmd_rm_adv();
    if (g_config.enable_advertising) {
        cmd_add_adv();
    } else {
        PRLOG("advertising disabled by command line option\n");
    }
#ifdef KEY
    cmd_load_long_term_keys();
    cmd_load_irks();
#endif

reconnect:
    // Init btgatt_hci variables
    btgatt_hci_clean_connection();

    // Wait connection from client
    PRLOG("Waiting for client connection (sec_level=%d, src_type=%d)\n",
          g_config.sec_level, g_config.src_type);
    g_recon = false;
    fd = l2cap_le_att_listen_and_accept(&g_src_addr, g_config.sec_level, g_config.src_type);
    if (fd < 0) {
        PRLOG("ERROR: Failed to accept L2CAP ATT connection, fd=%d, errno=%d (%s)\n",
              fd, errno, strerror(errno));
        PRLOG("Will restart advertising and wait for new connection...\n");
        
        // Don't exit, instead go back to reconnect to restart advertising
        goto reconnect;
    }
    PRLOG("L2CAP ATT connection accepted successfully, fd=%d\n", fd);

    // Create server
    server = server_create(fd, g_config.mtu);
    if (!server) {
        close(fd);
        goto reconnect;
    }
    
    // Wait for server quit
    PRLOG("running GATT server...\n");
    while (g_running) {
        // Brief sleep to avoid excessive CPU usage
        usleep(100000); // 100ms
        
        if (g_recon) {
            PRLOG("Reconnection requested\n");
            goto reconnect;
        }
    }

    // Cleanup
    PRLOG("shutting down...\n");
    server_destroy(server);
    cleanup_query_socket();
    btgatt_hci_deinit();

    PRLOG("gatt server exited.\n");
    return EXIT_SUCCESS;
}