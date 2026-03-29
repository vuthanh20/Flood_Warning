#include "hc_sr04_rmt.h"
#include "driver/gpio.h"
#include "driver/rmt_rx.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_rom_sys.h" 
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <string.h>
#include <inttypes.h>

static const char *TAG = "hc_sr04_rmt";

static hc_sr04_rmt_config_t s_cfg;
static bool s_inited = false;
static rmt_channel_handle_t s_rx_channel = NULL;
static QueueHandle_t s_rx_queue = NULL;

static uint64_t timeout_ns_from_max_cm(uint32_t max_cm) {
    float t_us = (float)max_cm / 0.01715f;
    t_us *= 1.2f; 
    if (t_us < 1000.0f) t_us = 1000.0f;
    if (t_us > 3000000.0f) t_us = 3000000.0f;
    return (uint64_t)(t_us * 1000.0f);
}

static bool rx_done_cb(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_ctx) {
    QueueHandle_t q = (QueueHandle_t)user_ctx;
    rmt_rx_done_event_data_t local = *edata; 
    xQueueSendFromISR(q, &local, NULL);
    return false;
}

esp_err_t hc_sr04_rmt_init(const hc_sr04_rmt_config_t *cfg) {
    if (!cfg) return ESP_ERR_INVALID_ARG;
    if (s_inited) return ESP_OK;

    s_cfg = *cfg;

    gpio_config_t trig_conf = {
        .pin_bit_mask = (1ULL << cfg->trig_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&trig_conf);
    gpio_set_level(cfg->trig_gpio, 0);

    rmt_rx_channel_config_t rx_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = cfg->rmt_resolution_hz,
        .mem_block_symbols = 64,
        .gpio_num = cfg->echo_gpio,
    };

    rmt_new_rx_channel(&rx_cfg, &s_rx_channel);

    s_rx_queue = xQueueCreate(1, sizeof(rmt_rx_done_event_data_t));
    rmt_rx_event_callbacks_t cbs = { .on_recv_done = rx_done_cb };
    rmt_rx_register_event_callbacks(s_rx_channel, &cbs, s_rx_queue);

    ESP_ERROR_CHECK(rmt_enable(s_rx_channel));
    s_inited = true;
    return ESP_OK;
}

static void trigger_pulse(gpio_num_t trig) {
    gpio_set_level(trig, 0);
    esp_rom_delay_us(2);
    gpio_set_level(trig, 1);
    esp_rom_delay_us(10);
    gpio_set_level(trig, 0);
}

static int64_t parse_first_high_us(const rmt_symbol_word_t *symbols, size_t symbol_num, uint32_t resolution_hz) {
    if (!symbols || symbol_num == 0) return -1;
    bool in_high = false;
    int64_t high_ticks = 0;
    for (size_t i = 0; i < symbol_num; ++i) {
        const rmt_symbol_word_t *s = &symbols[i];
        if (s->level0) {
            if (!in_high) in_high = true;
            if (in_high) high_ticks += s->duration0;
        } else if (in_high) return (high_ticks * (1000000LL / resolution_hz));
        
        if (s->level1) {
            if (!in_high) in_high = true;
            if (in_high) high_ticks += s->duration1;
        } else if (in_high) return (high_ticks * (1000000LL / resolution_hz));
    }
    return in_high ? (high_ticks * (1000000LL / resolution_hz)) : -1;
}

esp_err_t hc_sr04_rmt_measure_us(int64_t *time_us) {
    if (!s_inited) return ESP_ERR_INVALID_STATE;
    if (!time_us) return ESP_ERR_INVALID_ARG;

    rmt_symbol_word_t raw_symbols[128];
    memset(raw_symbols, 0, sizeof(raw_symbols));

    rmt_receive_config_t rx_cfg = {
        .signal_range_min_ns = 1000, 
        .signal_range_max_ns = timeout_ns_from_max_cm(s_cfg.max_distance_cm),
    };

    esp_err_t err = rmt_receive(s_rx_channel, raw_symbols, sizeof(raw_symbols), &rx_cfg);
    if (err != ESP_OK) return err;

    trigger_pulse(s_cfg.trig_gpio);

    rmt_rx_done_event_data_t rx_data;
    TickType_t wait_ticks = pdMS_TO_TICKS((rx_cfg.signal_range_max_ns / 1000000ULL) + 50);
    
    if (xQueueReceive(s_rx_queue, &rx_data, wait_ticks) != pdPASS) {
        rmt_disable(s_rx_channel);
        rmt_enable(s_rx_channel);
        return ESP_ERR_TIMEOUT;
    }

    int64_t high_us = parse_first_high_us(rx_data.received_symbols, rx_data.num_symbols, s_cfg.rmt_resolution_hz);
    if (high_us < 0) return ESP_ERR_TIMEOUT;

    *time_us = high_us;
    return ESP_OK;
}