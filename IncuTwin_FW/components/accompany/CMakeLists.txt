idf_component_register(
    SRCS "acc_logic.c" "accompany.c"
    INCLUDE_DIRS "include"
    REQUIRES twin_types
    PRIV_REQUIRES app_events identity net_wifi settings sound storage twin_model esp_timer heap
)
