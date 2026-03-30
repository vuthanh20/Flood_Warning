idf_component_register(SRCS "src/measure_setup.c"
                       INCLUDE_DIRS "include"
                       REQUIRES freertos hc_sr04_rmt aht2x_sensor water_level_processor)