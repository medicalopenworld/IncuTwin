idf_component_register(
    SRCS "sound.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES board settings app_events twin_types lvgl__lvgl espressif__esp_lvgl_port
)
