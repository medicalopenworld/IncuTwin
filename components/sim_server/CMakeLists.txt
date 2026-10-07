idf_component_register(
    SRCS "sim_server.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES app_config app_events esp_http_server twin_model twin_types scenarios esp_timer
                  espressif__cjson
)
