#include "push_setup.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "firebase_client.h" // Thư viện Firebase của bạn

// GỌI API CỦA COMPONENT ĐO LƯỜNG
#include "measure_setup.h"

static const char *TAG = "Push_Setup";
#define NODE_ID "tram_do_1"
#define FIREBASE_HOST "https://flood-c8eda-default-rtdb.asia-southeast1.firebasedatabase.app"

static void firebase_task(void *pvParameters) {
    char current_url[256];
    sprintf(current_url, "%s/%s.json", FIREBASE_HOST, NODE_ID);

    while (1) {
        float wl_send = 0.0f;
        char stat_send[20];

        // Lấy dữ liệu an toàn từ component measure_distance
        if (measure_get_latest_data(&wl_send, stat_send)) {
            ESP_LOGI(TAG, "Đang đẩy dữ liệu -> Nước: %.1f cm | TT: %s", wl_send, stat_send);
            
            // Hàm của bạn (Có thể cần ép kiểu (int)wl_send tùy header firebase)
            firebase_push_data(current_url, (int)wl_send, stat_send); 
            // firebase_post_history(NODE_ID, wl_send, stat_send); // Tùy chọn
        } else {
            ESP_LOGW(TAG, "Dữ liệu cảm biến không hợp lệ, bỏ qua chu kỳ push.");
        }

        vTaskDelay(pdMS_TO_TICKS(15000));
    }
}

void push_setup_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_LOGI(TAG, "Đang kết nối Wi-Fi...");
    ESP_ERROR_CHECK(example_connect());
    
    xTaskCreate(firebase_task, "firebase_task", 8192, NULL, 4, NULL);
    ESP_LOGI(TAG, "Push Task Initialized");
}