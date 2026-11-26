idf_component_register(
    SRCS "cloud_status.c" "status_payload.c" "hand_payload.c" "metrics_payload.c"
    INCLUDE_DIRS "include"
    REQUIRES rewards
    PRIV_REQUIRES accompany app_config settings usage_stats app_events identity mqtt_link twin_model net_wifi esp_app_format esp_timer
)
