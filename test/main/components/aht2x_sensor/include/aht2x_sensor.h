#pragma once
#include "esp_err.h"
#include "driver/i2c.h"

#define AHT2X_I2C_ADDR 0x38

typedef struct {
    i2c_port_t i2c_port;
    int sda_pin;
    int scl_pin;
} aht2x_config_t;

esp_err_t aht2x_init(const aht2x_config_t *cfg);
esp_err_t aht2x_read_temperature(float *temperature_c);