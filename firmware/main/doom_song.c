// "At Doom's Gate" (E1M1) main riff, as a 16th-note step sequence.
#include "synth.h"

#define E2 40
#define A2 45
#define MAX_STEPS 512

static step_t s_steps[MAX_STEPS];
static int s_n;

static void put(uint8_t riff_note, uint8_t drum)
{
    if (s_n >= MAX_STEPS) return;
    step_t *s = &s_steps[s_n++];
    if (riff_note == 0 || riff_note == NOTE_HOLD) {
        s->lead = s->bass = riff_note;
    } else {
        s->lead = riff_note + 12;
        s->bass = riff_note;
    }
    s->drum = drum;
}

static uint8_t drum_for(int i)
{
    switch (i & 15) {
    case 0: case 8: return 1;   // kick
    case 4: case 12: return 2;  // snare
    default: return (i & 1) ? 0 : 3;
    }
}

// Bar A: pedal on the root with a chromatic walk down from the octave
static void bar_a(uint8_t r)
{
    const int8_t off[16] = {0, 0, 12, 0, 0, 10, 0, 0, 8, 0, 0, 6, 0, 0, 7, 8};
    for (int i = 0; i < 16; i++) put(r + off[i], drum_for(i));
}

// Bar B: same walk, landing on the tritone and holding it
static void bar_b(uint8_t r)
{
    const int8_t off[12] = {0, 0, 12, 0, 0, 10, 0, 0, 8, 0, 0, 6};
    for (int i = 0; i < 12; i++) put(r + off[i], drum_for(i));
    put(NOTE_HOLD, drum_for(12));
    put(NOTE_HOLD, drum_for(13));
    put(NOTE_HOLD, drum_for(14));
    put(0, drum_for(15));
}

static void build(void)
{
    s_n = 0;
    for (int k = 0; k < 3; k++) { bar_a(E2); bar_b(E2); }
    bar_a(A2); bar_b(A2);
    bar_a(E2); bar_b(E2);
    // final hit
    put(E2, 1); put(NOTE_HOLD, 0); put(NOTE_HOLD, 0); put(NOTE_HOLD, 0);
    put(NOTE_HOLD, 0); put(NOTE_HOLD, 0); put(0, 0); put(0, 0);
}


const song_t *doom_song_get(void)
{
    static song_t song;
    build();
    song = (song_t){s_steps, s_n, 105};
    return &song;
}
