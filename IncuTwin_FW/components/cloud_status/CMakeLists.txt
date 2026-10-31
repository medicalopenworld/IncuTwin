idf_component_register(
    SRCS "cloud_status.c" "status_payload.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES app_config app_events identity mqtt_link twin_model net_wifi esp_app_format esp_timer
)
