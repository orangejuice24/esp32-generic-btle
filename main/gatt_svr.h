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

typedef uint8_t* gatt_svr_descriptor_callback_fn(size_t* buf_size, void* context);
typedef void gatt_svr_write_callback_fn(uint8_t* buf, size_t buf_size, void* context);

int gatt_svr_init(void);

void gatt_svr_register_descriptor_cb(gatt_svr_descriptor_callback_fn* fn,
                                     void* context);

void gatt_svr_register_write_cb(gatt_svr_write_callback_fn* fn,
                                void* context);


bool gatt_svr_set_battery_level(uint8_t value);

bool gatt_svr_set_read_value(const uint8_t* buf, size_t buf_size);

void gatt_svr_handle_subscribe(uint16_t conn_handle,
                               uint16_t attr_handle,
                               bool can_notify,
                               bool can_indicate);

void gatt_svr_client_disconnected(uint16_t conn_handle);

// Wrapper functions to work with other APIs
void gatt_svr_set_read_value_ctx(uint8_t* buf, size_t buf_size, void* context);

void gatt_svr_handle_subscribe_ctx(uint16_t conn_handle,
                                   uint16_t attr_handle,
                                   bool can_notify,
                                   bool can_indicate,
                                   void* context);

void gatt_svr_client_disconnected_ctx(uint16_t conn_handle, void* context);
