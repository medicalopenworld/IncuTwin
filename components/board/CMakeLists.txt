idf_component_register(
    SRCS "board.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES lvgl__lvgl espressif__esp_lvgl_port espressif__esp_lcd_touch
    PRIV_REQUIRES esp_lcd esp_driver_spi esp_driver_i2c esp_driver_ledc esp_driver_gpio
                  esp_psram heap
                  espressif__esp_lcd_touch_ft5x06 espressif__esp_lcd_touch_gt911
)
