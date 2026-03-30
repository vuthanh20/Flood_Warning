idf_component_register(
    SRCS "src/push_setup.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos nvs_flash esp_netif esp_http_client measure_distance firebase_client
)