idf_component_register(
    SRCS "baby_name.c" "name_logic.c"
    INCLUDE_DIRS "include"
    REQUIRES twin_types
    PRIV_REQUIRES storage log
)
