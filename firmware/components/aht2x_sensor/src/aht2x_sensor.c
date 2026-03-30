#include "aht2x_sensor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static i2c_port_t s_i2c_port;

esp_err_t aht2x_init(const aht2x_config_t *cfg) {
    s_i2c_port = cfg->i2c_port;
    
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = cfg->sda_pin,
        .scl_io_num = cfg->scl_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000, 
    };
    i2c_param_config(s_i2c_port, &i2c_conf);
    i2c_driver_install(s_i2c_port, i2c_conf.mode, 0, 0, 0);

    uint8_t init_cmd[3] = {0xBE, 0x08, 0x00};
    return i2c_master_write_to_device(s_i2c_port, AHT2X_I2C_ADDR, init_cmd, 3, pdMS_TO_TICKS(100));
}

esp_err_t aht2x_read_temperature(float *temperature_c) {
    uint8_t measure_cmd[3] = {0xAC, 0x33, 0x00};
    esp_err_t err = i2c_master_write_to_device(s_i2c_port, AHT2X_I2C_ADDR, measure_cmd, 3, pdMS_TO_TICKS(100));
    if (err != ESP_OK) return err;

    vTaskDelay(pdMS_TO_TICKS(80));

    uint8_t data[6];
    err = i2c_master_read_from_device(s_i2c_port, AHT2X_I2C_ADDR, data, 6, pdMS_TO_TICKS(100));
    if (err != ESP_OK) return err;

    if ((data[0] & 0x80) != 0) return ESP_ERR_INVALID_RESPONSE;

    uint32_t raw_temp = ((uint32_t)(data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | (uint32_t)data[5];
    *temperature_c = ((float)raw_temp / 1048576.0f) * 200.0f - 50.0f;

    return ESP_OK;
}