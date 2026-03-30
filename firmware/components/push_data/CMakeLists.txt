idf_component_register(
    SRCS "firebase_setup/src/firebase_setup.c"
         "firebase_client/src/firebase_client.c"
    INCLUDE_DIRS "firebase_setup/include"
                 "firebase_client/include"
    # Bỏ example đi, thêm esp_wifi và esp_event vào
    REQUIRES freertos nvs_flash esp_netif esp_http_client measure_distance esp-tls mbedtls json esp_wifi esp_event
)