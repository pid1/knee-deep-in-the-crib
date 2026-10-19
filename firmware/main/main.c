// Nanit Light & Sound custom firmware: on power-up, loop forever between a chiptune
// of the E1M1 riff (red lamp on the beat, "dOOM" scrolling in) and an interlude of
// Doom-style sound effects with matching light/display cues. USB console stays up.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"
#include "esp_vfs_usb_serial_jtag.h"
#include "esp_vfs_dev.h"
#include "board.h"
#include "synth.h"
#include "sfx.h"


static volatile bool s_stop;
static volatile bool s_loop = true;     // song -> sfx interlude -> song ... forever
static volatile bool s_restart;

// 7-seg "dOOM": d, O, O, and an n-shaped M
static const uint8_t k_doom[4] = {0x5e, 0x3f, 0x3f, 0x37};

static void show_doom(void)
{
    disp_brightness(7, true);
    disp_segments(k_doom[0], k_doom[1], k_doom[2], k_doom[3], false);
}

// Scroll "dOOM" in from the right: frame 0 = blank .. frame 4 = full word
static void show_scroll(int frame)
{
    uint8_t d[4] = {0};
    for (int i = 0; i < 4; i++) {
        int src = i - (4 - frame);
        if (src >= 0 && src < 4) d[i] = k_doom[src];
    }
    disp_segments(d[0], d[1], d[2], d[3], false);
}

static void on_step(int i, const step_t *s)
{
    // flash red on kick/snare, dark otherwise
    if (s->drum == 1 || s->drum == 2) lamp_set(true, false, false, false);
    else lamp_off();
    // display: scroll in over the first bar, then hold; blink on the final hit
    if (i <= 8 && (i & 1) == 0) show_scroll(i / 2);
    else if (s->bass == 40 && s->drum == 1 && s->lead == 52 && i > 150) disp_brightness(7, (i / 2) & 1);
}

static void audio_task(void *arg)
{
    for (;;) {
        s_stop = false;
        s_restart = false;
        disp_brightness(7, true);
        synth_play(doom_song_get(), on_step, &s_stop);
        show_doom();
        lamp_off();
        if (!s_restart && s_loop) sfx_interlude(&s_stop);
        while (!s_loop && !s_restart) vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// Control button: restart from the song
static void button_task(void *arg)
{
    int prev = 1;
    for (;;) {
        int lvl = gpio_get_level(PIN_BTN_CONTROL);
        if (prev == 1 && lvl == 0) { s_restart = true; s_stop = true; }
        prev = lvl;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static int hexbytes(char *s, uint8_t *out, int max)
{
    int n = 0;
    for (char *t = strtok(s, " "); t && n < max; t = strtok(NULL, " ")) out[n++] = strtol(t, NULL, 16);
    return n;
}

static void console(void)
{
    char line[160];
    printf("\ndoom console ready\n");
    for (;;) {
        if (!fgets(line, sizeof(line), stdin)) { vTaskDelay(10); continue; }
        line[strcspn(line, "\r\n")] = 0;
        char *arg = strchr(line, ' ');
        if (arg) *arg++ = 0; else arg = "";
        if (!strcmp(line, "led")) {
            int g = -1, d = 0;
            sscanf(arg, "%d %d", &g, &d);
            printf(led_set_gpio(g, d) ? "ok\n" : "no such led gpio\n");
        } else if (!strcmp(line, "ledall")) {
            led_all(atoi(arg));
            printf("ok\n");
        } else if (!strcmp(line, "lamp")) {
            int r = 0, g = 0, b = 0, w = 0;
            sscanf(arg, "%d %d %d %d", &r, &g, &b, &w);
            lamp_set(r, g, b, w);
            printf("ok\n");
        } else if (!strcmp(line, "blink")) {
            // blink <mode> <on_ms> <off_ms> <count>: 0 = lamp_set/lamp_off, 1 = gate GPIO9, 2 = gate red
            int mode = 0, on = 100, off = 300, n = 10;
            sscanf(arg, "%d %d %d %d", &mode, &on, &off, &n);
            lamp_set(true, false, false, false);
            for (int k = 0; k < n; k++) {
                if (mode == 0) lamp_set(true, false, false, false);
                else led_set_gpio(mode == 1 ? LAMP_EN : LAMP_R, 1023);
                vTaskDelay(pdMS_TO_TICKS(on));
                if (mode == 0) lamp_off();
                else led_set_gpio(mode == 1 ? LAMP_EN : LAMP_R, 0);
                vTaskDelay(pdMS_TO_TICKS(off));
            }
            printf("ok\n");
        } else if (!strcmp(line, "gpio")) {
            int g = -1, l = 0;
            sscanf(arg, "%d %d", &g, &l);
            gpio_out(g, l);
            printf("ok\n");
        } else if (!strcmp(line, "disp")) {
            uint8_t b[32];
            int n = hexbytes(arg, b, sizeof b);
            disp_raw(b, n);
            printf("sent %d\n", n);
        } else if (!strcmp(line, "dloop")) {
            uint8_t b[32], rx[64];
            int n = hexbytes(arg, b, sizeof b);
            int got = disp_loopback(b, n, rx, sizeof rx);
            printf("rx %d:", got);
            for (int i = 0; i < got; i++) printf(" %02x", rx[i]);
            printf("\n");
        } else if (!strcmp(line, "dpin")) {
            disp_set_pin(atoi(arg)); printf("ok\n");
        } else if (!strcmp(line, "dbaud")) {
            disp_set_baud(atoi(arg)); printf("ok\n");
        } else if (!strcmp(line, "dinv")) {
            disp_set_invert(atoi(arg)); printf("ok\n");
        } else if (!strcmp(line, "dpar")) {
            disp_set_parity(atoi(arg)); printf("ok\n");
        } else if (!strcmp(line, "text")) {
            disp_brightness(7, true);
            disp_text(arg);
            printf("ok\n");
        } else if (!strcmp(line, "bright")) {
            disp_brightness(atoi(arg), true);
            printf("ok\n");
        } else if (!strcmp(line, "play")) {
            s_loop = true; s_restart = true; s_stop = true;
            printf("ok\n");
        } else if (!strcmp(line, "stop")) {
            s_loop = false; s_stop = true;
            printf("ok\n");
        } else if (!strcmp(line, "sfx")) {
            if (!strcmp(arg, "shotgun")) sfx_shotgun();
            else if (!strcmp(arg, "pistol")) sfx_pistol();
            else if (!strcmp(arg, "imp")) sfx_imp_fireball();
            else if (!strcmp(arg, "pickup")) sfx_pickup();
            else if (!strcmp(arg, "door")) sfx_door();
            else if (!strcmp(arg, "bfg")) sfx_bfg();
            else sfx_interlude(NULL);
            printf("ok\n");
        } else if (!strcmp(line, "tone")) {
            float hz = 440; int ms = 1000;
            sscanf(arg, "%f %d", &hz, &ms);
            synth_tone(hz, ms);
            printf("ok\n");
        } else if (!strcmp(line, "vol")) {
            audio_volume(atoi(arg));
            printf("ok\n");
        } else if (!strcmp(line, "amp")) {
            audio_amp(atoi(arg));
            printf("ok\n");
        } else if (!strcmp(line, "i2c")) {
            i2c_scan();
            printf("done\n");
        } else if (line[0]) {
            printf("cmds: led <gpio> <0-1023> | ledall <d> | lamp <r> <g> <b> <w> | gpio <n> <0|1> | disp <hex..> | text <abcd> |"
                   " bright <0-7> | play | stop | sfx [shotgun|pistol|imp|pickup|door|bfg] | tone <hz> <ms> | vol <0-100> | amp <0|1> | i2c\n");
        }
    }
}

void app_main(void)
{
    usb_serial_jtag_driver_config_t uc = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    usb_serial_jtag_driver_install(&uc);
    esp_vfs_usb_serial_jtag_use_driver();
    setvbuf(stdin, NULL, _IONBF, 0);

    board_init();
    show_doom();
    xTaskCreatePinnedToCore(audio_task, "audio", 8192, NULL, 10, NULL, 1);
    xTaskCreate(button_task, "button", 2048, NULL, 5, NULL);
    console();
}
