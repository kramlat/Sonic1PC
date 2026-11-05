// Ported from MarkeyJester's 'SSRG Splash Screen'

#include "GM_SSRG.h"

#include "Game.h"
#include "Kosinski.h"
#include "Level.h"
#include "Nemesis.h"
#include "PLC.h"
#include "Palette.h"
#include "Sound.h"
#include "Sst.h"
#include "Video.h"

#include <string.h>

// SSRG memory
#define ssrg_scroll_fg (*(int16_t*)(buffer0000 + 0x7800))
#define ssrg_scroll_bg (*(int16_t*)(buffer0000 + 0x7808))

// SSRG assets
#include "Resource/SSRG/ArtLink.h"
#include "Resource/SSRG/ArtMain.h"
#include "Resource/SSRG/ArtSonic.h"
#include "Resource/SSRG/ArtSquare.h"
#include "Resource/SSRG/MapLink.h"
#include "Resource/SSRG/MapMain.h"
#include "Resource/SSRG/MapSquare.h"

static const uint16_t pal_ssrg[] = {
    0x0000, 0x0400, 0x0800, 0x0C00, 0x0E00, 0x0EEE, 0x0AAA, 0x0888, 0x0666, 0x0444, 0x0222, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0444, 0x0888, 0x0CCC, 0x0EEE, 0x0888, 0x0666, 0x0444, 0x0222, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0422, 0x0844, 0x0C66, 0x0E88, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0404, 0x0808, 0x0E0E, 0x00E0, 0x00C0, 0x00A0, 0x0080, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

// SSRG planes
static void CopyTilemap_Single(uint16_t v, plane_t *plane, size_t offset, size_t width, size_t height) {
    while (height-- > 0) {
        Plane_Seek(plane, offset);
        for (size_t x = 0; x < width; x++)
            Plane_Write(v);
        offset += PLANE_WIDTH * 2;
    }
}

static void CopyTilemap_Add(const uint8_t* tilemap, plane_t *plane, size_t offset, size_t width, size_t height, uint16_t add) {
    while (height-- > 0) {
        Plane_Seek(plane, offset);
        for (size_t x = 0; x < width; x++) {
            uint16_t v = ((tilemap[0] << 8) | (tilemap[1] << 0)) + add;
            tilemap += 2;
            Plane_Write(v);
        }
        offset += PLANE_WIDTH * 2;
    }
}

static void SRG_ScrollFG(void) {
    int16_t* bufp;

    // Scroll FG
    bufp = &hscroll_buffer[115][0];

    int16_t scroll_fg = ssrg_scroll_fg + 0x30;
    if (scroll_fg < 0xF7) {
        scroll_fg *= -2;
        scroll_fg += (PLANE_WIDTH - 64) * 8;
        for (int i = 0; i < 0x20; i++) {
            *bufp++ = scroll_fg;
            *bufp++ = 0;
        }
    }

    // Scroll BG
    bufp = &hscroll_buffer[0][0];

    int16_t scroll_bg = ssrg_scroll_bg;
    for (int i = 0; i < SCREEN_HEIGHT; i++) {
        bufp++;
        *bufp++ = scroll_bg;
    }
}

static void SRG_DrawFG(void) {
    // Get scroll value
    uint16_t scroll_fg = (ssrg_scroll_fg * 2) - 0x40;

    if (scroll_fg <= 0x110) {
        // Get scroll offset
        uint16_t scroll_off = ((scroll_fg & 0x01F8) >> 2);

        // Get scroll offsets
        size_t offset = MAP_PLANE(2, 14) + PLANE_WIDEADD + PLANE_TALLADD + scroll_off;
        const uint8_t* mapp = buffer0000 + scroll_off;

        // Write plane data
        for (int i = 0; i < 3; i++) {
            uint16_t v = ((mapp[0] << 8) | (mapp[1] << 0)) + 0x2000;
            Plane_Put(&screen1p.plane_a, offset, v);
            offset += PLANE_WIDTH << 1;
            mapp += 0x46;
        }
    } else {
        // Make sure we've reached the starting point
        scroll_fg -= 0x170;
        if (scroll_fg & 0x8000)
            return;

        // Have we reached PASSED the ending point?
        if (scroll_fg >= 0x60)
            return;

        // Flashing
        uint16_t add;
        if (scroll_fg >= 0x50 || (scroll_fg & 8) == 0)
            add = 0x0000; // White
        else
            add = 0x2000; // Grey
        // (The original address of plane A's row 14 ($C000 on the Genesis): this port wrote it there, into the tile space, among the art. It goes to the scratch plane memory now, where it does no harm and shows nothing.)
        plane_t at_c000;
        Plane_UseScratchAt(&at_c000, 0xC000);
        CopyTilemap_Add(buffer0000, &at_c000, 0x704 + PLANE_WIDEADD, 35, 3, add);
    }
}

// (the screen positions are signed words)
#define SSRG_X(obj) ((int16_t)(obj)->x_pos.f.u)
#define SSRG_Y(obj) ((int16_t)(obj)->y_pos.f.u)

static void UpdateScrollPositions(Sst *obj) {
    ssrg_scroll_bg = SSRG_X(obj);
    vid_bg_scrpos_y_dup = -SSRG_Y(obj);
}

// SSRG objects: they sit in screen coordinates (no level flag), and run in the order GM_SSRG calls them
static void SpeedToPosHud(Sst *obj) {
    uint32_t xadd = (int32_t)obj->x_vel << 8;
    obj->x_pos.f.u += xadd >> 16;
    obj->y_pos.v = (int32_t)((uint32_t)obj->y_pos.v + (uint32_t)((int32_t)obj->y_vel << 8)); // (the pixel and the sub-pixel word)
}

// The four letters, which the square knocks about
static Sst *ssrg_letters[4];

// Letters object
typedef struct {
    uint16_t pad; // 0x28-0x29
    uint16_t timer; // 0x2A
} Scratch_Letters;

static void Obj_Letters(Sst *obj) {
    Scratch_Letters* scratch = (Scratch_Letters*)obj->own.u8;

    static const uint16_t data[4][8] = {
        // 0xXXXX, 0xYYYY, 0xVRAM, 0xTIME, 0xXSPD, 0xYSPD, 0xMAPI, 0x????
        { 0x0182, 0x0180, 0x0001, 0x0001, 0xFF00, 0xF400, 0x0000, 0x0000 },
        { 0x01A8, 0x0180, 0x0001, 0x0011, 0xFF00, 0xF400, 0x0000, 0x0000 },
        { 0x01D0, 0x0180, 0x0011, 0x0021, 0xFF00, 0xF400, 0x0000, 0x0000 },
        { 0x01F8, 0x0180, 0x0021, 0x0031, 0xFF00, 0xF400, 0x0000, 0x0000 },
    };

    static const uint8_t mappings[] = { // (Sonic 3 mapping format)
        0x00, 0x02, 0x00, 0x01, 0xf0, 0x0f, 0x00, 0x00,
        0xff, 0xf0,
    };

    switch (obj->routine) {
    case 0: {
        // Increment routine counter and read object data
        obj->routine += 2;

        const uint16_t* datap = data[obj->subtype]; // (which letter)
        obj->x_pos.f.u = *datap++ + PLANE_WIDEADD * 4;
        obj->y_pos.f.u = *datap++;

        obj->art_tile = *datap++;

        scratch->timer = *datap++;
        obj->x_vel = *datap++;
        obj->y_vel = *datap++;
        obj->mapping_frame = *datap++;

        // Set object drawing information
        obj->mappings = mappings;
        obj->priority = 0; // (the original's $20 is in the front queue too)
        obj->width_pixels = 32;

        obj->height_pixels = 32;
    }
        // Fallthrough
    case 2: // Wait for timer to expire
        if (--scratch->timer == 0)
            obj->routine += 2;
        Sst_Draw(obj);
        break;
    case 4: // Fly up
        // Fall and draw
        SpeedToPosHud(obj);
        if ((obj->y_vel += 0x40) >= 0 && 0xF0 < SSRG_Y(obj))
            obj->routine += 2;
        Sst_Draw(obj);
        break;
    case 6: // Spring
        // Spring back upwards and draw
        SpeedToPosHud(obj);
        if ((obj->y_vel -= 0x80) < 0 && 0xE8 >= SSRG_Y(obj)) {
            obj->y_vel = 0;
            scratch->timer = 0;
            obj->routine += 2;
        }
        Sst_Draw(obj);
        break;
    case 8: // Slow down
        if (obj->x_vel < 0) {
            // Slow down then draw
            SpeedToPosHud(obj);
            obj->x_vel += 0x20;
            Sst_Draw(obj);
            break;
        } else if ((++scratch->timer) & 1) {
            // Handle flashing
            if ((obj->art_tile >> TILE_PALETTE_SHIFT) >= 2) {
                // Reset palette and state, increment routine
                obj->art_tile &= ~TILE_PALETTE_AND;
                obj->routine += 2;
                obj->x_vel = 0;
                obj->y_vel = 0;
                obj->y_pos.f.u &= 0x1FF; // Immediately overwritten
                obj->y_pos.f.u = 0xE4;
                Sst_Draw(obj);
                break;
            } else {
                // Increment palette
                obj->art_tile += TILE_MAP(0, 1, 0, 0, 0);
            }
        }
        // Fallthrough
    case 10: // Idle
        // Check if we've been knocked by the square
        SpeedToPosHud(obj);

        if (SSRG_Y(obj) != 0xE4) {
            if (SSRG_Y(obj) < 0xE4) {
                // Gravity
                obj->y_vel += 0x80;
            } else {
                // Reset Y speed and position
                obj->y_vel = 0;
                obj->y_pos.f.u = 0xE4;
            }
        }

        // Decrease X speed
        if (obj->x_vel < 0)
            obj->x_vel += 0x40;
        else if (obj->x_vel > 0)
            obj->x_vel -= 0x40;

        Sst_Draw(obj);
        break;
    }
}

// Square object
typedef struct {
    uint16_t pad; // 0x28-0x29
    uint16_t timer; // 0x2A
    int16_t speed; // 0x2C
} Scratch_Square;

static void Obj_Square(Sst* obj) {
    Scratch_Square* scratch = (Scratch_Square*)obj->own.u8;

    static const struct MapRamData {
        uint8_t* map;
        size_t offset;
        uint16_t width, height;
        uint32_t pad;
    } map_ram_data[] = {
        { &buffer0000[0x4000], MAP_PLANE(2, 2), 0x000B, 0x000B, 0 },
        { &buffer0000[0x4120], MAP_PLANE(0, 0), 0x000F, 0x000F, 0 },
        { &buffer0000[0x4320], MAP_PLANE(0, 0), 0x0010, 0x0010, 0 },
        { &buffer0000[0x4562], MAP_PLANE(0, 0), 0x000F, 0x000F, 0 },
    };

    switch (obj->routine) {
    case 0: // Initialization
        // Increment routine and initialize state
        obj->routine += 2;

        obj->x_pos.f.u = -0xC0;
        obj->y_pos.f.u = 0x20;
        scratch->timer = 0xA0 - (PLANE_WIDEADD / 2);
        obj->x_vel = 0x800;
        obj->y_vel = 0;

        UpdateScrollPositions(obj);
        return;
    case 2: // Wait for timer to expire
        if (--scratch->timer == 0) {
            obj->routine += 2;
            scratch->timer = (PLANE_WIDEADD / 2) * -2;
        }
        Sst_Draw(obj); // Null mappings, thank you Markey!
        return;
    case 4: // Play sound
        obj->routine += 2;
        PlaySound(sfx_Teleport);
        // Fallthrough
    case 6: // Spin in
        scratch->timer += 2;
        if (SSRG_X(obj) >= 0x60 + (PLANE_WIDEADD * 4)) {
            // Hit the 'SSRG' text
            obj->routine += 2;
            PlaySound(sfx_ChainStomp);
            obj->x_vel = -0x100;
            obj->y_vel = -0x400;

            // Knock 'SSRG' text
            ssrg_letters[0]->x_vel = 0x300;
            ssrg_letters[0]->y_vel = -0x400;
            ssrg_letters[1]->x_vel = 0x300;
            ssrg_letters[1]->y_vel = -0x300;
            ssrg_letters[2]->x_vel = 0x300;
            ssrg_letters[2]->y_vel = -0x200;
            ssrg_letters[3]->x_vel = 0x300;
            ssrg_letters[3]->y_vel = -0x100;
        }
        break;
    case 8: // Knocked back and landing
        scratch->timer -= 8;

        // Fall
        if ((obj->y_vel += 0x20) >= 0 && SSRG_Y(obj) >= 0x20) {
            // Landed
            obj->y_pos.f.u = 0x20;
            obj->x_vel = 0;
            obj->y_vel = 0;
            obj->routine += 2;
            scratch->speed = 0xA40;
        }
        break;
    case 10: {
        int16_t speed = scratch->speed;
        if (speed < 0) {
            // Invert palette brightness
            if (dry_palette[3][1] != 0xE0E)
                dry_palette[3][1] += 0x202;
            if (dry_palette[3][3] != 0x404)
                dry_palette[3][3] -= 0x202;
        } else {
            // Spin
            speed -= 0x18;
            scratch->timer -= speed >> 8;
            scratch->speed = speed;
        }
        break;
    }
    }

    // Move object and plane
    SpeedToPosHud(obj);
    UpdateScrollPositions(obj);

    // Clear previous plane art
    CopyTilemap_Single(0, &screen1p.plane_b, 0, 0x11, 0x11);

    // Copy new plane art
    const struct MapRamData* data = &map_ram_data[(scratch->timer & 0x18) >> 3];
    CopyTilemap(data->map, &screen1p.plane_b, data->offset, data->width + 1, data->height + 1);

#if (SCREEN_WIDTH > 320)
    // Clear unwanted tiles
    int16_t clip_tiles = -(SSRG_X(obj) >> 3);
    if (clip_tiles > 0)
        CopyTilemap_Single(0, &screen1p.plane_b, 0, clip_tiles, 0x11);
#endif
}

// Sonic neon object
typedef struct {
    uint16_t pad; // 0x28-0x29
    uint16_t timer; // 0x2A
    uint16_t speed; // 0x2C
} Scratch_SonicNeon;

static void Obj_SonicNeon(Sst* obj) {
    // For some reason this object is written completely differently to the others

    Scratch_SonicNeon* scratch = (Scratch_SonicNeon*)obj->own.u8;

    static const uint8_t mappings[] = { // (Sonic 3 mapping format)
        0x00, 0x0c, 0x00, 0x26, 0x00, 0x34, 0x00, 0x42,
        0x00, 0x5c, 0x00, 0x6a, 0x00, 0x04, 0xeb, 0x0d,
        0x00, 0x00, 0xff, 0xec, 0xfb, 0x09, 0x00, 0x08,
        0xff, 0xec, 0xfb, 0x06, 0x00, 0x0e, 0x00, 0x04,
        0x0b, 0x04, 0x00, 0x14, 0xff, 0xec, 0x00, 0x02,
        0xec, 0x0d, 0x00, 0x00, 0xff, 0xed, 0xfc, 0x0e,
        0x00, 0x16, 0xff, 0xf5, 0x00, 0x02, 0xed, 0x09,
        0x00, 0x22, 0xff, 0xf3, 0xfd, 0x0a, 0x00, 0x28,
        0xff, 0xf3, 0x00, 0x04, 0xeb, 0x09, 0x00, 0x22,
        0xff, 0xf4, 0xfb, 0x09, 0x00, 0x31, 0xff, 0xec,
        0xfb, 0x06, 0x00, 0x37, 0x00, 0x04, 0x0b, 0x04,
        0x00, 0x3d, 0xff, 0xec, 0x00, 0x02, 0xec, 0x09,
        0x00, 0x22, 0xff, 0xf3, 0xfc, 0x0e, 0x00, 0x3f,
        0xff, 0xeb, 0x00, 0x03, 0xed, 0x0d, 0x00, 0x00,
        0xff, 0xec, 0xfd, 0x0c, 0x00, 0x4b, 0xff, 0xf4,
        0x05, 0x09, 0x00, 0x4f, 0xff, 0xf4,
    };

    if (obj->routine == 0) {
        // Set object drawing information
        obj->art_tile = TILE_MAP(0, 3, 0, 0, 0x400);
        obj->mappings = mappings;

        // Initialize state
        obj->x_pos.f.u = 0;
        obj->y_pos.f.u = 0xDE;
        obj->priority = 0; // (the original's $20 is in the front queue too)

        obj->width_pixels = 32;
        obj->height_pixels = 32;

        scratch->timer = 192 - PLANE_WIDEADD;
        obj->x_vel = 0x400;

        // Increment routine
        obj->routine += 2;
    }

    // Wait for timer to expire
    if (scratch->timer != 0) {
        scratch->timer--;
        return;
    }

    // Check if we should stop
    if (obj->routine <= 2 && SSRG_X(obj) >= 0xDE + (PLANE_WIDEADD * 4)) {
        // Stop running and increment routine
        obj->x_pos.f.u = 0xDE + (PLANE_WIDEADD * 4);
        obj->x_vel = 0;
        PlaySound(sfx_Roll);
        obj->routine += 2;
    }

    // Update mapping frame
    scratch->speed += 0x40;

    uint8_t frame = scratch->speed >> 8;
    if (frame >= 6) {
        frame = 0;
        scratch->speed = 0;
    }

    obj->mapping_frame = frame;

    // Palette cycle
    if (!(ssrg_scroll_fg & 0x7)) {
        uint16_t* fromp = &dry_palette[3][4];
        uint16_t* top = fromp;
        uint16_t temp = *fromp++;
        *top++ = *fromp++;
        *top++ = *fromp++;
        *top++ = *fromp++;
        *top = temp;
    }

    // Move and draw
    SpeedToPosHud(obj);
    Sst_Draw(obj);
}

// SSRG splash game mode
void GM_SSRG() {
    StopAllSound();

    // Clear the pattern load queue and fade out
    ClearPLC();
    PaletteFadeOut();

    // Clear screen
    ClearScreen();

    // Clear object memory
    Sst_Reset();

    // Initialize VDP and video state
    VDP_SetBackgroundColour(0);
    vid_scrpos_y_dup = -8;
    vid_bg_scrpos_y_dup = -44;

    // Decompress art into VRAM
    VDP_SeekVRAM(0x0020);
    NemDec(SSRG_ArtMain);
    VDP_SeekVRAM(0x4000);
    NemDec(SSRG_ArtSquare);
    VDP_SeekVRAM(0x8000);
    NemDec(SSRG_ArtSonic);
    VDP_SeekVRAM(0x9000);
    NemDec(SSRG_ArtLink);

    // Decompress mappings
    KosDec(SSRG_MapLink, buffer0000);
    CopyTilemap(buffer0000, &screen1p.plane_a, MAP_PLANE(4, 24) + PLANE_WIDEADD + (PLANE_TALLADD * 2), 32, 1);

    KosDec(SSRG_MapMain, &buffer0000[0x0000]);
    KosDec(SSRG_MapSquare, &buffer0000[0x4000]);

    // Copy palette
    memcpy(&dry_palette_dup[0][0], pal_ssrg, sizeof(pal_ssrg));

    // Load objects and fade in
    for (int i = 0; i < 4; i++) { // S, S, R, G
        ssrg_letters[i] = Sst_Allocate();
        ssrg_letters[i]->code = Obj_Letters;
        ssrg_letters[i]->subtype = (uint8_t)i;
    }
    Sst *square = Sst_Allocate();
    square->code = Obj_Square;
    Sst *neon = Sst_Allocate();
    neon->code = Obj_SonicNeon;
    PaletteFadeIn();
    memset(&buffer0000[0x7800], 0, 4 * 3);

    // Run loop
    do {
        // Render frame
        vbla_routine = 0x04;
        WaitForVBla();
        ssrg_scroll_fg++;

        // Run objects
        for (int i = 0; i < 4; i++)
            Obj_Letters(ssrg_letters[i]); // S, S, R, G
        Obj_Square(square);
        Obj_SonicNeon(neon);

        // Draw screen
        SRG_ScrollFG();
        SRG_DrawFG();
        BuildSprites(NULL);
    } while (!(jpad1_press1 & JPAD_START) && ssrg_scroll_fg < 0x200);

    // Go to title gamemode
    gamemode = GameMode_Title;
}
