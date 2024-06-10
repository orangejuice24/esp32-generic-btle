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

#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "nvs_flash.h"

#include "ble_func.h"
#include "gatt_svr.h"
#include "generic_btle.h"

/* for nvs_storage*/
#define LOCAL_NAMESPACE "storage"

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 0, 0)
typedef  nvs_handle nvs_handle_t;
#endif

static const char *TAG = "Main";

nvs_handle_t Nvs_storage_handle = 0;

void handle_actuator_change(gble_actuator_id actuator_id, uint32_t value, void* context)
{
    ESP_LOGI(TAG, "Actuator #%lu changed to: %lu", actuator_id, value);
}

gble_actuator_feature actuators[] = {
    {
        .description = "Actuator 1",
        .feature_type = GBLE_ACTUATOR_TYPE_VIBRATE,
        .step_range_low = 0,
        .step_range_high = 20,
        .message_type = GBLE_ACTUATOR_MSG_SCALAR,
        .cb = handle_actuator_change,
        .cb_context = NULL,
    },
    {
        .description = "Actuator 2",
        .feature_type = GBLE_ACTUATOR_TYPE_VIBRATE,
        .step_range_low = 0,
        .step_range_high = 20,
        .message_type = GBLE_ACTUATOR_MSG_SCALAR,
        .cb = handle_actuator_change,
        .cb_context = NULL,
    },
};

gble_sensor_feature sensors[] = {
    {
        .description = "Sensor 1",
        .feature_type = GBLE_SENSOR_TYPE_PRESSURE,
        .value_range_low = 0,
        .value_range_high = 16,
        .message_type = GBLE_SENSOR_MSG_SUBSCRIBE,
    },
    {
        .description = "State 1",
        .feature_type = GBLE_SENSOR_TYPE_BUTTON,
        .value_range_low = 0,
        .value_range_high = 2,
        .message_type = GBLE_SENSOR_MSG_SUBSCRIBE,
    },
};

gble_server gble_server_instance;

void app_main(void)
{
    /* Initialize NVS — it is used to store PHY calibration data and Nimble bonding data */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(nvs_open(LOCAL_NAMESPACE, NVS_READWRITE, &Nvs_storage_handle));

    if (!gble_init(&gble_server_instance, "Generic Device", actuators, COUNT_OF(actuators), sensors, COUNT_OF(sensors)))
    {
        ESP_LOGE(TAG, "Failed to initialize gble gble_server_instance");
        vTaskDelay(pdMS_TO_TICKS(30000));
        esp_restart();
    }

    if (!ble_init(gatt_svr_init))
    {
        ESP_LOGE(TAG, "Failed to initialize ble stack");
        vTaskDelay(pdMS_TO_TICKS(30000));
        esp_restart();
    }

    ble_func_register_disconnect_cb(gatt_svr_client_disconnected_ctx, NULL);
    ble_func_register_subscribe_cb(gatt_svr_handle_subscribe_ctx, NULL);

    gatt_svr_register_descriptor_cb(gble_get_descriptor_ctx, &gble_server_instance);
    gatt_svr_register_write_cb(gble_handle_actuators_changed_ctx, &gble_server_instance);

    gble_set_sensor_callback_fn(&gble_server_instance, gatt_svr_set_read_value_ctx, NULL);

    ESP_LOGI(TAG, "BLE init ok");

    // Simulate changing sensor value
    bool increment = true;
    int32_t sensor_value = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        gble_set_sensor_value(&gble_server_instance, sensors[0].id, sensor_value);

        gble_set_sensor_value(&gble_server_instance, sensors[1].id, sensor_value % 3);

        sensor_value += (increment) ? 1 : -1;

        if (sensor_value == sensors[0].value_range_low || sensor_value == sensors[0].value_range_high)
        {
            increment = !increment;
        }
    }
}
