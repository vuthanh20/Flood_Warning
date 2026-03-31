#include "measure_setup.h"
#include "firebase_setup.h"
#include "sd_logger.h" // Thêm thư viện thẻ nhớ

void app_main(void) {
    // 1. Khởi động hệ thống đo lường
    measure_setup_init();
    
    // 2. Khởi động hệ thống lưu trữ thẻ nhớ (SD Card)
    sd_logger_init();

    // 3. Khởi động mạng và Firebase
    push_setup_init();
}