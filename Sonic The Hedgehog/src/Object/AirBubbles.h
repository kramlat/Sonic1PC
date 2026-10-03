#pragma once

#include "Object.h"

// Object 0x64 - LZ air bubbles: individual rising bubbles (small/medium/
// large, with a bursting animation once they reach the water surface) and
// the stationary "bubble maker" that spawns them at random intervals from
// the floor. Shares its mappings/animation/wobble-offset table with the
// drowning countdown's own number-bubble children (Object/DrownCount.h) --
// see s1disasm's "64 LZ Air Bubbles.asm" and "0A LZ Drowning Countdown.asm",
// both of which reference the same Map_Bub/Ani_Bub/Drown_WobbleData.

// obj->anim values for a regular (non-bubble-maker) bubble
#define BubbleAnim_Small  0
#define BubbleAnim_Medium 1
#define BubbleAnim_Large  2

// obj->subtype for a bubble maker: bit 7 set, low 7 bits = spawn frequency
// (frames between spawns). For a regular bubble, subtype instead selects
// its size (0 = small, 1 = medium, 2 = large/inhalable).
#define BUBBLE_MAKER_BIT 0x80

// VRAM tile base for the (statically PLC'd, not dynamic) bubble/countdown
// digit art -- see PLC.c's PLC_LZ ({ Art_Bubbles, 0x6900 }).
#define ArtTile_LZ_Bubbles 0x348

extern const uint8_t Mappings_Bubbles[];
extern const uint8_t Animation_Bubbles[];

void Obj_Bubble(Object *obj);
