idf_component_register(
    SRCS "setup/src/setup.c"
         "aht2x_sensor/src/aht2x_sensor.c"
         "hc_sr04_rmt/src/hc_sr04_rmt.c"
         "water_level_processor/src/water_level_processor.c"
    INCLUDE_DIRS "setup/include"
                 "aht2x_sensor/include"
                 "hc_sr04_rmt/include"
                 "water_level_processor/include"
    # Chỉ cần gọi các thư viện gốc của ESP-IDF
    REQUIRES freertos driver esp_timer 
)