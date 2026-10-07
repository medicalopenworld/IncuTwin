idf_component_register(
    SRCS "ui.c" "ui_i18n.c" "ui_home.c" "ui_settings.c" "ui_onboarding.c" "baby_widget.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES lvgl__lvgl
    PRIV_REQUIRES espressif__esp_lvgl_port assets twin_types twin_model settings identity
                  usage_stats sound app_events board storage captive_portal net_wifi
                  esp_app_format esp_system esp_timer heap
)
