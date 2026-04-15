idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES 
                            nvs_flash 
                            esp_netif 
                            esp_event 
                            driver
                            json
                            protocol_examples_common
                            aht2x_sensor
                            firebase_client
                            hc_sr04_rmt
                            water_level_processor
                            ds3231_rtc)
