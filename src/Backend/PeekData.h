#pragma once

// Plain data snapshots for the debug viewers (the in-frame overlays on SDL2,
// the tool windows on the Qt backend). Pure C types only, so C++ can include it.

#include <stdint.h>

// "Z80 Peek" -- live YM2612/SN76489 register state. Gathered fresh from
// sound_music each frame by Game.c's main loop (the one place that already has
// direct access to both SoundChipSet and the render backend); the renderers
// only ever see this plain struct.
typedef struct {
    uint8_t fm_alg_fb[2][3];  // [port][chan]: raw $B0+ch byte (algorithm low 3 bits, feedback next 3)
    uint8_t fm_tl[2][3][4];   // [port][chan][operator 0=op1..3=op4]: raw $40+op*4+ch byte
    uint8_t fm_pan[2][3];     // [port][chan]: raw $B4+ch byte (bit7 = left, bit6 = right)
    uint16_t fm_freq[2][3];   // [port][chan]: block (bits 13-11) and F-number (bits 10-0) from $A4+ch / $A0+ch
    uint8_t fm_keyon;         // persistent key state, one bit per channel (bit = port*3 + chan)
    uint16_t psg_tone_period[3];
    uint8_t psg_tone_atten[3];
    uint8_t psg_noise_atten;
    uint8_t psg_noise_shift_rate;
    uint8_t psg_noise_fb_white;
} Z80PeekData;

// One entry of the VDP sprite table, decoded (walked in link order, like the
// hardware does). x/y are screen coordinates (sprite coordinates minus 128).
typedef struct {
    uint8_t index;       // position in the table
    uint8_t link;        // next sprite in the chain (0 = end)
    int16_t x, y;
    uint8_t width, height; // in tiles, 1-4
    uint16_t pattern;    // first tile
    uint8_t palette;     // 0-3
    uint8_t priority;    // 1 = in front of high-priority planes
    uint8_t x_flip, y_flip;
} VdpSpritePeek;

// One object slot (the game's "RAM"): the fields that matter when watching objects.
typedef struct {
    uint8_t type, routine, routine_sec, frame, anim, render, status, subtype;
    int16_t x, y, xsp, ysp;
} ObjectPeek;

// Watchable game variables (DebugVars.c): what a variable is, for display and editing.
enum { VAR_U8, VAR_S8, VAR_U16, VAR_S16, VAR_U32, VAR_S32, VAR_BOOL, VAR_FIXED /* 16.16 */ };

typedef struct {
    const char *name, *group; // group = the header that declares it
    int count;                // 1 for a scalar, N for an array
    int elem_size;            // bytes per element
    int kind;                 // VAR_*
} VarPeek;
