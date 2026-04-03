#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "firebase_client.h"

static const char *TAG = "FIREBASE_CLIENT";

esp_err_t firebase_push_data(const char* db_url, int muc_nuoc, const char* trang_thai) {
    // 1. Tạo dữ liệu JSON
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "muc_nuoc", muc_nuoc);
    cJSON_AddStringToObject(root, "trang_thai", trang_thai);
    
    char *post_data = cJSON_PrintUnformatted(root);
    if (post_data == NULL) {
        ESP_LOGE(TAG, "Lỗi khi tạo chuỗi JSON");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    // 2. Cấu hình HTTP Client (MỞ RỘNG TÚI CHỨA TX LÊN 4096 BYTE)
    esp_http_client_config_t config = {
        .url = db_url,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach, 
        .buffer_size = 2048,   // Túi hứng phản hồi
        .buffer_size_tx = 4096 // <--- SỬA THÀNH 4096 ĐỂ ĐỰNG VỪA TOKEN KHỔNG LỒ
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Dùng PATCH để cập nhật dữ liệu
    esp_http_client_set_method(client, HTTP_METHOD_PATCH);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    // 3. Thực thi gửi dữ liệu
    ESP_LOGI(TAG, "Đang gửi: %s", post_data);
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Thành công! HTTP Status: %d", esp_http_client_get_status_code(client));
    } else {
        ESP_LOGE(TAG, "Lỗi kết nối Firebase: %s", esp_err_to_name(err));
    }

    // 4. Giải phóng RAM (Bắt buộc)
    esp_http_client_cleanup(client);
    free(post_data);
    cJSON_Delete(root);

    return err;
}