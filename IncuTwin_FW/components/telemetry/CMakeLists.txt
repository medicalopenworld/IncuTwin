idf_component_register(
    SRCS "tel_logic.c" "tel_queue.c" "telemetry.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES app_events mqtt_link storage esp_partition esp_timer esp_app_format app_update esp_system
)
