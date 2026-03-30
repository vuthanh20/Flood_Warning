#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "hc_sr04_rmt.h"
#include "aht2x_sensor.h"
#include "water_level_processor.h"

static const char *TAG = "main";

void app_main(void)
{
    // 1. Khởi tạo Cảm biến Siêu âm
    hc_sr04_rmt_config_t hc_cfg = {
        .trig_gpio = GPIO_NUM_18,
        .echo_gpio = GPIO_NUM_19,
        .rmt_resolution_hz = 1000000, 
        .max_distance_cm = 400
    };
    if (hc_sr04_rmt_init(&hc_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Loi khoi tao HC-SR04");
    }

    // 2. Khởi tạo Cảm biến Nhiệt độ (SDA=21, SCL=22)
    aht2x_config_t aht_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_pin = 21, 
        .scl_pin = 22
    };
    if (aht2x_init(&aht_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Loi khoi tao AHT2x I2C");
    }

    // 3. Khởi tạo Processor
    wl_processor_init(NULL);

    ESP_LOGI(TAG, "He thong san sang! Dang thu thap du lieu...");

    while (1) {
        float temp_c = 25.0f; // Nhiệt độ mặc định nếu AHT2x lỗi
        int64_t tof_us = 0;

        // B1: Đọc nhiệt độ môi trường
        if (aht2x_read_temperature(&temp_c) != ESP_OK) {
            ESP_LOGW(TAG, "Khong doc duoc AHT2x, dung mac dinh 25C");
        }

        // B2: Bắn sóng siêu âm và đo thời gian
        esp_err_t r = hc_sr04_rmt_measure_us(&tof_us);
        
        if (r == ESP_OK) {
            // B3: Tính toán khoảng cách có bù nhiệt
            float raw_dist = wl_processor_compute_raw(tof_us, temp_c);
            
            // B4: Lọc nhiễu (hiện tại chưa có filter nên nó giữ nguyên)
            float filtered_dist = wl_processor_apply_filter(raw_dist);
            
            // B5: Calib chốt số liệu
            float final_dist = wl_processor_apply_calibration(filtered_dist);

            ESP_LOGI(TAG, "Temp: %.1fC | Time: %lld us | Dist: %.2f cm", 
                     temp_c, tof_us, final_dist);
                     
        } else if (r == ESP_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "Timeout - Khong thay song dội");
        } else {
            ESP_LOGE(TAG, "Loi do luong HC-SR04: %d", r);
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}