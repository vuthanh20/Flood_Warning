#ifndef FIREBASE_CLIENT_H
#define FIREBASE_CLIENT_H

#include "esp_err.h"
#include <stddef.h> // Để dùng size_t

// Hàm đẩy dữ liệu lên Firebase (Lưu tức thời)
esp_err_t firebase_push_data(const char* db_url, int muc_nuoc, const char* trang_thai);

// HÀM MỚI: Tải dữ liệu JSON từ Firebase về ESP32
esp_err_t firebase_get_json(const char* url, char* response_buffer, size_t max_len);

#endif // FIREBASE_CLIENT_H