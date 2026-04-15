// rtc/main/main.c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ds3231_rtc.h"

void app_main(void) {
    printf("Khoi tao I2C va DS3231...\n");
    
    if (ds3231_init() == ESP_OK) {
        printf("Khoi tao thanh cong!\n");
    } else {
        printf("Loi khoi tao I2C!\n");
        return;
    }


    // ds3231_time_t time_to_set = {
    //     .second = 0,
    //     .minute = 23,
    //     .hour = 17,
    //     .day = 5,       // Thứ 5
    //     .date = 10,
    //     .month = 4,
    //     .year = 26      // Năm 2024
    // };
    // ds3231_set_time(&time_to_set);
    // printf("Da cap nhat thoi gian cho RTC.\n");


    ds3231_time_t current_time;

    while (1) {
        if (ds3231_get_time(&current_time) == ESP_OK) {
            printf("Thoi gian: %02d:%02d:%02d - %02d/%02d/20%02d\n",
                   current_time.hour, current_time.minute, current_time.second,
                   current_time.date, current_time.month, current_time.year);
        } else {
            printf("Loi doc du lieu tu DS3231. Kiem tra lai day dien!\n");
        }
        
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Chờ 1 giây
    }
}