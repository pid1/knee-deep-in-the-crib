#include "synth.h"
#include "board.h"
#include <math.h>
#include <string.h>

#define BLOCK 256

typedef struct {
    uint32_t phase, inc;  // 32-bit phase accumulator
    uint32_t duty;        // pulse width as a phase threshold
    int32_t amp, decay;   // envelope, Q15
    int32_t level;
} voice_t;

static uint32_t note_inc(int midi)
{
    double hz = 440.0 * pow(2.0, (midi - 69) / 12.0);
    return (uint32_t)(hz * 4294967296.0 / SAMPLE_RATE);
}

static inline int32_t pulse(voice_t *v)
{
    v->phase += v->inc;
    int32_t s = v->phase < v->duty ? v->level : -v->level;
    return (s * v->amp) >> 15;
}

static void trigger(voice_t *v, int midi, int32_t level)
{
    v->inc = note_inc(midi);
    v->amp = 32767;
    v->level = level;
}

static int16_t s_buf[BLOCK * 2];

void synth_play(const song_t *song, step_cb_t cb, volatile bool *stop)
{
    voice_t lead = {.duty = 0x40000000u, .decay = 32740};  // 25% pulse, slow decay
    voice_t bass = {.duty = 0x80000000u, .decay = 32755};  // square
    uint32_t lfsr = 0xACE1u;
    int32_t noise_amp = 0, noise_decay = 0, noise_level = 0;
    int32_t kick_amp = 0;
    uint32_t kick_phase = 0, kick_inc = 0;

    const int samples_per_step = SAMPLE_RATE * song->step_ms / 1000;
    for (int i = 0; i < song->count && !(stop && *stop); i++) {
        const step_t *s = &song->steps[i];
        if (cb) cb(i, s);
        if (s->lead == 0) lead.amp = 0;
        else if (s->lead != NOTE_HOLD) trigger(&lead, s->lead, 5200);
        if (s->bass == 0) bass.amp = 0;
        else if (s->bass != NOTE_HOLD) trigger(&bass, s->bass, 4200);
        if (s->drum == 1) { kick_amp = 32767; kick_inc = note_inc(40); kick_phase = 0; }
        if (s->drum == 2) { noise_amp = 32767; noise_decay = 32600; noise_level = 4200; }
        if (s->drum == 3) { noise_amp = 32767; noise_decay = 31500; noise_level = 2000; }

        int left = samples_per_step;
        while (left > 0) {
            int n = left < BLOCK ? left : BLOCK;
            for (int k = 0; k < n; k++) {
                int32_t mix = pulse(&lead) + pulse(&bass);
                lead.amp = (lead.amp * lead.decay) >> 15;
                bass.amp = (bass.amp * bass.decay) >> 15;
                if (noise_amp > 64) {
                    lfsr = (lfsr >> 1) ^ (-(lfsr & 1u) & 0xB400u);
                    int32_t ns = (lfsr & 1) ? noise_level : -noise_level;
                    mix += (ns * noise_amp) >> 15;
                    noise_amp = (noise_amp * noise_decay) >> 15;
                }
                if (kick_amp > 64) {
                    // pitch-dropping square thump
                    kick_phase += kick_inc;
                    kick_inc -= kick_inc >> 9;
                    int32_t ks = (kick_phase & 0x80000000u) ? 6000 : -6000;
                    mix += (ks * kick_amp) >> 15;
                    kick_amp = (kick_amp * 32680) >> 15;
                }
                if (mix > 32767) mix = 32767;
                if (mix < -32768) mix = -32768;
                s_buf[2 * k] = s_buf[2 * k + 1] = (int16_t)mix;
            }
            audio_write(s_buf, n);
            left -= n;
        }
    }
    memset(s_buf, 0, sizeof(s_buf));
    audio_write(s_buf, BLOCK);
}

void synth_tone(float hz, int ms)
{
    uint32_t phase = 0, inc = (uint32_t)(hz * 4294967296.0 / SAMPLE_RATE);
    int total = SAMPLE_RATE * ms / 1000;
    while (total > 0) {
        int n = total < BLOCK ? total : BLOCK;
        for (int k = 0; k < n; k++) {
            phase += inc;
            int16_t v = (phase & 0x80000000u) ? 6000 : -6000;
            s_buf[2 * k] = s_buf[2 * k + 1] = v;
        }
        audio_write(s_buf, n);
        total -= n;
    }
    memset(s_buf, 0, sizeof(s_buf));
    audio_write(s_buf, BLOCK);
}
