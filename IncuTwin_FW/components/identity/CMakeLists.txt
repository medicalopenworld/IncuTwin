idf_component_register(
    SRCS "identity.c"
    INCLUDE_DIRS "include"
    REQUIRES twin_types
    PRIV_REQUIRES storage esp_hw_support
)
