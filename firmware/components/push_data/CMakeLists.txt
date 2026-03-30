idf_component_register(
    SRCS "firebase_setup/src/firebase_setup.c"
         "firebase_client/src/firebase_client.c"
    INCLUDE_DIRS "firebase_setup/include"
                 "firebase_client/include"
    REQUIRES freertos nvs_flash esp_netif esp_http_client measure_distance
)