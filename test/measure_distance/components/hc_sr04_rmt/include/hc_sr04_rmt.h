#pragma once

#include "esp_err.h"
#include "driver/gpio.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gpio_num_t trig_gpio;
    gpio_num_t echo_gpio;
    uint32_t rmt_resolution_hz;
    uint32_t max_distance_cm;
} hc_sr04_rmt_config_t;

esp_err_t hc_sr04_rmt_init(const hc_sr04_rmt_config_t *cfg);

// Đã đổi thành trả về thời gian sóng dội (microseconds)
esp_err_t hc_sr04_rmt_measure_us(int64_t *time_us);

#ifdef __cplusplus
}
#endif