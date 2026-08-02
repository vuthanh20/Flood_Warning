#include "ssd1306_oled.h"
#include "font5x7.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SSD1306";
static i2c_port_t s_port;
static uint8_t    s_addr;

static uint8_t s_framebuf[SSD1306_WIDTH * (SSD1306_HEIGHT / 8)];

static esp_err_t ssd1306_write_cmd(uint8_t cmd)
{
    uint8_t buf[2] = {0x00, cmd};
    return i2c_master_write_to_device(s_port, s_addr, buf, sizeof(buf), pdMS_TO_TICKS(100));
}

static esp_err_t ssd1306_write_data(const uint8_t *data, size_t len)
{
    const size_t CHUNK = 128;
    uint8_t buf[CHUNK + 1];
    buf[0] = 0x40;

    for (size_t offset = 0; offset < len; offset += CHUNK) {
        size_t remaining = len - offset;
        size_t chunk_len = (remaining < CHUNK) ? remaining : CHUNK;
        memcpy(buf + 1, data + offset, chunk_len);

        esp_err_t err = i2c_master_write_to_device(s_port, s_addr, buf, chunk_len + 1, pdMS_TO_TICKS(100));
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}

esp_err_t ssd1306_init(i2c_port_t port, uint8_t addr)
{
    s_port = port;
    s_addr = addr;

    static const uint8_t init_cmds[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
        0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12,
        0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF
    };

    esp_err_t err;
    for (size_t i = 0; i < sizeof(init_cmds); i++) {
        err = ssd1306_write_cmd(init_cmds[i]);
        if (err != ESP_OK) return err;
    }

    ssd1306_clear();
    err = ssd1306_display();
    return err;
}

void ssd1306_clear(void)
{
    memset(s_framebuf, 0x00, sizeof(s_framebuf));
}

static void set_pixel(int x, int y)
{
    if (x < 0 || x >= SSD1306_WIDTH || y < 0 || y >= SSD1306_HEIGHT) return;
    s_framebuf[x + (y / 8) * SSD1306_WIDTH] |= (1 << (y % 8));
}

void ssd1306_draw_char(int x, int y, char c)
{
    if (c < 32 || c > 127) c = '?';
    const uint8_t *glyph = font5x7[(int)c - 32];
    for (int col = 0; col < 5; col++) {
        uint8_t column_data = glyph[col];
        for (int row = 0; row < 7; row++) {
            if (column_data & (1 << row)) {
                set_pixel(x + col, y + row);
            }
        }
    }
}

void ssd1306_draw_string(int x, int y, const char *str)
{
    if (!str) return;
    int cursor_x = x;
    while (*str) {
        if (cursor_x + 5 > SSD1306_WIDTH) break;
        ssd1306_draw_char(cursor_x, y, *str);
        cursor_x += 6;
        str++;
    }
}

void ssd1306_draw_hline(int x, int y, int width)
{
    for (int i = 0; i < width; i++) {
        set_pixel(x + i, y);
    }
}

esp_err_t ssd1306_display(void)
{
    esp_err_t err;
    err = ssd1306_write_cmd(0x21);
    if (err != ESP_OK) return err;
    err = ssd1306_write_cmd(0x00);
    if (err != ESP_OK) return err;
    err = ssd1306_write_cmd(0x7F);
    if (err != ESP_OK) return err;

    err = ssd1306_write_cmd(0x22);
    if (err != ESP_OK) return err;
    err = ssd1306_write_cmd(0x00);
    if (err != ESP_OK) return err;
    err = ssd1306_write_cmd(0x07);
    if (err != ESP_OK) return err;

    return ssd1306_write_data(s_framebuf, sizeof(s_framebuf));
}
