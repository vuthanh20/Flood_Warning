idf_component_register(
    SRCS "src/sd_logger.c"
    INCLUDE_DIRS "include"
    # Chỉ gọi thư viện hệ điều hành (freertos) và thư viện quản lý file (fatfs, vfs)
    REQUIRES freertos fatfs vfs
)