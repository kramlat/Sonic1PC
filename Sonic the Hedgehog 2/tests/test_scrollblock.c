#include "test.h"

#include "Camera.h"
#include "LevelScroll.h"
#include "Video.h"
#include "Level.h"
#include "HTZQuake.h"
#include "ScrollEngine.h"
#include "LevelDraw.h"
#include "Backend/VDP.h"
#include <stdlib.h>
#include <string.h>

// The zones' backgrounds through the scrolling engine: Emerald Hill's lines, and every zone's plane over a run of the camera.

extern void Deform_EHZ(void);
extern void Deform_TitleScreen(void);

// Emerald Hill's scroll lines (sky, clouds, the wavy line, hills, the water's gradient in steps of one, two and three lines) over a sweep of camera positions, the title screen's too; the hash is
// of the lines the original's loops wrote, recorded before the zone was written as band tables.
static void ScrollBlock_EmeraldHillsLinesAreTheOriginals(void) {
    uint64_t hash = 0xcbf29ce484222325ull;
    for (int f = 0; f < 500; f++) {
        scrpos_x.f.u = (int16_t)(f * 13 - 100);
        Deform_EHZ();
        for (int l = 0; l < SCREEN_HEIGHT; l++)
            for (int k = 0; k < 2; k++) {
                hash = (hash ^ (uint8_t)hscroll_buffer[l][k]) * 0x100000001b3ull;
                hash = (hash ^ (uint8_t)((uint16_t)hscroll_buffer[l][k] >> 8)) * 0x100000001b3ull;
            }
    }
    scrpos_x.f.u = 0x1BC0;
    for (int f = 0; f < 40; f++) { // (the title's camera runs on by 8 up to $1C00)
        Deform_TitleScreen();
        for (int l = 0; l < SCREEN_HEIGHT; l++)
            for (int k = 0; k < 2; k++) {
                hash = (hash ^ (uint8_t)hscroll_buffer[l][k]) * 0x100000001b3ull;
                hash = (hash ^ (uint8_t)((uint16_t)hscroll_buffer[l][k] >> 8)) * 0x100000001b3ull;
            }
    }
    CHECK(hash == 0x356e9d00d3e1008bull);
}

// ---- the zones' backgrounds through the scrolling engine ----
// What a zone's background plane shows (the plane's tile under every 8th pixel of every line, through the scroll lines) over a run of 1200 frames of the camera going right and down, stopping, going
// left and up, then small moves both ways; the hash is of what the flag-driven drawing showed, recorded before the zones moved to the engine's band layers.

void LevelScroll_RegisterZones(void);
void LoadTilesAsYouMove(void);
extern uint8_t bg1_xblock, bg2_xblock, bg3_xblock, bg1_yblock, bg2_yblock, bg3_yblock;

static uint64_t ZoneBackgroundHash(int zone, int act, int start_x) {
    uint64_t hash = 0xcbf29ce484222325ull;
    level_id = LEVEL_ID(zone, act);
    last_lamp = 0;
    camera_split = false;
    LevelScroll_RegisterZones();
    LevelDataLoad();
    Plane_Clear(&screen1p.plane_a);
    Plane_Clear(&screen1p.plane_b);
    int16_t x = StartLocArray[zone][act][0], y = StartLocArray[zone][act][1];
    x = (int16_t)(x < 160 ? 0 : x - 160);
    if (start_x >= 0)
        x = (int16_t)start_x;
    y = (int16_t)(y < 96 ? 0 : y - 96);
    scrpos_x.v = (int32_t)x << 16;
    scrpos_y.v = (int32_t)y << 16;
    bg_scrpos_x.v = bg_scrpos_y.v = bg2_scrpos_x.v = bg2_scrpos_y.v = bg3_scrpos_x.v = bg3_scrpos_y.v = 0;
    BgScrollSpeed(x, y);
    bg1_xblock = bg2_xblock = bg3_xblock = 0;
    bg1_yblock = bg2_yblock = bg3_yblock = 0;
    bg1_scroll_flags = bg2_scroll_flags = bg3_scroll_flags = 0;
    scrshift_x = scrshift_y = 0;
    vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;
    LoadTilesFromStart();
    for (int frame = 0; frame < 1200; frame++) {
        int sx, sy;
        if (frame < 400) { sx = (frame / 25) % 9; sy = (frame / 20) % 7; }
        else if (frame < 450) { sx = 0; sy = 0; }
        else if (frame < 900) { sx = -((frame / 25) % 9); sy = -((frame / 20) % 7); }
        else { sx = (frame % 7) - 3; sy = (frame % 11) - 5; }
        if (scrpos_x.f.u + sx < 0) sx = 0;
        if (scrpos_y.f.u + sy < 0) sy = 0;
        scrshift_x = (int16_t)(sx << 8);
        scrshift_y = (int16_t)(sy << 8);
        scrpos_x.v += (int32_t)sx << 16;
        scrpos_y.v += (int32_t)sy << 16;
        bg1_scroll_flags = bg2_scroll_flags = bg3_scroll_flags = 0;
        if (zone == ZoneId_HTZ)
            HTZQuake_Events(); // (Hill Top's quake starts and stops by where the camera is)
        ScrollEngine_Events(zone, act);
        bg_scrpos_x_dup.v = bg_scrpos_x.v; bg_scrpos_y_dup.v = bg_scrpos_y.v;
        bg2_scrpos_x_dup.v = bg2_scrpos_x.v; bg2_scrpos_y_dup.v = bg2_scrpos_y.v;
        bg3_scrpos_x_dup.v = bg3_scrpos_x.v; bg3_scrpos_y_dup.v = bg3_scrpos_y.v;
        bg1_scroll_flags_dup = bg1_scroll_flags; bg2_scroll_flags_dup = bg2_scroll_flags; bg3_scroll_flags_dup = bg3_scroll_flags;
        LoadTilesAsYouMove();
        int vsy = vid_bg_scrpos_y_dup;
        hash = (hash ^ (uint16_t)vsy) * 0x100000001b3ull;
        for (int l = 0; l < SCREEN_HEIGHT; l++) {
            hash = (hash ^ (uint16_t)hscroll_buffer[l][0]) * 0x100000001b3ull; // (the foreground's lines too)
            hash = (hash ^ (uint16_t)hscroll_buffer[l][1]) * 0x100000001b3ull;
            int row = ((l + vsy) >> 3) & (PLANE_HEIGHT - 1);
            int x0 = -hscroll_buffer[l][1];
            for (int px = 0; px < SCREEN_WIDTH; px += 8) {
                int col = ((x0 + px) >> 3) & (PLANE_WIDTH - 1);
                uint16_t w = Plane_Word(&screen1p.plane_b, (size_t)(row * PLANE_WIDTH + col) * 2);
                hash = (hash ^ (w & 0xFF)) * 0x100000001b3ull;
                hash = (hash ^ (w >> 8)) * 0x100000001b3ull;
            }
        }
    }
    return hash;
}

typedef struct { int zone, act, start_x; uint64_t hash; } ZoneGolden;
static const ZoneGolden zone_goldens[] = {
    { ZoneId_HTZ, 0, 0x1900, 0xa34ab936b29da417ull }, // (the camera starts before the first quake's stretch and runs through it)
    { ZoneId_HTZ, 1, 0x1500, 0x28a964487a2578baull },
    { ZoneId_EHZ, 0, -1, 0x9537d67c422b1650ull },
    { ZoneId_CPZ, 0, -1, 0x196c4cf1138ff992ull },
    { ZoneId_HPZ, 0, -1, 0x83bc803ef861421cull },
    { ZoneId_HTZ, 0, -1, 0xba0eebd73ad15cf6ull },
    { ZoneId_WZ, 0, -1, 0x7ac5405b04af7249ull },
    { ZoneId_MTZ, 0, -1, 0x4cb5b9b96bfc1efeull },
    { ZoneId_OOZ, 0, -1, 0xd5dd899da2d6ba05ull },
    { ZoneId_MCZ, 0, -1, 0x73d0d845ffd7d591ull },
    { ZoneId_MCZ, 1, -1, 0x6822c9a4af424564ull },
    { ZoneId_CNZ, 0, -1, 0xe1c598a3e37241a7ull },
    { ZoneId_SSZ, 0, -1, 0x1ade4a656e40c98aull },
    { ZoneId_ARZ, 0, -1, 0xc27c2630b75c5e4aull },
    { ZoneId_ARZ, 1, -1, 0x9de67dc826666277ull },
};

static void ScrollZones_BackgroundsShowWhatTheOriginalsDid(void) {
    for (size_t i = 0; i < sizeof(zone_goldens) / sizeof(zone_goldens[0]); i++) {
        uint64_t h = ZoneBackgroundHash(zone_goldens[i].zone, zone_goldens[i].act, zone_goldens[i].start_x);
        CHECK(h == zone_goldens[i].hash);
    }
}

void RegisterScrollBlockTests(void) {
    RUN_TEST(ScrollZones_BackgroundsShowWhatTheOriginalsDid);
    RUN_TEST(ScrollBlock_EmeraldHillsLinesAreTheOriginals);
}
