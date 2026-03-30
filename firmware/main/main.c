#include "measure_setup.h"
#include "firebase_setup.h"

void app_main(void) {
    // 1. Khởi động hệ thống đo lường (Cảm biến, Lọc nhiễu)
    measure_setup_init();
    
    // 2. Khởi động hệ thống mạng (WiFi, Firebase)
    push_setup_init();
}