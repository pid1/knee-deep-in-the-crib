#include "board.h"
#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/uart.h"
#include "esp_rom_gpio.h"
#include "soc/gpio_sig_map.h"
#include "soc/gpio_periph.h"
#include "soc/io_mux_reg.h"
#include "driver/i2c.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "board";

const int led_pins[LED_COUNT] = {10, 11, 12, 46, 9, 4, 13};

#define DISP_UART UART_NUM_2
#define I2C_PORT  I2C_NUM_0

static i2s_chan_handle_t s_tx;
static esp_codec_dev_handle_t s_codec;

// ---------------------------------------------------------------- LEDs
static void leds_init(void)
{
    ledc_timer_config_t t = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&t));
    for (int i = 0; i < LED_COUNT; i++) {
        ledc_channel_config_t c = {
            .gpio_num = led_pins[i],
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = (ledc_channel_t)i,
            .timer_sel = LEDC_TIMER_0,
            .duty = 0,
        };
        ESP_ERROR_CHECK(ledc_channel_config(&c));
    }
}

bool led_set_gpio(int gpio, int duty)
{
    if (duty < 0) duty = 0;
    if (duty > 1023) duty = 1023;
    for (int i = 0; i < LED_COUNT; i++) {
        if (led_pins[i] == gpio) {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)i, duty);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)i);
            return true;
        }
    }
    return false;
}

void led_all(int duty)
{
    for (int i = 0; i < LED_COUNT; i++) led_set_gpio(led_pins[i], duty);
}

void lamp_set(bool r, bool g, bool b, bool w)
{
    led_set_gpio(LAMP_LATCH, 0);
    led_set_gpio(LAMP_R, r ? 1023 : 0);
    led_set_gpio(LAMP_G, g ? 1023 : 0);
    led_set_gpio(LAMP_B, b ? 1023 : 0);
    led_set_gpio(LAMP_W, w ? 1023 : 0);
    led_set_gpio(LAMP_EN, 1023);
    if (r || g || b || w) led_set_gpio(LAMP_LATCH, 1023);
}

void lamp_off(void)
{
    led_set_gpio(LAMP_LATCH, 0);
}

void gpio_out(int gpio, int level)
{
    gpio_reset_pin(gpio);
    gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(gpio, level);
}

// ---------------------------------------------------------------- display
// Stock protocol (nanit-lite-fw display driver @0x4207e35c):
//   digits:     08 c0 c1 colon(00/ff) c2 c3     (standard gfedcba segment bytes)
//   brightness: 18 (level_bits | 0x0e)          (level_bits from table below; 0 = off)
static const uint8_t k_bright[8] = {0x00, 0x80, 0xc0, 0xa0, 0xe0, 0x90, 0xd0, 0xb0};

// Stock font table @0x3c31af10, ASCII 0x20..0x7e
static const uint8_t k_font[95] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,
    0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f,0x80,0x00,0x00,0x09,0x00,0x00,
    0x00,0x77,0x7f,0x39,0x3f,0x79,0x71,0x3d,0x76,0x06,0x1e,0x69,0x38,0x15,0x37,0x3f,
    0x73,0x67,0x31,0x6d,0x78,0x3e,0x2a,0x1d,0x76,0x6e,0x5b,0x39,0x00,0x0f,0x00,0x08,
    0x00,0x5f,0x7c,0x58,0x5e,0x7b,0x31,0x6f,0x74,0x04,0x0e,0x75,0x30,0x55,0x54,0x5c,
    0x73,0x67,0x50,0x6d,0x78,0x1c,0x2a,0x1d,0x76,0x6e,0x47,0x00,0x06,0x00,0x00,
};

static void disp_init(void)
{
    uart_config_t c = {
        .baud_rate = DISP_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_ODD,   // TM1652: 19200 8O1
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(DISP_UART, 256, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(DISP_UART, &c));
    ESP_ERROR_CHECK(uart_set_pin(DISP_UART, PIN_DISP_TX, UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

void disp_raw(const uint8_t *b, size_t n)
{
    uart_write_bytes(DISP_UART, b, n);
    uart_wait_tx_done(DISP_UART, pdMS_TO_TICKS(50));
    // TM1652 needs an idle gap between commands or it merges and drops them
    vTaskDelay(pdMS_TO_TICKS(5));
}

void disp_brightness(int level, bool on)
{
    if (level < 0) level = 0;
    if (level > 7) level = 7;
    uint8_t v = k_bright[level] | 0x0e;
    uint8_t f[2] = {0x18, on ? v : (uint8_t)(v & 0x0f)};
    disp_raw(f, 2);
}

void disp_segments(uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3, bool colon)
{
    uint8_t f[6] = {0x08, d0, d1, colon ? 0xff : 0x00, d2, d3};
    disp_raw(f, 6);
}

int disp_loopback(const uint8_t *b, size_t n, uint8_t *rx, size_t rxmax)
{
    PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[PIN_DISP_TX]);
    esp_rom_gpio_connect_in_signal(PIN_DISP_TX, U2RXD_IN_IDX, false);
    uart_flush_input(DISP_UART);
    disp_raw(b, n);
    int got = uart_read_bytes(DISP_UART, rx, rxmax, pdMS_TO_TICKS(100));
    return got;
}

void disp_set_pin(int tx) { uart_set_pin(DISP_UART, tx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE); }
void disp_set_baud(int baud) { uart_set_baudrate(DISP_UART, baud); }
void disp_set_invert(bool inv) { uart_set_line_inverse(DISP_UART, inv ? UART_SIGNAL_TXD_INV : 0); }
void disp_set_parity(int p) { uart_set_parity(DISP_UART, (uart_parity_t)p); }

static uint8_t glyph(char c)
{
    if (c < 0x20 || c > 0x7e) return 0;
    return k_font[c - 0x20];
}

void disp_text(const char *s)
{
    uint8_t d[4] = {0};
    for (int i = 0; i < 4 && s[i]; i++) d[i] = glyph(s[i]);
    disp_segments(d[0], d[1], d[2], d[3], false);
}

// ---------------------------------------------------------------- audio
static void i2c_init(void)
{
    i2c_config_t c = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &c));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0));
}

void i2c_scan(void)
{
    for (int a = 0x08; a < 0x78; a++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (a << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t r = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(20));
        i2c_cmd_link_delete(cmd);
        if (r == ESP_OK) printf("i2c: 0x%02x\n", a);
    }
}

static void audio_init(void)
{
    i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    cc.auto_clear = true;
    ESP_ERROR_CHECK(i2s_new_channel(&cc, &s_tx, NULL));
    i2s_std_config_t sc = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = PIN_I2S_BCLK,
            .ws = PIN_I2S_WS,
            .dout = PIN_I2S_DOUT,
            .din = I2S_GPIO_UNUSED,
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_tx, &sc));
    ESP_ERROR_CHECK(i2s_channel_enable(s_tx));

    audio_codec_i2c_cfg_t ic = {.port = I2C_PORT, .addr = ES8311_CODEC_DEFAULT_ADDR};
    const audio_codec_ctrl_if_t *ctrl = audio_codec_new_i2c_ctrl(&ic);
    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    es8311_codec_cfg_t ec = {
        .ctrl_if = ctrl,
        .gpio_if = gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .pa_pin = PIN_AMP_EN,
        .use_mclk = false,
        .hw_gain = {.pa_voltage = 5.0, .codec_dac_voltage = 3.3},
    };
    const audio_codec_if_t *codec_if = es8311_codec_new(&ec);
    audio_codec_i2s_cfg_t dc = {.port = I2S_NUM_0, .tx_handle = s_tx};
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&dc);
    esp_codec_dev_cfg_t dev = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = codec_if,
        .data_if = data_if,
    };
    s_codec = esp_codec_dev_new(&dev);
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = SAMPLE_RATE, .channel = 2, .bits_per_sample = 16,
    };
    int r = esp_codec_dev_open(s_codec, &fs);
    ESP_LOGI(TAG, "codec open: %d", r);
    esp_codec_dev_set_out_vol(s_codec, 70);
}

void audio_write(const int16_t *stereo, size_t frames)
{
    size_t w;
    i2s_channel_write(s_tx, stereo, frames * 4, &w, portMAX_DELAY);
}

void audio_volume(int vol)
{
    esp_codec_dev_set_out_vol(s_codec, vol);
}

void audio_amp(bool on)
{
    gpio_set_level(PIN_AMP_EN, on);
}

void board_init(void)
{
    gpio_config_t btn = {
        .pin_bit_mask = 1ULL << PIN_BTN_CONTROL,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&btn);
    leds_init();
    disp_init();
    i2c_init();
    audio_init();
}
