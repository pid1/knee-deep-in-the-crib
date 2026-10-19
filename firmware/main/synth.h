#pragma once
#include <stdint.h>
#include <stdbool.h>

// One sequencer step. Notes are MIDI numbers, 0 = rest, 255 = hold previous.
typedef struct {
    uint8_t lead;
    uint8_t bass;
    uint8_t drum; // 0 none, 1 kick, 2 snare, 3 hat
} step_t;

#define NOTE_HOLD 255

typedef struct {
    const step_t *steps;
    int count;
    int step_ms;
} song_t;

const song_t *doom_song_get(void);

// Called from the audio task at the start of each step.
typedef void (*step_cb_t)(int index, const step_t *s);

void synth_play(const song_t *song, step_cb_t cb, volatile bool *stop);
void synth_tone(float hz, int ms);
