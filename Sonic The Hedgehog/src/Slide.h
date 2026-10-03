#pragma once

#include <stdint.h>

// Slide surfaces: stretches of level where Sonic is carried along at a fixed speed whatever the pad says (Labyrinth's water slides; Oil Ocean's will use the same).
// A surface is a 128x128 foreground chunk, and the speed Sonic is carried at in pixels per frame (negative: to the left).
typedef struct {
    uint8_t chunk;
    int8_t speed;
} SlideSurface;

// Runs for the player once a frame: on a slide chunk on the ground it faces and moves him along (and plays the slide animation and the splashing sound while it lasts);
// when he leaves it, or leaves the ground, his control comes back after a moment (f_slidemode is on while it holds him).
void Slide_Update(const SlideSurface *surfaces, int count);
