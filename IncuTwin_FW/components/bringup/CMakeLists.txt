idf_component_register(
    SRCS "bringup_screen.c"
    INCLUDE_DIRS "include"
    REQUIRES board lvgl__lvgl espressif__esp_lvgl_port espressif__esp_lcd_touch
    PRIV_REQUIRES app_config esp_timer heap
)
