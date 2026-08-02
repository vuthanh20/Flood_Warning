#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "firebase_client.h" 
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_http_client.h" 
#include "esp_crt_bundle.h"
#include "cJSON.h" 
#include "esp_sntp.h"
#include <time.h>
#include <sys/time.h>

#include "hc_sr04_rmt.h"
#include "aht2x_sensor.h" 
#include "water_level_processor.h"
#include "ds3231_rtc.h" 
#include "ssd1306_oled.h"

static const char *TAG = "The_Hidden_Gems";

#define NODE_ID "tram_do_1"
#define FIREBASE_HOST "https://flood-c8eda-default-rtdb.asia-southeast1.firebasedatabase.app"
#define SENSOR_HEIGHT_CM 200.0f 

#define BUZZER_PIN 33
#define BUZZER_ON  1 
#define BUZZER_OFF 0  

volatile float g_thresh_warning = 25.0f; 
volatile float g_thresh_danger  = 40.0f; 
volatile bool g_web_alarm_enabled = false; 
volatile bool g_current_buzzer_state = false; 
volatile bool g_cmd_mute = false;   
volatile bool g_cmd_unmute = false; 

typedef struct {
    float water_level;
    char status[20];
    int buzzer_sync_cmd; 
    char rtc_time[32]; 
} sensor_data_t;

QueueHandle_t firebase_queue; 

void firebase_post_history(const char* node_id, float water_level, const char* status, const char* rtc_time) {
    char url[256];
    char post_data[256]; 
    sprintf(url, "%s/lich_su/%s.json", FIREBASE_HOST, node_id);
    
    sprintf(post_data, "{\"muc_nuoc\": %.1f, \"trang_thai\": \"%s\", \"thoi_gian_rtc\": \"%s\", \"timestamp\": {\".sv\": \"timestamp\"}}", 
            water_level, status, rtc_time);

    esp_http_client_config_t config = {
        .url = url, .method = HTTP_METHOD_POST, .crt_bundle_attach = esp_crt_bundle_attach, 
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}

void firebase_put_current(const char* node_id, float water_level, const char* status, const char* rtc_time) {
    char url[256];
    char post_data[256];
    sprintf(url, "%s/%s.json", FIREBASE_HOST, node_id);
    
    sprintf(post_data, "{\"muc_nuoc\": %.1f, \"trang_thai\": \"%s\", \"thoi_gian_rtc\": \"%s\"}", 
            water_level, status, rtc_time);

    esp_http_client_config_t config = {
        .url = url, .method = HTTP_METHOD_PUT, .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}

void firebase_patch_buzzer(const char* node_id, bool state) {
    char url[256];
    char post_data[50];
    sprintf(url, "%s/canh_bao/%s.json", FIREBASE_HOST, node_id);
    sprintf(post_data, "{\"trang_thai\": %s}", state ? "true" : "false");

    esp_http_client_config_t config = {
        .url = url, .method = HTTP_METHOD_PATCH, .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}

void firebase_task(void *pvParameters) {
    ESP_LOGI(TAG, "[MẠNG] Đang kết nối Wi-Fi ngầm...");
    if (example_connect() == ESP_OK) {
        ESP_LOGI(TAG, "[MẠNG] Wi-Fi OK! Bắt đầu đồng bộ Firebase.");

        // Khởi tạo đồng bộ thời gian SNTP
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
        esp_sntp_setservername(0, "pool.ntp.org");
        esp_sntp_setservername(1, "time.nist.gov");
        esp_sntp_init();
        setenv("TZ", "UTC-7", 1); // Timezone Việt Nam (UTC+7)
        tzset();
    }

    sensor_data_t rx_data;
    char url_buffer[256];
    char json_buffer[1024]; 
    TickType_t last_sync_time = 0;

    while (1) {
        if (xTaskGetTickCount() - last_sync_time >= pdMS_TO_TICKS(2000)) {
            sprintf(url_buffer, "%s/nguong_canh_bao/%s.json", FIREBASE_HOST, NODE_ID);
            if (firebase_get_json(url_buffer, json_buffer, sizeof(json_buffer)) == ESP_OK) {
                cJSON *root = cJSON_Parse(json_buffer);
                if (root) {
                    cJSON *c_warn = cJSON_GetObjectItem(root, "canh_bao");
                    if (c_warn && cJSON_IsNumber(c_warn)) g_thresh_warning = c_warn->valuedouble;
                    cJSON *c_danger = cJSON_GetObjectItem(root, "nguy_hiem");
                    if (c_danger && cJSON_IsNumber(c_danger)) g_thresh_danger = c_danger->valuedouble;
                    cJSON_Delete(root);
                }
            }

            sprintf(url_buffer, "%s/canh_bao/%s.json", FIREBASE_HOST, NODE_ID);
            if (firebase_get_json(url_buffer, json_buffer, sizeof(json_buffer)) == ESP_OK) {
                cJSON *root = cJSON_Parse(json_buffer);
                if (root) {
                    cJSON *c_status = cJSON_GetObjectItem(root, "trang_thai");
                    if (c_status && cJSON_IsBool(c_status)) {
                        bool new_web_state = cJSON_IsTrue(c_status);
                        if (g_web_alarm_enabled == true && new_web_state == false) g_cmd_mute = true;
                        if (g_web_alarm_enabled == false && new_web_state == true) g_cmd_unmute = true;
                        g_web_alarm_enabled = new_web_state;
                    }
                    cJSON_Delete(root);
                }
            }
            last_sync_time = xTaskGetTickCount();
        }

        // Đồng bộ thời gian DS3231 từ NTP (Lần đầu và mỗi 12 tiếng)
        static TickType_t last_ntp_sync = 0;
        time_t now = 0;
        struct tm timeinfo = { 0 };
        time(&now);
        localtime_r(&now, &timeinfo);
        
        // now > 1600000000 means year > 2020 (NTP is synced)
        if (now > 1600000000 && 
            (last_ntp_sync == 0 || (xTaskGetTickCount() - last_ntp_sync >= pdMS_TO_TICKS(12 * 60 * 60 * 1000)))) {
            
            ds3231_time_t rtc_time;
            rtc_time.year   = timeinfo.tm_year % 100;
            rtc_time.month  = timeinfo.tm_mon + 1;
            rtc_time.date   = timeinfo.tm_mday;
            rtc_time.day    = timeinfo.tm_wday + 1;
            rtc_time.hour   = timeinfo.tm_hour;
            rtc_time.minute = timeinfo.tm_min;
            rtc_time.second = timeinfo.tm_sec;
            
            if (ds3231_set_time(&rtc_time) == ESP_OK) {
                ESP_LOGI(TAG, "RTC cap nhat tu NTP: %02d/%02d/20%02d %02d:%02d:%02d",
                         rtc_time.date, rtc_time.month, rtc_time.year,
                         rtc_time.hour, rtc_time.minute, rtc_time.second);
                last_ntp_sync = xTaskGetTickCount() ? xTaskGetTickCount() : 1; // Đảm bảo khác 0
            } else {
                ESP_LOGW(TAG, "Loi ghi thoi gian xuong RTC!");
            }
        }

        if (xQueueReceive(firebase_queue, &rx_data, pdMS_TO_TICKS(100)) == pdPASS) {
            ESP_LOGW(TAG, "=> FIREBASE: %s | %.1f cm | %s", rx_data.rtc_time, rx_data.water_level, rx_data.status);
            
            firebase_put_current(NODE_ID, rx_data.water_level, rx_data.status, rx_data.rtc_time);
            firebase_post_history(NODE_ID, rx_data.water_level, rx_data.status, rx_data.rtc_time);

            if (rx_data.buzzer_sync_cmd == 1) {
                firebase_patch_buzzer(NODE_ID, true);
                g_web_alarm_enabled = true; 
            } else if (rx_data.buzzer_sync_cmd == 2) {
                firebase_patch_buzzer(NODE_ID, false);
                g_web_alarm_enabled = false;
            }
        }
    }
}

void sensor_task(void *pvParameters) {
    TickType_t last_sent_time = 0;
    static bool was_alert = false, was_danger = false, is_muted = false;
    static TickType_t mute_start_time = 0;
    
    const TickType_t NORMAL_INTERVAL = pdMS_TO_TICKS(10000); 
    const TickType_t ALERT_INTERVAL = pdMS_TO_TICKS(5000);   

    while (1) {
        float temp_c = 25.0f; 
        int64_t tof_us = 0;

        aht2x_read_temperature(&temp_c);
        esp_err_t r = hc_sr04_rmt_measure_us(&tof_us);
        
        if (r == ESP_OK) {
            float raw_dist = wl_processor_compute_raw(tof_us, temp_c);
            float filtered_dist = wl_processor_apply_filter(raw_dist);
            float final_dist = wl_processor_apply_calibration(filtered_dist);
            float current_water_level = SENSOR_HEIGHT_CM - final_dist;
            
            bool is_danger = (current_water_level >= g_thresh_danger);
            bool is_warning = (current_water_level >= g_thresh_warning);
            
            char current_status[20];
            if (is_danger) strcpy(current_status, "Nguy hiem");
            else if (is_warning) strcpy(current_status, "Canh bao");
            else strcpy(current_status, "An toan");

            if (g_cmd_mute) { is_muted = true; mute_start_time = xTaskGetTickCount(); g_cmd_mute = false; }
            if (g_cmd_unmute) { is_muted = false; g_cmd_unmute = false; }
            if (is_muted && (xTaskGetTickCount() - mute_start_time >= pdMS_TO_TICKS(30000))) is_muted = false;

            bool target_buzzer_state = false;
            int sync_cmd = 0; 
            if (is_danger) {
                target_buzzer_state = !is_muted; 
            } else {
                is_muted = false; 
                target_buzzer_state = g_web_alarm_enabled; 
                if (was_danger) target_buzzer_state = false; 
            }

            if (target_buzzer_state != g_current_buzzer_state) {
                gpio_set_level(BUZZER_PIN, target_buzzer_state ? BUZZER_ON : BUZZER_OFF);
                g_current_buzzer_state = target_buzzer_state;
                if (target_buzzer_state == true && g_web_alarm_enabled == false) sync_cmd = 1; 
                if (target_buzzer_state == false && g_web_alarm_enabled == true && was_danger) sync_cmd = 2; 
            }

            bool is_alert = is_danger || is_warning;

            ds3231_time_t rtc_now;
            bool rtc_ok = (ds3231_get_time(&rtc_now) == ESP_OK);
            
            ssd1306_clear();
            char oled_buf[32];
            if (rtc_ok) {
                sprintf(oled_buf, "%02d/%02d/20%02d %02d:%02d", rtc_now.date, rtc_now.month, rtc_now.year, rtc_now.hour, rtc_now.minute);
            } else {
                strcpy(oled_buf, "--/--/---- --:--");
            }
            ssd1306_draw_string(0, 0, oled_buf);
            ssd1306_draw_hline(0, 10, 128);
            
            sprintf(oled_buf, "Muc nuoc: %.1f cm", current_water_level);
            ssd1306_draw_string(0, 16, oled_buf);
            
            sprintf(oled_buf, "Nhiet do: %.1f C", temp_c);
            ssd1306_draw_string(0, 28, oled_buf);
            
            ssd1306_draw_string(0, 44, current_status);
            ssd1306_display();

            bool should_send = false;
            if (is_alert && !was_alert) should_send = true; 
            else if (is_alert && (xTaskGetTickCount() - last_sent_time >= ALERT_INTERVAL)) should_send = true; 
            else if (!is_alert && (xTaskGetTickCount() - last_sent_time >= NORMAL_INTERVAL)) should_send = true; 
            if (sync_cmd != 0) should_send = true; 

            if (should_send) {
                sensor_data_t tx_data;
                tx_data.water_level = current_water_level;
                strcpy(tx_data.status, current_status);
                tx_data.buzzer_sync_cmd = sync_cmd; 

                if (rtc_ok) {
                    sprintf(tx_data.rtc_time, "%02d:%02d:%02d - %02d/%02d/20%02d",
                            rtc_now.hour, rtc_now.minute, rtc_now.second,
                            rtc_now.date, rtc_now.month, rtc_now.year);
                } else {
                    strcpy(tx_data.rtc_time, "Loi_Mat_Ket_Noi_RTC");
                }

                if (xQueueSend(firebase_queue, &tx_data, 0) == pdPASS) {
                    last_sent_time = xTaskGetTickCount(); 
                }
            }
            was_alert = is_alert; 
            was_danger = is_danger;
        } 
        vTaskDelay(pdMS_TO_TICKS(500));
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

    gpio_reset_pin(BUZZER_PIN);
    gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(BUZZER_PIN, BUZZER_OFF);

    hc_sr04_rmt_config_t hc_cfg = { .trig_gpio = 18, .echo_gpio = 19, .rmt_resolution_hz = 1000000, .max_distance_cm = 400 };
    hc_sr04_rmt_init(&hc_cfg);

    aht2x_config_t aht_cfg = { .i2c_port = I2C_NUM_0, .sda_pin = 25, .scl_pin = 26 };
    aht2x_init(&aht_cfg);

    if (ds3231_init() == ESP_OK) {
        ESP_LOGI(TAG, "Khoi tao RTC DS3231 thanh cong!");
    } else {
        ESP_LOGE(TAG, "Loi khoi tao RTC DS3231!");
    }

    if (ssd1306_init(I2C_NUM_0, 0x3C) == ESP_OK) {
        ESP_LOGI(TAG, "Khoi tao OLED SSD1306 thanh cong!");
    } else {
        ESP_LOGE(TAG, "Loi khoi tao OLED SSD1306!");
    }

    wl_processor_init(NULL);
    firebase_queue = xQueueCreate(10, sizeof(sensor_data_t));

    ESP_LOGI(TAG, "Hệ thống đã khởi động. Đang chạy đo đạc cục bộ...");
    xTaskCreatePinnedToCore(sensor_task, "sensor_task", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(firebase_task, "firebase_task", 8192, NULL, 4, NULL, 1);
}