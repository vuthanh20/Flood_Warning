#include "measure_setup.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include <string.h>

#include "hc_sr04_rmt.h"
#include "aht2x_sensor.h"
#include "water_level_processor.h"

static const char *TAG = "Measure_Setup";

#define DISTANCE_TO_BOTTOM_CM 200.0f

// Biến cục bộ để lưu dữ liệu mới nhất
static float s_current_water_level = 0.0f;
static char s_current_status[20] = "An toan";
static bool s_data_valid = false;

// Mutex bảo vệ dữ liệu khi chạy đa luồng
static SemaphoreHandle_t s_data_mutex = NULL;

static void measure_task(void *pvParameters) {
    hc_sr04_rmt_config_t hc_cfg = { .trig_gpio = 18, .echo_gpio = 19, .rmt_resolution_hz = 1000000, .max_distance_cm = 400 };
    hc_sr04_rmt_init(&hc_cfg);

    aht2x_config_t aht_cfg = { .i2c_port = I2C_NUM_0, .sda_pin = 21, .scl_pin = 22 };
    aht2x_init(&aht_cfg);

    wl_processor_config_t wl_cfg = { .calib_scale_a = 1.012f, .calib_offset_b = -0.402f };
    wl_processor_init(&wl_cfg);

    while (1) {
        float temp_c = 28.0f; 
        int64_t tof_us = 0;

        aht2x_read_temperature(&temp_c);
        esp_err_t err = hc_sr04_rmt_measure_us(&tof_us);
        
        // Khóa Mutex để cập nhật dữ liệu an toàn
        xSemaphoreTake(s_data_mutex, portMAX_DELAY);
        
        if (err == ESP_OK) {
            float raw_dist = wl_processor_compute_raw(tof_us, temp_c);
            float filtered_dist = wl_processor_apply_filter(raw_dist);
            float final_dist = wl_processor_apply_calibration(filtered_dist);

            float water_level = DISTANCE_TO_BOTTOM_CM - final_dist;
            if (water_level < 0) water_level = 0.0f; 

            if (water_level > 100.0f) strcpy(s_current_status, "Nguy hiem");
            else if (water_level > 50.0f) strcpy(s_current_status, "Canh bao");
            else strcpy(s_current_status, "An toan");

            s_current_water_level = water_level;
            s_data_valid = true;
        } else {
            s_data_valid = false;
        }
        
        xSemaphoreGive(s_data_mutex);
        vTaskDelay(pdMS_TO_TICKS(500)); 
    }
}

void measure_setup_init(void) {
    s_data_mutex = xSemaphoreCreateMutex();
    xTaskCreate(measure_task, "measure_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "Measure Task Initialized");
}

bool measure_get_latest_data(float *water_level, char *status_buffer) {
    bool valid = false;
    if (xSemaphoreTake(s_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (s_data_valid) {
            *water_level = s_current_water_level;
            strcpy(status_buffer, s_current_status);
            valid = true;
        }
        xSemaphoreGive(s_data_mutex);
    }
    return valid;
}