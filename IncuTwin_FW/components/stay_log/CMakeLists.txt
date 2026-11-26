idf_component_register(
    SRCS "stay_log.c" "stays.c"
    INCLUDE_DIRS "include"
    REQUIRES twin_types
    PRIV_REQUIRES app_events storage esp_event
)
