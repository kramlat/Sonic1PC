#include "Object.h"
#include "Level.h"
#include "Constants.h"
#include "Video.h"

#include <string.h>

// Sonic 2 (Nick Arcade): the animated level art and the animated blocks (_inc/Animated Stage Tiles.asm).
//
// Animated art: each zone has a list of scripts. A script copies the next frame of an animation's art over the same tiles of the level art whenever its timer runs out
// (AniArt_Load's Dynamic_Normal). Animated blocks: a few blocks of the level's block table are overwritten when the level loads (LoadAnimatedBlocks), so that the chunks
// can show the animated art's tiles.

#include "Resource/S2Art/EHZFlower1.h"
#include "Resource/S2Art/EHZFlower2.h"
#include "Resource/S2Art/EHZFlower3.h"
#include "Resource/S2Art/EHZFlower4.h"
#include "Resource/S2Art/EHZFlower5.h"
#include "Resource/S2Art/HPZGlowingBall.h"

typedef struct {
    const uint8_t *art;       // the animation's frames
    uint16_t vram;            // where its tiles are in VRAM (a byte address)
    uint8_t frames;           // how many frames the script runs through
    uint8_t tiles;            // how many tiles a frame is
    int8_t duration;          // how long each frame stays (ticks - 1), or negative if every frame has its own: then the frame data is (tile, duration) pairs
    const uint8_t *frame_data; // the first tile of each frame in the art
} AnimScript;

static const uint8_t EHZ1_Frames[] = { 0, 0x7F, 2, 0x13, 0, 7, 2, 7, 0, 7, 2, 7 };
static const uint8_t EHZ2_Frames[] = { 2, 0x7F, 0, 0x0B, 2, 0x0B, 0, 0x0B, 2, 5, 0, 5, 2, 5, 0, 5 };
static const uint8_t EHZ3_Frames[] = { 0, 2 };
static const uint8_t EHZ4_Frames[] = { 0, 0x7F, 2, 7, 0, 7, 2, 7, 0, 7, 2, 0x0B, 0, 0x0B, 2, 0x0B };
static const uint8_t EHZ5_Frames[] = { 0, 2, 4, 6, 4, 2 };
static const AnimScript AnimCue_EHZ[] = {
    { S2Art_EHZFlower1, 0x7280, 6, 2, -1, EHZ1_Frames },
    { S2Art_EHZFlower2, 0x72C0, 8, 2, -1, EHZ2_Frames },
    { S2Art_EHZFlower3, 0x7300, 2, 2, 7, EHZ3_Frames },
    { S2Art_EHZFlower4, 0x7340, 8, 2, -1, EHZ4_Frames },
    { S2Art_EHZFlower5, 0x7380, 6, 2, 1, EHZ5_Frames },
};

static const uint8_t HPZ1_Frames[] = { 0x00, 0x00, 0x08, 0x10, 0x10, 0x08 };
static const uint8_t HPZ2_Frames[] = { 0x08, 0x10, 0x10, 0x08, 0x00, 0x00 };
static const uint8_t HPZ3_Frames[] = { 0x10, 0x08, 0x00, 0x00, 0x08, 0x10 };
static const AnimScript AnimCue_HPZ[] = {
    { S2Art_HPZGlowingBall, 0x5D00, 6, 8, 8, HPZ1_Frames },
    { S2Art_HPZGlowingBall, 0x5E00, 6, 8, 8, HPZ2_Frames },
    { S2Art_HPZGlowingBall, 0x5F00, 6, 8, 8, HPZ3_Frames },
};

static struct {
    int8_t time;
    uint8_t frame;
} anim_counters[8];

static void Dynamic_Normal(const AnimScript *scripts, int count) {
    for (int i = 0; i < count; i++) {
        const AnimScript *s = &scripts[i];
        if (--anim_counters[i].time >= 0)
            continue;
        uint8_t frame = anim_counters[i].frame;
        if (frame >= s->frames)
            frame = 0;
        anim_counters[i].frame = (uint8_t)(frame + 1);
        uint8_t tile;
        if (s->duration < 0) {
            tile = s->frame_data[frame * 2];
            anim_counters[i].time = (int8_t)s->frame_data[frame * 2 + 1];
        } else {
            tile = s->frame_data[frame];
            anim_counters[i].time = s->duration;
        }
        VDP_SeekVRAM(s->vram);
        VDP_WriteVRAM(s->art + tile * 0x20, s->tiles * 0x20);
    }
}

// AniArt_Load: Green Hill, the empty second slot, Chemical Plant and the ending have none; Emerald Hill and Hill Top animate their flowers, Hidden Palace its glowing ball
void S2_AnimateLevelArt(void) {
    switch (LEVEL_ZONE(level_id)) {
    case ZoneId_SLZ: // Emerald Hill
    case ZoneId_SBZ: // Hill Top
        Dynamic_Normal(AnimCue_EHZ, (int)(sizeof(AnimCue_EHZ) / sizeof(AnimCue_EHZ[0])));
        break;
    case ZoneId_SYZ: // Hidden Palace
        Dynamic_Normal(AnimCue_HPZ, (int)(sizeof(AnimCue_HPZ) / sizeof(AnimCue_HPZ[0])));
        break;
    default:
        break;
    }
}

static const uint16_t APM_GHZ[] = {
    0x4502, 0x4504, 0x4503, 0x4505, 0x4506, 0x4508, 0x4507, 0x4509,
    0x450A, 0x450C, 0x450B, 0x450D, 0x450E, 0x4510, 0x450F, 0x4511,
    0x4512, 0x4514, 0x4513, 0x4515, 0x4516, 0x4518, 0x4517, 0x4519,
    0x651A, 0x651C, 0x651B, 0x651D, 0x651E, 0x6520, 0x651F, 0x6521,
    0x439C, 0x4B9C, 0x439D, 0x4B9D, 0x4158, 0x439C, 0x4159, 0x439D,
    0x4B9C, 0x4958, 0x4B9D, 0x4959, 0x6394, 0x6B94, 0x6395, 0x6B95,
    0xE396, 0xEB96, 0xE397, 0xEB97, 0x6398, 0x6B98, 0x6399, 0x6B99,
    0xE39A, 0xEB9A, 0xE39B, 0xEB9B,
};
static const uint16_t APM_CPZ[] = {
    0x43D1, 0x43D1, 0x43D1, 0x43D1, 0x43D2, 0x43D2, 0x43D3, 0x43D3,
    0x43D4, 0x43D4, 0x43D5, 0x43D5, 0x43D6, 0x43D6, 0x43D7, 0x43D7,
};
static const uint16_t APM_HPZ[] = {
    0x62E8, 0x62E9, 0x62EA, 0x62EB, 0x62EC, 0x62ED, 0x62EE, 0x62EF,
    0x62F0, 0x62F1, 0x62F2, 0x62F3, 0x62F4, 0x62F5, 0x62F6, 0x62F7,
    0x62F8, 0x62F9, 0x62FA, 0x62FB, 0x62FC, 0x62FD, 0x62FE, 0x62FF,
    0x42E8, 0x42E9, 0x42EA, 0x42EB, 0x42EC, 0x42ED, 0x42EE, 0x42EF,
    0x42F0, 0x42F1, 0x42F2, 0x42F3, 0x42F4, 0x42F5, 0x42F6, 0x42F7,
    0x42F8, 0x42F9, 0x42FA, 0x42FB, 0x42FC, 0x42FD, 0x42FE, 0x42FF,
    0x0000, 0x62E8, 0x0000, 0x62EA, 0x62E9, 0x62EC, 0x62EB, 0x62EE,
    0x62ED, 0x0000, 0x62EF, 0x0000, 0x0000, 0x62F0, 0x0000, 0x62F2,
    0x62F1, 0x62F4, 0x62F3, 0x62F6, 0x62F5, 0x0000, 0x62F7, 0x0000,
    0x0000, 0x62F8, 0x0000, 0x62FA, 0x62F9, 0x62FC, 0x62FB, 0x62FE,
    0x62FD, 0x0000, 0x62FF, 0x0000, 0x0000, 0x42E8, 0x0000, 0x42EA,
    0x42E9, 0x42EC, 0x42EB, 0x42EE, 0x42ED, 0x0000, 0x42EF, 0x0000,
    0x0000, 0x42F0, 0x0000, 0x42F2, 0x42F1, 0x42F4, 0x42F3, 0x42F6,
    0x42F5, 0x0000, 0x42F7, 0x0000, 0x0000, 0x42F8, 0x0000, 0x42FA,
    0x42F9, 0x42FC, 0x42FB, 0x42FE, 0x42FD, 0x0000, 0x42FF, 0x0000,
};

typedef struct {
    uint16_t offset; // in bytes, into the block table (level_map16)
    const uint16_t *words;
    int count;
} AnimBlocks;

static const AnimBlocks APM_Green = { 0x1788, APM_GHZ, sizeof(APM_GHZ) / 2 };
static const AnimBlocks APM_Chemical = { 0x17E0, APM_CPZ, sizeof(APM_CPZ) / 2 };
static const AnimBlocks APM_Hidden = { 0x1710, APM_HPZ, sizeof(APM_HPZ) / 2 };

// LoadAnimatedBlocks: at level start, once the level's blocks are loaded (and the art's counters start again). Green Hill, Emerald Hill and Hill Top get the same ones.
void S2_LoadAnimatedBlocks(void) {
    memset(anim_counters, 0, sizeof(anim_counters));
    const AnimBlocks *b = NULL;
    switch (LEVEL_ZONE(level_id)) {
    case ZoneId_GHZ:
    case ZoneId_SLZ:
    case ZoneId_SBZ:
        b = &APM_Green;
        break;
    case ZoneId_MZ:
        b = &APM_Chemical;
        break;
    case ZoneId_SYZ:
        b = &APM_Hidden;
        break;
    default:
        break;
    }
    if (!b)
        return;
    for (int i = 0; i < b->count; i++) { // (the block table holds big-endian words, as the files do)
        level_map16[b->offset + i * 2] = (uint8_t)(b->words[i] >> 8);
        level_map16[b->offset + i * 2 + 1] = (uint8_t)(b->words[i] & 0xFF);
    }
}
