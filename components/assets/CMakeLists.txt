idf_component_register(
    SRCS "img_baby.c" "img_heart.c" "img_logo.c" "img_extra.c"
         "fonts/lv_font_es_12.c" "fonts/lv_font_es_14.c" "fonts/lv_font_es_16.c" "fonts/lv_font_es_20.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl__lvgl
)
# Imagenes grandes generadas: sin warnings de formato ni de tamano
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-const-variable)
