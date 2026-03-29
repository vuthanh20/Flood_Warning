#include "water_level_processor.h"

static float s_scale_a = 1.012f;
static float s_offset_b = -0.402f;

void wl_processor_init(const wl_processor_config_t *cfg) {
    if (cfg) {
        s_scale_a = cfg->calib_scale_a;
        s_offset_b = cfg->calib_offset_b;
    }
}

float wl_processor_compute_raw(int64_t tof_us, float temp_c) {
    // Tốc độ âm thanh (m/s) = 331.4 + 0.606 * Temp
    float velocity_m_s = 331.4f + (0.606f * temp_c);
    float velocity_cm_us = (velocity_m_s * 100.0f) / 1000000.0f;
    return ((float)tof_us * velocity_cm_us) / 2.0f;
}

float wl_processor_apply_filter(float raw_distance) {
    // ĐỂ TRỐNG ĐỂ SO SÁNH SAU. Tạm thời trả về nguyên bản.
    return raw_distance;
}

float wl_processor_apply_calibration(float filtered_distance) {
    // y = Ax + B
    return (s_scale_a * filtered_distance) + s_offset_b;
}