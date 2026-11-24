set(COMP ${CMAKE_CURRENT_LIST_DIR}/../../components)

idf_component_register(
    SRCS "test_main.c" "test_twin_logic.c" "test_twin_parse.c" "test_commands.c" "test_backoff_status.c"
         "test_ota.c" "test_hand_hold.c" "test_twin_geo.c" "test_world_map.c" "test_accompany.c" "test_name_logic.c" "test_twin_notify.c" "test_event_ring.c" "test_quiet_hours.c"
         "${COMP}/accompany/acc_logic.c"
         "${COMP}/baby_name/name_logic.c"
         "${COMP}/event_log/event_ring.c"
         "${COMP}/settings/quiet_hours.c"
         # modulos puros de otros componentes, compilados aqui para no arrastrar LVGL ni la red
         "${COMP}/mqtt_link/backoff.c"
         "${COMP}/commands/commands_parse.c"
         "${COMP}/cloud_status/status_payload.c"
         "${COMP}/ota_update/ota_validate.c"
         "${COMP}/hand_hold/hand_hold.c"
         "${COMP}/cloud_status/hand_payload.c"
         "${COMP}/world_map/world_map.c"
         "${COMP}/world_map/map_land.c"
    INCLUDE_DIRS "." "${COMP}/mqtt_link/include" "${COMP}/commands/include" "${COMP}/cloud_status/include"
                 "${COMP}/ota_update/include" "${COMP}/hand_hold/include"
                 "${COMP}/world_map/include" "${COMP}/accompany/include"
                 "${COMP}/baby_name/include" "${COMP}/event_log/include" "${COMP}/settings/include"
    PRIV_REQUIRES unity twin_model twin_types espressif__cjson
    WHOLE_ARCHIVE   # los TEST_CASE se registran por constructores: no dejar que el linker los descarte
)
