idf_component_register(
    SRCS "mqtt_link.c" "backoff.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES app_config app_events identity twin_types espressif__mqtt esp_event esp_timer
                  mbedtls esp-tls esp_hw_support
)
