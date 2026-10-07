# IncuTwin — gemelo digital de la incubadora IncuNest
# Placa: Elecrow CrowPanel Advance 2.8" (ESP32-S3-WROOM-1-N16R8)
# Build: ESP-IDF >= 6.0 con idf.py (ver README.md)
cmake_minimum_required(VERSION 3.22)

# La version del firmware (PROJECT_VER) la lee el build de version.txt:
# es la unica fuente para el log de arranque, el pie de ajustes y "fw" en status.
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(incutwin)
