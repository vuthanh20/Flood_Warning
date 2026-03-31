idf_component_register(
    SRCS "src/sd_logger.c"
    INCLUDE_DIRS "include"
    # Gọi các thư viện lõi và measure_distance để lấy data
    REQUIRES freertos fatfs vfs measure_distance
)