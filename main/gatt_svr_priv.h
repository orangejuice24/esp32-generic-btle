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

#pragma once

#include <stdbool.h>

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

// Make the IDE happy
#ifndef CONFIG_NIMBLE_MAX_CONNECTIONS
#define CONFIG_NIMBLE_MAX_CONNECTIONS 3
#endif

struct gatt_server
{
    // Called when a client reads the descriptor
    gatt_svr_descriptor_callback_fn* descriptor_cb;
    void* descriptor_cb_context;

    // Called when a client sends a write
    gatt_svr_write_callback_fn* write_cb;
    void* write_cb_context;

    // Cached read values
    uint8_t read_buf[256];
    size_t read_buf_size;

    uint8_t battery_level;

    bool conn_handle_battery_subs[CONFIG_NIMBLE_MAX_CONNECTIONS];
    bool conn_handle_read_subs[CONFIG_NIMBLE_MAX_CONNECTIONS];
};
typedef struct gatt_server gatt_server;


/** GATT server. */
#define GATT_UUID_GBLE_SERVICE                  0xffe0
#define GATT_UUID_GBLE_FIRMWARE_CHR             0xffe1
#define GATT_UUID_GBLE_RX_CHR                   0xffe2
#define GATT_UUID_GBLE_TX_CHR                   0xffe3

#define GATT_UUID_BAT_PRESENT_DESCR             0x2904

#define BLE_SVC_DIS_MODEL_NUMBER_DEFAULT        "0x0102"
#define BLE_SVC_DIS_SERIAL_NUMBER_DEFAULT       "0x0001"
#define BLE_SVC_DIS_FIRMWARE_REVISION_DEFAULT   "0x1409"
#define BLE_SVC_DIS_HARDWARE_REVISION_DEFAULT   "0x0001"
#define BLE_SVC_DIS_SOFTWARE_REVISION_DEFAULT   "0x1409"
#define BLE_SVC_DIS_MANUFACTURER_NAME_DEFAULT   "Manufacturer"
#define BLE_SVC_DIS_SYSTEM_ID_DEFAULT           "esp32"
#define BLE_SVC_DIS_PNP_INFO_DEFAULT            {0x00,0x47,0x00,0xff,0xff,0xff,0xff}

/*
Defines default permissions for reading characteristics. Can be
zero to allow read without extra permissions or combination of:
    BLE_GATT_CHR_F_READ_ENC
    BLE_GATT_CHR_F_READ_AUTHEN
    BLE_GATT_CHR_F_READ_AUTHOR
*/

#define BLE_SVC_DIS_MODEL_NUMBER_READ_PERM      0
#define BLE_SVC_DIS_SERIAL_NUMBER_READ_PERM     0
#define BLE_SVC_DIS_HARDWARE_REVISION_READ_PERM 0
#define BLE_SVC_DIS_FIRMWARE_REVISION_READ_PERM 0
#define BLE_SVC_DIS_SOFTWARE_REVISION_READ_PERM 0
#define BLE_SVC_DIS_MANUFACTURER_NAME_READ_PERM 0
#define BLE_SVC_DIS_SYSTEM_ID_READ_PERM         0

#define DEFAULT_MIN_KEY_SIZE            0

#define NVS_IO_CAP_NUM "io_cap_number"
#define DEFAULT_IO_CAP BLE_HS_IO_NO_INPUT_OUTPUT

// Battery level presentation descriptor value format
struct prf_char_pres_fmt
{
    /// Format
    uint8_t format;
    /// Exponent
    uint8_t exponent;
    /// Unit (The Unit is a UUID)
    uint16_t unit;
    /// Name space
    uint8_t name_space;
    /// Description
    uint16_t description;
} __attribute__((packed));

enum attr_handles {
    HANDLE_BATTERY_LEVEL,               //  0
    HANDLE_DIS_MODEL_NUMBER,            //  1
    HANDLE_DIS_SERIAL_NUMBER,           //  2
    HANDLE_DIS_HARDWARE_REVISION,       //  3
    HANDLE_DIS_FIRMWARE_REVISION,       //  4
    HANDLE_DIS_SOFWARE_REVISION,        //  5
    HANDLE_DIS_MANUFACTURER_NAME,       //  6
    HANDLE_DIS_SYSTEM_ID,               //  7
    HANDLE_DIS_PNP_INFO,                //  8

    // Main service
    HANDLE_MAIN_FIRMWARE,             //  9
    HANDLE_MAIN_RX,             //  9
    HANDLE_MAIN_TX,           // 10
    HANDLE_HID_COUNT                    // 11
};

// Globals

extern gatt_server gatt_server_instance;

/* handles for all characteristics in GATT services */
extern uint16_t Svc_char_handles[];

extern const struct ble_gatt_svc_def Gatt_svr_included_services[];
extern const struct ble_gatt_svc_def Gatt_svr_svcs[];

extern struct prf_char_pres_fmt Battery_level_units;

extern struct ble_svc_dis_data Ble_svc_dis_data;

// Access function for battery service
int gatt_svr_battery_access(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg);

// Access function for device information service
int gatt_svr_dis_access(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg);

// Access function for user services
int gatt_svr_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg);