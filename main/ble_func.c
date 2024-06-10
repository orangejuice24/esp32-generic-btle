/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_mac.h"

#include "console/console.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nimble/ble.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "services/gatt/ble_svc_gatt.h"
#include "services/gap/ble_svc_gap.h"
#include "services/bas/ble_svc_bas.h"
#include "svc_dis.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/util/util.h"

#include "ble_func.h"

#define MAC2STR_REV(a) (a)[5], (a)[4], (a)[3], (a)[2], (a)[1], (a)[0]

static const char *TAG = "BleFunc";

static int bleprph_gap_event(struct ble_gap_event *event, void *arg);
static uint8_t own_addr_type;

ble_func_disconnect_callback_fn* disconnect_cb = NULL;
void* disconnect_cb_context = NULL;

void ble_func_register_disconnect_cb(ble_func_disconnect_callback_fn* fn,
                                     void* context)
{
    disconnect_cb = fn;
    disconnect_cb_context = context;
}

ble_func_subscribe_callback_fn* subscribe_cb = NULL;
void* subscribe_cb_context = NULL;

void ble_func_register_subscribe_cb(ble_func_subscribe_callback_fn* fn,
                                    void* context)
{
    subscribe_cb = fn;
    subscribe_cb_context = context;
}

/**
 * Logs information about a connection to the console.
 */
static void bleprph_print_conn_desc(struct ble_gap_conn_desc *desc)
{
    ESP_LOGI(TAG, "handle=%d our_ota_addr_type=%d our_ota_addr=" MACSTR
                  " our_id_addr_type=%d our_id_addr=" MACSTR
                  " peer_ota_addr_type=%d peer_ota_addr=" MACSTR
                  " peer_id_addr_type=%d peer_id_addr=" MACSTR
                  " conn_itvl=%d conn_latency=%d supervision_timeout=%d "
                  "encrypted=%d authenticated=%d bonded=%d\n",
                  desc->conn_handle,
                  desc->our_ota_addr.type,
                  MAC2STR_REV(desc->our_ota_addr.val),
                  desc->our_id_addr.type,
                  MAC2STR_REV(desc->our_id_addr.val),
                  desc->peer_ota_addr.type,
                  MAC2STR_REV(desc->peer_ota_addr.val),
                  desc->peer_id_addr.type,
                  MAC2STR_REV(desc->peer_id_addr.val),
                  desc->conn_itvl, desc->conn_latency,
                  desc->supervision_timeout,
                  desc->sec_state.encrypted,
                  desc->sec_state.authenticated,
                  desc->sec_state.bonded);
}

int user_parse(const struct ble_hs_adv_field *data, void *arg)
{
    ESP_LOGI(TAG, "Parse data: field len %d, type %x, total %d bytes",
             data->length, data->type, data->length + 2); /* first byte type and second byte length */
    return 1;
}

/**
 * Enables advertising with the following parameters:
 *     o General discoverable mode.
 *     o Undirected connectable mode.
 */
static void bleprph_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    const char *name;
    int rc;

    /**
     *  Set the advertisement data included in our advertisements:
     *     o Flags (indicates advertisement type and other general info).
     *     o Advertising tx power.
     *     o Device name.
     *     o 16-bit service UUIDs (alert notifications).
     */

    memset(&fields, 0, sizeof fields);

    /* Advertise two flags:
     *     o Discoverability in forthcoming advertisement (general)
     *     o BLE-only (BR/EDR unsupported).
     */
    fields.flags = BLE_HS_ADV_F_DISC_GEN |
                   BLE_HS_ADV_F_BREDR_UNSUP;

    /* Indicate that the TX power level field should be included; have the
     * stack fill this value automatically.  This is done by assigning the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    fields.adv_itvl_is_present = 1;
    fields.adv_itvl = 40;

    name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    uint8_t buf[50], buf_sz;

    rc = ble_hs_adv_set_fields(&fields, buf, &buf_sz, 50);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "error setting advertisement data to buf; rc=%d", rc);
        return;
    }
    if (buf_sz > BLE_HS_ADV_MAX_SZ)
    {
        ESP_LOGE(TAG, "Too long advertising data: name %s, appearance %x, advsize = %d",
            name, fields.appearance, buf_sz);
        ble_hs_adv_parse(buf, buf_sz, user_parse, NULL);
        return;
    }

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "error setting advertisement data; rc=%d", rc);
        return;
    }

    /* Begin advertising. */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, bleprph_gap_event, NULL);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "error enabling advertisement; rc=%d", rc);
        return;
    }
}

// default password for bonding
int Disp_password = 123456;

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that forms.
 * bleprph uses the same callback for all connections.
 *
 * @param event                 The type of event being signalled.
 * @param arg                   Application-specified argument; unused by
 *                                  bleprph.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int bleprph_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type)
    {
        case BLE_GAP_EVENT_CONNECT:
            /* A new connection was established or a connection attempt failed. */
            ESP_LOGI(TAG, "connection %s; status=%d ",
                     event->connect.status == 0 ? "established" : "failed",
                     event->connect.status);

            if (event->connect.status == 0)
            {
                if (ble_gap_conn_find(event->connect.conn_handle, &desc) != 0)
                {
                    ESP_LOGE(TAG, "Failed to find connection for handle %hu",
                             event->connect.conn_handle);
                    return 0;
                }
                bleprph_print_conn_desc(&desc);
            }
            else
            {
                /* Connection failed; resume advertising. */
                bleprph_advertise();
            }
            return 0;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "disconnect; reason=%d ", event->disconnect.reason);

            if (disconnect_cb)
            {
                disconnect_cb(event->disconnect.conn.conn_handle, disconnect_cb_context);
            }

            /* Connection terminated; resume advertising. */
            bleprph_advertise();
            return 0;

        case BLE_GAP_EVENT_CONN_UPDATE_REQ:
            /* The central requested the connection parameters update. */
            ESP_LOGI(TAG, "connection update request");
            return 0;

        case BLE_GAP_EVENT_CONN_UPDATE:
            /* The central has updated the connection parameters. */
            ESP_LOGI(TAG, "connection updated; status=%d",
                     event->conn_update.status);
            return 0;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGI(TAG, "advertise complete; reason=%d",
                     event->adv_complete.reason);
            bleprph_advertise();
            return 0;

        case BLE_GAP_EVENT_ENC_CHANGE:
            /* Encryption has been enabled or disabled for this connection. */
            ESP_LOGI(TAG, "encryption change event; status=%d",
                     event->enc_change.status);
            return 0;

        case BLE_GAP_EVENT_SUBSCRIBE:
            ESP_LOGI(TAG, "subscribe event; conn_handle=%d attr_handle=%04X "
                     "reason=%d prev_notify=%d cur_notify=%d prev_indicate=%d cur_indicate=%d",
                     event->subscribe.conn_handle,
                     event->subscribe.attr_handle,
                     event->subscribe.reason,
                     event->subscribe.prev_notify,
                     event->subscribe.cur_notify,
                     event->subscribe.prev_indicate,
                     event->subscribe.cur_indicate);

            if (subscribe_cb)
            {
                subscribe_cb(event->subscribe.conn_handle,
                             event->subscribe.attr_handle,
                             event->subscribe.cur_notify,
                             event->subscribe.cur_indicate,
                             subscribe_cb_context);
            }
            return 0;

        case BLE_GAP_EVENT_NOTIFY_TX:
            ESP_LOGI(TAG, "notify event; status=%d conn_handle=%d attr_handle=%04X type=%s",
                     event->notify_tx.status,
                     event->notify_tx.conn_handle,
                     event->notify_tx.attr_handle,
                     event->notify_tx.indication ? "indicate" : "notify");
            return 0;

        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG, "mtu update event; conn_handle=%d cid=%d mtu=%d",
                     event->mtu.conn_handle,
                     event->mtu.channel_id,
                     event->mtu.value);
            return 0;

        case BLE_GAP_EVENT_REPEAT_PAIRING:
            /* We already have a bond with the peer, but it is attempting to
            * establish a new secure link.  This app sacrifices security for
            * convenience: just throw away the old bond and accept the new link.
            */

            /* Delete the old bond. */
            rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
            assert(rc == 0);

            ble_store_util_delete_peer(&desc.peer_id_addr);

            /* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
            * continue with the pairing operation.
            */
            return BLE_GAP_REPEAT_PAIRING_RETRY;

        case BLE_GAP_EVENT_PASSKEY_ACTION:
            ESP_LOGI(TAG, "PASSKEY_ACTION_EVENT started");
            struct ble_sm_io pkey = {0};

            if (event->passkey.params.action == BLE_SM_IOACT_DISP)
            {
                pkey.action = event->passkey.params.action;

                /* This is the passkey to be entered on peer */
                pkey.passkey = Disp_password;

                ESP_LOGI(TAG, "Enter passkey %lu on the peer side", pkey.passkey);

                rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);

                ESP_LOGI(TAG, "ble_sm_inject_io result: %d", rc);
            }
            else if(event->passkey.params.action == BLE_SM_IOACT_INPUT ||
                    event->passkey.params.action == BLE_SM_IOACT_NUMCMP ||
                    event->passkey.params.action == BLE_SM_IOACT_OOB )
            {
                ESP_LOGE(TAG, "BLE_SM_IOACT_INPUT, BLE_SM_IOACT_NUMCMP, BLE_SM_IOACT_OOB"
                         " bonding not supported!");
            }
            return 0;

        default:
            ESP_LOGW(TAG, "Unknown GAP event: %d", event->type);
    }

    return 0;
}

static void
bleprph_on_reset(int reason)
{
    ESP_LOGE(TAG, "Resetting state; reason=%d", reason);
}

static void
bleprph_on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    /* Figure out address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "error determining address type; rc=%d", rc);
        return;
    }

    /* Printing ADDR */
    uint8_t addr_val[6] = {0};
    rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);

    ESP_LOGI(TAG, "Device Address: "MACSTR, MAC2STR_REV(addr_val));

    /* Begin advertising. */
    bleprph_advertise();
}

void
bleprph_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task Started");

    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}

void ble_gatt_svc_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op)
    {
        case BLE_GATT_REGISTER_OP_SVC:
            ESP_LOGI(TAG,"uuid16 %s handle=%d (%04X)",
                     ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                     ctxt->svc.handle, ctxt->svc.handle);
            break;

        case BLE_GATT_REGISTER_OP_CHR:
            ESP_LOGI(TAG, "uuid16 %s arg %d def_handle=%d (%04X) val_handle=%d (%04X)",
                     ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                     (int)ctxt->chr.chr_def->arg,
                     ctxt->chr.def_handle, ctxt->chr.def_handle,
                     ctxt->chr.val_handle, ctxt->chr.val_handle);
            break;

        case BLE_GATT_REGISTER_OP_DSC:
            ESP_LOGI(TAG, "uuid16 %s arg %d handle=%d (%04X)",
                     ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                     (int)ctxt->dsc.dsc_def->arg,
                     ctxt->dsc.handle, ctxt->dsc.handle);
            break;
    }
}

/* this declaration was taken from examples */
void ble_store_config_init(void);

bool ble_init(gatt_init_callback_fn* gatt_init_fn)
{
    nimble_port_init();

    /* Initialize the NimBLE host configuration. */
    ble_hs_cfg.reset_cb = bleprph_on_reset;
    ble_hs_cfg.sync_cb = bleprph_on_sync;
    ble_hs_cfg.gatts_register_cb = ble_gatt_svc_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    // set to BLE_SM_IO_CAP_NO_IO to bond with no prompt
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;

    // TODO: Do these work?
    // ble_hs_cfg.sm_bonding = 1;
    // ble_hs_cfg.sm_mitm = 1;
    // ble_hs_cfg.sm_sc = 1;
    // ble_hs_cfg.sm_our_key_dist = 1;
    // ble_hs_cfg.sm_their_key_dist = 1;

    ble_hs_cfg.sm_sc = 0;

    int rc = gatt_init_fn();
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Failed to init gatt_svr; rc=%d", rc);
        return false;
    }

    /* Set the default device name. */
    rc = ble_svc_gap_device_name_set("GEN-0");
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Failed to set device name; rc=%d", rc);
        return false;
    }

    ble_store_config_init();

    nimble_port_freertos_init(bleprph_host_task);

    return true;
}
