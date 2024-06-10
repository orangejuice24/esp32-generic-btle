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
#include <stdbool.h>

#include "esp_log.h"
#include "generic_btle.h"

static const char* TAG = "GenericBtle";

#define GBLE_DESCRIPTOR_STATE_ERROR             -1
#define GBLE_DESCRIPTOR_STATE_INIT              0
#define GBLE_DESCRIPTOR_STATE_ACTUATORS_ADDED   1
#define GBLE_DESCRIPTOR_STATE_SENSORS_ADDED     2
#define GBLE_DESCRIPTOR_STATE_FINISHED          3

#define CBOR_CHECKED_RET(stmt, ret_val) \
    do { \
        int rc = (stmt); \
        if (rc != CborNoError) \
        { \
            ESP_LOGE(TAG, "%s:%d CBOR Encode failed: %s", __FILE__, __LINE__, cbor_error_string(rc)); \
            return ret_val; \
        } \
    } while (0)

#define CBOR_CHECKED_RET_FALSE(stmt) \
    CBOR_CHECKED_RET(stmt, false)

#define CBOR_CHECKED(stmt) \
    CBOR_CHECKED_RET(stmt,)


bool gble_init(gble_server* server, const char* name,
               gble_actuator_feature* actuators, size_t actuator_count,
               gble_sensor_feature* sensors, size_t sensor_count)
{
    server->descriptor_size = 0;
    server->name = name;
    server->actuators = actuators;
    server->actuator_count = actuator_count;
    server->sensors = sensors;
    server->sensors_count = sensor_count;

    CborEncoder root_encoder;
    CborEncoder root_array_encoder;

    cbor_encoder_init(&root_encoder, server->descriptor_buffer, sizeof(server->descriptor_buffer), 0);

    CBOR_CHECKED_RET_FALSE(cbor_encoder_create_array(&root_encoder, &root_array_encoder, 4));

    // Add version and name first
    CBOR_CHECKED_RET_FALSE(cbor_encode_uint(&root_array_encoder, GBLE_VERSION));
    CBOR_CHECKED_RET_FALSE(cbor_encode_text_stringz(&root_array_encoder, name));

    // Then add the actuators
    CborEncoder actuators_array;
    CBOR_CHECKED_RET_FALSE(cbor_encoder_create_array(&root_array_encoder, &actuators_array, actuator_count));

    gble_actuator_feature* actuator = actuators;
    for (size_t idx = 0; idx < actuator_count; ++idx)
    {
        CborEncoder tmp;
        CBOR_CHECKED_RET_FALSE(cbor_encoder_create_array(&actuators_array, &tmp, 5));

        CBOR_CHECKED_RET_FALSE(cbor_encode_text_stringz(&tmp, actuator->description));
        CBOR_CHECKED_RET_FALSE(cbor_encode_uint(&tmp, actuator->feature_type));
        CBOR_CHECKED_RET_FALSE(cbor_encode_uint(&tmp, actuator->step_range_low));
        CBOR_CHECKED_RET_FALSE(cbor_encode_uint(&tmp, actuator->step_range_high));
        CBOR_CHECKED_RET_FALSE(cbor_encode_uint(&tmp, actuator->message_type));

        CBOR_CHECKED_RET_FALSE(cbor_encoder_close_container(&actuators_array, &tmp));

        actuator->id = idx;

        ++actuator;
    }

    CBOR_CHECKED_RET_FALSE(cbor_encoder_close_container(&root_array_encoder, &actuators_array));

    // Now add the sensors
    CborEncoder sensors_array;
    CBOR_CHECKED_RET_FALSE(cbor_encoder_create_array(&root_array_encoder, &sensors_array, sensor_count));

    gble_sensor_feature* sensor = sensors;
    for (size_t idx = 0; idx < sensor_count; ++idx)
    {
        CborEncoder tmp;
        CBOR_CHECKED_RET_FALSE(cbor_encoder_create_array(&sensors_array, &tmp, 5));

        CBOR_CHECKED_RET_FALSE(cbor_encode_text_stringz(&tmp, sensor->description));
        CBOR_CHECKED_RET_FALSE(cbor_encode_uint(&tmp, sensor->feature_type));
        CBOR_CHECKED_RET_FALSE(cbor_encode_uint(&tmp, sensor->value_range_low));
        CBOR_CHECKED_RET_FALSE(cbor_encode_uint(&tmp, sensor->value_range_high));
        CBOR_CHECKED_RET_FALSE(cbor_encode_uint(&tmp, sensor->message_type));

        CBOR_CHECKED_RET_FALSE(cbor_encoder_close_container(&sensors_array, &tmp));

        sensor->id = idx;

        ++sensor;
    }

    CBOR_CHECKED_RET_FALSE(cbor_encoder_close_container(&root_array_encoder, &sensors_array));

    CBOR_CHECKED_RET_FALSE(cbor_encoder_close_container(&root_encoder, &root_array_encoder));

    server->descriptor_size = cbor_encoder_get_buffer_size(&root_encoder, server->descriptor_buffer);

    return true;
}

void gble_set_sensor_callback_fn(gble_server* server, gble_sensor_callback_fn* cb, void* cb_context)
{
    server->sensor_cb = cb;
    server->sensor_cb_context = cb_context;
}

void gble_handle_actuators_changed(gble_server* server, uint8_t* buf, size_t buf_size)
{
    CborParser parser;
    CborValue root;

    CBOR_CHECKED(cbor_parser_init(buf, buf_size, 0, &parser, &root));

    if (!cbor_value_is_array(&root))
    {
        ESP_LOGE(TAG, "Expected actuators message to be an array, got: %hhu",
                 cbor_value_get_type(&root));
        return;
    }

    size_t array_len;
    CBOR_CHECKED(cbor_value_get_array_length(&root, &array_len));

    if (array_len != 2)
    {
        ESP_LOGE(TAG, "Expected 2 elements in message, got: %zu",
                 array_len);
        return;
    }

    CborValue item;
    CBOR_CHECKED(cbor_value_enter_container(&root, &item));

    // Check and get actuator id first

    if (!cbor_value_is_integer(&item))
    {
        ESP_LOGE(TAG, "Expected integer for actuator id, got: %hhu",
                 cbor_value_get_type(&root));
        return;
    }

    // TODO: Not sure how to actually get a uint32_t here
    int int_value;
    CBOR_CHECKED(cbor_value_get_int(&item, &int_value));

    uint32_t actuator_id = (uint32_t)int_value;
    if (actuator_id >= server->actuator_count)
    {
        ESP_LOGE(TAG, "Invalid actuator id, got: %lu",
                 actuator_id);
        return;
    }

    CBOR_CHECKED(cbor_value_advance(&item));

    // Check and get actuator value second

    if (!cbor_value_is_integer(&item))
    {
        ESP_LOGE(TAG, "Expected integer for actuator value, got: %hhu",
                 cbor_value_get_type(&root));
        return;
    }

    // TODO: Not sure how to actually get a uint32_t here
    CBOR_CHECKED(cbor_value_get_int(&item, &int_value));

    gble_actuator_feature* actuator = &server->actuators[actuator_id];

    uint32_t new_value = (uint32_t)int_value;

    uint32_t last_value = actuator->last_value;

    if (new_value != last_value && actuator->cb)
    {
        actuator->cb(actuator_id, new_value, actuator->cb_context);
    }

    actuator->last_value = new_value;
}

uint8_t* gble_get_descriptor(gble_server* server, size_t* descriptor_size)
{
    *descriptor_size = server->descriptor_size;
    return server->descriptor_buffer;
}

bool gble_set_sensor_value(gble_server* server, gble_sensor_id id, int32_t value)
{
    if (id >= server->sensors_count)
    {
        ESP_LOGE(TAG, "Invalid sensor ID %lu", id);
        return false;
    }

    server->sensors[id].last_value = value;

    if (server->sensor_cb)
    {
        uint8_t tmp_buf[512];

        CborEncoder root_encoder;

        cbor_encoder_init(&root_encoder, tmp_buf, sizeof(tmp_buf), 0);

        CborEncoder array_encoder;

        CBOR_CHECKED_RET_FALSE(cbor_encoder_create_array(&root_encoder, &array_encoder, 2));

        CBOR_CHECKED_RET_FALSE(cbor_encode_uint(&array_encoder, id));

        CBOR_CHECKED_RET_FALSE(cbor_encode_int(&array_encoder, value));

        CBOR_CHECKED_RET_FALSE(cbor_encoder_close_container(&root_encoder, &array_encoder));

        const size_t buf_size = cbor_encoder_get_buffer_size(&root_encoder, tmp_buf);

        server->sensor_cb(tmp_buf, buf_size, server->sensor_cb_context);
    }

    return true;
}

// Wrapper functions to work with other APIs
void gble_handle_actuators_changed_ctx(uint8_t* buf, size_t buf_size, void* context)
{
    gble_handle_actuators_changed((gble_server*)context, buf, buf_size);
}

uint8_t* gble_get_descriptor_ctx(size_t* buf_size, void* context)
{
    return gble_get_descriptor((gble_server*)context, buf_size);
}
