idf_component_register(
    SRCS "ui.c" "ui_i18n.c" "ui_home.c" "ui_settings.c" "ui_onboarding.c" "baby_widget.c"
         "ui_pages.c" "ui_name.c" "ui_rel_time.c" "ui_nav.c" "ui_history.c" "ui_notice.c"
         "ui_streak_fx.c" "ui_notices.c" "ui_rewards.c" "ui_hearts.c" "ui_shop.c" "ui_debug.c" "ui_ota.c" "ui_incupray.c" "ui_nowifi.c" "ui_toy.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES lvgl__lvgl
    PRIV_REQUIRES espressif__esp_lvgl_port assets twin_types twin_model settings identity
                  usage_stats sound app_events board storage captive_portal net_wifi hand_hold world_map accompany
                  baby_name event_log stay_log rewards ota_update incupray
                  esp_app_format esp_system esp_timer heap telemetry demo_mode mbedtls
)
