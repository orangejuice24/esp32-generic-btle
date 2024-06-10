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

#include <stdio.h>

#include "gatt_svr.h"
#include "gatt_svr_priv.h"

#define SUPPORT_REPORT_VENDOR false

#define NO_MINKEYSIZE     .min_key_size = DEFAULT_MIN_KEY_SIZE
#define NO_ARG_MINKEYSIZE .arg = NULL, NO_MINKEYSIZE
#define NO_ARG_DESCR_MKS  .descriptors = NULL, NO_ARG_MINKEYSIZE
#define NO_DESCR_MKS      .descriptors = NULL, NO_MINKEYSIZE

gatt_server gatt_server_instance;

// Default services
const struct ble_gatt_svc_def Gatt_svr_included_services[] = {
    {
        /*** Battery Service. */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_SVC_BAS_UUID16),
        .includes = NULL,
        .characteristics = (struct ble_gatt_chr_def[]) { {
        /*** Battery level characteristic */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_BAS_CHR_UUID16_BATTERY_LEVEL),
            .access_cb = gatt_svr_battery_access,
            .arg = (void *) HANDLE_BATTERY_LEVEL,
            .val_handle = &Svc_char_handles[HANDLE_BATTERY_LEVEL],
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_INDICATE,
            NO_MINKEYSIZE,
            .descriptors = (struct ble_gatt_dsc_def[]) { {
                .uuid = BLE_UUID16_DECLARE(GATT_UUID_BAT_PRESENT_DESCR),
                .att_flags = BLE_ATT_F_READ | BLE_ATT_F_READ_ENC,
                .access_cb = gatt_svr_battery_access,
                NO_ARG_MINKEYSIZE,
            }, {
                0, /* No more descriptors in this characteristic. */
            } },
        }, {
            0, /* No more characteristics in this service. */
        } },
    },

    { /*** Service: Device Information Service (DIS). */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_SVC_DIS_UUID16),
        .includes = NULL,
        .characteristics = (struct ble_gatt_chr_def[]) { {
        /*** Characteristic: Model Number String */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_DIS_CHR_UUID16_MODEL_NUMBER),
            .access_cb = gatt_svr_dis_access,
            .val_handle = &Svc_char_handles[HANDLE_DIS_MODEL_NUMBER],
            .flags = BLE_GATT_CHR_F_READ | (BLE_SVC_DIS_MODEL_NUMBER_READ_PERM),
            NO_ARG_DESCR_MKS,
        }, {
        /*** Characteristic: Serial Number String */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_DIS_CHR_UUID16_SERIAL_NUMBER),
            .access_cb = gatt_svr_dis_access,
            .val_handle = &Svc_char_handles[HANDLE_DIS_SERIAL_NUMBER],
            .flags = BLE_GATT_CHR_F_READ | (BLE_SVC_DIS_SERIAL_NUMBER_READ_PERM),
            NO_ARG_DESCR_MKS,
        }, {
        /*** Characteristic: Hardware Revision String */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_DIS_CHR_UUID16_HARDWARE_REVISION),
            .access_cb = gatt_svr_dis_access,
            .val_handle = &Svc_char_handles[HANDLE_DIS_HARDWARE_REVISION],
            .flags = BLE_GATT_CHR_F_READ | (BLE_SVC_DIS_HARDWARE_REVISION_READ_PERM),
            NO_ARG_DESCR_MKS,
        }, {
        /*** Characteristic: Firmware Revision String */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_DIS_CHR_UUID16_FIRMWARE_REVISION),
            .access_cb = gatt_svr_dis_access,
            .val_handle = &Svc_char_handles[HANDLE_DIS_FIRMWARE_REVISION],
            .flags = BLE_GATT_CHR_F_READ | (BLE_SVC_DIS_FIRMWARE_REVISION_READ_PERM),
            NO_ARG_DESCR_MKS,
        }, {
        /*** Characteristic: Software Revision String */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_DIS_CHR_UUID16_SOFTWARE_REVISION),
            .access_cb = gatt_svr_dis_access,
            .val_handle = &Svc_char_handles[HANDLE_DIS_SOFWARE_REVISION],
            .flags = BLE_GATT_CHR_F_READ | (BLE_SVC_DIS_SOFTWARE_REVISION_READ_PERM),
            NO_ARG_DESCR_MKS,
        }, {
        /*** Characteristic: Manufacturer Name */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_DIS_CHR_UUID16_MANUFACTURER_NAME),
            .access_cb = gatt_svr_dis_access,
            .val_handle = &Svc_char_handles[HANDLE_DIS_MANUFACTURER_NAME],
            .flags = BLE_GATT_CHR_F_READ | (BLE_SVC_DIS_MANUFACTURER_NAME_READ_PERM),
            NO_ARG_DESCR_MKS,
        }, {
      /*** Characteristic: System Id */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_DIS_CHR_UUID16_SYSTEM_ID),
            .access_cb = gatt_svr_dis_access,
            .val_handle = &Svc_char_handles[HANDLE_DIS_SYSTEM_ID],
            .flags = BLE_GATT_CHR_F_READ | (BLE_SVC_DIS_SYSTEM_ID_READ_PERM),
            NO_ARG_DESCR_MKS,
        }, {
      /*** Characteristic: System Id */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_DIS_CHR_UUID16_PNP_INFO),
            .access_cb = gatt_svr_dis_access,
            .val_handle = &Svc_char_handles[HANDLE_DIS_PNP_INFO],
            .flags = BLE_GATT_CHR_F_READ,
            NO_ARG_DESCR_MKS,
        }, {
            0, /* No more characteristics in this service */
        }, }
    },

    {
        0, /* No more services. */
    },
};

const struct ble_gatt_svc_def *Inc_svcs[] = {
    &Gatt_svr_included_services[0],
    NULL,
};

// User services
const struct ble_gatt_svc_def Gatt_svr_svcs[] = {
    {
        /*** Main Service */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(GATT_UUID_GBLE_SERVICE),
        .includes = Inc_svcs,
        .characteristics = (struct ble_gatt_chr_def[])
        {
            {
            /*** Firmware */
                .uuid = BLE_UUID16_DECLARE(GATT_UUID_GBLE_FIRMWARE_CHR),
                .access_cb = gatt_svr_chr_access,
                .val_handle = &Svc_char_handles[HANDLE_MAIN_FIRMWARE],
                .flags = BLE_GATT_CHR_F_READ,
                NO_ARG_DESCR_MKS,
            }, {
            /*** RX */
                .uuid = BLE_UUID16_DECLARE(GATT_UUID_GBLE_RX_CHR),
                .access_cb = gatt_svr_chr_access,
                .val_handle = &Svc_char_handles[HANDLE_MAIN_RX],
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_INDICATE,
                NO_ARG_DESCR_MKS,
            }, {
            /*** TX */
                .uuid = BLE_UUID16_DECLARE(GATT_UUID_GBLE_TX_CHR),
                .access_cb = gatt_svr_chr_access,
                .val_handle = &Svc_char_handles[HANDLE_MAIN_TX],
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                NO_ARG_DESCR_MKS,
            }, {
                0, /* No more characteristics in this service. */
            }
        },
    },

    {
        0, /* No more services. */
    },
};

/* handles for all characteristics in GATT services */
uint16_t Svc_char_handles[HANDLE_HID_COUNT];

// battery level unit - percents
struct prf_char_pres_fmt Battery_level_units = {
    .format = 4,      // Unsigned 8-bit
    .exponent = 0,
    .unit = 0x27AD,   // percentage
    .name_space = 1,  // BLUETOOTH SIG
    .description = 0
};

/* Device information
variable name restricted in ESP_IDF
components/bt/host/nimble/nimble/nimble/host/services/dis/include/services/dis/ble_svc_dis.h
*/
struct ble_svc_dis_data Ble_svc_dis_data = {
    .model_number      = BLE_SVC_DIS_MODEL_NUMBER_DEFAULT,
    .serial_number     = BLE_SVC_DIS_SERIAL_NUMBER_DEFAULT,
    .firmware_revision = BLE_SVC_DIS_FIRMWARE_REVISION_DEFAULT,
    .hardware_revision = BLE_SVC_DIS_HARDWARE_REVISION_DEFAULT,
    .software_revision = BLE_SVC_DIS_SOFTWARE_REVISION_DEFAULT,
    .manufacturer_name = BLE_SVC_DIS_MANUFACTURER_NAME_DEFAULT,
    .system_id         = BLE_SVC_DIS_SYSTEM_ID_DEFAULT,
    .pnp_info          = BLE_SVC_DIS_PNP_INFO_DEFAULT,
};
