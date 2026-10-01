#pragma once

#include "Types.h"

//Special Stage constants
#define SS_SRCDIM 64
#define SS_DIM 128
#define SS_PAD (SS_DIM - SS_SRCDIM)
#define SS_PAD2 (SS_PAD >> 1)

//Special Stage state
extern word_u ss_angle;
extern uint16_t ss_rotate;
extern uint16_t palss_num;
extern int16_t palss_time; // signed: the cycle runs when this counts below zero

extern uint8_t last_special;

extern uint8_t emeralds;
extern uint8_t emerald_list[8];

extern uint8_t ss_layout[SS_DIM * SS_DIM];

//Special Stage functions
void SS_ShowLayout(uint8_t sprite_i);
void SS_Load(void);

// Special stage block IDs (the bytes of the stage layout), from "Special Stage Mappings & VRAM Pointers.asm".
enum {
    SSB_Blank = 0x00,
    SSB_Bumper = 0x25,
    SSB_GOAL = 0x27,
    SSB_1Up = 0x28,       // hardcoded non-solid
    SSB_UP = 0x29,
    SSB_DOWN = 0x2A,
    SSB_R = 0x2B,
    SSB_RedWhite = 0x2C,
    SSB_Glass1_Blue = 0x2D,
    SSB_Glass2_Green = 0x2E,
    SSB_Glass3_Yellow = 0x2F,
    SSB_Glass4_Pink = 0x30,
    SSB_R_Ani = 0x31,
    SSB_Bumper_Ani1 = 0x32,
    SSB_Bumper_Ani2 = 0x33,
    SSB_Ring = 0x3A,      // first non-solid block
    SSB_Emerald1_Blue = 0x3B,
    SSB_Emerald6_Grey = 0x40,
    SSB_Ghost = 0x41,
    SSB_Ring_Ani1 = 0x42,
    SSB_Ring_Ani2 = 0x43,
    SSB_Ring_Ani3 = 0x44,
    SSB_Ring_Ani4 = 0x45,
    SSB_Emerald_Ani1 = 0x46,
    SSB_Emerald_Ani2 = 0x47,
    SSB_Emerald_Ani3 = 0x48,
    SSB_Emerald_Ani4 = 0x49,
    SSB_InvGhostTrigger = 0x4A,
    SSB_Glass_Ani1 = 0x4B,
    SSB_Glass_Ani2 = 0x4C,
    SSB_Glass_Ani3 = 0x4D,
    SSB_Glass_Ani4 = 0x4E,
};

#define SS_BLOCKSIZE 24    // logical size of a block (ss_blocksize)
#define SS_ROTATESPEED 0x40 // base rotation speed (ss_rotatespeed)
#define SS_TIMEOUT 30       // delay after touching an UP/DOWN or R block (ss_timeout)

// Touched-block animations (v_ss_animations): rings sparkle, bumpers bounce, glass blinks and weakens, ...
// Each slot animates one block of the layout by rewriting its ID every few frames.
typedef enum {
    SSAni_None = 0,
    SSAni_RingSparks,
    SSAni_Bumper,
    SSAni_1Up,
    SSAni_Reverse,
    SSAni_EmeraldSparks,
    SSAni_GlassBlock,
} SSAnimId;

typedef struct {
    uint8_t id;        // SSAnimId; 0 = free slot
    int8_t delay;      // frames until the next step
    uint8_t frame;     // step in the animation script
    uint8_t next_id;   // glass only: block ID it becomes when the animation ends
    uint32_t block;    // index of the block in ss_layout
} SS_Animation;

SS_Animation *SS_FindFreeAnimationSlot(void); // NULL if every slot is busy
void SS_ClearAnimations(void);

// Special stage background (SpecialStageBG.c): the grid/fish/bird canvases on plane A and the clouds and bubbles on
// plane B, switched by moving the planes around in VRAM. See "Special Stage Background & Palette Cycle.asm".
extern uint16_t ss_bg_anim; // v_ssbganim: which background mode is showing (0 grid, 2-6 fish, 8-$C birds and clouds)
void SS_BGLoad(void);       // writes the canvases and the cloud/bubble tilemaps into VRAM
void SS_BGSetMode(uint16_t anim, uint16_t bg_plane_tile); // PalCycle_SS's register writes: plane A/B locations and scroll
void SS_BGAnimate(void);    // per frame: bubbles wobble, clouds drift, per-line scroll
