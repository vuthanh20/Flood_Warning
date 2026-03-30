#include "water_level_processor.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Hệ số Calib chuẩn đã đo đạc (A = 1.012, B = -0.402)
static float s_scale_a = 1.012f;
static float s_offset_b = -0.402f;

// --- CẤU HÌNH BỘ LỌC CASCADE ---
#define MEDIAN_WINDOW 5
static float s_median_buf[MEDIAN_WINDOW];
static int s_median_idx = 0;
static bool s_median_filled = false;

static float s_ema_prev = -1.0f; 
static const float EMA_ALPHA = 0.15f; // Trọng số EMA (0.1 đến 0.2 là đẹp)

// Hàm phụ trợ cho thuật toán sắp xếp (qsort)
static int compare_float(const void *a, const void *b) {
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    return (fa > fb) - (fa < fb);
}

void wl_processor_init(const wl_processor_config_t *cfg) {
    if (cfg) {
        s_scale_a = cfg->calib_scale_a;
        s_offset_b = cfg->calib_offset_b;
    }
}

float wl_processor_compute_raw(int64_t tof_us, float temp_c) {
    float velocity_m_s = 331.4f + (0.606f * temp_c);
    float velocity_cm_us = (velocity_m_s * 100.0f) / 1000000.0f;
    return ((float)tof_us * velocity_cm_us) / 2.0f;
}

// ========================================================
// HÀM XỬ LÝ NHIỄU CHÍNH (CASCADE: MEDIAN + EMA)
// ========================================================
float wl_processor_apply_filter(float raw_distance) {
    
    // --- TẦNG 1: LỌC TRUNG VỊ (MEDIAN FILTER) ---
    s_median_buf[s_median_idx] = raw_distance;
    s_median_idx++;
    
    if (s_median_idx >= MEDIAN_WINDOW) {
        s_median_idx = 0;
        s_median_filled = true;
    }

    // Trong vài nhịp đo đầu tiên (chưa đầy mảng), tạm thời trả về giá trị thô
    if (!s_median_filled) {
        return raw_distance;
    }

    // Copy mảng ra để sắp xếp (tránh làm mất thứ tự thời gian của mảng gốc)
    float sorted_buf[MEDIAN_WINDOW];
    memcpy(sorted_buf, s_median_buf, sizeof(s_median_buf));
    qsort(sorted_buf, MEDIAN_WINDOW, sizeof(float), compare_float);
    
    // Bắt lấy giá trị nằm chính giữa (vứt bỏ các gai nhiễu ở 2 đầu)
    float median_val = sorted_buf[MEDIAN_WINDOW / 2];

    // --- TẦNG 2: LỌC TRUNG BÌNH ĐỘNG HÀM MŨ (EMA FILTER) ---
    // Nếu là lần đầu tiên chạy qua EMA, lấy luôn median làm mốc
    if (s_ema_prev < 0.0f) {
        s_ema_prev = median_val; 
    }

    // Công thức: y(t) = alpha * x(t) + (1 - alpha) * y(t-1)
    float ema_val = (EMA_ALPHA * median_val) + ((1.0f - EMA_ALPHA) * s_ema_prev);
    
    // Lưu lại trạng thái cho chu kỳ tiếp theo
    s_ema_prev = ema_val;

    return ema_val;
}

float wl_processor_apply_calibration(float filtered_distance) {
    return (s_scale_a * filtered_distance) + s_offset_b;
}