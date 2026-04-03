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
#include "esp_crt_bundle.h" 
#include "cJSON.h" // Thư viện bóc tách JSON

static const char *TAG = "The_Hidden_Gems";

#define NODE_ID "tram_do_1"  
#define FIREBASE_HOST "https://flood-c8eda-default-rtdb.asia-southeast1.firebasedatabase.app"

// Các biến lưu trữ cấu hình (Sẽ được cập nhật liên tục từ Firebase)
int threshold_warning = 25; // Ngưỡng Cảnh báo mặc định
int threshold_danger = 40;  // Ngưỡng Nguy hiểm mặc định
bool alarm_enabled = false; // Nút gạt bật/tắt loa trên Web

// Hàm đẩy lịch sử (Giữ nguyên)
void firebase_post_history(const char* node_id, int water_level, const char* status) {
    char url[256];
    char post_data[200];
    sprintf(url, "%s/lich_su/%s.json", FIREBASE_HOST, node_id);
    sprintf(post_data, "{\"muc_nuoc\": %d, \"trang_thai\": \"%s\", \"timestamp\": {\".sv\": \"timestamp\"}}", water_level, status);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}

// Luồng xử lý chính
void firebase_task(void *pvParameters) {
    int water_level = 0;
    const char* status = "An toan";
    
    char url_buffer[256];
    char json_buffer[512]; // Bộ đệm chứa chuỗi JSON tải về

    while (1) {
        ESP_LOGI(TAG, "====================================");
        
        // ----------------------------------------------------
        // BƯỚC 1: LẤY CÀI ĐẶT NGƯỠNG NƯỚC TỪ FIREBASE
        // ----------------------------------------------------
        sprintf(url_buffer, "%s/nguong_canh_bao/%s.json", FIREBASE_HOST, NODE_ID);
        if (firebase_get_json(url_buffer, json_buffer, sizeof(json_buffer)) == ESP_OK) {
            
            // Ép kiểu chuỗi tải về thành đối tượng JSON
            cJSON *root = cJSON_Parse(json_buffer);
            if (root) {
                // Lấy thông số Cảnh báo
                cJSON *c_canh_bao = cJSON_GetObjectItem(root, "canh_bao");
                if (c_canh_bao && cJSON_IsNumber(c_canh_bao)) {
                    threshold_warning = c_canh_bao->valueint;
                }

                // Lấy thông số Nguy hiểm
                cJSON *c_nguy_hiem = cJSON_GetObjectItem(root, "nguy_hiem");
                if (c_nguy_hiem && cJSON_IsNumber(c_nguy_hiem)) {
                    threshold_danger = c_nguy_hiem->valueint;
                }
                
                cJSON_Delete(root); // Bắt buộc xóa để giải phóng RAM
            }
            // IN KẾT QUẢ RA TERMINAL ĐỂ KIỂM TRA
            ESP_LOGW(TAG, "[CẤU HÌNH] Ngưỡng cài đặt: Cảnh báo=%dcm | Nguy hiểm=%dcm", threshold_warning, threshold_danger);
        } else {
            ESP_LOGE(TAG, "[LỖI] Không tải được cấu hình ngưỡng nước!");
        }

        // ----------------------------------------------------
        // BƯỚC 2: LẤY LỆNH BẬT/TẮT LOA TỪ WEB
        // ----------------------------------------------------
        sprintf(url_buffer, "%s/canh_bao/%s.json", FIREBASE_HOST, NODE_ID);
        if (firebase_get_json(url_buffer, json_buffer, sizeof(json_buffer)) == ESP_OK) {
            
            cJSON *root = cJSON_Parse(json_buffer);
            if (root) {
                // Lấy trạng thái True/False của loa
                cJSON *c_trang_thai = cJSON_GetObjectItem(root, "trang_thai");
                if (c_trang_thai && cJSON_IsBool(c_trang_thai)) {
                    alarm_enabled = cJSON_IsTrue(c_trang_thai) ? true : false;
                }
                cJSON_Delete(root);
            }
            // IN KẾT QUẢ RA TERMINAL ĐỂ KIỂM TRA
            ESP_LOGW(TAG, "[CẤU HÌNH] Trạng thái Loa trên Web: %s", alarm_enabled ? "BẬT (TRUE)" : "TẮT (FALSE)");
        } else {
            ESP_LOGE(TAG, "[LỖI] Không tải được trạng thái loa!");
        }

        // ----------------------------------------------------
        // BƯỚC 3: ĐỌC CẢM BIẾN & ĐÁNH GIÁ (Dùng ngưỡng động vừa lấy)
        // ----------------------------------------------------
        water_level += 5; // Giả lập mức nước
        if (water_level > 60) water_level = 0; 

        if (water_level >= threshold_danger) {
            status = "Nguy hiem";
        } else if (water_level >= threshold_warning) {
            status = "Canh bao";
        } else {
            status = "An toan";
        }

        // ----------------------------------------------------
        // BƯỚC 4: GỬI KẾT QUẢ HIỆN TẠI & LỊCH SỬ LÊN LẠI FIREBASE
        // ----------------------------------------------------
        sprintf(url_buffer, "%s/%s.json", FIREBASE_HOST, NODE_ID);
        firebase_push_data(url_buffer, water_level, status);
        firebase_post_history(NODE_ID, water_level, status);

        vTaskDelay(pdMS_TO_TICKS(15000)); 
    }
}

void app_main(void) {
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
    ESP_LOGI(TAG, "Wi-Fi đã kết nối thành công!");

    xTaskCreate(&firebase_task, "firebase_task", 8192, NULL, 5, NULL);
}