#pragma once

/* LovyanGFX driver for the CrowPanel Advance 2.8" (ST7789, SPI).
 * Values taken from the official Elecrow example. */

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "pins_config.h"

class LGFX_CrowPanel28 : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789 _panel_instance;
    lgfx::Bus_SPI _bus_instance;
    lgfx::Touch_FT5x06 _touch_instance;

  public:
    LGFX_CrowPanel28(void) {
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host = SPI2_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = 80000000;
            cfg.freq_read = 16000000;
            cfg.spi_3wire = false;
            cfg.use_lock = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk = LCD_PIN_SCLK;
            cfg.pin_mosi = LCD_PIN_MOSI;
            cfg.pin_miso = LCD_PIN_MISO;
            cfg.pin_dc = LCD_PIN_DC;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs = LCD_PIN_CS;
            cfg.pin_rst = LCD_PIN_RST;
            cfg.pin_busy = -1;
            cfg.memory_width = 240;
            cfg.memory_height = 320;
            cfg.panel_width = 240;
            cfg.panel_height = 320;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            /* portrait mount; 2 = 180 deg from the first portrait try
             * (verified on hardware). The touch offset stays as-is: the
             * 180 is compensated in touchpad_read_cb (main.cpp). */
            cfg.offset_rotation = 2;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits = 1;
            cfg.readable = false;
            cfg.invert = true;
            cfg.rgb_order = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = true;
            _panel_instance.config(cfg);
        }
        {
            /* FT5x06/FT6336 capacitive touch, confirmed on the I2C bus
             * scan of this unit at 0x38 (some batches ship GT911 at 0x5D
             * instead, per the Elecrow example — not the case here). */
            auto cfg = _touch_instance.config();
            cfg.x_min = 0;
            cfg.x_max = 239;
            cfg.y_min = 0;
            cfg.y_max = 319;
            cfg.pin_int = TOUCH_FT5X06_INT;
            cfg.bus_shared = false;
            cfg.offset_rotation = 6;
            cfg.i2c_port = 0;
            cfg.i2c_addr = TOUCH_FT5X06_ADDR;
            cfg.pin_sda = TOUCH_SDA;
            cfg.pin_scl = TOUCH_SCL;
            cfg.freq = 400000;
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }
        setPanel(&_panel_instance);
    }
};
