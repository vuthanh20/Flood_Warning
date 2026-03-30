#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "firebase_client.h" 
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_http_client.h" 
#include "esp_crt_bundle.h" // Thư viện chứng chỉ bảo mật HTTPS (Bắt buộc cho Firebase)

static const char *TAG = "The_Hidden_Gems";

// ==========================================
// CẤU HÌNH TRẠM ĐO (Sửa NODE_ID khi nạp cho mạch khác)
// ==========================================
#define NODE_ID "tram_do_2"  // Ví dụ: Đổi thành "tram_do_2" cho mạch thứ 2

// Link gốc của Firebase (Lưu ý không có dấu / ở cuối)
#define FIREBASE_HOST "https://flood-c8eda-default-rtdb.asia-southeast1.firebasedatabase.app"


// ==========================================
// HÀM ĐẨY LỊCH SỬ (Dùng HTTP POST để lưu danh sách vẽ biểu đồ)
// ==========================================
void firebase_post_history(const char* node_id, int water_level, const char* status) {
    char url[256];
    char post_data[200];
    
    // Tự động lắp ghép đường link lịch sử theo tên trạm: /lich_su/tram_do_1.json
    sprintf(url, "%s/lich_su/%s.json", FIREBASE_HOST, node_id);
    
    // Đóng gói chuỗi JSON với biến thời gian tự động của server Firebase (".sv": "timestamp")
    sprintf(post_data, "{\"muc_nuoc\": %d, \"trang_thai\": \"%s\", \"timestamp\": {\".sv\": \"timestamp\"}}", water_level, status);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .crt_bundle_attach = esp_crt_bundle_attach, // Khắc phục lỗi ESP_ERR_MBEDTLS_SSL_SETUP_FAILED
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Đã lưu Lịch sử [%s]: %d cm - %s", node_id, water_level, status);
    } else {
        ESP_LOGE(TAG, "Lỗi khi lưu Lịch sử [%s]: %s", node_id, esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
}


// ==========================================
// LUỒNG XỬ LÝ CHÍNH CỦA CẢM BIẾN
// ==========================================
void firebase_task(void *pvParameters) {
    int water_level = 0;
    const char* status = "An toan";
    char current_url[256];

    // Tạo link cập nhật trạng thái hiện tại cho trạm này: /tram_do_1.json
    sprintf(current_url, "%s/%s.json", FIREBASE_HOST, NODE_ID);

    while (1) {
        // [TODO] Giả lập logic nước dâng - Sau này thay bằng hàm đọc cảm biến thực tế của bạn
        water_level += 5; 
        if (water_level > 100) {
            status = "Nguy hiem";
        } else if (water_level > 50) {
            status = "Canh bao";
        } else {
            status = "An toan";
        }

        ESP_LOGI(TAG, "====================================");
        ESP_LOGI(TAG, "Đang đồng bộ dữ liệu trạm: %s", NODE_ID);

        // 1. CẬP NHẬT TỨC THỜI: Ghi đè dữ liệu để Dashboard và Bản đồ nháy số nhanh nhất
        firebase_push_data(current_url, water_level, status);

        // 2. LƯU LỊCH SỬ: Tạo một dòng mới để vẽ Biểu đồ đường
        firebase_post_history(NODE_ID, water_level, status);

        // Chờ 15 giây trước khi đo và gửi mẻ dữ liệu tiếp theo
        vTaskDelay(pdMS_TO_TICKS(15000)); 
    }
}


// ==========================================
// HÀM KHỞI TẠO (CHẠY 1 LẦN KHI BẬT NGUỒN)
// ==========================================
void app_main(void) {
    // Khởi tạo bộ nhớ NVS (Bắt buộc cho kết nối Wi-Fi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_LOGI(TAG, "Hệ thống Cảnh báo ngập lụt đang khởi động...");
    ESP_LOGI(TAG, "Đang kết nối Wi-Fi...");
    
    // Kết nối Wi-Fi thông qua cấu hình example_connect()
    ESP_ERROR_CHECK(example_connect());
    ESP_LOGI(TAG, "Wi-Fi đã kết nối thành công!");

    // Tạo luồng chạy độc lập cho tác vụ đo đạc & đẩy Firebase
    xTaskCreate(&firebase_task, "firebase_task", 8192, NULL, 5, NULL);
}