idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES app_config board bringup storage app_events identity settings usage_stats twin_model
                  sound ui demo_mode net_wifi support_console mqtt_link cloud_status commands ota_update
                  sim_server
                  esp_app_format app_update esp_system esp_timer
)
