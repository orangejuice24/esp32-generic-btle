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

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"

#include "gatt_svr.h"
#include "gatt_svr_priv.h"
#include "generic_btle.h"


static const char *TAG = "GattSvr";

int gatt_svr_init(void)
{
    memset(&gatt_server_instance, 0, sizeof(gatt_server_instance));

    ble_svc_gap_init();
    ble_svc_gatt_init();

    memset(&Svc_char_handles, 0, sizeof(Svc_char_handles[0]) * HANDLE_HID_COUNT);

    int rc = ble_gatts_count_cfg(Gatt_svr_included_services);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "error counting included services; rc=%d", rc);
        return rc;
    }

    rc = ble_gatts_add_svcs(Gatt_svr_included_services);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "error adding included services; rc=%d", rc);
        return rc;
    }

    ESP_LOGI(TAG, "GATT included services added");

    rc = ble_gatts_count_cfg(Gatt_svr_svcs);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "error counting user services; rc=%d", rc);
        return rc;
    }

    rc = ble_gatts_add_svcs(Gatt_svr_svcs);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "error adding user services; rc=%d", rc);
        return rc;
    }

    ESP_LOGI(TAG, "GATT services added");

    return 0;
}

void gatt_svr_register_descriptor_cb(gatt_svr_descriptor_callback_fn* fn,
                                     void* context)

{
    gatt_server_instance.descriptor_cb = fn;
    gatt_server_instance.descriptor_cb_context = context;
}

void gatt_svr_register_write_cb(gatt_svr_write_callback_fn* fn,
                                void* context)
{
    gatt_server_instance.write_cb = fn;
    gatt_server_instance.write_cb_context = context;
}

bool gatt_svr_set_read_value(const uint8_t* buf, size_t buf_size)
{
    if (buf_size > sizeof(gatt_server_instance.read_buf))
    {
        ESP_LOGE(TAG, "read value too large for buffer: %zu > %zu",
                 buf_size, sizeof(gatt_server_instance.read_buf));
        return false;
    }

    memcpy(gatt_server_instance.read_buf, buf, buf_size);
    gatt_server_instance.read_buf_size = buf_size;

    ESP_LOGD(TAG, "Notifying read subscribers");

    for (int conn_handle = 0; conn_handle < sizeof(gatt_server_instance.conn_handle_read_subs); ++conn_handle)
    {
        if (gatt_server_instance.conn_handle_read_subs[conn_handle])
        {
            ESP_LOGD(TAG, "Notifying client %hu for read change", conn_handle);
            ble_gatts_notify(conn_handle, Svc_char_handles[HANDLE_MAIN_RX]);
        }
    }

    return true;
}

bool gatt_svr_set_battery_level(uint8_t value)
{
    if (gatt_server_instance.battery_level == value)
    {
        return false;
    }

    ESP_LOGI(TAG, "Updating battery level to %hhu", value);
    gatt_server_instance.battery_level = value;

    for (int conn_handle = 0; conn_handle < sizeof(gatt_server_instance.conn_handle_battery_subs); ++conn_handle)
    {
        if (gatt_server_instance.conn_handle_battery_subs[conn_handle])
        {
            ESP_LOGI(TAG, "Notifying client %hu for battery level change", conn_handle);
            ble_gatts_notify(conn_handle, Svc_char_handles[HANDLE_BATTERY_LEVEL]);
        }
    }

    return true;
}

void gatt_svr_handle_subscribe(uint16_t conn_handle,
                               uint16_t attr_handle,
                               bool can_notify,
                               bool can_indicate)
{
    ESP_LOGI(TAG, "Connection %hu subscribing to attr %hu (notify: %s, indicate: %s)",
             conn_handle, attr_handle, BOOL_STR(can_notify), BOOL_STR(can_indicate));

    if (conn_handle > COUNT_OF(gatt_server_instance.conn_handle_battery_subs))
    {
        ESP_LOGE(TAG, "Invalid connection handle: %hu", conn_handle);
        return;
    }

    if (attr_handle == Svc_char_handles[HANDLE_BATTERY_LEVEL])
    {
        gatt_server_instance.conn_handle_battery_subs[conn_handle] = can_notify;
    }
    else if (attr_handle == Svc_char_handles[HANDLE_MAIN_RX])
    {
        gatt_server_instance.conn_handle_read_subs[conn_handle] = can_notify;
    }
    else
    {
        ESP_LOGW(TAG, "Connection %hu unknown attr: %hu", conn_handle, attr_handle);
        return;
    }
}

void gatt_svr_client_disconnected(uint16_t conn_handle)
{
    ESP_LOGI(TAG, "Client %hu disconnected", conn_handle);

    if (conn_handle > COUNT_OF(gatt_server_instance.conn_handle_battery_subs))
    {
        ESP_LOGE(TAG, "Invalid connection handle: %hu", conn_handle);
        return;
    }

    gatt_server_instance.conn_handle_battery_subs[conn_handle] = false;
    gatt_server_instance.conn_handle_read_subs[conn_handle] = false;
}

int gatt_svr_battery_access(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint16_t uuid16 = ble_uuid_u16(ctxt->chr->uuid);

    ESP_LOGI(TAG, "%s: UUID %04X attr %04X arg %d op %d",
             __FUNCTION__, uuid16, attr_handle, (int) arg, ctxt->op);

    switch (uuid16)
    {
        case BLE_SVC_BAS_CHR_UUID16_BATTERY_LEVEL:
            if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR)
            {
                int rc = os_mbuf_append(ctxt->om,
                                        &gatt_server_instance.battery_level,
                                        sizeof(gatt_server_instance.battery_level));
                if (rc)
                {
                    ESP_LOGW(TAG, "Error reading battery level, rc = %d", rc);
                    return BLE_ATT_ERR_INSUFFICIENT_RES;
                }
            }
            else
            {
                return BLE_ATT_ERR_UNLIKELY;
            }
            break;

        case GATT_UUID_BAT_PRESENT_DESCR:
            if (ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC)
            {
                ESP_LOGI(TAG, "battery character presentation descriptor read, op: %d", ctxt->op);
                int rc = os_mbuf_append(ctxt->om, &Battery_level_units,
                                        sizeof(Battery_level_units));

                if (rc)
                {
                    ESP_LOGW(TAG, "Error reading character presentation descriptor, rc = %d", rc);
                    return BLE_ATT_ERR_INSUFFICIENT_RES;
                }
            }
            break;

        default:
            return BLE_ATT_ERR_UNLIKELY;
    }

    return 0;
}

int gatt_svr_dis_access(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint16_t uuid = ble_uuid_u16(ctxt->chr->uuid);

    const char *info = NULL;
    int info_len = 0;

    ESP_LOGI(TAG, "%s: UUID %04X attr %04X arg %d op %d",
             __FUNCTION__, uuid, attr_handle, (int)arg, ctxt->op);

    switch(uuid)
    {
        case BLE_SVC_DIS_CHR_UUID16_MODEL_NUMBER:
            info = Ble_svc_dis_data.model_number;
            if (info == NULL)
            {
                info = (BLE_SVC_DIS_MODEL_NUMBER_DEFAULT);
            }
            break;
        case BLE_SVC_DIS_CHR_UUID16_SERIAL_NUMBER:
            info = Ble_svc_dis_data.serial_number;
            if (info == NULL)
            {
                info = (BLE_SVC_DIS_SERIAL_NUMBER_DEFAULT);
            }
            break;
        case BLE_SVC_DIS_CHR_UUID16_FIRMWARE_REVISION:
            info = Ble_svc_dis_data.firmware_revision;
            if (info == NULL)
            {
                info = (BLE_SVC_DIS_FIRMWARE_REVISION_DEFAULT);
            }
            break;
        case BLE_SVC_DIS_CHR_UUID16_HARDWARE_REVISION:
            info = Ble_svc_dis_data.hardware_revision;
            if (info == NULL)
            {
                info = (BLE_SVC_DIS_HARDWARE_REVISION_DEFAULT);
            }
            break;
        case BLE_SVC_DIS_CHR_UUID16_SOFTWARE_REVISION:
            info = Ble_svc_dis_data.software_revision;
            if (info == NULL)
            {
                info = (BLE_SVC_DIS_SOFTWARE_REVISION_DEFAULT);
            }
            break;
        case BLE_SVC_DIS_CHR_UUID16_MANUFACTURER_NAME:
            info = Ble_svc_dis_data.manufacturer_name;
            if (info == NULL)
            {
                info = (BLE_SVC_DIS_MANUFACTURER_NAME_DEFAULT);
            }
            break;
        case BLE_SVC_DIS_CHR_UUID16_SYSTEM_ID:
            info = Ble_svc_dis_data.system_id;
            if (info == NULL)
            {
                info = (BLE_SVC_DIS_SYSTEM_ID_DEFAULT);
            }
            break;
        case BLE_SVC_DIS_CHR_UUID16_PNP_INFO:
            info = (char *)Ble_svc_dis_data.pnp_info;
            info_len = sizeof(Ble_svc_dis_data.pnp_info);
            if (info == NULL)
            {
                info = BLE_SVC_DIS_SYSTEM_ID_DEFAULT;
            }
            break;
        default:
            return BLE_ATT_ERR_UNLIKELY;
    }

    if (!info_len)
    {
        info_len =  strlen(info);
    }

    int rc = os_mbuf_append(ctxt->om, info, info_len);
    if (rc)
    {
        return BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    return 0;
}

int gatt_svr_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint16_t uuid16 = ble_uuid_u16(ctxt->chr->uuid);

    ESP_LOGI(TAG, "%s: UUID %04X attr %04X arg %d op %d",
             __FUNCTION__, uuid16, attr_handle, (int)arg, ctxt->op);

    switch (uuid16)
    {
        case GATT_UUID_GBLE_FIRMWARE_CHR:
        case GATT_UUID_GBLE_RX_CHR:
            if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR)
            {
                ESP_LOGW(TAG, "Invalid op %d", ctxt->op);
                break;
            }

            uint8_t* resp_buf = NULL;
            size_t resp_len = 0;

            if (uuid16 == GATT_UUID_GBLE_FIRMWARE_CHR && gatt_server_instance.descriptor_cb)
            {
                void* ctx = gatt_server_instance.descriptor_cb_context;
                resp_buf = gatt_server_instance.descriptor_cb(&resp_len, ctx);
            }
            else if (uuid16 == GATT_UUID_GBLE_RX_CHR)
            {
                resp_buf = gatt_server_instance.read_buf;
                resp_len = gatt_server_instance.read_buf_size;
            }

            if (resp_len > 0)
            {
                int rc = os_mbuf_append(ctxt->om, resp_buf, resp_len);
                if (rc)
                {
                    ESP_LOGW(TAG, "Error filling buffer, rc = %d", rc);
                    return BLE_ATT_ERR_INSUFFICIENT_RES;
                }
            }

            return 0;

        case GATT_UUID_GBLE_TX_CHR:
            if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR)
            {
                ESP_LOGW(TAG, "Invalid op %d for tx chr", ctxt->op);
                break;
            }

            if (gatt_server_instance.write_cb)
            {
                uint8_t scratch[256];

                uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
                if (om_len > sizeof(scratch)) {
                    return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
                }

                uint16_t flat_len;
                int rc = ble_hs_mbuf_to_flat(ctxt->om, scratch, sizeof(scratch), &flat_len);
                if (rc != 0)
                {
                    ESP_LOGE(TAG, "Error copying to scratch buffer, rc= %d", rc);
                    return BLE_ATT_ERR_UNLIKELY;
                }

                void* ctx = gatt_server_instance.write_cb_context;
                gatt_server_instance.write_cb(scratch, flat_len, ctx);
            }

            return 0;

        default:
            ESP_LOGW(TAG, "Unknown attr UUID %02hx", uuid16);
            break;
    }

    return BLE_ATT_ERR_UNLIKELY;
}

// Wrapper functions to work with other APIs
void gatt_svr_set_read_value_ctx(uint8_t* buf, size_t buf_size, void* context)
{
    gatt_svr_set_read_value(buf, buf_size);
}

void gatt_svr_handle_subscribe_ctx(uint16_t conn_handle,
                                   uint16_t attr_handle,
                                   bool can_notify,
                                   bool can_indicate,
                                   void* context)
{
    gatt_svr_handle_subscribe(conn_handle, attr_handle, can_notify, can_indicate);
}

void gatt_svr_client_disconnected_ctx(uint16_t conn_handle, void* context)
{
    gatt_svr_client_disconnected(conn_handle);
}