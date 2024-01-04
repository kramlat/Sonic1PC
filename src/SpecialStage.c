#include "SpecialStage.h"

#include "Game.h"
#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Video.h"

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
// From Object/Bumper.c
#include "Resource/Mappings/Bumper.h"

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
    { 0, Mappings_SSRotate, TILE_MAP(0, 0, 0, 0, 0x2F7) },
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
    // Glass animation
    { 0, Mappings_SSGlass, TILE_MAP(0, 1, 0, 0, 0x5F0) },
    { 1, Mappings_SSGlass, TILE_MAP(0, 1, 0, 0, 0x5F0) },
    { 2, Mappings_SSGlass, TILE_MAP(0, 1, 0, 0, 0x5F0) },
    { 3, Mappings_SSGlass, TILE_MAP(0, 1, 0, 0, 0x5F0) },
    //?
    { 2, Mappings_SSRotate, TILE_MAP(0, 1, 0, 0, 0x4F0) },
    //?
    { 0, Mappings_SSGlass, TILE_MAP(0, 0, 0, 0, 0x5F0) },
    { 0, Mappings_SSGlass, TILE_MAP(0, 3, 0, 0, 0x5F0) },
    { 0, Mappings_SSGlass, TILE_MAP(0, 1, 0, 0, 0x5F0) },
    { 0, Mappings_SSGlass, TILE_MAP(0, 2, 0, 0, 0x5F0) },
};

// Special Stage state
word_u ss_angle;
uint16_t ss_rotate;
uint16_t palss_num, palss_time;

// uint8_t last_special;

uint8_t emeralds;
uint8_t emerald_list[8];

int16_t ss_drawtable[16 * 16 * 2];

uint8_t ss_collected[0x100];

uint8_t ss_layout[SS_DIM * SS_DIM]; // SS_DIM x SS_DIM (128x128)
uint8_t ss_layout_tmp[SS_SRCDIM * SS_SRCDIM]; // SS_SRCDIM x SS_SRCDIM (64x64)

// Special Stage mappings
struct SS_Mapping {
    const uint8_t* mapping;
    uint8_t pad, frame;
    uint16_t tile;
} ss_mappings[1 + SS_MAPPINGS];

// Special Stage functions
void SS_AniWallsRings()
{
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

void SS_AniItems()
{
}

void SS_ShowLayout(uint8_t sprite_i)
{
    // Animate stage
    SS_AniWallsRings();
    SS_AniItems();

    // Get rotation
    int16_t sin, cos;
    CalcSine(ss_angle.f.u & 0xFC, &sin, &cos); // Remove this AND for smooth rotation

    int16_t d2 = -((uint16_t)scrpos_x.f.u % 24) - 180;
    int16_t d3 = -((uint16_t)scrpos_y.f.u % 24) - 180;
    int16_t d4 = sin * 24;
    int16_t d5 = cos * 24;

    int16_t* to = ss_drawtable;
    for (int i = 0; i < 16; i++) {
        int32_t d2b = (d2 * cos) + (d3 * -sin);
        int32_t d1b = (d2 * sin) + (d3 * cos);
        for (int j = 0; j < 16; j++) {
            *to++ = d2b >> 8;
            *to++ = d1b >> 8;
            d2b += d5;
            d1b += d4;
        }
        d3 += 24;
    }

    // Get layout offset
    uint16_t ly = ((uint16_t)scrpos_y.f.u / 24) * SS_DIM;
    uint16_t lx = (uint16_t)scrpos_x.f.u / 24;

    // Draw sprites
    uint8_t* layout = &ss_layout[lx + ly];
    const int16_t* pos = ss_drawtable;
    uint16_t* sprite = &sprite_buffer[sprite_i][0];

    for (int i = 0; i < 16; i++, layout += SS_DIM - 16) {
        for (int j = 0; j < 16; j++, pos += 2) {
            // Draw block
            uint8_t block = *layout++;
            if (block != 0 && block <= SS_MAPPINGS) {
                // Get block position
                uint16_t x = pos[0] + (0x80 + (SCREEN_WIDTH >> 1));
                uint16_t y = pos[1] + (0x80 + (SCREEN_HEIGHT >> 1));
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

void SS_Load()
{
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

    // Clear collected array
    memset(ss_collected, 0, sizeof(ss_collected));
}
