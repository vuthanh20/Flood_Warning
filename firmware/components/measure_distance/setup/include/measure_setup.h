#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Khởi tạo phần cứng và tạo luồng (Task) đo lường chạy ngầm
void measure_setup_init(void);

// Hàm API để các Component khác (như push_data) lấy dữ liệu mới nhất
// Trả về true nếu dữ liệu hợp lệ (cảm biến không bị timeout)
bool measure_get_latest_data(float *water_level, char *status_buffer);

#ifdef __cplusplus
}
#endif