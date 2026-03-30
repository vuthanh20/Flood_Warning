#include "firebase_setup.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "firebase_client.h"
#include "measure_setup.h" // Gọi API đo lường

// === CẤU HÌNH WIFI CỦA BẠN Ở ĐÂY ===
#define WIFI_SSID      "P502"
#define WIFI_PASS      "55555555"
#define MAXIMUM_RETRY  5

static const char *TAG = "Firebase_Setup";
#define NODE_ID "tram_do_1"
#define FIREBASE_HOST "https://flood-c8eda-default-rtdb.asia-southeast1.firebasedatabase.app"

// Biến quản lý trạng thái WiFi
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static int s_retry_num = 0;

// Hàm xử lý sự kiện WiFi (Bắt sự kiện kết nối, rớt mạng, lấy IP)
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Đang thử kết nối lại WiFi lần %d...", s_retry_num);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Đã lấy được IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// Hàm khởi tạo WiFi chuẩn
void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "Đang chờ kết nối WiFi...");
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Kết nối THÀNH CÔNG tới SSID: %s", WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Kết nối THẤT BẠI tới SSID: %s", WIFI_SSID);
    }
}

// Luồng đẩy dữ liệu Firebase (Giữ nguyên logic của bạn)
static void firebase_task(void *pvParameters) {
    char current_url[256];
    sprintf(current_url, "%s/%s.json", FIREBASE_HOST, NODE_ID);

    while (1) {
        float wl_send = 0.0f;
        char stat_send[20];

        if (measure_get_latest_data(&wl_send, stat_send)) {
            ESP_LOGI(TAG, "Bắn Firebase -> Nước: %.1f cm | TT: %s", wl_send, stat_send);
            // Giả sử hàm thư viện của bạn nhận tham số (char*, float, char*)
            // Hãy sửa lại kiểu dữ liệu của hàm này nếu thư viện của bạn yêu cầu (int)
            firebase_push_data(current_url, wl_send, stat_send); 
        } else {
            ESP_LOGW(TAG, "Dữ liệu đo lường chưa sẵn sàng.");
        }
        vTaskDelay(pdMS_TO_TICKS(15000));
    }
}

// Điểm bắt đầu của Component
void push_setup_init(void) {
    // Khởi tạo NVS Flash (Bắt buộc cho WiFi lưu mật khẩu ngầm)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 1. Kết nối mạng
    wifi_init_sta();
    
    // 2. Chạy luồng đẩy dữ liệu
    xTaskCreate(firebase_task, "firebase_task", 8192, NULL, 4, NULL);
    ESP_LOGI(TAG, "Firebase Task Initialized");
}