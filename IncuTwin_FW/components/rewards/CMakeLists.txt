idf_component_register(
    SRCS "rewards_logic.c" "rewards_tel.c" "rewards.c"
    INCLUDE_DIRS "include"
    REQUIRES twin_types
    PRIV_REQUIRES app_events storage stay_log net_wifi esp_event
)
