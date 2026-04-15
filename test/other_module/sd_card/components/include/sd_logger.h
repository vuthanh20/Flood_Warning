#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Khởi tạo giao tiếp SPI, Mount thẻ nhớ SD (định dạng FAT32) 
 * và chạy Task ghi dữ liệu giả (Dummy Data) ngầm.
 */
void sd_logger_init(void);

#ifdef __cplusplus
}
#endif