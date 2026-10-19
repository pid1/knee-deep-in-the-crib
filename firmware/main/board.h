// Nanit Light & Sound ("Cullinan", nanit-lite-fw) board support.
// Pin assignments recovered from the stock v1.3.1 firmware (see notes/FINDINGS.md).
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// I2C: ES8311 codec @0x18, mic ADC @0x40, fuel gauge @0x62
#define PIN_I2C_SDA 41
#define PIN_I2C_SCL 38

// I2S to ES8311 (stock: TDM, no MCLK; codec derives its clock from BCLK)
#define PIN_I2S_BCLK 40
#define PIN_I2S_WS   42
#define PIN_I2S_DOUT 39
#define PIN_I2S_DIN  45

#define PIN_AMP_EN 7

// Display: TM1652 LED driver on UART2, TX only, 19200 8O1
#define PIN_DISP_TX 2
#define DISP_BAUD   19200

// Buttons, active low
#define PIN_BTN_CONTROL 0

// LEDC PWM outputs used by stock firmware (channel order as stock assigns them)
#define LED_COUNT 7
extern const int led_pins[LED_COUNT];

#define SAMPLE_RATE 32000

void board_init(void);

// LEDs: duty 0..1023 on the LEDC channel driving `gpio`
bool led_set_gpio(int gpio, int duty);
void led_all(int duty);

// Lamp (LEDC): colour channels R=10 G=11 B=12 W=13; GPIO9 + GPIO46 must be high.
// Colours only take effect if set while GPIO46 is low, so lamp_set() drops 46,
// sets the colours, then raises 46 again.
#define LAMP_R 10
#define LAMP_G 11
#define LAMP_B 12
#define LAMP_W 13
#define LAMP_EN 9
#define LAMP_LATCH 46
void lamp_set(bool r, bool g, bool b, bool w);
void lamp_off(void);
void gpio_out(int gpio, int level);

// Display
void disp_raw(const uint8_t *b, size_t n);
void disp_brightness(int level /*0..7*/, bool on);
void disp_segments(uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3, bool colon);
void disp_text(const char *s);
int disp_loopback(const uint8_t *b, size_t n, uint8_t *rx, size_t rxmax);
void disp_set_pin(int tx);
void disp_set_baud(int baud);
void disp_set_invert(bool inv);
void disp_set_parity(int p);

// Audio
void audio_write(const int16_t *stereo, size_t frames);
void audio_volume(int vol /*0..100*/);
void audio_amp(bool on);

void i2c_scan(void);
