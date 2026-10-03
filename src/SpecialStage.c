#include "SpecialStage.h"

#include "Game.h"
#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Video.h"
#include "Object/Sonic.h"
#include "Sound.h"

#include "Macros.h"

#include <string.h>

// Special stage layouts
#include "Resource/SSLayout/1.h"
#include "Resource/SSLayout/2.h"
#include "Resource/SSLayout/3.h"
#include "Resource/SSLayout/4.h"
#ifdef SCP_REV00
#include "Resource/SSLayout/5REV00.h"
#include "Resource/SSLayout/6REV00.h"
#else
#include "Resource/SSLayout/5REV01.h"
#include "Resource/SSLayout/6REV01.h"
#endif

static const uint8_t* ss_layouts[] = {
    SSLayout_1, SSLayout_2, SSLayout_3,
#ifdef SCP_REV00
    SSLayout_4, SSLayout_5REV00, SSLayout_6REV00
#else
    SSLayout_4, SSLayout_5REV01, SSLayout_6REV01
#endif
};

static const int16_t ss_startpos[6][2] = {
    { 0x03D0, 0x02E0 },
    { 0x0328, 0x0574 },
    { 0x04E4, 0x02E0 },
    { 0x03AD, 0x02E0 },
    { 0x0340, 0x06B8 },
    { 0x049B, 0x0358 },
};

// Special Stage mappings
#ifdef SCP_REV00
extern const uint8_t Mappings_RingREV00[]; // From Object/Ring.c
#else
extern const uint8_t Mappings_RingREV01[]; // From Object/Ring.c
#endif
extern const uint8_t Mappings_Bumper[]; // From Object/Bumper.c

#include "Resource/Mappings/SSDown.h"
#include "Resource/Mappings/SSEmerald.h"
#include "Resource/Mappings/SSGlass.h"
#include "Resource/Mappings/SSResultEmerald.h"
#include "Resource/Mappings/SSRotate.h"
#include "Resource/Mappings/SSUp.h"
#include "Resource/Mappings/SSWall.h"

#define SS_MAPPINGS 78

static const struct SS_SrcMapping {
    uint8_t frame; // The original put this in the most significant byte of the 24-bit mapping pointer
    const uint8_t* mapping;
    uint16_t tile;
} ss_src_mappings[SS_MAPPINGS] = {
    // Palette 0 wall
    { 0, Mappings_SSWall, TILE_MAP(0, 0, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 0, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 0, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 0, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 0, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 0, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 0, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 0, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 0, 0, 0, 0x142) },
    // Palette 1 wall
    { 0, Mappings_SSWall, TILE_MAP(0, 1, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 1, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 1, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 1, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 1, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 1, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 1, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 1, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 1, 0, 0, 0x142) },
    // Palette 2 wall
    { 0, Mappings_SSWall, TILE_MAP(0, 2, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 2, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 2, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 2, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 2, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 2, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 2, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 2, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 2, 0, 0, 0x142) },
    // Palette 3 wall
    { 0, Mappings_SSWall, TILE_MAP(0, 3, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 3, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 3, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 3, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 3, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 3, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 3, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 3, 0, 0, 0x142) },
    { 0, Mappings_SSWall, TILE_MAP(0, 3, 0, 0, 0x142) },
    // Stationary bumper
    { 0, Mappings_Bumper, TILE_MAP(0, 0, 0, 0, 0x23B) },
    //?
    { 0, Mappings_SSRotate, TILE_MAP(0, 0, 0, 0, 0x570) },
    { 0, Mappings_SSRotate, TILE_MAP(0, 0, 0, 0, 0x251) },
    { 0, Mappings_SSRotate, TILE_MAP(0, 0, 0, 0, 0x370) },
    // Up/down block
    { 0, Mappings_SSUp, TILE_MAP(0, 0, 0, 0, 0x263) },
    { 0, Mappings_SSDown, TILE_MAP(0, 0, 0, 0, 0x263) },
    //?
    { 0, Mappings_SSRotate, TILE_MAP(0, 1, 0, 0, 0x2F0) },
    // Glass blocks
    { 0, Mappings_SSGlass, TILE_MAP(0, 0, 0, 0, 0x470) },
    { 0, Mappings_SSGlass, TILE_MAP(0, 0, 0, 0, 0x5F0) },
    { 0, Mappings_SSGlass, TILE_MAP(0, 3, 0, 0, 0x5F0) },
    { 0, Mappings_SSGlass, TILE_MAP(0, 1, 0, 0, 0x5F0) },
    { 0, Mappings_SSGlass, TILE_MAP(0, 2, 0, 0, 0x5F0) },
    //?
    { 0, Mappings_SSRotate, TILE_MAP(0, 0, 0, 0, 0x2F0) }, // R block (touched): same art as the idle one, just the other palette
    // Hit bumper
    { 1, Mappings_Bumper, TILE_MAP(0, 0, 0, 0, 0x23B) },
    { 2, Mappings_Bumper, TILE_MAP(0, 0, 0, 0, 0x23B) },
    //?
    { 0, Mappings_SSRotate, TILE_MAP(0, 0, 0, 0, 0x797) },
    { 0, Mappings_SSRotate, TILE_MAP(0, 0, 0, 0, 0x7A0) },
    { 0, Mappings_SSRotate, TILE_MAP(0, 0, 0, 0, 0x7A9) },
    { 0, Mappings_SSRotate, TILE_MAP(0, 0, 0, 0, 0x797) },
    { 0, Mappings_SSRotate, TILE_MAP(0, 0, 0, 0, 0x7A0) },
    { 0, Mappings_SSRotate, TILE_MAP(0, 0, 0, 0, 0x7A9) },
// Stationary ring
#ifdef SCP_REV00
    { 0, Mappings_RingREV00, TILE_MAP(0, 1, 0, 0, 0x7B2) },
#else
    { 0, Mappings_RingREV01, TILE_MAP(0, 1, 0, 0, 0x7B2) },
#endif
    // Chaos Emeralds
    { 0, Mappings_SSEmerald + 8, TILE_MAP(0, 0, 0, 0, 0x770) },
    { 0, Mappings_SSEmerald + 8, TILE_MAP(0, 1, 0, 0, 0x770) },
    { 0, Mappings_SSEmerald + 8, TILE_MAP(0, 2, 0, 0, 0x770) },
    { 0, Mappings_SSEmerald + 8, TILE_MAP(0, 3, 0, 0, 0x770) },
    { 0, Mappings_SSEmerald + 0, TILE_MAP(0, 0, 0, 0, 0x770) },
    { 0, Mappings_SSEmerald + 4, TILE_MAP(0, 0, 0, 0, 0x770) },
    //?
    { 0, Mappings_SSRotate, TILE_MAP(0, 0, 0, 0, 0x4F0) },
// Ring sparkles
#ifdef SCP_REV00
    { 4, Mappings_RingREV00, TILE_MAP(0, 1, 0, 0, 0x7B2) },
    { 5, Mappings_RingREV00, TILE_MAP(0, 1, 0, 0, 0x7B2) },
    { 6, Mappings_RingREV00, TILE_MAP(0, 1, 0, 0, 0x7B2) },
    { 7, Mappings_RingREV00, TILE_MAP(0, 1, 0, 0, 0x7B2) },
#else
    { 4, Mappings_RingREV01, TILE_MAP(0, 1, 0, 0, 0x7B2) },
    { 5, Mappings_RingREV01, TILE_MAP(0, 1, 0, 0, 0x7B2) },
    { 6, Mappings_RingREV01, TILE_MAP(0, 1, 0, 0, 0x7B2) },
    { 7, Mappings_RingREV01, TILE_MAP(0, 1, 0, 0, 0x7B2) },
#endif
    // Emerald sparkles (when an emerald is collected; ArtTile_SS_Emerald_Sparkle)
    { 0, Mappings_SSGlass, TILE_MAP(0, 1, 0, 0, 0x3F0) },
    { 1, Mappings_SSGlass, TILE_MAP(0, 1, 0, 0, 0x3F0) },
    { 2, Mappings_SSGlass, TILE_MAP(0, 1, 0, 0, 0x3F0) },
    { 3, Mappings_SSGlass, TILE_MAP(0, 1, 0, 0, 0x3F0) },
    //?
    { 2, Mappings_SSRotate, TILE_MAP(0, 0, 0, 0, 0x4F0) }, // invisible ghost-block trigger
    //?
    { 0, Mappings_SSGlass, TILE_MAP(0, 0, 0, 0, 0x5F0) },
    { 0, Mappings_SSGlass, TILE_MAP(0, 3, 0, 0, 0x5F0) },
    { 0, Mappings_SSGlass, TILE_MAP(0, 1, 0, 0, 0x5F0) },
    { 0, Mappings_SSGlass, TILE_MAP(0, 2, 0, 0, 0x5F0) },
};

// Special Stage state
word_u ss_angle;
uint16_t ss_rotate;
uint16_t palss_num;
int16_t palss_time;

// uint8_t last_special;

uint8_t emeralds;
uint8_t emerald_list[8];

#define SS_GRID_MAX 20 // blocks across the rotating window of the biggest picture
int16_t ss_drawtable[SS_GRID_MAX * SS_GRID_MAX * 2];


uint8_t ss_layout[SS_DIM * SS_DIM]; // SS_DIM x SS_DIM (128x128)
uint8_t ss_layout_tmp[SS_SRCDIM * SS_SRCDIM]; // SS_SRCDIM x SS_SRCDIM (64x64)

// Special Stage mappings
struct SS_Mapping {
    const uint8_t* mapping;
    uint8_t pad, frame;
    uint16_t tile;
} ss_mappings[1 + SS_MAPPINGS];

// Special Stage functions
void SS_AniWallsRings(void) {
    // Update wall angle
    uint8_t angle = (ss_angle.f.u >> 2) & 0xF;
    for (int i = 0; i < 36; i++)
        ss_mappings[1 + i].frame = angle;

    // Animate rings
    if (--sprite_anim[1].time < 0) {
        sprite_anim[1].time = 7;
        sprite_anim[1].frame = (sprite_anim[1].frame + 1) & 0x3;
    }
    ss_mappings[58].frame = sprite_anim[1].frame;

    // Animate various blocks
    if (--sprite_anim[2].time < 0) {
        sprite_anim[2].time = 7;
        sprite_anim[2].frame = (sprite_anim[2].frame + 1) & 0x1;
    }
    ss_mappings[39].frame = sprite_anim[2].frame;
    ss_mappings[44].frame = sprite_anim[2].frame;
    ss_mappings[41].frame = sprite_anim[2].frame;
    ss_mappings[42].frame = sprite_anim[2].frame;
    ss_mappings[59].frame = sprite_anim[2].frame;
    ss_mappings[39].frame = sprite_anim[2].frame;
    ss_mappings[60].frame = sprite_anim[2].frame;
    ss_mappings[61].frame = sprite_anim[2].frame;
    ss_mappings[62].frame = sprite_anim[2].frame;
    ss_mappings[63].frame = sprite_anim[2].frame;

    // Animate more blocks
    if (--sprite_anim[3].time < 0) {
        sprite_anim[3].time = 4;
        sprite_anim[3].frame = (sprite_anim[3].frame + 1) & 0x3;
    }
    ss_mappings[45].frame = sprite_anim[3].frame;
    ss_mappings[46].frame = sprite_anim[3].frame;
    ss_mappings[47].frame = sprite_anim[3].frame;
    ss_mappings[48].frame = sprite_anim[3].frame;

    // Animate wall blocks
    if (--sprite_anim[0].time < 0) {
        sprite_anim[0].time = 7;
        sprite_anim[0].frame = (sprite_anim[0].frame + 1) & 0x7;
    }

#define BTILE0 TILE_MAP(0, 0, 0, 0, 0x142)
#define BTILE1 TILE_MAP(0, 1, 0, 0, 0x142)
#define BTILE2 TILE_MAP(0, 2, 0, 0, 0x142)
#define BTILE3 TILE_MAP(0, 3, 0, 0, 0x142)
    static const uint16_t block_tile[4][16] = {
        { BTILE0, BTILE3, BTILE0, BTILE0, BTILE0, BTILE0, BTILE0, BTILE3, BTILE0, BTILE3, BTILE0, BTILE0, BTILE0, BTILE0, BTILE0, BTILE3 },
        { BTILE1, BTILE0, BTILE1, BTILE1, BTILE1, BTILE1, BTILE1, BTILE0, BTILE1, BTILE0, BTILE1, BTILE1, BTILE1, BTILE1, BTILE1, BTILE0 },
        { BTILE2, BTILE1, BTILE2, BTILE2, BTILE2, BTILE2, BTILE2, BTILE1, BTILE2, BTILE1, BTILE2, BTILE2, BTILE2, BTILE2, BTILE2, BTILE1 },
        { BTILE3, BTILE2, BTILE3, BTILE3, BTILE3, BTILE3, BTILE3, BTILE2, BTILE3, BTILE2, BTILE3, BTILE3, BTILE3, BTILE3, BTILE3, BTILE2 }
    };
#undef BTILE0
#undef BTILE1
#undef BTILE2
#undef BTILE3

    struct SS_Mapping* mapping = &ss_mappings[2];
    for (int i = 0; i < 4; i++, mapping += 9) {
        for (int j = 0; j < 8; j++)
            mapping[j].tile = block_tile[i][sprite_anim[0].frame + j];
    }
}

// ---------------------------------------------------------------------------
// Touched-block animations (SS_FindFreeAnimationSlot / SS_ExecuteAnimationQueue)
// ---------------------------------------------------------------------------

#define SS_ANIMATIONS 0x20

static SS_Animation ss_animations[SS_ANIMATIONS];

void SS_ClearAnimations(void) {
    memset(ss_animations, 0, sizeof(ss_animations));
}

SS_Animation *SS_FindFreeAnimationSlot(void) {
    for (int i = 0; i < SS_ANIMATIONS; i++)
        if (ss_animations[i].id == SSAni_None)
            return &ss_animations[i];
    return NULL;
}

static void SS_AnimDone(SS_Animation *a) {
    memset(a, 0, sizeof(*a));
}

// Steps a script of block IDs ending in 0. Returns the new block ID (0 = the script has ended).
static uint8_t SS_AnimNext(SS_Animation *a, int8_t delay, const uint8_t *script) {
    if (--a->delay >= 0)
        return 0xFF; // not time yet
    a->delay = delay;
    return script[a->frame++];
}

static void SS_AniRingSparks(SS_Animation *a) {
    static const uint8_t script[] = { SSB_Ring_Ani1, SSB_Ring_Ani2, SSB_Ring_Ani3, SSB_Ring_Ani4, 0 };
    uint8_t id = SS_AnimNext(a, 5, script);
    if (id == 0xFF)
        return;
    ss_layout[a->block] = id;
    if (id == 0)
        SS_AnimDone(a); // the ring is gone
}

static void SS_AniBumper(SS_Animation *a) {
    static const uint8_t script[] = { SSB_Bumper_Ani1, SSB_Bumper_Ani2, SSB_Bumper_Ani1, SSB_Bumper_Ani2, 0 };
    uint8_t id = SS_AnimNext(a, 7, script);
    if (id == 0xFF)
        return;
    if (id == 0) {
        ss_layout[a->block] = SSB_Bumper; // back to the idle bumper
        SS_AnimDone(a);
    } else {
        ss_layout[a->block] = id;
    }
}

static void SS_Ani1Up(SS_Animation *a) {
    static const uint8_t script[] = { SSB_Emerald_Ani1, SSB_Emerald_Ani2, SSB_Emerald_Ani3, SSB_Emerald_Ani4, 0 };
    uint8_t id = SS_AnimNext(a, 5, script);
    if (id == 0xFF)
        return;
    ss_layout[a->block] = id;
    if (id == 0)
        SS_AnimDone(a);
}

static void SS_AniReverse(SS_Animation *a) {
    static const uint8_t script[] = { SSB_R, SSB_R_Ani, SSB_R, SSB_R_Ani, 0 };
    uint8_t id = SS_AnimNext(a, 7, script);
    if (id == 0xFF)
        return;
    if (id == 0) {
        ss_layout[a->block] = SSB_R; // back to the idle R block
        SS_AnimDone(a);
    } else {
        ss_layout[a->block] = id;
    }
}

static void SS_AniEmeraldSparks(SS_Animation *a) {
    static const uint8_t script[] = { SSB_Emerald_Ani1, SSB_Emerald_Ani2, SSB_Emerald_Ani3, SSB_Emerald_Ani4, 0 };
    uint8_t id = SS_AnimNext(a, 5, script);
    if (id == 0xFF)
        return;
    ss_layout[a->block] = id;
    if (id == 0) {
        SS_AnimDone(a);
        player->routine = 4; // Sonic's ExitStage routine: this starts the actual exit
        QueueSound2(sfx_SSGoal);
    }
}

static void SS_AniGlassBlock(SS_Animation *a) {
    static const uint8_t script[] = {
        SSB_Glass_Ani1, SSB_Glass_Ani2, SSB_Glass_Ani3, SSB_Glass_Ani4,
        SSB_Glass_Ani1, SSB_Glass_Ani2, SSB_Glass_Ani3, SSB_Glass_Ani4, 0,
    };
    uint8_t id = SS_AnimNext(a, 1, script);
    if (id == 0xFF)
        return;
    ss_layout[a->block] = id;
    if (id == 0) {
        ss_layout[a->block] = a->next_id; // the glass block becomes its weaker version (or is gone)
        SS_AnimDone(a);
    }
}

void SS_AniItems(void) {
    for (int i = 0; i < SS_ANIMATIONS; i++) {
        SS_Animation *a = &ss_animations[i];
        switch (a->id) {
        case SSAni_RingSparks:    SS_AniRingSparks(a); break;
        case SSAni_Bumper:        SS_AniBumper(a); break;
        case SSAni_1Up:           SS_Ani1Up(a); break;
        case SSAni_Reverse:       SS_AniReverse(a); break;
        case SSAni_EmeraldSparks: SS_AniEmeraldSparks(a); break;
        case SSAni_GlassBlock:    SS_AniGlassBlock(a); break;
        }
    }
}

// Division rounding down, for the cells before the stage's top and left edges (negative positions).
static int FloorDiv(int a, int b) {
    int q = a / b;
    return (a % b != 0 && (a < 0) != (b < 0)) ? q - 1 : q;
}

void SS_ShowLayout(uint8_t sprite_i) {
    int sonic_screen_x, sonic_screen_y; // where Sonic is on the screen
    // Animate stage
    SS_AniWallsRings();
    SS_AniItems();

    // Get rotation
    int16_t sin, cos;
    CalcSine(ss_angle.f.u & 0xFC, &sin, &cos); // Remove this AND for smooth rotation

    // The rotating window of blocks: 16 x 16 for the original picture (it covers it however it is turned); a bigger picture needs a
    // bigger window to reach its corners.
    const int grid = (SCREEN_WIDTH > 320 || SCREEN_HEIGHT > 224) ? SS_GRID_MAX : 16;
    const int margin = (grid - 16) / 2; // extra cells before Sonic's

    // The window of blocks is worked out from where Sonic really is: his place in the stage and his place on the screen (the centre of
    // the picture, unless the camera is held back at an edge of the stage). The first cell is the one a centred camera would start at,
    // less the extra cells; its offset from Sonic, plus the original's own offsets (20 across, 68 down), is where the window starts
    // relative to him. With the original 320x224 picture and its 16 cells that comes to the original's 180 either way.
    const int sonic_x = (uint16_t)player->pos.l.x.f.u, sonic_y = (uint16_t)player->pos.l.y.f.u;
    sonic_screen_x = sonic_x - (uint16_t)scrpos_x.f.u;
    sonic_screen_y = sonic_y - (uint16_t)scrpos_y.f.u;
    const int first_x = FloorDiv(sonic_x - SCREEN_WIDTH / 2, 24) - margin;
    const int first_y = FloorDiv(sonic_y - SCREEN_HEIGHT / 2, 24) - margin;
    int16_t d2 = (int16_t)(first_x * 24 - sonic_x - 20);
    int16_t d3 = (int16_t)(first_y * 24 - sonic_y - 68);
    int16_t d4 = sin * 24;
    int16_t d5 = cos * 24;

    int16_t* to = ss_drawtable;
    for (int i = 0; i < grid; i++) {
        int32_t d2b = (d2 * cos) + (d3 * -sin);
        int32_t d1b = (d2 * sin) + (d3 * cos);
        for (int j = 0; j < grid; j++) {
            *to++ = d2b >> 8;
            *to++ = d1b >> 8;
            d2b += d5;
            d1b += d4;
        }
        d3 += 24;
    }

    // Get layout offset
    int ly = first_y;
    int lx = first_x;

    // Draw sprites
    const int16_t* pos = ss_drawtable;
    uint16_t* sprite = &sprite_buffer[sprite_i][0];

    for (int i = 0; i < grid; i++) {
        for (int j = 0; j < grid; j++, pos += 2) {
            // Draw block
            int cx = lx + j, cy = ly + i;
            uint8_t block = (cx >= 0 && cx < SS_DIM && cy >= 0 && cy < SS_DIM) ? ss_layout[cx + cy * SS_DIM] : 0;
            if (block != 0 && block <= SS_MAPPINGS) {
                // Get block position
                uint16_t x = pos[0] + (0x80 + sonic_screen_x);
                uint16_t y = pos[1] + (0x80 + sonic_screen_y);
                if (x >= 0x70 && x < (0x1D0 + SCREEN_WIDEADD) && y >= 0x70 && y < (0x170 + SCREEN_TALLADD)) {
                    // Get block mapping
                    struct SS_Mapping* mapping = &ss_mappings[block];
                    const uint8_t* mapping_ind = mapping->mapping + (mapping->frame << 1);
                    const uint8_t* mapping_data = mapping->mapping + ((mapping_ind[0] << 8) | (mapping_ind[1] << 0));

                    // Draw block mapping
                    uint8_t pieces = *mapping_data++;
                    if (pieces)
                        BuildSpr_Normal(&sprite, &sprite_i, x, y, mapping->tile, mapping_data, pieces - 1);
                }
            }
        }
    }

    // Terminate end of sprite list
    sprite_count = sprite_i;
    if (sprite_i >= BUFFER_SPRITES) {
        sprite[-3] &= 0xFF00; // Clear link byte
    } else {
        *sprite++ = 0;
        *sprite++ = 0;
    }
}

void SS_Load(void) {
SS_Load_Branch:;
    // Get special stage to load
    uint8_t stage = last_special;
    if (++last_special >= 6)
        last_special = 0;

    // Check if stage is available
    if (emeralds != 6) {
        int i;
        if ((i = emeralds - 1) >= 0) {
            do {
                if (emerald_list[i] == stage)
                    goto SS_Load_Branch;
            } while (i-- > 0);
        }
    }

    // Set player start position
    player->pos.l.x.f.u = ss_startpos[stage][0];
    player->pos.l.y.f.u = ss_startpos[stage][1];

    // Read layout
    memcpy(ss_layout_tmp, ss_layouts[stage], SS_SRCDIM * SS_SRCDIM);

    // Copy layout from 64x64 temp buffer to 128x128 buffer
    uint8_t* tol = &ss_layout[(SS_PAD2 * SS_DIM) + SS_PAD2];
    const uint8_t* froml = ss_layout_tmp;

    for (int i = 0; i < SS_SRCDIM; i++) {
        for (int j = 0; j < SS_SRCDIM; j++)
            *tol++ = *froml++;
        tol += SS_PAD;
    }

    // Load mappings
    struct SS_Mapping* tom = &ss_mappings[1];
    const struct SS_SrcMapping* fromm = ss_src_mappings;

    for (int i = 0; i < SS_MAPPINGS; i++, tom++, fromm++) {
        tom->mapping = fromm->mapping;
        tom->pad = 0;
        tom->frame = fromm->frame;
        tom->tile = fromm->tile;
    }

    SS_ClearAnimations();
}
