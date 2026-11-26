idf_component_register(
    SRCS "settings.c" "quiet_hours.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES storage board app_events esp_timer telemetry
)
