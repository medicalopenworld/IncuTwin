idf_component_register(
    SRCS "console_cmds.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES console identity usage_stats twin_model storage app_events esp_app_format
                  app_update esp_system board
)
