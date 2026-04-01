#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// Gọi các thư viện Component của bạn
#include "measure_setup.h"
#include "firebase_setup.h"
// #include "sd_logger.h" // Tạm thời comment lại nếu bạn chưa cắm thẻ SD

static const char *TAG = "MONITOR";

// === LUỒNG HIỂN THỊ TERMINAL (1 GIÂY / LẦN) ===
static void terminal_monitor_task(void *pvParameters) {
    while (1) {
        float water_level = 0.0f;
        char status[20];

        // Lấy dữ liệu mới nhất từ cảm biến
        if (measure_get_latest_data(&water_level, status)) {
            // In ra Terminal với màu xanh lá cây (chữ I trong ESP_LOGI)
            ESP_LOGI(TAG, "💧 Mực nước hiện tại: %.1f cm | Trạng thái: %s", water_level, status);
        } else {
            ESP_LOGW(TAG, "⏳ Đang chờ dữ liệu từ cảm biến...");
        }
        
        // Dừng 1 giây (1000 ms) rồi mới in tiếp để đỡ trôi màn hình
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "=== KHỞI ĐỘNG HỆ THỐNG CẢNH BÁO NGẬP LỤT ===");

    // 1. Khởi động hệ thống đo lường (Cảm biến HC-SR04)
    measure_setup_init();

    // 2. Khởi động mạng và Firebase (Đẩy data 15s/lần)
    push_setup_init();

    // 3. Khởi động luồng In ra màn hình Terminal (1s/lần)
    xTaskCreate(terminal_monitor_task, "monitor_task", 2048, NULL, 5, NULL);
}