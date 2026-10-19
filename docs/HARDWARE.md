# Hardware

What's inside a Nanit Light & Sound, and how to drive it. Everything here came from
static analysis of the stock firmware (`nanit-lite-fw` v1.3.1, written in Rust on
ESP-IDF 5.2.2), and was then confirmed on a real unit: by microphone for audio, by
camera for the lamp and display.

- MCU: ESP32-S3-WROOM-1U, 16 MB flash, 8 MB octal PSRAM
- Flash encryption off, secure boot off, USB-Serial-JTAG enabled

## Pin map

| function | pins | notes |
|---|---|---|
| I2S to ES8311 | BCLK **40**, WS **42**, DOUT **39**, DIN 45 | stock runs TDM at 32 kHz with **no MCLK**; the ES8311 must clock from BCLK |
| I2C | SDA **41**, SCL **38** | 0x18 ES8311 codec, 0x40 mic ADC, 0x44 SHT40, 0x62 fuel gauge |
| Amp enable | **7** | active high |
| Display | UART2 TX **GPIO2** | TM1652 LED driver, see below |
| Lamp | LEDC on 9, 10, 11, 12, 13, 46 (+4) | see below |
| Buttons | 0 (top), 14, 21, 47, 48 | active low; holding the top button at power-up enters download mode |
| GPIO6 | input | stock reads it as a status line |
| GPIO19/20 | USB | stock drives them, which is why a stock unit never shows up over USB |

## Display: TM1652 on UART2, 19200 baud, 8O1

- Digits: `08 d0 d1 colon d2 d3`. Segment bytes are standard gfedcba (bit 0 = a);
  `colon` is `00` or `ff`.
- Brightness and power: `18 (level | 0e)`, with `level` taken from
  `00 80 c0 a0 e0 90 d0 b0` (`00` = off, the last entry = brightest; the bit order is
  reversed by design).
- **Leave an idle gap of a few milliseconds between commands.** Frames sent back to
  back get merged and dropped. That, plus the odd parity, is the whole trick.
- The chip holds its last frame while powered. A display still showing the stock clock
  doesn't mean anything is driving it.

## Lamp (LEDC, 5 kHz)

| GPIO | role |
|---|---|
| 10 | red |
| 11 | green |
| 12 | blue |
| 13 | white |
| 9 | enable; needs roughly 100 ms high before light appears |
| 46 | latch: colours only take effect on a rising edge |
| 4 | not needed for colour; role unknown |

To set a colour: drop 46, set the colour channels, raise 9, raise 46 (`lamp_set()` in
`firmware/main/board.c`). Dropping 46 turns the lamp off, and re-latching is fast
enough to flash on the beat. Partial duty on a colour channel does not dim it.

## Stock flash layout

The partition table is at **0x10000**, not the usual 0x8000:

| partition | offset | contents |
|---|---|---|
| nvs_factory | 0x11000 | per-device keys and pairing state |
| nvs | 0x14000 | settings, Wi-Fi |
| otadata | 0x2d000 | |
| phy_init | 0x2f000 | |
| factory | 0x30000 | v0.8.4 |
| ota_0 | 0x430000 | v1.3.1 |
| ota_1 | 0x9b0000 | |

## Where things are in the stock image

For anyone repeating the analysis on their own dump (`tools/xref.py` helps): the I2S
TDM setup is a monomorphized esp-idf-hal constructor that passes constant pins to
`i2s_channel_init_tdm_mode`. Seven `ledc_channel_config` call sites carry the lamp
pins. The display driver calls `uart_write_bytes` with the frames above, and its font
table in rodata is immediately followed by the string "Failed to set time". Stock uses the legacy
I2C driver, and `gpio_set_level` is called on 3, 7, 19 and 20.

## Notes from bring-up

- Stock disables USB once it boots, so the first flash needs download mode: hold the
  top button while plugging in or powering on. This firmware keeps USB up, so after
  that, esptool can reset and reflash it without touching the device.
- Don't pass `--baud` to esptool over USB-Serial-JTAG.
- Opening the serial port with the wrong DTR/RTS states resets the chip into download
  mode. `tools/con.py` keeps EN released.
- macOS may silently refuse camera access to command-line tools. The camera checks here
  were done by grabbing the Photo Booth preview (`tools/look.sh`; `tools/nudge` keeps
  Photo Booth from pausing).
