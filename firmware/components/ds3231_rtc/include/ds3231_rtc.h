// rtc/components/include/ds3231_rtc.h
#ifndef DS3231_RTC_H
#define DS3231_RTC_H

#include <stdint.h>
#include "esp_err.h"

// Cấu trúc lưu trữ thời gian
typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;    // Thứ trong tuần (1-7)
    uint8_t date;   // Ngày trong tháng (1-31)
    uint8_t month;
    uint8_t year;   // Hai số cuối của năm (ví dụ: 24 cho 2024)
} ds3231_time_t;

// Khai báo các hàm
esp_err_t ds3231_init(void);
esp_err_t ds3231_set_time(ds3231_time_t *time);
esp_err_t ds3231_get_time(ds3231_time_t *time);

#endif // DS3231_RTC_H