#include "sfx.h"
#include "board.h"
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BLOCK 256
static int16_t s_buf[BLOCK * 2];
static uint32_t s_lfsr = 0x1234567u;

static inline float noise(void)
{
    s_lfsr ^= s_lfsr << 13; s_lfsr ^= s_lfsr >> 17; s_lfsr ^= s_lfsr << 5;
    return (int32_t)s_lfsr / 2147483648.0f;
}

// Render `ms` of audio from a per-sample generator. t = seconds since start.
typedef float (*gen_t)(float t, void *ctx);

static void render(int ms, gen_t g, void *ctx)
{
    int total = SAMPLE_RATE * ms / 1000, done = 0;
    while (done < total) {
        int n = total - done < BLOCK ? total - done : BLOCK;
        for (int k = 0; k < n; k++) {
            float v = g((float)(done + k) / SAMPLE_RATE, ctx);
            if (v > 1) v = 1;
            if (v < -1) v = -1;
            s_buf[2 * k] = s_buf[2 * k + 1] = (int16_t)(v * 26000);
        }
        audio_write(s_buf, n);
        done += n;
    }
}

static void silence(int ms)
{
    memset(s_buf, 0, sizeof s_buf);
    int total = SAMPLE_RATE * ms / 1000;
    while (total > 0) {
        int n = total < BLOCK ? total : BLOCK;
        audio_write(s_buf, n);
        total -= n;
    }
}

// ---- generators ------------------------------------------------------------
typedef struct { float lp, lp2, phase; float cutoff, decay, thump, gain; } blast_t;

// Gunshot: filtered noise burst + low pitch-dropping thump
static float gen_blast(float t, void *c)
{
    blast_t *b = c;
    float env = expf(-t / b->decay);
    b->lp += b->cutoff * (noise() - b->lp);
    b->lp2 += b->cutoff * (b->lp - b->lp2);
    float f = 90.0f * expf(-t * 18.0f) + 38.0f;
    b->phase += f / SAMPLE_RATE;
    float th = sinf(2 * (float)M_PI * b->phase) * expf(-t / 0.09f) * b->thump;
    return (b->lp2 * 3.0f * env + th) * b->gain;
}

// Shotgun pump: two short bright clicks
static float gen_click(float t, void *c)
{
    float *hp = c;
    float n = noise();
    float v = n - *hp; *hp = n * 0.6f + *hp * 0.4f;
    return v * expf(-t / 0.012f) * 0.7f;
}

typedef struct { float phase, lp; } tone_t;

// Imp fireball: roaring noise swell with a falling growl underneath
static float gen_fireball(float t, void *c)
{
    tone_t *s = c;
    float env = t < 0.15f ? t / 0.15f : expf(-(t - 0.15f) / 0.35f);
    s->lp += 0.08f * (noise() - s->lp);
    float f = 260.0f * expf(-t * 1.6f) + 60.0f + 18.0f * sinf(2 * (float)M_PI * 23.0f * t);
    s->phase += f / SAMPLE_RATE;
    float sq = fmodf(s->phase, 1.0f) < 0.5f ? 0.35f : -0.35f;
    return (s->lp * 2.2f + sq) * env;
}

// Item pickup: two-step square chirp
static float gen_pickup(float t, void *c)
{
    tone_t *s = c;
    float f = t < 0.07f ? 1046.5f : 1568.0f;
    s->phase += f / SAMPLE_RATE;
    float env = t < 0.07f ? 1.0f : expf(-(t - 0.07f) / 0.08f);
    return (fmodf(s->phase, 1.0f) < 0.5f ? 0.45f : -0.45f) * env;
}

// Door: grinding low rumble with motor whine rising
static float gen_door(float t, void *c)
{
    tone_t *s = c;
    float env = t < 0.1f ? t / 0.1f : (t > 0.9f ? expf(-(t - 0.9f) / 0.08f) : 1.0f);
    s->lp += 0.02f * (noise() - s->lp);
    s->phase += (70.0f + 50.0f * t) / SAMPLE_RATE;
    float saw = fmodf(s->phase, 1.0f) * 2 - 1;
    return (s->lp * 5.0f + saw * 0.25f) * env * 0.8f;
}

// BFG charge: rising square whine with warble
static float gen_bfg_charge(float t, void *c)
{
    tone_t *s = c;
    float f = 150.0f * powf(9.0f, t / 1.1f) * (1 + 0.03f * sinf(2 * (float)M_PI * 30 * t));
    s->phase += f / SAMPLE_RATE;
    return (fmodf(s->phase, 1.0f) < 0.5f ? 0.4f : -0.4f) * (0.3f + 0.7f * t / 1.1f);
}

// ---- effects ----------------------------------------------------------------
static void lamp_flash(bool r, bool g, bool b, bool w) { lamp_set(r, g, b, w); }

void sfx_pistol(void)
{
    blast_t b = {.cutoff = 0.55f, .decay = 0.05f, .thump = 0.35f, .gain = 0.8f};
    lamp_flash(true, true, false, false);  // muzzle flash: yellow
    render(60, gen_blast, &b);
    lamp_off();
    render(140, gen_blast, &b);
}

void sfx_shotgun(void)
{
    disp_text("SHOt");
    blast_t b = {.cutoff = 0.35f, .decay = 0.11f, .thump = 0.9f, .gain = 1.0f};
    lamp_flash(true, true, true, true);    // big white flash
    render(90, gen_blast, &b);
    lamp_flash(true, true, false, false);
    render(120, gen_blast, &b);
    lamp_off();
    render(250, gen_blast, &b);
    silence(120);
    float hp = 0;
    render(60, gen_click, &hp);            // ka-
    silence(90);
    hp = 0;
    render(70, gen_click, &hp);            // -chunk
    silence(200);
}

void sfx_imp_fireball(void)
{
    disp_text("FIrE");
    tone_t s = {0};
    for (int i = 0; i < 4; i++) {          // flickering red/orange
        lamp_flash(true, i & 1, false, false);
        render(150, gen_fireball, &s);
    }
    lamp_off();
    render(300, gen_fireball, &s);
    // impact
    blast_t b = {.cutoff = 0.25f, .decay = 0.15f, .thump = 1.0f, .gain = 0.9f};
    lamp_flash(true, false, false, false);
    render(120, gen_blast, &b);
    lamp_off();
    render(280, gen_blast, &b);
}

void sfx_pickup(void)
{
    disp_text("Ar 1");
    tone_t s = {0};
    lamp_flash(false, true, false, false);  // green
    render(220, gen_pickup, &s);
    lamp_off();
    silence(150);
}

void sfx_door(void)
{
    disp_text("dOOr");
    tone_t s = {0};
    lamp_flash(false, false, true, false);  // blue
    render(1000, gen_door, &s);
    lamp_off();
    silence(200);
}

void sfx_bfg(void)
{
    disp_text("bFG ");
    tone_t s = {0};
    // charge: green blinking faster and faster
    int t = 0, period = 220;
    bool on = false;
    while (t < 1100) {
        on = !on;
        if (on) lamp_flash(false, true, false, false); else lamp_off();
        int seg = period / 2;
        // render a slice of the charge with the right time offset
        int total = SAMPLE_RATE * seg / 1000;
        for (int done = 0; done < total;) {
            int n = total - done < BLOCK ? total - done : BLOCK;
            for (int k = 0; k < n; k++) {
                float v = gen_bfg_charge((float)(SAMPLE_RATE * t / 1000 + done + k) / SAMPLE_RATE, &s);
                s_buf[2 * k] = s_buf[2 * k + 1] = (int16_t)(v * 26000);
            }
            audio_write(s_buf, n);
            done += n;
        }
        t += seg;
        if (period > 60) period -= 25;
    }
    // blast
    blast_t b = {.cutoff = 0.2f, .decay = 0.35f, .thump = 1.0f, .gain = 1.0f};
    disp_segments(0x7f, 0x7f, 0x7f, 0x7f, true);
    lamp_flash(false, true, false, true);
    render(200, gen_blast, &b);
    lamp_flash(false, true, false, false);
    render(300, gen_blast, &b);
    lamp_off();
    render(500, gen_blast, &b);
    disp_text("    ");
    silence(200);
}

void sfx_interlude(volatile bool *stop)
{
#define STEP(x) do { if (stop && *stop) goto out; x; } while (0)
    STEP(sfx_door());
    STEP(sfx_pistol());
    STEP(silence(150));
    STEP(sfx_pistol());
    STEP(silence(300));
    STEP(sfx_imp_fireball());
    STEP(silence(250));
    STEP(sfx_shotgun());
    STEP(sfx_pickup());
    STEP(silence(300));
    STEP(sfx_bfg());
    STEP(silence(500));
out:
    lamp_off();
#undef STEP
}
