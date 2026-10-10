#include "SpecialStage.h"
#include "Constants.h"

#include "Backend/VDP.h"
#include "Enigma.h"
#include "Game.h"
#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Video.h"

#include <string.h>

#include "Resource/Tilemap/SSBackground1.h"
#include "Resource/Tilemap/SSBackground2.h"

// The special stage's background, ported from "_inc/Special Stage Background & Palette Cycle.asm". It uses the
// VDP's own tricks: plane A and B are 64x64 nametables whose VRAM address is switched every few frames, so a
// handful of pre-drawn "canvases" (checker grid, 7 steps of fish turning into birds) cost no per-frame drawing; only
// the per-line horizontal scroll (the wobbling bubbles, the drifting cloud bands) changes each frame.

#define SSBG_ANIMALSIZE 8 // each bird/fish is an 8x8-cell tilemap

uint16_t ss_bg_anim;

// ---------------------------------------------------------------------------
// Loading
// ---------------------------------------------------------------------------

// TilemapToVRAM: a width x height block of tilemap words (big-endian bytes) into a nametable at `vram`.
static void SS_WriteTilemap(const uint8_t *tilemap, size_t vram, size_t width, size_t height) {
    plane_t canvas = { (uint16_t *)(VDP_TileSpace() + vram), vram < VRAM_SIZE ? VRAM_SIZE - vram : 0 }; // (a canvas is a nametable kept among the tiles)
    CopyTilemap(tilemap, &canvas, 0, width, height);
}

void SS_BGLoad(void) {
    static uint8_t buffer[0x2000];

    // The birds and fish: tilemap 0 is the checker pattern, 1-7 the seven bird/fish frames.
    memset(buffer, 0, sizeof(buffer));
    EniDec(Tilemap_SSBackground1, buffer, TILE_MAP(0, 2, 0, 0, ArtTile_SS_Background_Fish));

    size_t canvas = ArtTile_SS_Plane_1 * TILE_SIZE + 0x1000; // first of 7 canvases, 4KB (64x32 cells) each
    const uint8_t *frame = buffer + SSBG_ANIMALSIZE * SSBG_ANIMALSIZE * 2;
    for (int d7 = 7 - 1; d7 >= 0; d7--, canvas += 0x1000, frame += SSBG_ANIMALSIZE * SSBG_ANIMALSIZE * 2) {
        size_t row_base = canvas;
        int d4 = (d7 >= 4 - 1) ? 0 : 1; // birds start on a blank square, fish on an animal
        for (int row = 0; row < 4; row++) {
            size_t x = row_base;
            for (int square = 0; square < 8; square++, x += SSBG_ANIMALSIZE * 2) {
                d4 ^= 1;
                if (d4 != 0) { // an animal
                    SS_WriteTilemap(frame, x, SSBG_ANIMALSIZE, SSBG_ANIMALSIZE);
                } else if (d7 == 7 - 1) { // blank square: the first canvas uses the checker pattern instead
                    SS_WriteTilemap(buffer, x, SSBG_ANIMALSIZE, SSBG_ANIMALSIZE);
                }
            }
            row_base += (size_t)SSBG_ANIMALSIZE * PLANE_ROW_BYTES; // 8 rows down
            d4 ^= 1;                                               // stagger the pattern
        }
    }

    // The bubbles and clouds: a 64x64 tilemap. Its top half goes to plane 5; all of it again just after, so plane 6
    // (4KB further on) shows the bottom half.
    memset(buffer, 0, sizeof(buffer));
    EniDec(Tilemap_SSBackground2, buffer, TILE_MAP(0, 2, 0, 0, ArtTile_SS_Background_Clouds));
    SS_WriteTilemap(buffer, ArtTile_SS_Plane_5 * TILE_SIZE, 64, 32);
    SS_WriteTilemap(buffer, ArtTile_SS_Plane_5 * TILE_SIZE + 0x1000, 64, 64);

}

// ---------------------------------------------------------------------------
// Background modes (SS_BG_Modes): which nametable plane A shows and which half of it
// ---------------------------------------------------------------------------

static const struct { uint16_t plane_tile; uint8_t yscroll; } ss_bg_modes[7] = {
    { ArtTile_SS_Plane_1, 1 }, // 0  - grid
    { ArtTile_SS_Plane_2, 0 }, // 2  - fish morph 1
    { ArtTile_SS_Plane_2, 1 }, // 4  - fish morph 2
    { ArtTile_SS_Plane_3, 0 }, // 6  - fish
    { ArtTile_SS_Plane_3, 1 }, // 8  - bird morph 1
    { ArtTile_SS_Plane_4, 0 }, // $A - bird morph 2
    { ArtTile_SS_Plane_4, 1 }, // $C - bird
};

// PalCycle_SS's VDP writes: plane A shows the canvas for this mode (its 64x64 nametable is 0x2000 bytes, so the
// "Y scroll direction" of 1 shows the second 4KB), plane B the clouds/bubbles at the given plane.
void SS_BGSetMode(uint16_t anim, uint16_t bg_plane_tile) {
    ss_bg_anim = anim;
    uint16_t mode = anim >> 1;
    if (mode >= 7)
        mode = 0;
    Plane_UseTiles(&screen1p.plane_a, ss_bg_modes[mode].plane_tile);
    vid_scrpos_y_dup = (int16_t)(ss_bg_modes[mode].yscroll << 8);
    Plane_UseTiles(&screen1p.plane_b, bg_plane_tile);
}

// ---------------------------------------------------------------------------
// Animation
// ---------------------------------------------------------------------------

static const uint8_t ss_bubble_scroll_blocks[] = { 10 - 1, 0x28, 0x18, 0x10, 0x28, 0x18, 0x10, 0x30, 0x18, 8, 0x10 };
static const uint8_t ss_cloud_scroll_blocks[] = { 7 - 1, 0x30, 0x30, 0x30, 0x28, 0x18, 0x18, 0x18 };
static const int8_t ss_bubble_wobble[10][2] = {
    { 8, 2 }, { 4, -1 }, { 2, 3 }, { 8, -1 }, { 4, 2 }, { 2, 3 }, { 8, -3 }, { 4, 2 }, { 2, 3 }, { 2, -1 },
};

static struct { int16_t value, phase; } ss_scroll_bubbles[10];
static int32_t ss_scroll_clouds[7];

// SS_Scroll_CloudsBubbles: fills the per-line horizontal scroll table. Plane A scrolls with the bg3 position, plane B
// with each band's own value; the bands start at the line given by the background's vertical position.
static void SS_ScrollBands(const uint8_t *blocks, int16_t (*value)(int)) {
    int16_t plane_a = (int16_t)-bg3_scrpos_x.f.u;
    int count = blocks[0] + 1;
    uint16_t line = (uint16_t)(((uint16_t)-bg_scrpos_y.f.u) & 0xFF);
    for (int block = 0; block < count; block++) {
        int16_t plane_b = value(block);
        int lines = blocks[1 + block];
        for (int i = 0; i < lines; i++) {
            if (line < SCREEN_HEIGHT) {
                hscroll_buffer[line][0] = plane_a;
                hscroll_buffer[line][1] = plane_b;
            }
            line = (uint16_t)((line + 1) & 0xFF);
        }
    }
}

static int16_t SS_BubbleValue(int block) { return ss_scroll_bubbles[block].value; }
static int16_t SS_CloudValue(int block) { return (int16_t)(ss_scroll_clouds[block] >> 16); }

void SS_BGAnimate(void) {
    uint16_t anim = ss_bg_anim;
    if (anim == 0) {
        bg_scrpos_y.f.u = 0;
        vid_bg_scrpos_y_dup = bg_scrpos_y.f.u; // reset the vertical scroll of the bubble/cloud layer
    }

    if (anim >= 8) { // birds and clouds
        if (anim == 0xC) {
            bg3_scrpos_x.f.u--;
            int32_t step = 0x18000;
            for (int i = 0; i < 7; i++) {
                ss_scroll_clouds[i] -= step;
                step -= 0x2000;
            }
        }
        SS_ScrollBands(ss_cloud_scroll_blocks, SS_CloudValue);
        return;
    }

    if (anim == 6) { // fish: the bubbles drift
        bg3_scrpos_x.f.u++;
        bg_scrpos_y.f.u++;
        vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;
    }

    // The bubbles wobble sideways.
    for (int i = 0; i < 10; i++) {
        int16_t sin, cos;
        CalcSine((uint8_t)ss_scroll_bubbles[i].phase, &sin, &cos);
        ss_scroll_bubbles[i].value = (int16_t)((sin * ss_bubble_wobble[i][0]) >> 8);
        ss_scroll_bubbles[i].phase = (int16_t)(ss_scroll_bubbles[i].phase + ss_bubble_wobble[i][1]);
    }
    SS_ScrollBands(ss_bubble_scroll_blocks, SS_BubbleValue);
}
