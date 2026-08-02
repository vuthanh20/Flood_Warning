#ifndef SSD1306_OLED_H
#define SSD1306_OLED_H

#include "esp_err.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SSD1306_WIDTH   128
#define SSD1306_HEIGHT  64

esp_err_t ssd1306_init(i2c_port_t port, uint8_t addr);
void ssd1306_clear(void);
void ssd1306_draw_char(int x, int y, char c);
void ssd1306_draw_string(int x, int y, const char *str);
void ssd1306_draw_hline(int x, int y, int width);
esp_err_t ssd1306_display(void);

#ifdef __cplusplus
}
#endif

#endif // SSD1306_OLED_H
