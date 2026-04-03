#include "sd_logger.h"

void app_main(void) {
    // Chỉ gọi duy nhất hàm khởi tạo của thẻ SD
    sd_logger_init();
}