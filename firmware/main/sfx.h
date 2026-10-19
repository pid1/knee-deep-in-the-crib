#pragma once
#include <stdbool.h>

// Synthesized Doom-style sound effects. Each call blocks while it renders;
// lamp and display cues are issued from inside so they line up with the audio.
void sfx_shotgun(void);
void sfx_pistol(void);
void sfx_imp_fireball(void);
void sfx_pickup(void);
void sfx_door(void);
void sfx_bfg(void);

// The whole interlude between song loops (~8 s).
void sfx_interlude(volatile bool *stop);
