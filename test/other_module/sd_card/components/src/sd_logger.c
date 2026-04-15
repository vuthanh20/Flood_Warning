#include "sd_logger.h"
#include "driver/gpio.h"
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SD_Logger_Test";

#define MOUNT_POINT "/sdcard"

// #define PIN_NUM_MISO 12
// #define PIN_NUM_MOSI 13
// #define PIN_NUM_CLK  14
// #define PIN_NUM_CS   15

#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18
#define PIN_NUM_CS   5

// Task ghi dữ liệu giả (Dummy Data) để test
static void sd_log_test_task(void *pvParameters) {
    char file_path[64];
    sprintf(file_path, "%s/test_log.txt", MOUNT_POINT);

    int test_counter = 0;

    while (1) {
        // Mở file để ghi nối (append)
        FILE *f = fopen(file_path, "a");
        if (f == NULL) {
            ESP_LOGE(TAG, "Không thể mở file test_log.txt để ghi!");
        } else {
            test_counter++;
            float dummy_water_level = 10.5f + (test_counter * 0.1f); // Số tăng dần
            
            fprintf(f, "Dong thu %d | Muc nuoc gia lap: %.2f cm\n", test_counter, dummy_water_level);
            fclose(f);
            ESP_LOGI(TAG, "Đã ghi dòng thứ %d vào thẻ SD.", test_counter);
        }
        
        // Cứ 5 giây ghi 1 lần cho nhanh thấy kết quả
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void sd_logger_init(void) {
    esp_err_t ret;
    
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(TAG, "Đang khởi tạo thẻ SD...");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    
    host.max_freq_khz = 1000; 

    gpio_set_pull_mode(19, GPIO_PULLUP_ONLY); // MISO
    gpio_set_pull_mode(23, GPIO_PULLUP_ONLY); // MOSI
    gpio_set_pull_mode(18, GPIO_PULLUP_ONLY); // CLK
    gpio_set_pull_mode(5,  GPIO_PULLUP_ONLY); // CS

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = 23,
        .miso_io_num = 19,
        .sclk_io_num = 18,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    
    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Lỗi khởi tạo bus SPI.");
        return;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Không thể mount thẻ nhớ. Hãy format thẻ sang định dạng FAT32.");
        } else {
            ESP_LOGE(TAG, "Không tìm thấy thẻ nhớ. Hãy kiểm tra lại dây cắm!");
        }
        return;
    }

    sdmmc_card_print_info(stdout, card);

    // Chạy luồng test ghi file
    xTaskCreate(sd_log_test_task, "sd_test_task", 4096, NULL, 4, NULL);
    ESP_LOGI(TAG, "SD Test Task Bắt Đầu Chạy!");
}