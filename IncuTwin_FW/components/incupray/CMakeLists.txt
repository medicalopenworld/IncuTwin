idf_component_register(
    SRCS "incupray_logic.c" "incupray.c"
    INCLUDE_DIRS "include"
    REQUIRES twin_types
    PRIV_REQUIRES espressif__cjson app_events storage mqtt_link rewards net_wifi esp_timer
)
