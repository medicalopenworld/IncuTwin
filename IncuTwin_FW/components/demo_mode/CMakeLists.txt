idf_component_register(
    SRCS "demo_mode.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES incupray board twin_model scenarios sound esp_timer telemetry
)
