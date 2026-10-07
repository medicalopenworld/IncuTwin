idf_component_register(
    SRCS "twin_logic.c" "twin_parse.c" "twin_model.c"
    INCLUDE_DIRS "include"
    REQUIRES twin_types
    PRIV_REQUIRES app_config app_events storage esp_timer espressif__cjson
)
