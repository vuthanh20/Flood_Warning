#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "firebase_client.h"

// Đặt tên thẻ TAG để dễ nhìn trên Terminal
static const char *TAG = "FIREBASE_CLIENT";

// ==========================================
// HÀM ĐẨY DỮ LIỆU HIỆN TẠI LÊN FIREBASE
// ==========================================
esp_err_t firebase_push_data(const char* db_url, int muc_nuoc, const char* trang_thai) {
    // 1. Đóng gói dữ liệu thành JSON
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "muc_nuoc", muc_nuoc);
    cJSON_AddStringToObject(root, "trang_thai", trang_thai);
    
    char *post_data = cJSON_PrintUnformatted(root);
    if (post_data == NULL) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    // 2. Cấu hình HTTP Client
    esp_http_client_config_t config = {
        .url = db_url,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach, // Khắc phục lỗi HTTPS
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    // Dùng lệnh PATCH để cập nhật dữ liệu (không xóa các node khác)
    esp_http_client_set_method(client, HTTP_METHOD_PATCH);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    // 3. Thực thi và KIỂM TRA LỖI (In Log ra Terminal)
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code == 200) {
            ESP_LOGI(TAG, "[ĐẨY DỮ LIỆU] Thành công cập nhật tức thời: %d cm", muc_nuoc);
        } else {
            ESP_LOGW(TAG, "[CẢNH BÁO] Đẩy dữ liệu bị lỗi HTTP Status: %d", status_code);
        }
    } else {
        ESP_LOGE(TAG, "[LỖI] Không thể đẩy dữ liệu tức thời: %s", esp_err_to_name(err));
    }

    // 4. Dọn dẹp RAM
    esp_http_client_cleanup(client);
    free(post_data);
    cJSON_Delete(root);
    
    return err;
}

// ==========================================
// HÀM TẢI DỮ LIỆU JSON TỪ FIREBASE VỀ
// ==========================================
esp_err_t firebase_get_json(const char* url, char* response_buffer, size_t max_len) {
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return ESP_FAIL;

    esp_err_t err = esp_http_client_open(client, 0);
    if (err == ESP_OK) {
        esp_http_client_fetch_headers(client);
        int read_len = esp_http_client_read_response(client, response_buffer, max_len - 1);
        if (read_len >= 0) {
            response_buffer[read_len] = '\0'; 
        } else {
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "[LỖI] Mở kết nối tải dữ liệu thất bại: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
    return err;
}