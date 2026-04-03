#include "firebase_setup.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "firebase_client.h"
#include "measure_setup.h"
#include "esp_crt_bundle.h"

// =========================================================
// 🌟 CÔNG TẮC CHUYỂN NODE (MASTER SWITCH) 🌟
// Bạn muốn nạp code cho Node nào thì sửa số ở đây thành 1 hoặc 2
#define ACTIVE_NODE 1
// =========================================================

// === CẤU HÌNH MẠNG & FIREBASE CHUNG ===
#define WIFI_SSID      "P502"  // <-- Điền WiFi
#define WIFI_PASS      "55555555"     // <-- Điền Pass WiFi
#define MAXIMUM_RETRY  5

#define API_KEY        "AIzaSyAeHzNYqR72furmtDr-o3zK1fcYwpaHctY"
#define FIREBASE_HOST  "https://flood-c8eda-default-rtdb.asia-southeast1.firebasedatabase.app"

// === TỰ ĐỘNG CẤU HÌNH THEO NODE ĐÃ CHỌN ===
#if ACTIVE_NODE == 1
    #define NODE_ID    "tram_do_1"
    #define NODE_EMAIL "node1@gmail.com"
    #define NODE_PASS  "66666666"
    static const char *TAG = "Node_1_Secure";
#elif ACTIVE_NODE == 2
    #define NODE_ID    "tram_do_2"
    #define NODE_EMAIL "node2@gmail.com"
    #define NODE_PASS  "88888888"
    static const char *TAG = "Node_2_Secure";
#else
    #error "Vui lòng chọn ACTIVE_NODE là 1 hoặc 2"
#endif

// Biến lưu trữ Token bảo mật sau khi đăng nhập
static char id_token[2048] = {0}; 

// === QUẢN LÝ KẾT NỐI WIFI ===
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static int s_retry_num = 0;

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Thử kết nối lại WiFi lần %d...", s_retry_num);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);
    wifi_config_t wifi_config = { .sta = { .ssid = WIFI_SSID, .password = WIFI_PASS } };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Đang chờ kết nối WiFi...");
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
}

// === HÀM ĐĂNG NHẬP LẤY TOKEN TỪ GOOGLE ===
// === HÀM ĐĂNG NHẬP LẤY TOKEN TỪ GOOGLE (BẢN FIX LỖI CRASH) ===
bool firebase_login() {
    ESP_LOGI(TAG, "Đang gửi yêu cầu đăng nhập cho %s...", NODE_ID);
    char auth_url[256];
    sprintf(auth_url, "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=%s", API_KEY);

    esp_http_client_config_t config = {
        .url = auth_url,
        .method = HTTP_METHOD_POST,
        .crt_bundle_attach = esp_crt_bundle_attach, 
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");

    char post_data[128];
    sprintf(post_data, "{\"email\":\"%s\",\"password\":\"%s\",\"returnSecureToken\":true}", NODE_EMAIL, NODE_PASS);
    
    bool login_success = false;

    // 1. Mở kết nối và khai báo độ dài gói tin gửi đi
    esp_err_t err = esp_http_client_open(client, strlen(post_data));
    if (err == ESP_OK) {
        // 2. Bắn dữ liệu Email/Pass lên Google
        esp_http_client_write(client, post_data, strlen(post_data));
        
        // 3. Đọc Header trả về (Bắt buộc phải gọi)
        esp_http_client_fetch_headers(client);
        
        // 4. Tạo một cái giỏ tĩnh 2048 byte (để chống lỗi Crash StoreProhibited)
        char response[2048] = {0};
        int read_len = esp_http_client_read_response(client, response, sizeof(response) - 1);
        
        if (read_len > 0) {
            cJSON *json = cJSON_Parse(response);
            if (json != NULL) {
                cJSON *token_item = cJSON_GetObjectItem(json, "idToken");
                if (token_item != NULL && cJSON_IsString(token_item)) {
                    strncpy(id_token, token_item->valuestring, sizeof(id_token) - 1);
                    ESP_LOGI(TAG, "--- ĐĂNG NHẬP THÀNH CÔNG! ĐÃ LẤY ĐƯỢC TOKEN ---");
                    login_success = true;
                } else {
                    ESP_LOGE(TAG, "Sai Email hoặc Password! Phản hồi: %s", response);
                }
                cJSON_Delete(json);
            }
        } else {
            ESP_LOGE(TAG, "Không đọc được dữ liệu phản hồi từ Firebase");
        }
    } else {
        ESP_LOGE(TAG, "Lỗi kết nối HTTP: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
    return login_success;
}

// // === LUỒNG LÀM VIỆC CHÍNH ===
// static void firebase_task(void *pvParameters) {
//     while (!firebase_login()) {
//         ESP_LOGW(TAG, "Đăng nhập thất bại, thử lại sau 5 giây...");
//         vTaskDelay(pdMS_TO_TICKS(5000));
//     }

//     char current_url[2560];
//     sprintf(current_url, "%s/nodes/%s/telemetry.json?auth=%s", FIREBASE_HOST, NODE_ID, id_token);

//     while (1) {
//         float wl_send = 0.0f;
//         char stat_send[20];

//         if (measure_get_latest_data(&wl_send, stat_send)) {
//             ESP_LOGI(TAG, "Bắn Firebase -> Nước: %.1f cm", wl_send);
//             firebase_push_data(current_url, wl_send, stat_send); 
//         }
//         vTaskDelay(pdMS_TO_TICKS(15000));
//     }
// }

// void push_setup_init(void) {
//     esp_err_t ret = nvs_flash_init();
//     if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
//       ESP_ERROR_CHECK(nvs_flash_erase());
//       ret = nvs_flash_init();
//     }
//     ESP_ERROR_CHECK(ret);

//     wifi_init_sta();
//     xTaskCreate(firebase_task, "firebase_task", 20480, NULL, 4, NULL);
// }

extern float shared_mock_wl;
extern char shared_mock_status[20];

// === LUỒNG LÀM VIỆC CHÍNH (ĐÃ SỬA ĐỂ ĐỌC DỮ LIỆU ẢO) ===
static void firebase_task(void *pvParameters) {
    while (!firebase_login()) {
        ESP_LOGW(TAG, "Đăng nhập thất bại, thử lại sau 5 giây...");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    char current_url[2560];
    sprintf(current_url, "%s/nodes/%s/telemetry.json?auth=%s", FIREBASE_HOST, NODE_ID, id_token);

    while (1) {
        // Lấy số liệu ảo trực tiếp từ biến toàn cục của main.c
        float wl_send = shared_mock_wl; 
        char stat_send[20];
        strcpy(stat_send, shared_mock_status);

        // Bắn lên Firebase
        ESP_LOGI(TAG, "Bắn Firebase (DỮ LIỆU ẢO) -> Nước: %.1f cm", wl_send);
        firebase_push_data(current_url, wl_send, stat_send); 
        
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

    wifi_init_sta();
    xTaskCreate(firebase_task, "firebase_task", 20480, NULL, 4, NULL);
}