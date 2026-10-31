idf_component_register(
    SRCS "commands.c" "commands_parse.c" "pairing.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES app_events identity mqtt_link twin_model settings sound esp_timer esp_system
                  esp_hw_support espressif__cjson
)
