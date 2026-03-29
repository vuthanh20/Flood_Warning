#pragma once
#include <stdint.h>

typedef struct {
    float calib_scale_a;  
    float calib_offset_b; 
} wl_processor_config_t;

void wl_processor_init(const wl_processor_config_t *cfg);
float wl_processor_compute_raw(int64_t tof_us, float temp_c);
float wl_processor_apply_filter(float raw_distance);
float wl_processor_apply_calibration(float filtered_distance);