idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS ""
    REQUIRES hc_sr04_rmt aht2x_sensor water_level_processor
)