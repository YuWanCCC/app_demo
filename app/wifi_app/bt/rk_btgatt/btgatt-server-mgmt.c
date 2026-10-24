// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  BlueZ - Bluetooth protocol stack for Linux
 *  Management Interface Handler Implementation
 */

#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "btgatt-server.h"

// PHY configuration structure
struct phy_config {
    const char *name;
    uint32_t tx_phys;
    uint32_t rx_phys;
    uint16_t phy_opts;
};

// PHY setting state
static const char *settings_str[] = {
                "powered",
                "connectable",
                "fast-connectable",
                "discoverable",
                "bondable",
                "link-security",
                "ssp",
                "br/edr",
                "hs",
                "le",
                "advertising",
                "secure-conn",
                "debug-keys",
                "privacy",
                "configuration",
                "static-addr",
                "phy-configuration",
                "wide-band-speech",
                "cis-central",
                "cis-peripheral",
                "iso-broadcaster",
                "sync-receiver"
};

static const char *typestr(uint8_t type)
{
    static const char *str[] = { "BR/EDR", "LE Public", "LE Random" };

    if (type <= BDADDR_LE_RANDOM)
        return str[type];

    return "(unknown)";
}

/* Wrapper to get the index and opcode to the response callback */
struct command_data {
    uint16_t id;
    uint16_t op;
    void (*callback) (uint16_t id, uint16_t op, uint8_t status,
                    uint16_t len, const void *param);
};

static void cmd_rsp(uint8_t status, uint16_t len, const void *param,
                            void *user_data)
{
    struct command_data *data = user_data;
    data->callback(data->op, data->id, status, len, param);
}

static unsigned int send_cmd(struct mgmt *mgmt, uint16_t op, uint16_t id,
                uint16_t len, const void *param,
                void (*cb)(uint16_t id, uint16_t op,
                        uint8_t status, uint16_t len,
                        const void *param))
{
    struct command_data *data;
    unsigned int send_id;

    data = new0(struct command_data, 1);
    if (!data)
        return 0;

    data->id = id;
    data->op = op;
    data->callback = cb;

    send_id = mgmt_send(mgmt, op, id, len, param, cmd_rsp, data, free);
    if (send_id == 0)
        free(data);

    return send_id;
}

static const char *settings2str(uint32_t settings)
{
    static char str[256];
    unsigned i;
    int off;

    off = 0;
    str[0] = '\0';

    for (i = 0; i < NELEM(settings_str); i++) {
        if ((settings & (1 << i)) != 0)
            off += snprintf(str + off, sizeof(str) - off, "%s ",
                            settings_str[i]);
    }

    return str;
}

static void setting_rsp(uint16_t op, uint16_t id, uint8_t status, uint16_t len,
                            const void *param)
{
    const uint32_t *rp = param;

    if (status != 0) {
        PRLOG("ERROR: %s for hci%u failed with status 0x%02x (%s)\n",
            mgmt_opstr(op), id, status, mgmt_errstr(status));
        return;
    }

    if (len < sizeof(*rp)) {
        PRLOG("ERROR: Too small %s response (%u bytes)\n",
                            mgmt_opstr(op), len);
        return;
    }

    PRLOG("hci%u %s:%d complete, settings: %s\n", id, mgmt_opstr(op), op,
                        settings2str(get_le32(rp)));
    return;
}

void cmd_setting(uint16_t op, uint8_t val)
{
    int index;

    index = mgmt_index;
    if (index == MGMT_INDEX_NONE)
        index = 0;

    if (send_cmd(g_mgmt, op, index, sizeof(val), &val, setting_rsp) == 0) {
        PRLOG("ERROR: Unable to send %s cmd\n", mgmt_opstr(op));
        return;
    } else {
        PRLOG("send %s cmd ok\n", mgmt_opstr(op));
    }
}

static void add_adv_rsp(uint8_t status, uint16_t len, const void *param,
                                void *user_data)
{
    UNUSED(user_data);
    const struct mgmt_rp_add_advertising *rp = param;

    if (status != 0) {
        PRLOG("ERROR: Add Advertising failed with status 0x%02x (%s)\n",
                        status, mgmt_errstr(status));
        return;
    }

    if (len != sizeof(*rp)) {
        PRLOG("ERROR: Invalid Add Advertising response length (%u)\n", len);
        return;
    }

    PRLOG("Instance added: %u\n", rp->instance);

    return;
}

static void rm_adv_rsp(uint8_t status, uint16_t len, const void *param,
                                void *user_data)
{
    UNUSED(user_data);
    const struct mgmt_rp_remove_advertising *rp = param;

    if (status != 0) {
        PRLOG("Remove Advertising failed - %s.\n", mgmt_errstr(status));
        return;
    }

    if (len != sizeof(*rp)) {
        PRLOG("Invalid Remove Advertising response length (%u).\n", len);
        return;
    }

    PRLOG("Instance removed: %u\n", rp->instance);
}

static void load_irks_complete(uint8_t status, uint16_t length,
                    const void *param, void *user_data)
{
    UNUSED(length); UNUSED(param); UNUSED(user_data);
    if (status == MGMT_STATUS_UNKNOWN_COMMAND) {
        PRLOG("Load IRKs failed: Kernel doesn't support LE Privacy\n");
        return;
    }

    if (status != MGMT_STATUS_SUCCESS) {
        PRLOG("Failed to load IRKs for hci0: %s (0x%02x)\n", mgmt_errstr(status), status);
        return;
    }

    PRLOG("IRKs loaded for hci0\n");
}

static void ltks_rsp(uint8_t status, uint16_t length, const void *param,
                            void *user_data)
{
    UNUSED(length); UNUSED(param); UNUSED(user_data);
    if (status != 0)
        PRLOG("ERROR: Load keys failed with status 0x%02x (%s)\n",
                        status, mgmt_errstr(status));
    else
        PRLOG("Long term keys successfully loaded\n");
}

void cmd_load_long_term_keys(void)
{
    uint16_t index;
    int cp_size;

    index = mgmt_index;
    if (index == MGMT_INDEX_NONE)
        index = 0;

    struct mgmt_cp_load_long_term_keys *cp;
    struct mgmt_ltk_info *key;
    struct mgmt_ev_new_long_term_key ltk_store;

    cp_size = sizeof(*cp) + sizeof(*key);

    cp = malloc(cp_size);

    FILE *fp = fopen("/data/ltk_store.bin", "rb");
    if (fp) {
        fread(&ltk_store, sizeof(ltk_store), 1, fp);
        fclose(fp);
        PRLOG("read LTK from /data/ltk_store.bin\n\n");
    } else {
        PRLOG("failed to open /data/ltk_store.bin: %s\n", strerror(errno));
    }

    key = &ltk_store.key;

    cp->key_count = 1;
    cp->keys[0] = *key;

    if (mgmt_send(g_mgmt, MGMT_OP_LOAD_LONG_TERM_KEYS, index, cp_size, cp,
                        ltks_rsp, NULL, NULL) == 0) {
        PRLOG("ERROR: Unable to send load_ltks cmd\n");
    }

    free(cp);
}

void cmd_load_irks(void)
{
    uint16_t index;
    struct mgmt_cp_load_irks *cp;
    struct mgmt_ev_new_irk irk_received;
    struct mgmt_irk_info *irk;

    index = mgmt_index;
    if (index == MGMT_INDEX_NONE)
        index = 0;

    FILE *fp = fopen("/data/irk_received.bin", "rb");
    if (fp) {
        fread(&irk_received, sizeof(irk_received), 1, fp);
        fclose(fp);
        PRLOG("read IRK from /data/irk_received.bin\n");
    }

    cp = malloc(sizeof(*cp) + sizeof(*irk));
    cp->irk_count = 1;
    cp->irks[0] = irk_received.key;

    if (mgmt_send(g_mgmt, MGMT_OP_LOAD_IRKS, index,
        sizeof(*cp) + sizeof(*irk), cp, load_irks_complete, NULL, NULL) == 0) {
        PRLOG("ERROR: Unable to send load_irks cmd\n");
    }
    free(cp);
}

static void io_cap_rsp(uint8_t status, uint16_t length, const void *param,
                            void *user_data)
{
    UNUSED(length); UNUSED(param); UNUSED(user_data);
    if (status != 0)
        PRLOG("ERROR: Could not set IO Capability with status 0x%02x (%s)\n",
                        status, mgmt_errstr(status));
    else
        PRLOG("IO Capabilities successfully set\n");

}

void cmd_io_cap(void)
{
    struct mgmt_cp_set_io_capability cp;
    uint8_t cap;
    uint16_t index;

    index = mgmt_index;
    if (index == MGMT_INDEX_NONE)
        index = 0;

    cap = 3;
    memset(&cp, 0, sizeof(cp));
    cp.io_capability = cap;

    if (mgmt_send(g_mgmt, MGMT_OP_SET_IO_CAPABILITY, index, sizeof(cp), &cp,
                    io_cap_rsp, NULL, NULL) == 0) {
        PRLOG("ERROR: Unable to send set-io-cap cmd\n");
    }
}

void cmd_privacy(void)
{
    struct mgmt_cp_set_privacy cp;
    uint16_t index;

    cp.privacy = 0;

    index = mgmt_index;
    if (index == MGMT_INDEX_NONE)
        index = 0;

    FILE *irk_file = fopen("/data/irk.bin", "rb");
    if (irk_file && fread(cp.irk, sizeof(cp.irk), 1, irk_file) == 1) {
        PRLOG("Read IRK from /data/irk.bin\n");
        fclose(irk_file);
    } else {
        if (irk_file)
            fclose(irk_file);
            
        int fd = open("/dev/urandom", O_RDONLY);
        if (fd < 0) {
            PRLOG("ERROR: open file /dev/uarandom error: %s\n", strerror(errno));
            return;
        }
        
        if (read(fd, cp.irk, sizeof(cp.irk)) != sizeof(cp.irk)) {
            PRLOG("ERROR: read file /dev/urandom error\n");
            close(fd);
            return;
        }
        close(fd);
        
        FILE *save_file = fopen("/data/irk.bin", "wb");
        if (save_file) {
            fwrite(cp.irk, sizeof(cp.irk), 1, save_file);
            fclose(save_file);
            PRLOG("saved IRK to /data/irk.bin\n");
        }
    }

    if (send_cmd(g_mgmt, MGMT_OP_SET_PRIVACY, index, sizeof(cp), &cp,
                            setting_rsp) == 0) {
        PRLOG("ERROR: Unable to send Set Privacy command\n");
    }
}

void cmd_add_adv(void)
{
    struct mgmt_cp_add_advertising *cp = NULL;
    uint8_t *adv_data = NULL, *scan_rsp = NULL;
    size_t adv_len = 0, scan_rsp_len = 0;
    size_t cp_len;
    size_t uuid_bytes = 0;
    uint16_t timeout = 0, duration = 0;
    uint8_t instance;
    uint32_t flags = 0;
    uint16_t index;

    PRLOG("cmd_add_adv with name: %s\n", g_config.adv_name);

    //local name
    adv_len = strlen(g_config.adv_name) + 2;
    adv_data = malloc0(adv_len);
    if (!adv_data)
        goto done;
    adv_data[0] = adv_len - 1;
    adv_data[1] = 9;
    memcpy(&adv_data[2], g_config.adv_name, adv_len - 2);

    instance = 1;

    index = 0;

    cp_len = sizeof(*cp) + uuid_bytes + adv_len + scan_rsp_len;
    cp = malloc0(cp_len);
    if (!cp)
        goto done;

    cp->instance = instance;

    flags = MGMT_ADV_FLAG_CONNECTABLE | MGMT_ADV_FLAG_DISCOV;
    put_le32(flags, &cp->flags);

    put_le16(timeout, &cp->timeout);

    put_le16(duration, &cp->duration);

    cp->adv_data_len = adv_len + uuid_bytes;
    cp->scan_rsp_len = scan_rsp_len;

    if (adv_len)
        memcpy(cp->data + uuid_bytes, adv_data, adv_len);

    if (scan_rsp_len)
        memcpy(cp->data + uuid_bytes + adv_len, scan_rsp, scan_rsp_len);

    if (!mgmt_send(g_mgmt, MGMT_OP_ADD_ADVERTISING, index, cp_len, cp,
                        add_adv_rsp, NULL, NULL)) {
        PRLOG("ERROR: unable to send \"Add Advertising\" command");
        goto done;
    } else {
        PRLOG("send Add Advertising command ok\n");
    }

done:
    free(adv_data);
    free(scan_rsp);
    free(cp);
}

void cmd_rm_adv()
{
    struct mgmt_cp_remove_advertising cp;
    uint8_t instance;
    uint16_t index;

    instance = 1;

    index = mgmt_index;
    if (index == MGMT_INDEX_NONE)
        index = 0;

    memset(&cp, 0, sizeof(cp));

    cp.instance = instance;

    if (!mgmt_send(g_mgmt, MGMT_OP_REMOVE_ADVERTISING, index, sizeof(cp), &cp,
                        rm_adv_rsp, NULL, NULL)) {
        PRLOG("ERROR: Unable to send \"Remove Advertising\" command\n");
    }
}

/**
 * Handle device connection event
 * 
 * This function is called when a device connects to the GATT server.
 * It uses the connection handle obtained from HCI events, configures PHY settings if requested,
 * and updates the global connection state.
 * 
 * @param index Adapter index
 * @param len Length of the event data
 * @param param Event data
 * @param user_data User data passed during registration
 */
static void connected(uint16_t index, uint16_t len, const void *param,
                            void *user_data)
{
    UNUSED(index); UNUSED(user_data);
    const struct mgmt_ev_device_connected *ev = param;
    uint16_t eir_len;
    char addr[18];
    
    if (len < sizeof(*ev) || 
        (eir_len = get_le16(&ev->eir_len), len != sizeof(*ev) + eir_len)) {
        PRLOG("Invalid connected event length\n");
        return;
    }

    ba2str(&ev->addr.bdaddr, addr);
    
    if (g_config.is_connected) {
        PRLOG("Device %s tried to connect, but another device is already connected\n", addr);
        return;
    }
    
    // Set connection state
    g_config.is_connected = true;
    PRLOG("Device %s connected\n", addr);
}

static void disconnected(uint16_t index, uint16_t len, const void *param,
                            void *user_data)
{
    UNUSED(user_data);
    const struct mgmt_ev_device_disconnected *ev = param;
    char addr[18];
    uint8_t reason;

    if (len < sizeof(struct mgmt_addr_info)) {
        PRLOG("ERROR: Invalid disconnected event length (%u bytes)\n", len);
        return;
    }

    // Reset connection state
    g_config.is_connected = false;
    PRLOG("Device disconnected, connection state reset\n");

    if (len < sizeof(*ev))
        reason = MGMT_DEV_DISCONN_UNKNOWN;
    else
        reason = ev->reason;

    ba2str(&ev->addr.bdaddr, addr);
    PRLOG("hci%u %s type %s disconnected with reason %u\n",
            index, addr, typestr(ev->addr.type), reason);
}

static void conn_failed(uint16_t index, uint16_t len, const void *param,
                            void *user_data)
{
    UNUSED(user_data);
    const struct mgmt_ev_connect_failed *ev = param;
    char addr[18];

    if (len != sizeof(*ev)) {
        PRLOG("ERROR: Invalid connect_failed event length (%u bytes)\n", len);
        return;
    }

    ba2str(&ev->addr.bdaddr, addr);
    PRLOG("hci%u %s type %s connect failed (status 0x%02x, %s)\n",
            index, addr, typestr(ev->addr.type), ev->status,
            mgmt_errstr(ev->status));
}

static void new_settings(uint16_t index, uint16_t len,
                    const void *param, void *user_data)
{
    UNUSED(user_data);
    const uint32_t *ev = param;

    if (len < sizeof(*ev)) {
        PRLOG("ERROR: Too short new_settings event (%u)\n", len);
        return;
    }

    PRLOG("hci%u new_settings: %s\n", index, settings2str(get_le32(ev)));
}

static void flags_changed(uint16_t index, uint16_t len, const void *param,
                            void *user_data)
{
    UNUSED(user_data);
    const struct mgmt_ev_device_flags_changed *ev = param;
    char addr[18];

    if (len < sizeof(*ev)) {
        PRLOG("ERROR: Too small (%u bytes) %s event\n", len, __func__);
        return;
    }

    ba2str(&ev->addr.bdaddr, addr);
    PRLOG("hci%u device_flags_changed: %s (%s)\n", index, addr,
                            typestr(ev->addr.type));
    PRLOG("     supp: 0x%08x  curr: 0x%08x\n",
                    ev->supported_flags, ev->current_flags);
}

static void advertising_added(uint16_t index, uint16_t len,
                    const void *param, void *user_data)
{
    UNUSED(user_data);
    const struct mgmt_ev_advertising_added *ev = param;

    if (len < sizeof(*ev)) {
        PRLOG("ERROR: Too small (%u bytes) advertising_added event\n", len);
        return;
    }

    PRLOG("hci%u advertising_added: instance %u\n", index, ev->instance);
}

static void advertising_removed(uint16_t index, uint16_t len,
                    const void *param, void *user_data)
{
    UNUSED(user_data);
    const struct mgmt_ev_advertising_removed *ev = param;

    if (len < sizeof(*ev)) {
        PRLOG("ERROR: Too small (%u bytes) advertising_removed event\n", len);
        return;
    }

    PRLOG("hci%u advertising_removed: instance %u\n", index, ev->instance);
}

static void confirm_rsp(uint8_t status, uint16_t len, const void *param,
                            void *user_data)
{
    UNUSED(len); UNUSED(param); UNUSED(user_data);
    if (status != 0) {
        PRLOG("ERROR: User Confirm reply failed. status 0x%02x (%s)\n",
                        status, mgmt_errstr(status));
    }

    PRLOG("User Confirm Reply successful\n");
}


static int mgmt_confirm_reply(uint16_t index, const struct mgmt_addr_info *addr)
{
    struct mgmt_cp_user_confirm_reply cp;

    memset(&cp, 0, sizeof(cp));
    memcpy(&cp.addr, addr, sizeof(*addr));

    return mgmt_reply(g_mgmt, MGMT_OP_USER_CONFIRM_REPLY, index,
                sizeof(cp), &cp, confirm_rsp, NULL, NULL);
}

static void user_confirm(uint16_t index, uint16_t len, const void *param,
                            void *user_data)
{
    UNUSED(user_data);
    const struct mgmt_ev_user_confirm_request *ev = param;
    uint32_t val;
    char addr[18];

    if (len != sizeof(*ev)) {
        PRLOG("ERROR: Invalid user_confirm request length (%u)\n", len);
        return;
    }

    ba2str(&ev->addr.bdaddr, addr);
    val = get_le32(&ev->value);

    PRLOG("hci%u %s User Confirm %06u hint %u\n", index, addr,
                            val, ev->confirm_hint);

    if (ev->confirm_hint) {
        PRLOG("Accept pairing with %s yes\n", addr);
        mgmt_confirm_reply(index, &ev->addr);
    } else {
        PRLOG("Confirm value %06u for %s (yes/no)\n", val, addr);
        mgmt_confirm_reply(index, &ev->addr);
    }
}

static void new_long_term_key_callback(uint16_t index, uint16_t length,
                    const void *param, void *user_data)
{
    UNUSED(index); UNUSED(user_data);
    char dst[18];
    char key_str[33];
    int i;
    struct mgmt_ev_new_long_term_key *ltk_store = (struct mgmt_ev_new_long_term_key *)param;

    PRLOG("new_long_term_key_callback: %d %d\n\n", length, sizeof(*ltk_store));

    FILE *fp = fopen("/data/ltk_store.bin", "wb");
    if (fp) {
        fwrite(param, length, 1, fp);
        fclose(fp);
        PRLOG("Wrote LTK to /data/ltk_store.bin\n");
    } else {
        PRLOG("Failed writing LTK to /data/ltk_store.bin: %s\n", strerror(errno));
    }

    for (i = 0; i < 16; i++)
        sprintf(key_str + (i * 2), "%2.2X", ltk_store->key.val[i]);

    ba2str(&ltk_store->key.addr.bdaddr, dst);

    PRLOG("new LTK for %s type %u central %u enc_size %u ediv %u rand %llu key: %s\n",
            dst,
            ltk_store->key.type,
            ltk_store->key.central,
            ltk_store->key.enc_size,
            ltk_store->key.ediv,
            ltk_store->key.rand,
            key_str);
}

static void index_added(uint16_t index, uint16_t len,
                const void *param, void *user_data)
{
    UNUSED(len); UNUSED(param); UNUSED(user_data);
    PRLOG("hci%u added\n", index);
    
    // 设置全局mgmt_index
    mgmt_index = index;
    PRLOG("mgmt_index set to %u\n", mgmt_index);
}

static void index_removed(uint16_t index, uint16_t len,
                const void *param, void *user_data)
{
    UNUSED(len); UNUSED(param); UNUSED(user_data);
    PRLOG("hci%u removed\n", index);
}

static void new_irk_callback(uint16_t index, uint16_t length,
                    const void *param, void *user_data)
{
    UNUSED(index); UNUSED(user_data);
    const struct mgmt_ev_new_irk *ev = param;
    const struct mgmt_addr_info *addr = &ev->key.addr;
    char dst[18], rpa[18];

    FILE *fp = fopen("/data/irk_received.bin", "wb");
    if (fp) {
        fwrite(param, length, 1, fp);
        fclose(fp);
        PRLOG("Wrote IRK to /data/irk_received.bin\n");
    } else {
        PRLOG("Failed writing IRK to /data/irk_received.bin: %s\n", strerror(errno));
    }

    ba2str(&addr->bdaddr, dst);
    ba2str(&ev->rpa, rpa);

    PRLOG("new IRK for %s RPA %s\n", dst, rpa);
}


static char *eir_get_name(const uint8_t *eir, uint16_t eir_len)
{
    uint8_t parsed = 0;

    if (eir_len < 2)
        return NULL;

    while (parsed < eir_len - 1) {
        uint8_t field_len = eir[0];

        if (field_len == 0)
            break;

        parsed += field_len + 1;

        if (parsed > eir_len)
            break;

        /* Check for short of complete name */
        if (eir[1] == 0x09 || eir[1] == 0x08)
            return strndup((char *) &eir[2], field_len - 1);

        eir += field_len + 1;
    }

    return NULL;
}


static unsigned int eir_get_flags(const uint8_t *eir, uint16_t eir_len)
{
    uint8_t parsed = 0;

    if (eir_len < 2)
        return 0;

    while (parsed < eir_len - 1) {
        uint8_t field_len = eir[0];

        if (field_len == 0)
            break;

        parsed += field_len + 1;

        if (parsed > eir_len)
            break;

        /* Check for flags */
        if (eir[1] == 0x01)
            return eir[2];

        eir += field_len + 1;
    }

    return 0;
}

static bool g_discovery = false;

static void device_found(uint16_t index, uint16_t len, const void *param,
                            void *user_data)
{
    UNUSED(user_data);
    const struct mgmt_ev_device_found *ev = param;
    uint16_t eir_len;
    uint32_t flags;

    if (len < sizeof(*ev)) {
        PRLOG("ERROR: Too short device_found length (%u bytes)\n", len);
        return;
    }

    flags = btohl(ev->flags);

    eir_len = get_le16(&ev->eir_len);
    if (len != sizeof(*ev) + eir_len) {
        PRLOG("ERROR: dev_found: expected %zu bytes, got %u bytes\n",
                        sizeof(*ev) + eir_len, len);
        return;
    }

    if (g_discovery) {
        char addr[18], *name;

        ba2str(&ev->addr.bdaddr, addr);
        PRLOG("hci%u dev_found: %s type %s rssi %d flags 0x%04x\n", index, addr,
            typestr(ev->addr.type), ev->rssi, flags);

        if (ev->addr.type != BDADDR_BREDR)
            PRLOG("AD flags 0x%02x \n",
                    eir_get_flags(ev->eir, eir_len));

        name = eir_get_name(ev->eir, eir_len);
        if (name)
            PRLOG("name %s\n", name);
        else
            PRLOG("eir_len %u\n", eir_len);

        free(name);
    }
}

static void phy_config_changed(uint16_t index, uint16_t len,
                              const void *param, void *user_data)
{
    UNUSED(len); UNUSED(index); UNUSED(user_data);
    const struct mgmt_ev_phy_configuration_changed *ev = param;
    PRLOG("PHY Configuration changed: 0x%04x\n", le16_to_cpu(ev->selected_phys));
}

// Register management interface callbacks
void register_mgmt_callbacks(struct mgmt *mgmt, uint16_t index)
{
    if (!mgmt) {
        PRLOG("ERROR: mgmt is NULL\n");
        return;
    }

    mgmt_register(mgmt, MGMT_EV_NEW_IRK, index, new_irk_callback, NULL, NULL);
    mgmt_register(mgmt, MGMT_EV_NEW_LONG_TERM_KEY, index, new_long_term_key_callback, NULL, NULL);
    mgmt_register(mgmt, MGMT_EV_INDEX_ADDED, index, index_added, NULL, NULL);
    mgmt_register(mgmt, MGMT_EV_INDEX_REMOVED, index, index_removed, NULL, NULL);
    mgmt_register(mgmt, MGMT_EV_NEW_SETTINGS, index, new_settings, NULL, NULL);
    mgmt_register(mgmt, MGMT_EV_DEVICE_CONNECTED, index, connected, NULL, NULL);
    mgmt_register(mgmt, MGMT_EV_DEVICE_DISCONNECTED, index, disconnected, NULL, NULL);
    mgmt_register(mgmt, MGMT_EV_CONNECT_FAILED, index, conn_failed, NULL, NULL);
    mgmt_register(mgmt, MGMT_EV_ADVERTISING_ADDED, index, advertising_added, NULL, NULL);
    mgmt_register(mgmt, MGMT_EV_ADVERTISING_REMOVED, index, advertising_removed, NULL, NULL);
    mgmt_register(mgmt, MGMT_EV_DEVICE_FLAGS_CHANGED, index, flags_changed, NULL, NULL);
    mgmt_register(mgmt, MGMT_EV_USER_CONFIRM_REQUEST, index, user_confirm, mgmt, NULL);
    mgmt_register(mgmt, MGMT_EV_DEVICE_FOUND, index, device_found, mgmt, NULL);
    mgmt_register(mgmt, MGMT_EV_PHY_CONFIGURATION_CHANGED, index, phy_config_changed, NULL, NULL);
}