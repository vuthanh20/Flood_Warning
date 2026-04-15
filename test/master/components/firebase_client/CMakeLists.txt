idf_component_register(SRCS "src/firebase_client.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_http_client esp-tls json log mbedtls)