#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "firebase_setup.h"

static const char *TAG = "MOCK_MAIN";

// === CẦU NỐI DỮ LIỆU (SHARED VARIABLES) ===
// Khai báo biến toàn cục để truyền dữ liệu ảo sang luồng Firebase
float shared_mock_wl = 150.0f;
char shared_mock_status[20] = "Binh thuong";

// === LUỒNG TẠO DỮ LIỆU GIẢ (1 GIÂY / LẦN) ===
static void terminal_monitor_task(void *pvParameters) {
    float fake_step = -5.0f; // Mỗi giây giảm 5cm (mô phỏng nước dâng)

    while (1) {
        // 1. Cập nhật dữ liệu ảo liên tục
        shared_mock_wl += fake_step;
        if (shared_mock_wl <= 20.0f || shared_mock_wl >= 150.0f) {
            fake_step = -fake_step; // Đảo chiều khi chạm ngưỡng
        }

        // 2. Định tuyến trạng thái cảnh báo
        if (shared_mock_wl <= 50.0f) {
            strcpy(shared_mock_status, "Canh bao");
        } else {
            strcpy(shared_mock_status, "An toan");
        }

        ESP_LOGI(TAG, "💧 Mực nước (ẢO): %.1f cm | %s", shared_mock_wl, shared_mock_status);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "=== KHỞI ĐỘNG HỆ THỐNG (CHẾ ĐỘ MOCK SENSOR) ===");

    // 1. Đóng băng phần cứng đo lường thật để không báo lỗi
    // measure_setup_init();

    // 2. Khởi động mạng và Firebase (Chạy thật 100%)
    push_setup_init();

    // 3. Khởi động luồng in Terminal
    xTaskCreate(terminal_monitor_task, "monitor_task", 2048, NULL, 5, NULL);
}