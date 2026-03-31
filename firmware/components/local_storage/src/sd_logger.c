#include "sd_logger.h"
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

// Gọi API lấy dữ liệu từ cảm biến
#include "measure_setup.h"

static const char *TAG = "SD_Logger";

#define MOUNT_POINT "/sdcard"

static void sd_log_task(void *pvParameters) {
    char file_path[64];
    sprintf(file_path, "%s/data_log.txt", MOUNT_POINT);

    while (1) {
        float water_level = 0.0f;
        char status[20];

        // Nếu lấy được dữ liệu cảm biến hợp lệ
        if (measure_get_latest_data(&water_level, status)) {
            // Mở file ở chế độ "a" (Append - Ghi nối tiếp vào cuối file)
            FILE *f = fopen(file_path, "a");
            if (f == NULL) {
                ESP_LOGE(TAG, "Không thể mở file để ghi!");
            } else {
                // Lấy thời gian hoạt động của hệ thống (Uptime) làm mốc
                uint32_t uptime_sec = esp_log_timestamp() / 1000;
                
                // Ghi 1 dòng dữ liệu vào thẻ nhớ
                fprintf(f, "Uptime: %lu s | Muc nuoc: %.1f cm | Trang thai: %s\n", 
                        uptime_sec, water_level, status);
                fclose(f);
                ESP_LOGI(TAG, "Đã lưu vào thẻ SD thành công.");
            }
        }
        
        // Cứ 15 giây lưu 1 lần (Có thể chỉnh lại tùy ý)
        vTaskDelay(pdMS_TO_TICKS(15000));
    }
}

void sd_logger_init(void) {
    esp_err_t ret;
    
    // 1. Cấu hình thẻ nhớ FATFS
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false, // Không tự format nếu lỗi (Bảo vệ thẻ)
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(TAG, "Đang khởi tạo thẻ SD...");

    // 2. Cấu hình bus SPI
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = 23,
        .miso_io_num = 19,
        .sclk_io_num = 18,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    // Khởi tạo kênh SPI số 2 (HSPI)
    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Lỗi khởi tạo bus SPI.");
        return;
    }

    // 3. Gắn thẻ nhớ vào bus SPI
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = 5;
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

    // In thông tin thẻ nhớ ra Terminal cho ngầu
    sdmmc_card_print_info(stdout, card);

    // 4. Khởi chạy luồng ghi file ngầm
    xTaskCreate(sd_log_task, "sd_log_task", 4096, NULL, 4, NULL);
    ESP_LOGI(TAG, "SD Logger Task Initialized");
}