// rtc/components/src/ds3231_rtc.c
#include "ds3231_rtc.h"
#include "driver/i2c.h"

#define I2C_MASTER_PORT I2C_NUM_0
#define DS3231_ADDR     0x68    // Địa chỉ I2C của DS3231

#define I2C_MASTER_SDA_IO 21    // Thay đổi nếu dùng chân khác
#define I2C_MASTER_SCL_IO 22    // Thay đổi nếu dùng chân khác
#define I2C_MASTER_FREQ_HZ 100000

// Hàm phụ trợ: Chuyển đổi BCD (Binary-Coded Decimal) sang Decimal và ngược lại
static uint8_t bcd2dec(uint8_t val) { return (val >> 4) * 10 + (val & 0x0f); }
static uint8_t dec2bcd(uint8_t val) { return ((val / 10) << 4) + (val % 10); }

esp_err_t ds3231_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_PORT, &conf);
    return i2c_driver_install(I2C_MASTER_PORT, conf.mode, 0, 0, 0);
}

esp_err_t ds3231_set_time(ds3231_time_t *time) {
    uint8_t data[8];
    data[0] = 0x00; // Trỏ tới thanh ghi 0x00 (Seconds)
    data[1] = dec2bcd(time->second);
    data[2] = dec2bcd(time->minute);
    data[3] = dec2bcd(time->hour);
    data[4] = dec2bcd(time->day);
    data[5] = dec2bcd(time->date);
    data[6] = dec2bcd(time->month);
    data[7] = dec2bcd(time->year);

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, data, 8, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_PORT, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t ds3231_get_time(ds3231_time_t *time) {
    uint8_t data[7];
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    // Ghi địa chỉ thanh ghi bắt đầu đọc (0x00)
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x00, true);
    
    // Khởi động lại I2C để bắt đầu đọc
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, 6, I2C_MASTER_ACK); // Đọc 6 byte đầu gửi ACK
    i2c_master_read_byte(cmd, data + 6, I2C_MASTER_NACK); // Byte cuối gửi NACK
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_PORT, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK) {
        time->second = bcd2dec(data[0]);
        time->minute = bcd2dec(data[1]);
        time->hour   = bcd2dec(data[2]);
        time->day    = bcd2dec(data[3]);
        time->date   = bcd2dec(data[4]);
        time->month  = bcd2dec(data[5] & 0x1F); // Bỏ qua bit thế kỷ (Century bit)
        time->year   = bcd2dec(data[6]);
    }
    return ret;
}