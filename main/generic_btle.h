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

#include <stdlib.h>
#include <stdint.h>

#include "cbor.h"

#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))
#define BOOL_STR(b) (b) ? "true" : "false"

#define GBLE_VERSION 1

typedef uint32_t gble_actuator_id;

#define GBLE_ACTUATOR_TYPE_VIBRATE   1
#define GBLE_ACTUATOR_TYPE_ROTATE    2
#define GBLE_ACTUATOR_TYPE_OSCILLATE 3
#define GBLE_ACTUATOR_TYPE_CONSTRICT 4
#define GBLE_ACTUATOR_TYPE_INFLATE   5
#define GBLE_ACTUATOR_TYPE_POSITION  6
typedef uint8_t gble_actuator_type;

#define GBLE_ACTUATOR_MSG_SCALAR 1
#define GBLE_ACTUATOR_MSG_ROTATE 2
#define GBLE_ACTUATOR_MSG_LINEAR 3
typedef uint8_t gble_actuator_msg;

typedef void gble_actuator_callback_fn(gble_actuator_id actuator_id, uint32_t value, void* context);

struct gble_actuator_feature {
    const char* description;
    gble_actuator_type feature_type;
    uint32_t step_range_low;
    uint32_t step_range_high;
    gble_actuator_msg message_type;

    gble_actuator_callback_fn* cb;
    void* cb_context;

    // Filled in by gble_init
    gble_actuator_id id;

    // Filled in by gble_set_sensor_value
    uint32_t last_value;
};
typedef struct gble_actuator_feature gble_actuator_feature;

typedef uint32_t gble_sensor_id;

#define GBLE_SENSOR_TYPE_BATTERY  1
#define GBLE_SENSOR_TYPE_RSSI     2
#define GBLE_SENSOR_TYPE_BUTTON   3
#define GBLE_SENSOR_TYPE_PRESSURE 4
typedef uint8_t gble_sensor_type;

#define GBLE_SENSOR_MSG_READ      1
#define GBLE_SENSOR_MSG_SUBSCRIBE 2
typedef uint8_t gble_sensor_msg;

struct gble_sensor_feature {
    const char* description;
    gble_sensor_type feature_type;
    int32_t value_range_low;
    int32_t value_range_high;
    gble_sensor_msg message_type;

    // Filled in by gble_init
    gble_sensor_id id;

    // Filled in by gble_set_sensor_value
    int32_t last_value;
};
typedef struct gble_sensor_feature gble_sensor_feature;

struct gble_descriptor {
    CborEncoder rootEncoder;
    CborEncoder rootArrayEncoder;

    uint8_t* buffer;
    size_t buffer_size;
    int8_t state;
};
typedef struct gble_descriptor gble_descriptor;

typedef void gble_sensor_callback_fn(uint8_t* buf, size_t buf_size, void* context);

struct gble_server
{
    uint8_t descriptor_buffer[512];
    size_t descriptor_size;

    const char* name;

    gble_actuator_feature* actuators;
    size_t actuator_count;

    gble_sensor_feature* sensors;
    size_t sensors_count;

    gble_sensor_callback_fn* sensor_cb;
    void* sensor_cb_context;
};
typedef struct gble_server gble_server;

bool gble_init(gble_server* server, const char* name,
               gble_actuator_feature* actuators, size_t actuator_count,
               gble_sensor_feature* sensors, size_t sensors_count);

void gble_set_sensor_callback_fn(gble_server* server, gble_sensor_callback_fn* cb, void* cb_context);

void gble_handle_actuators_changed(gble_server* server, uint8_t* buf, size_t buf_size);

uint8_t* gble_get_descriptor(gble_server* server, size_t* descriptor_size);

bool gble_set_sensor_value(gble_server* server, gble_sensor_id id, int32_t value);

// Wrapper functions to work with other APIs
void gble_handle_actuators_changed_ctx(uint8_t* buf, size_t buf_size, void* context);

uint8_t* gble_get_descriptor_ctx(size_t* buf_size, void* context);
