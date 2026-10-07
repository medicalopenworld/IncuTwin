set(COMP ${CMAKE_CURRENT_LIST_DIR}/../../components)

idf_component_register(
    SRCS "test_main.c" "test_twin_logic.c" "test_twin_parse.c" "test_commands.c" "test_backoff_status.c"
         "test_ota.c"
         # modulos puros de otros componentes, compilados aqui para no arrastrar LVGL ni la red
         "${COMP}/mqtt_link/backoff.c"
         "${COMP}/commands/commands_parse.c"
         "${COMP}/cloud_status/status_payload.c"
         "${COMP}/ota_update/ota_validate.c"
    INCLUDE_DIRS "." "${COMP}/mqtt_link/include" "${COMP}/commands/include" "${COMP}/cloud_status/include"
                 "${COMP}/ota_update/include"
    PRIV_REQUIRES unity twin_model twin_types espressif__cjson
    WHOLE_ARCHIVE   # los TEST_CASE se registran por constructores: no dejar que el linker los descarte
)
