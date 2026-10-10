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
#include "Resource/S2Art/MTZCylinder.h"
#include "Resource/S2Art/MTZLava.h"
#include "Resource/S2Art/MTZAnimBack.h"
#include "Resource/S2Art/MTZDrills.h"
#include "Resource/S2Art/OOZPulseBall.h"
#include "Resource/S2Art/OOZSquareBall1.h"
#include "Resource/S2Art/OOZSquareBall2.h"
#include "Resource/S2Art/OOZOil1.h"
#include "Resource/S2Art/OOZOil2.h"
#include "Resource/S2Art/NGHZWaterfall1.h"
#include "Resource/S2Art/NGHZWaterfall2.h"
#include "Resource/S2Art/NGHZWaterfall3.h"
#include "Resource/S2Art/CPZAnimBack.h"

#include "HTZBackground.h"
#include "SplitScreen.h"

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
static const uint8_t EHZ5_Frames[] = { 0, 0x17, 2, 9, 4, 0x0B, 6, 0x17, 4, 0x0B, 2, 9 }; // (the pulsing ball: each frame has its own time in the prototype)
static const AnimScript AnimCue_EHZ[] = {
    { S2Art_EHZFlower1, 0x7280, 6, 2, -1, EHZ1_Frames },
    { S2Art_EHZFlower2, 0x72C0, 8, 2, -1, EHZ2_Frames },
    { S2Art_EHZFlower3, 0x7300, 2, 2, 7, EHZ3_Frames },
    { S2Art_EHZFlower4, 0x7340, 8, 2, -1, EHZ4_Frames },
    { S2Art_EHZFlower5, 0x7380, 6, 2, -1, EHZ5_Frames },
};

static const uint8_t HPZ1_Frames[] = { 0x00, 0x00, 0x08, 0x10, 0x10, 0x08 };
static const uint8_t HPZ2_Frames[] = { 0x08, 0x10, 0x10, 0x08, 0x00, 0x00 };
static const uint8_t HPZ3_Frames[] = { 0x10, 0x08, 0x00, 0x00, 0x08, 0x10 };
static const AnimScript AnimCue_HPZ[] = {
    { S2Art_HPZGlowingBall, 0x5D00, 6, 8, 8, HPZ1_Frames },
    { S2Art_HPZGlowingBall, 0x5E00, 6, 8, 8, HPZ2_Frames },
    { S2Art_HPZGlowingBall, 0x5F00, 6, 8, 8, HPZ3_Frames },
};

// Metropolis (loc_226FC): the cylinder, the lava, two sections of the background and two drills
static const uint8_t MTZ1_Frames[] = { 0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70 };
static const uint8_t MTZ2_Frames[] = { 0x00, 0x0C, 0x18, 0x24, 0x18, 0x0C };
static const uint8_t MTZ3_Frames[] = { 0x00, 0x13, 0x06, 0x07, 0x0C, 0x13, 0x06, 0x07 };
static const uint8_t MTZ4_Frames[] = { 0x0C, 0x13, 0x06, 0x07, 0x00, 0x13, 0x06, 0x07 };
static const uint8_t MTZ5_Frames[] = { 0x00, 0x08, 0x10, 0x18 };
static const AnimScript AnimCue_MTZ[] = {
    { S2Art_MTZCylinder, 0x6980, 8, 0x10, 0, MTZ1_Frames },
    { S2Art_MTZLava, 0x6800, 6, 0x0C, 0x0D, MTZ2_Frames },
    { S2Art_MTZAnimBack, 0x6B80, 4, 6, -1, MTZ3_Frames },
    { S2Art_MTZAnimBack, 0x6C40, 4, 6, -1, MTZ4_Frames },
    { S2Art_MTZDrills, 0x6D00, 4, 8, 5, MTZ5_Frames },
    { S2Art_MTZDrills, 0x6E00, 4, 8, 5, MTZ5_Frames },
};

// Oil Ocean (loc_227E4): the pulsing ball, two squares turning round a ball, and two layers of oil
static const uint8_t OOZ1_Frames[] = { 0x00, 0x0B, 0x04, 0x05, 0x08, 0x09, 0x04, 0x03 };
static const uint8_t OOZ2_Frames[] = { 0x00, 0x04, 0x08, 0x0C };
static const uint8_t OOZ3_Frames[] = { 0x00, 0x10, 0x20, 0x30, 0x20, 0x10 };
static const AnimScript AnimCue_OOZ[] = {
    { S2Art_OOZPulseBall, 0x5A00, 4, 4, -1, OOZ1_Frames },
    { S2Art_OOZSquareBall1, 0x5A80, 4, 4, 6, OOZ2_Frames },
    { S2Art_OOZSquareBall2, 0x5B00, 4, 4, 6, OOZ2_Frames },
    { S2Art_OOZOil1, 0x5B80, 6, 0x10, 0x11, OOZ3_Frames },
    { S2Art_OOZOil2, 0x5D80, 6, 0x10, 0x11, OOZ3_Frames },
};

// Chemical Plant (CPz_Animate): one section of the background, 8 frames of 2 tiles, every 5 ticks
static const uint8_t CPZ1_Frames[] = { 0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E };
static const AnimScript AnimCue_CPZ[] = {
    { S2Art_CPZAnimBack, 0x6E00, 8, 2, 4, CPZ1_Frames },
};

// Neo Green Hill (loc_2283C): the water falls, four pairs of tiles each swapping between two frames
static const uint8_t ARZ1_Frames[] = { 0x00, 0x04 };
static const uint8_t ARZ2_Frames[] = { 0x04, 0x00 };
static const AnimScript AnimCue_ARZ[] = {
    { S2Art_NGHZWaterfall1, 0x7F80, 2, 4, 5, ARZ1_Frames },
    { S2Art_NGHZWaterfall1, 0x7F00, 2, 4, 5, ARZ2_Frames },
    { S2Art_NGHZWaterfall2, 0x7E80, 2, 4, 5, ARZ1_Frames },
    { S2Art_NGHZWaterfall3, 0x7E00, 2, 4, 5, ARZ1_Frames },
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

// The first frame of every script, put in VRAM without starting its counters (so the animation's own phase is the prototype's): at the level's load, before the title card, so that the art is there from
// the first picture and the animated tiles do not show what an earlier screen (the title's TM) left there until the level loop's first frame
static void Prime(const AnimScript *scripts, int count) {
    for (int i = 0; i < count; i++) {
        const AnimScript *s = &scripts[i];
        VDP_SeekVRAM(s->vram);
        VDP_WriteVRAM(s->art + s->frame_data[0] * 0x20, s->tiles * 0x20);
    }
}

void S2_PrimeLevelArt(void) {
    switch (LEVEL_ZONE(level_id)) {
    case ZoneId_HTZ:
        HTZBackground_Animate(); // (the mountains' tiles: they are not in the level's art at all)
        Prime(AnimCue_EHZ, (int)(sizeof(AnimCue_EHZ) / sizeof(AnimCue_EHZ[0])));
        break;
    case ZoneId_EHZ:
        Prime(AnimCue_EHZ, (int)(sizeof(AnimCue_EHZ) / sizeof(AnimCue_EHZ[0])));
        break;
    case ZoneId_HPZ:
        Prime(AnimCue_HPZ, (int)(sizeof(AnimCue_HPZ) / sizeof(AnimCue_HPZ[0])));
        break;
    case ZoneId_MTZ:
    case ZoneId_MTZ3:
        Prime(AnimCue_MTZ, (int)(sizeof(AnimCue_MTZ) / sizeof(AnimCue_MTZ[0])));
        break;
    case ZoneId_CPZ:
        Prime(AnimCue_CPZ, (int)(sizeof(AnimCue_CPZ) / sizeof(AnimCue_CPZ[0])));
        break;
    case ZoneId_OOZ:
        Prime(AnimCue_OOZ, (int)(sizeof(AnimCue_OOZ) / sizeof(AnimCue_OOZ[0])));
        break;
    case ZoneId_ARZ:
        Prime(AnimCue_ARZ, (int)(sizeof(AnimCue_ARZ) / sizeof(AnimCue_ARZ[0])));
        break;
    default:
        break;
    }
}

// AniArt_Load: Emerald Hill and Hill Top animate their flowers, Hidden Palace its glowing ball
void S2_AnimateLevelArt(void) {
    switch (LEVEL_ZONE(level_id)) {
    case ZoneId_HTZ: // (the prototype's loc_2244E: the mountains' tiles, then the flowers, whose script is Emerald Hill's, on counters of their own)
        HTZBackground_Animate();
        Dynamic_Normal(AnimCue_EHZ, (int)(sizeof(AnimCue_EHZ) / sizeof(AnimCue_EHZ[0])));
        break;
    case ZoneId_EHZ:
        Dynamic_Normal(AnimCue_EHZ, (int)(sizeof(AnimCue_EHZ) / sizeof(AnimCue_EHZ[0])));
        break;
    case ZoneId_HPZ:
        Dynamic_Normal(AnimCue_HPZ, (int)(sizeof(AnimCue_HPZ) / sizeof(AnimCue_HPZ[0])));
        break;
    case ZoneId_MTZ:
    case ZoneId_MTZ3:
        Dynamic_Normal(AnimCue_MTZ, (int)(sizeof(AnimCue_MTZ) / sizeof(AnimCue_MTZ[0])));
        break;
    case ZoneId_CPZ:
        Dynamic_Normal(AnimCue_CPZ, (int)(sizeof(AnimCue_CPZ) / sizeof(AnimCue_CPZ[0])));
        break;
    case ZoneId_OOZ:
        Dynamic_Normal(AnimCue_OOZ, (int)(sizeof(AnimCue_OOZ) / sizeof(AnimCue_OOZ[0])));
        break;
    case ZoneId_ARZ:
        Dynamic_Normal(AnimCue_ARZ, (int)(sizeof(AnimCue_ARZ) / sizeof(AnimCue_ARZ[0])));
        break;
    default:
        break;
    }
}

// LoadAnimatedBlocks (Map16Delta in the prototype): at level start, once the level's blocks are loaded (and the art's counters start again), a few blocks past the ones in the zone's block file are
// written into the block table, so that the chunks can show the animated art's tiles (the pulsing balls of Emerald Hill's wall, Metropolis's cylinders, ...). Tables as the prototype has them (the
// two-player halving of the tile numbers is not needed: the split screen draws ordinary tiles).
static const uint16_t APM_Green[] = {
    0x4500, 0x4504, 0x4501, 0x4505, 0x4508, 0x450C, 0x4509, 0x450D,
    0x4510, 0x4514, 0x4511, 0x4515, 0x4502, 0x4506, 0x4503, 0x4507,
    0x450A, 0x450E, 0x450B, 0x450F, 0x4512, 0x4516, 0x4513, 0x4517,
    0x6518, 0x651A, 0x6519, 0x651B, 0x651C, 0x651E, 0x651D, 0x651F,
    0x439C, 0x4B9C, 0x439D, 0x4B9D, 0x4158, 0x439C, 0x4159, 0x439D,
    0x4B9C, 0x4958, 0x4B9D, 0x4959, 0x6394, 0x6B94, 0x6395, 0x6B95,
    0xE396, 0xEB96, 0xE397, 0xEB97, 0x6398, 0x6B98, 0x6399, 0x6B99,
    0xE39A, 0xEB9A, 0xE39B, 0xEB9B,
};
static const uint16_t APM_Metropolis[] = {
    0x235C, 0x2B5C, 0x235D, 0x2B5D, 0x235E, 0x2B5E, 0x235F, 0x2B5F,
    0x635A, 0x635A, 0x635B, 0x635B, 0x6358, 0x6358, 0x6359, 0x6359,
    0x6356, 0x6356, 0x6357, 0x6357, 0x6354, 0x6354, 0x6355, 0x6355,
    0x6352, 0x6352, 0x6353, 0x6353, 0x6350, 0x6350, 0x6351, 0x6351,
    0x634E, 0x634E, 0x634F, 0x634F, 0x634C, 0x634C, 0x634D, 0x634D,
    0x2360, 0x2B60, 0x2361, 0x2B61, 0x2362, 0x2B62, 0x2363, 0x2B63,
    0x2364, 0x2B64, 0x2365, 0x2B65, 0x2366, 0x2B66, 0x2367, 0x2B67,
    0x0000, 0x0000, 0x4340, 0x4341, 0x0000, 0x0000, 0x4342, 0x4343,
    0x4344, 0x4345, 0x4348, 0x4349, 0x4346, 0x4347, 0x434A, 0x434B,
    0xE35A, 0xE35A, 0xE35B, 0xE35B, 0xE358, 0xE358, 0xE359, 0xE359,
    0xE356, 0xE356, 0xE357, 0xE357, 0xE354, 0xE354, 0xE355, 0xE355,
    0xE352, 0xE352, 0xE353, 0xE353, 0xE350, 0xE350, 0xE351, 0xE351,
    0xE34E, 0xE34E, 0xE34F, 0xE34F, 0xE34C, 0xE34C, 0xE34D, 0xE34D,
};
static const uint16_t APM_Hidden[] = {
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
static const uint16_t APM_Oil[] = {
    0x82D0, 0x82D2, 0x82D1, 0x82D3, 0xE2D4, 0xE2D5, 0xE2D6, 0xE2D7,
    0x0000, 0x62D8, 0x0000, 0x62DA, 0x62D9, 0x0000, 0x62DB, 0x0000,
    0xC2DC, 0xC2DD, 0xC2E4, 0xC2E5, 0xC2DE, 0xC2DF, 0xC2E6, 0xC2E7,
    0xC2E0, 0xC2E1, 0xC2E8, 0xC2E9, 0xC2E2, 0xC2E3, 0xC2EA, 0xC2EB,
    0xC2EC, 0xC2ED, 0xC2F4, 0xC2F5, 0xC2EE, 0xC2EF, 0xC2F6, 0xC2F7,
    0xC2F0, 0xC2F1, 0xC2F8, 0xC2F9, 0xC2F2, 0xC2F3, 0xC2FA, 0xC2FB,
};
static const uint16_t APM_Casino[] = {
    0x43D2, 0x43D4, 0x43D3, 0x43D5, 0x4BD4, 0x43D6, 0x4BD5, 0x43D7,
    0x53D3, 0x53D5, 0x53D2, 0x53D4, 0x5BD5, 0x5BD3, 0x5BD4, 0x5BD2,
    0x43D8, 0x43DA, 0x43D9, 0x43DB, 0x4BDA, 0x4BD8, 0x4BDB, 0x4BD9,
    0x43DC, 0x43DE, 0x43DD, 0x43DF, 0x4BDE, 0x4BDC, 0x4BDF, 0x4BDD,
    0x43E0, 0x43E2, 0x43E1, 0x43E3, 0x4BE2, 0x4BE0, 0x4BE3, 0x4BE1,
    0x43E4, 0x43E6, 0x43E5, 0x43E7, 0x4BE6, 0x4BE4, 0x4BE7, 0x4BE5,
    0x63E8, 0x63EA, 0x63E9, 0x63EB, 0x63EC, 0x63EE, 0x63ED, 0x63EF,
    0x63F0, 0x63F2, 0x63F1, 0x63F3, 0x63F4, 0x63F6, 0x63F5, 0x63F7,
    0x7BF7, 0x7BF5, 0x7BF6, 0x7BF4, 0x63F8, 0x63FA, 0x63F9, 0x63FB,
    0x6BF6, 0x6BF4, 0x6BF7, 0x6BF5, 0x7BEB, 0x7BE9, 0x7BEA, 0x7BE8,
};
static const uint16_t APM_Chemical[] = {
    0x4370, 0x4371, 0x4370, 0x4371,
};
static const uint16_t APM_Neo[] = {
    0xC3F0, 0xC3F1, 0xC3F2, 0xC3F3, 0xC3F4, 0xC3F5, 0xC3F6, 0xC3F7,
    0xC3F8, 0xC3F9, 0xC3FA, 0xC3FB, 0xC3FC, 0xC3FD, 0xC3FE, 0xC3FF,
    0x43F0, 0x43F1, 0x43F2, 0x43F3, 0x43F4, 0x43F5, 0x43F6, 0x43F7,
    0x43F8, 0x43F9, 0x43FA, 0x43FB, 0x43FC, 0x43FD, 0x43FE, 0x43FF,
};

typedef struct {
    uint16_t offset; // in bytes, into the block table (level_map16)
    const uint16_t *words;
    int count;
} AnimBlocks;

static const AnimBlocks ABK_Green = { 0x1788, APM_Green, sizeof(APM_Green) / 2 };
static const AnimBlocks ABK_Metropolis = { 0x1730, APM_Metropolis, sizeof(APM_Metropolis) / 2 };
static const AnimBlocks ABK_Hidden = { 0x1710, APM_Hidden, sizeof(APM_Hidden) / 2 };
static const AnimBlocks ABK_Oil = { 0x17A0, APM_Oil, sizeof(APM_Oil) / 2 };
static const AnimBlocks ABK_Casino = { 0x1760, APM_Casino, sizeof(APM_Casino) / 2 };
static const AnimBlocks ABK_Chemical = { 0x17F8, APM_Chemical, sizeof(APM_Chemical) / 2 };
static const AnimBlocks ABK_Neo = { 0x17C0, APM_Neo, sizeof(APM_Neo) / 2 };

void S2_LoadAnimatedBlocks(void) {
    memset(anim_counters, 0, sizeof(anim_counters));
    const AnimBlocks *b = NULL;
    switch (LEVEL_ZONE(level_id)) {
    case ZoneId_EHZ:
    case ZoneId_HTZ:
        b = &ABK_Green;
        break;
    case ZoneId_MTZ:
    case ZoneId_MTZ3:
        b = &ABK_Metropolis;
        break;
    case ZoneId_HPZ:
        b = &ABK_Hidden;
        break;
    case ZoneId_OOZ:
        b = &ABK_Oil;
        break;
    case ZoneId_CNZ:
        b = &ABK_Casino;
        break;
    case ZoneId_CPZ:
        b = &ABK_Chemical;
        break;
    case ZoneId_ARZ:
        b = &ABK_Neo;
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
