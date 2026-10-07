#include "board.h"

#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_io_i2c.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_lcd_touch_ft5x06.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_psram.h"
#include "sdkconfig.h"

#include "board_pins.h"

static const char *TAG = "board";

/* Kconfig no define los bool en "n": normalizar a 0/1 para usarlos como valor. */
#ifdef CONFIG_INCUTWIN_LCD_SWAP_XY
#define LCD_SWAP_XY 1
#else
#define LCD_SWAP_XY 0
#endif
#ifdef CONFIG_INCUTWIN_LCD_MIRROR_X
#define LCD_MIRROR_X 1
#else
#define LCD_MIRROR_X 0
#endif
#ifdef CONFIG_INCUTWIN_LCD_MIRROR_Y
#define LCD_MIRROR_Y 1
#else
#define LCD_MIRROR_Y 0
#endif
#ifdef CONFIG_INCUTWIN_LCD_INVERT_COLOR
#define LCD_INVERT 1
#else
#define LCD_INVERT 0
#endif
#ifdef CONFIG_INCUTWIN_TOUCH_SWAP_XY
#define TOUCH_SWAP_XY 1
#else
#define TOUCH_SWAP_XY 0
#endif
#ifdef CONFIG_INCUTWIN_TOUCH_MIRROR_X
#define TOUCH_MIRROR_X 1
#else
#define TOUCH_MIRROR_X 0
#endif
#ifdef CONFIG_INCUTWIN_TOUCH_MIRROR_Y
#define TOUCH_MIRROR_Y 1
#else
#define TOUCH_MIRROR_Y 0
#endif

/* LVGL dibuja por franjas: dos buffers de 64 lineas en RAM interna con DMA
 * (2 x 30 KB). Un framebuffer completo en PSRAM seria mas lento por DMA. */
#define LVGL_BUF_LINES 64

/* LEDC: la retroiluminacion y el zumbador usan temporizadores distintos para
 * que cambiar el tono no altere el brillo (spec board-bringup). */
#define BL_TIMER   LEDC_TIMER_0
#define BL_CHANNEL LEDC_CHANNEL_0
#define BL_RES     LEDC_TIMER_8_BIT
#define BL_FREQ_HZ 5000
#define BZ_TIMER   LEDC_TIMER_1
#define BZ_CHANNEL LEDC_CHANNEL_1
#define BZ_RES     LEDC_TIMER_10_BIT
#define BZ_IDLE_HZ 2000

static esp_lcd_panel_io_handle_t s_lcd_io;
static esp_lcd_panel_handle_t s_panel;
static i2c_master_bus_handle_t s_i2c_bus;
static esp_lcd_touch_handle_t s_touch;
static board_touch_chip_t s_touch_chip = BOARD_TOUCH_NONE;
static lv_display_t *s_disp;
static lv_indev_t *s_indev;
static uint8_t s_backlight_pct = 100;

/* ------------------------------------------------------------------ display */

static esp_err_t display_init(void)
{
    spi_bus_config_t bus = {
        .sclk_io_num = BOARD_LCD_PIN_SCLK,
        .mosi_io_num = BOARD_LCD_PIN_MOSI,
        .miso_io_num = BOARD_LCD_PIN_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BOARD_LCD_H_RES * LVGL_BUF_LINES * sizeof(uint16_t) + 16,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BOARD_LCD_SPI_HOST, &bus, SPI_DMA_CH_AUTO),
                        TAG, "spi bus");

    esp_lcd_panel_io_spi_config_t io = {
        .cs_gpio_num = BOARD_LCD_PIN_CS,
        .dc_gpio_num = BOARD_LCD_PIN_DC,
        .spi_mode = 0,
        .pclk_hz = CONFIG_INCUTWIN_LCD_PCLK_MHZ * 1000 * 1000,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BOARD_LCD_SPI_HOST,
                                                 &io, &s_lcd_io),
                        TAG, "panel io");

    esp_lcd_panel_dev_config_t dev = {
        .reset_gpio_num = BOARD_LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(s_lcd_io, &dev, &s_panel), TAG, "st7789");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "init");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(s_panel, LCD_INVERT), TAG, "invert");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(s_panel, LCD_SWAP_XY), TAG, "swap");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(s_panel, LCD_MIRROR_X, LCD_MIRROR_Y), TAG, "mirror");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_set_gap(s_panel, 0, 0), TAG, "gap");

    /* Limpiar la GRAM a negro antes de encender la retroiluminacion: la
     * memoria del panel arranca con basura y se veria un destello. */
    const size_t strip_px = BOARD_LCD_H_RES * 16;
    uint16_t *black = heap_caps_calloc(strip_px, sizeof(uint16_t), MALLOC_CAP_DMA);
    if (black) {
        for (int y = 0; y < BOARD_LCD_V_RES; y += 16) {
            esp_lcd_panel_draw_bitmap(s_panel, 0, y, BOARD_LCD_H_RES, y + 16, black);
        }
        heap_caps_free(black);
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), TAG, "disp on");
    ESP_LOGI(TAG, "st7789 %dx%d spi %d MHz swap=%d mirror=%d/%d invert=%d",
             BOARD_LCD_H_RES, BOARD_LCD_V_RES, CONFIG_INCUTWIN_LCD_PCLK_MHZ,
             LCD_SWAP_XY, LCD_MIRROR_X, LCD_MIRROR_Y, LCD_INVERT);
    return ESP_OK;
}

/* -------------------------------------------------------------------- touch */

static esp_err_t touch_init(void)
{
    i2c_master_bus_config_t bus = {
        .i2c_port = BOARD_TOUCH_I2C_PORT,
        .sda_io_num = BOARD_TOUCH_PIN_SDA,
        .scl_io_num = BOARD_TOUCH_PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus, &s_i2c_bus), TAG, "i2c bus");

    /* Elecrow monta FT5x06 o GT911 segun el lote: se sondea el bus. */
    esp_lcd_panel_io_i2c_config_t io;
    if (i2c_master_probe(s_i2c_bus, ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS, 50) == ESP_OK) {
        s_touch_chip = BOARD_TOUCH_FT5X06;
        io = (esp_lcd_panel_io_i2c_config_t)ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
    } else if (i2c_master_probe(s_i2c_bus, ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS, 50) == ESP_OK) {
        s_touch_chip = BOARD_TOUCH_GT911;
        io = (esp_lcd_panel_io_i2c_config_t)ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    } else if (i2c_master_probe(s_i2c_bus, ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP, 50) == ESP_OK) {
        s_touch_chip = BOARD_TOUCH_GT911;
        io = (esp_lcd_panel_io_i2c_config_t)ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
        io.dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP;
    } else {
        ESP_LOGE(TAG, "tactil: ningun chip responde en I2C (0x38 / 0x5D / 0x14)");
        return ESP_ERR_NOT_FOUND;
    }
    io.scl_speed_hz = 400 * 1000;

    esp_lcd_panel_io_handle_t tp_io;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(s_i2c_bus, &io, &tp_io), TAG, "touch io");

    esp_lcd_touch_config_t cfg = {
        .x_max = BOARD_LCD_H_RES,
        .y_max = BOARD_LCD_V_RES,
        .rst_gpio_num = BOARD_TOUCH_PIN_RST,
        .int_gpio_num = BOARD_TOUCH_PIN_INT,
        .levels = { .reset = 0, .interrupt = 0 },
        .flags = {
            .swap_xy = TOUCH_SWAP_XY,
            .mirror_x = TOUCH_MIRROR_X,
            .mirror_y = TOUCH_MIRROR_Y,
        },
    };
    esp_err_t err = (s_touch_chip == BOARD_TOUCH_FT5X06)
                        ? esp_lcd_touch_new_i2c_ft5x06(tp_io, &cfg, &s_touch)
                        : esp_lcd_touch_new_i2c_gt911(tp_io, &cfg, &s_touch);
    ESP_RETURN_ON_ERROR(err, TAG, "touch driver");
    ESP_LOGI(TAG, "tactil %s @0x%02x swap=%d mirror=%d/%d", board_touch_chip_name(),
             (unsigned)io.dev_addr, cfg.flags.swap_xy, cfg.flags.mirror_x, cfg.flags.mirror_y);
    return ESP_OK;
}

/* --------------------------------------------------------------------- lvgl */

static uint32_t s_touch_errors;

/* Tarea de LVGL, cada LV_DEF_REFR_PERIOD. Nunca aborta: un fallo I2C = sin toque. */
static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    data->state = LV_INDEV_STATE_RELEASED;
    esp_err_t err = esp_lcd_touch_read_data(s_touch);
    if (err != ESP_OK) {
        s_touch_errors++;
        if (s_touch_errors == 1 || s_touch_errors % 100 == 0) {
            ESP_LOGW(TAG, "tactil: error de lectura %s (x%lu)", esp_err_to_name(err),
                     (unsigned long)s_touch_errors);
        }
        return;
    }
    uint16_t x, y;
    uint8_t n = 0;
    if (esp_lcd_touch_get_coordinates(s_touch, &x, &y, NULL, &n, 1) && n > 0) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    }
}

static esp_err_t lvgl_init(void)
{
    lvgl_port_cfg_t port = ESP_LVGL_PORT_INIT_CONFIG();
    port.task_priority = 4;
    port.task_stack = 8192;
    port.task_affinity = 1; /* la red va en el core 0 */
    port.timer_period_ms = 5;
    ESP_RETURN_ON_ERROR(lvgl_port_init(&port), TAG, "lvgl port");

    lvgl_port_display_cfg_t disp = {
        .io_handle = s_lcd_io,
        .panel_handle = s_panel,
        .buffer_size = BOARD_LCD_H_RES * LVGL_BUF_LINES,
        .double_buffer = true,
        .hres = BOARD_LCD_H_RES,
        .vres = BOARD_LCD_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = LCD_SWAP_XY,
            .mirror_x = LCD_MIRROR_X,
            .mirror_y = LCD_MIRROR_Y,
        },
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0,
            .swap_bytes = 1, /* el ST7789 espera RGB565 big-endian por SPI */
        },
    };
    s_disp = lvgl_port_add_disp(&disp);
    ESP_RETURN_ON_FALSE(s_disp, ESP_FAIL, TAG, "lvgl display");

    if (s_touch) {
        /* Indev propio en vez de lvgl_port_add_touch(): el del port envuelve la
         * lectura I2C en ESP_ERROR_CHECK y un NACK transitorio del FT5x06
         * aborta todo el firmware (core dump del 1-oct-2026). Aqui un error de
         * lectura cuenta como "sin toque" y se registra. */
        lvgl_port_lock(0);
        s_indev = lv_indev_create();
        lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(s_indev, touch_read_cb);
        lv_indev_set_display(s_indev, s_disp);
        lvgl_port_unlock();
    }
    return ESP_OK;
}

/* ------------------------------------------------------------ pwm & button */

static esp_err_t pwm_init(void)
{
    ledc_timer_config_t bl_t = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = BL_RES,
        .timer_num = BL_TIMER,
        .freq_hz = BL_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&bl_t), TAG, "bl timer");
    ledc_channel_config_t bl_c = {
        .gpio_num = BOARD_PIN_BACKLIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = BL_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = BL_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&bl_c), TAG, "bl channel");

    ledc_timer_config_t bz_t = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = BZ_RES,
        .timer_num = BZ_TIMER,
        .freq_hz = BZ_IDLE_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&bz_t), TAG, "bz timer");
    ledc_channel_config_t bz_c = {
        .gpio_num = BOARD_PIN_BUZZER,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = BZ_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = BZ_TIMER,
        .duty = 0, /* en silencio desde el primer ciclo: sin "clic" al arrancar */
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&bz_c), TAG, "bz channel");
    return ESP_OK;
}

static esp_err_t button_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << BOARD_PIN_BUTTON,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    return gpio_config(&io);
}

/* ---------------------------------------------------------------------- API */

esp_err_t board_init(void)
{
    ESP_LOGI(TAG, "psram: %u MB", (unsigned)(esp_psram_get_size() >> 20));
    ESP_RETURN_ON_ERROR(pwm_init(), TAG, "pwm");
    ESP_RETURN_ON_ERROR(button_init(), TAG, "button");
    ESP_RETURN_ON_ERROR(display_init(), TAG, "display");
    esp_err_t terr = touch_init();
    if (terr != ESP_OK) {
        ESP_LOGW(TAG, "sin tactil (%s): la UI seguira sin entrada", esp_err_to_name(terr));
        s_touch = NULL;
        s_touch_chip = BOARD_TOUCH_NONE;
    }
    ESP_RETURN_ON_ERROR(lvgl_init(), TAG, "lvgl");
    board_backlight_set(100);
    board_log_memory(TAG);
    return ESP_OK;
}

lv_display_t *board_display(void) { return s_disp; }
lv_indev_t *board_touch_indev(void) { return s_indev; }
esp_lcd_touch_handle_t board_touch_handle(void) { return s_touch; }
board_touch_chip_t board_touch_chip(void) { return s_touch_chip; }

void board_touch_default_flags(bool *swap_xy, bool *mirror_x, bool *mirror_y)
{
    if (swap_xy) *swap_xy = TOUCH_SWAP_XY;
    if (mirror_x) *mirror_x = TOUCH_MIRROR_X;
    if (mirror_y) *mirror_y = TOUCH_MIRROR_Y;
}

const char *board_touch_chip_name(void)
{
    switch (s_touch_chip) {
    case BOARD_TOUCH_FT5X06: return "FT5x06";
    case BOARD_TOUCH_GT911:  return "GT911";
    default:                 return "none";
    }
}

void board_backlight_set(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }
    s_backlight_pct = percent;
    uint32_t duty = ((1u << 8) - 1) * percent / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BL_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BL_CHANNEL);
}

uint8_t board_backlight_get(void) { return s_backlight_pct; }

void board_buzzer_tone(uint32_t freq_hz, uint16_t duty)
{
    if (freq_hz == 0 || duty == 0) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, BZ_CHANNEL, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, BZ_CHANNEL);
        return;
    }
    if (duty > 1023) {
        duty = 1023;
    }
    ledc_set_freq(LEDC_LOW_SPEED_MODE, BZ_TIMER, freq_hz);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BZ_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BZ_CHANNEL);
}

bool board_button_pressed(void) { return gpio_get_level(BOARD_PIN_BUTTON) == 0; }

uint32_t board_touch_error_count(void) { return s_touch_errors; }

void board_log_memory(const char *tag)
{
    ESP_LOGI(tag, "heap libre interno %u KB (bloque mayor %u KB), psram %u KB",
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) >> 10),
             (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) >> 10),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) >> 10));
}
