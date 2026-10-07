idf_component_register(
    SRCS "ota_update.c" "ota_validate.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES app_config app_events esp_http_client esp-tls mbedtls app_update esp_partition
                  esp_timer esp_system esp_hw_support espressif__cjson
)
