#ifndef FIREBASE_CLIENT_H
#define FIREBASE_CLIENT_H

#include "esp_err.h"

// Hàm đẩy dữ liệu lên Firebase
esp_err_t firebase_push_data(const char* db_url, int muc_nuoc, const char* trang_thai);

#endif // FIREBASE_CLIENT_H