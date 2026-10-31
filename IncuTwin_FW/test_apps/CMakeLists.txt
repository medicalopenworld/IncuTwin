# IncuTwin — tests Unity on-target de la logica pura (twin_model).
# Build/flash: idf.py -C test_apps -p COM12 flash && python tools/serial_capture.py --port COM12 --reset --seconds 20
cmake_minimum_required(VERSION 3.22)

set(EXTRA_COMPONENT_DIRS "../components")
# Solo main y sus dependencias transitivas: no arrastrar LVGL ni la pantalla.
set(COMPONENTS main)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(incutwin_tests)
