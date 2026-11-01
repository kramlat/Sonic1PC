#include <string.h>

#include "Constants.h"
#include "Game.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"

#include "Video.h"
#include "Viewport.h"
#include "Palette.h"

#include "Backend/VDP.h"
#include "Object/SegaScreen.h"
#include "Resource/Art/SegaSonic.h"
#include "Resource/Mappings/SegaSprites.h"

// The objects of the SEGA screen of the Sonic 2 pre-beta (the final game's), loaded by GM_Sega.c: $B0, Sonic running across the logo at twice his size, and $B1, the two blocks that hide the trademark of
// the logo (the pre-beta makes it for every console but the Japanese one, where the trademark stays). Stage 2 of the screen has Sonic; the letters' lines and the palette sweeps are stage 3.

bool sega_animation_done;

// ---- Sonic's art, doubled (Sub_Sega_Intro of the pre-beta, as it does it) ------------------------------------------------------------------------------------------------------------------------------------
// The tiles of his four running frames (the lists of the pre-beta's $B0: a tile of Art_Sonic and how many, in the word) are put one after the other in a buffer, and each piece of them is made into one twice as
// big, a pixel a 2 x 2 block: a source byte (two pixels) becomes two bytes on this row and on the next, so a tile becomes four, down the left column and then down the right. $0B00 on is the result (the
// pre-beta DMAs it to tile $88)
#define SEGA_ART_FIRST 0x2B6 // the first tile of Art_Sonic that the lists use (the resource holds the tiles from there)
static uint8_t sega_ram[0x4000];

static void SegaDoubleByte(unsigned src, unsigned dst) {
    const uint8_t b = sega_ram[src];
    const uint8_t hi = (uint8_t)(b & 0xF0), lo = (uint8_t)(b & 0x0F);
    sega_ram[dst] = sega_ram[dst + 4] = (uint8_t)(hi | (hi >> 4));
    sega_ram[dst + 1] = sega_ram[dst + 5] = (uint8_t)(lo | (lo << 4));
}

static unsigned SegaDoubleRow(unsigned src, unsigned dst) { // (two source bytes)
    SegaDoubleByte(src, dst);
    SegaDoubleByte(src + 1, dst + 2);
    return src + 2;
}

typedef struct { unsigned a1, a2, a3, a5; } SegaBlit;

// Offset_0x034BA4 / 34BBC: D5 + 1 tiles of the source, a row of 4 pixels to each of the two columns
static void SegaBlitTiles(SegaBlit *b, unsigned *c1, unsigned *c2, int d5) {
    for (;;) {
        for (int row = 0; row < 8; row++) {
            b->a1 = SegaDoubleRow(b->a1, *c1);
            *c1 += 8;
            b->a1 = SegaDoubleRow(b->a1, *c2);
            *c2 += 8;
        }
        if (d5-- == 0)
            break;
    }
}

// Offset_0x034B2A
static void SegaIntroPart(SegaBlit *b, int d0, int d1, unsigned v2, unsigned v3) {
    unsigned a2 = b->a2, a4 = a2, a3 = a2 + v2, a5;
    SegaBlitTiles(b, &a2, &a3, d1 & 1);
    b->a3 = a3;
    if (d1 & 2) {
        a4 += (v2 * 2) << (d0 & 1);
        a5 = a4 + v3;
        SegaBlitTiles(b, &a4, &a5, d1 >> 1);
        b->a5 = a5;
    }
    if (d0 & 3) {
        a2 += v2;
        a3 = a2 + v2;
        SegaBlitTiles(b, &a2, &a3, d1 & 1);
        b->a3 = a3;
        if (d1 & 2) {
            a4 += v3;
            a5 = a4 + v3;
            SegaBlitTiles(b, &a4, &a5, d1 >> 1);
            b->a5 = a5;
        }
    }
    b->a2 = a2;
}

// Sub_Sega_Intro: from a1, to a2, d0 and d1 saying how many tiles and in what shape
static void SegaIntro(unsigned a1, unsigned a2, int d0, int d1) {
    SegaBlit b = { a1, a2, 0, 0 };
    const unsigned v2 = (unsigned)((d1 & 1) + 1) << 6, v3 = (unsigned)((d1 >> 1) + 1) << 6;
    SegaIntroPart(&b, d0, d1, v2, v3);
    if (!(d0 & 2))
        return;
    if (!(d1 & 2))
        b.a5 = b.a3;
    b.a2 = b.a5;
    SegaIntroPart(&b, d0, d1, v2, v3);
}

// Offset_0x0345B4 on: Sonic's frames (four lists of a 6 tile and a 16 tile piece of Art_Sonic), then the eight doublings; the result goes to VRAM at tile $88 (the mappings count from there)
#define SEGA_SONIC_TILE 0x88
static void SegaMakeSonicArt(void) {
    static const uint16_t lists[4][2] = { { 0x52B6, 0xF2BC }, { 0x52CC, 0xF2D2 }, { 0x52B6, 0xF2E2 }, { 0x52CC, 0xF2F2 } };
    static const struct { uint16_t src, dst; uint8_t d0, d1; } blits[8] = {
        { 0x0000, 0x0B00, 2, 1 }, { 0x00C0, 0x0E00, 3, 3 }, { 0x02C0, 0x1600, 2, 1 }, { 0x0380, 0x1900, 3, 3 },
        { 0x0580, 0x2100, 2, 1 }, { 0x0640, 0x2400, 3, 3 }, { 0x0840, 0x2C00, 2, 1 }, { 0x0900, 0x2F00, 3, 3 },
    };
    memset(sega_ram, 0, sizeof(sega_ram));
    unsigned at = 0;
    for (int l = 0; l < 4; l++)
        for (int i = 0; i < 2; i++) {
            const unsigned tile = lists[l][i] & 0xFFF, count = (unsigned)(lists[l][i] >> 12) + 1;
            memcpy(sega_ram + at, Art_SegaSonic + (tile - SEGA_ART_FIRST) * TILE_SIZE, count * TILE_SIZE);
            at += count * TILE_SIZE;
        }
    for (int i = 0; i < 8; i++)
        SegaIntro(blits[i].src, blits[i].dst, blits[i].d0, blits[i].d1);
    VDP_SeekVRAM(ART_VRAM(SEGA_SONIC_TILE));
    VDP_WriteVRAM(sega_ram + 0x0B00, 0x2C00);
}

// ---- The blocks over the letters --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Plane A holds a strip of the block graphic (tiles $80-$87: seven steps of a gradient and a solid block, repeated), in rows 9 to 18 of the picture, and the scroll of every second line moves it across: a
// line is a pixel high, so the letters are covered in blinds. The first sweep is on the even lines, a strip with the gradient in front from the right edge going left; the second on the odd lines, from
// the left (the plane wraps) going right with the gradient at its end. How far each line is from the others (SEGA_LINE_OFFSETS) is what makes the edge ragged.
#define SEGA_STRIP_ROWS 10
#define SEGA_STRIP_FIRST_ROW 9
#define SEGA_LINES 35
#define SEGA_FIRST_LINE 78

static const uint8_t sega_line_offsets[SEGA_LINES] = {
    0x12, 0x04, 0x04, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x06,
    0x0A, 0x08, 0x06, 0x04, 0x04, 0x04, 0x04, 0x06, 0x06, 0x08, 0x08, 0x0A, 0x0A, 0x0C, 0x0E, 0x10, 0x16,
};

// A row's words, written one after another from `col` (past the plane's row it goes on in the next, as the VDP's autoincrement does): a word with $0400 set is the solid block, 42 times
static void SegaWriteStrip(unsigned col, const uint16_t *words, int count) {
    for (int row = 0; row < SEGA_STRIP_ROWS; row++) {
        size_t at = (size_t)(SEGA_STRIP_FIRST_ROW + SegaPlaneRows() + row) * SEGA_PLANE_WIDTH * 2 + col * 2;
        for (int i = 0; i < count; i++) {
            const bool solid = (words[i] & 0x0400) != 0;
            for (int n = 0; n < (solid ? 42 : 1); n++, at += 2)
                Plane_Put(&screen1p.plane_a, at, (uint16_t)(words[i] & ~0x0400));
        }
    }
}

static void SegaStripFirst(void) { // the gradient at the left end, from just past the right edge
    static const uint16_t words[8] = { 0xA080, 0xA081, 0xA082, 0xA083, 0xA084, 0xA085, 0xA086, 0xA487 };
    SegaWriteStrip((unsigned)((SCREEN_WIDTH + 7) / 8), words, 8);
}

static void SegaStripSecond(void) { // the solid block first, the gradient at the right end, ending where the plane's row does
    static const uint16_t words[8] = { 0xA487, 0xA086, 0xA085, 0xA084, 0xA083, 0xA082, 0xA081, 0xA080 };
    SegaWriteStrip(80, words, 8);
}

// Every second line from `odd`'s (0: the even ones the first sweep moves, 1: the odd ones): by `step`, or put at the offsets of the table (by `sign` times it)
static int16_t *SegaLine(int line, int odd) {
    return &hscroll_buffer[SEGA_FIRST_LINE + SegaPlaneRows() * 8 + line * 2 + odd][0];
}

static void SegaLinesStep(int odd, int16_t step) {
    for (int i = 0; i < SEGA_LINES; i++)
        *SegaLine(i, odd) = (int16_t)(*SegaLine(i, odd) + step);
}

static void SegaLinesOffset(int odd, int sign) {
    for (int i = 0; i < SEGA_LINES; i++)
        *SegaLine(i, odd) = (int16_t)(*SegaLine(i, odd) + sign * sega_line_offsets[i]);
}

// ---- The palette sweeps ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Seven steps of eight colours, one every four frames (the lists of the pre-beta's $B0); the first goes to colours 8-15 of the first line, the second to colours 0-7
static const uint16_t sega_sweep_a[7][8] = {
    { 0x0E60, 0x0E60, 0x0E60, 0x0E60, 0x0E60, 0x0E60, 0x0E60, 0x0EEE },
    { 0x0E62, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE },
    { 0x0E84, 0x0E62, 0x0E60, 0x0E60, 0x0E60, 0x0E60, 0x0E60, 0x0EEE },
    { 0x0EA6, 0x0E84, 0x0E62, 0x0E60, 0x0E60, 0x0E60, 0x0E60, 0x0EEE },
    { 0x0EC8, 0x0EA6, 0x0E84, 0x0E62, 0x0E60, 0x0E60, 0x0E60, 0x0EEE },
    { 0x0EEC, 0x0EC8, 0x0EA6, 0x0E84, 0x0E62, 0x0E60, 0x0E60, 0x0EEE },
    { 0x0EEE, 0x0EEC, 0x0EC8, 0x0EA6, 0x0E84, 0x0E62, 0x0E60, 0x0EEE },
};
static const uint16_t sega_sweep_b[7][8] = {
    { 0x0EEE, 0x0E60, 0x0E60, 0x0E60, 0x0E60, 0x0E60, 0x0E60, 0x0E60 },
    { 0x0EEE, 0x0E62, 0x0E60, 0x0E60, 0x0E60, 0x0E60, 0x0E60, 0x0E60 },
    { 0x0EEE, 0x0E84, 0x0E62, 0x0E60, 0x0E60, 0x0E60, 0x0E60, 0x0E60 },
    { 0x0EEE, 0x0EA6, 0x0E84, 0x0E62, 0x0E60, 0x0E60, 0x0E60, 0x0E60 },
    { 0x0EEE, 0x0EC8, 0x0EA6, 0x0E84, 0x0E62, 0x0E60, 0x0E60, 0x0E60 },
    { 0x0EEE, 0x0EEC, 0x0EC8, 0x0EA6, 0x0E84, 0x0E62, 0x0E60, 0x0E60 },
    { 0x0EEE, 0x0EEE, 0x0EEC, 0x0EC8, 0x0EA6, 0x0E84, 0x0E62, 0x0E60 },
};
enum { SEGA_SWEEP_STEPS = 7, SEGA_SWEEP_DELAY = 4 };

// Offset_0x0347BC: true once the last step has had its four frames
typedef struct {
    int16_t timer;
    uint8_t sweep_delay, sweep_step;
} Own_SegaSonic;

static void SegaSweepStart(Own_SegaSonic *own) {
    own->sweep_delay = 1;
    own->sweep_step = 0xFF;
}

static bool SegaSweep(Own_SegaSonic *own, const uint16_t (*list)[8], int first_colour) {
    if (--own->sweep_delay != 0)
        return false;
    const uint8_t step = (uint8_t)(own->sweep_step + 1);
    if (step >= SEGA_SWEEP_STEPS)
        return true;
    own->sweep_step = step;
    own->sweep_delay = SEGA_SWEEP_DELAY;
    for (int i = 0; i < 8; i++)
        dry_palette[0][first_colour + i] = list[step][i];
    return false;
}

// ---- Sonic ($B0) -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// He runs left across the logo (twelve frames of $20 pixels) while the first strip goes by on the even lines, is out of sight for the first palette sweep, runs back right (twelve frames) with the second strip
// on the odd lines, and is out of sight for the second sweep: the voice is then played (GM_Sega.c). A wide picture has more to run: the frames it takes to cross it are added to both runs.
enum { SEGA_RUN_STEPS = 11, SEGA_TAIL_STEPS = 12 };

static const uint8_t sega_sonic_anim[] = { 0x00, 0x02, 0x00, 0x00, 0x01, 0x02, 0x03, 0xFF }; // (every frame, 0-3 round)

static int SegaExtraFrames(void) {
    return (PLANE_WIDEADD * 8 + 0x1F) / 0x20;
}

void Obj_SegaSonic(Sst *self) {
    Own_SegaSonic *own = (Own_SegaSonic *)self->own.u8;

    switch (self->routine) {
    case 0:
        self->routine += 2;
        SegaMakeSonicArt();
        self->mappings = Mappings_SegaSprites;
        self->art_tile = TILE_MAP(1, 2, 0, 0, SEGA_SONIC_TILE);
        self->render_flags = SST_RENDER_X_FLIP; // (faces the way he runs: left)
        self->status = SST_STATUS_X_FLIP;
        self->priority = 1 * 0x80;
        self->width_pixels = 0x10;
        self->x_pos.f.u = 0x1E8 + (PLANE_WIDEADD * 8);
        self->y_pos.f.u = 0xF0 + SCREEN_TALLADD2;
        own->timer = SEGA_RUN_STEPS + SegaExtraFrames();
        SegaStripFirst();
        SegaLinesOffset(0, 1);
        break;
    case 2: // runs left, the even lines go with the strip
        self->x_pos.f.u -= 0x20;
        if (--own->timer >= 0) {
            SegaLinesStep(0, -0x20);
            Sst_Animate(self, sega_sonic_anim);
        } else {
            self->routine += 2;
            own->timer = SEGA_TAIL_STEPS;
            SegaSweepStart(own);
        }
        Sst_Draw(self);
        break;
    case 4: // out of sight: the strip goes on, and the first palette sweep
        if (own->timer != 0) {
            own->timer--;
            SegaLinesStep(0, -0x20);
        }
        if (SegaSweep(own, sega_sweep_a, 8)) {
            self->routine += 2;
            self->status &= (uint8_t)~SST_STATUS_X_FLIP; // (turns round: the render flags follow with his animation)
            self->x_pos.f.u -= 0x28;
            own->timer = SEGA_RUN_STEPS + SegaExtraFrames();
            memset(hscroll_buffer, 0, sizeof(hscroll_buffer));
            SegaLinesOffset(1, -1);
            Plane_Clear(&screen1p.plane_a);
            SegaStripSecond();
        }
        break;
    case 6: // runs right, the odd lines go with the second strip
        if (--own->timer >= 0) {
            self->x_pos.f.u += 0x20;
            SegaLinesStep(1, 0x20);
            Sst_Animate(self, sega_sonic_anim);
        } else {
            self->routine += 2;
            own->timer = SEGA_TAIL_STEPS;
            SegaSweepStart(own);
        }
        Sst_Draw(self);
        break;
    case 8: // out of sight: the second palette sweep; then the voice
        if (own->timer != 0) {
            own->timer--;
            SegaLinesStep(1, 0x20);
        }
        if (SegaSweep(own, sega_sweep_b, 0)) {
            self->routine += 2;
            sega_animation_done = true;
        }
        break;
    }
}

// ---- The trademark's hider ($B1) ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void Obj_SegaTrademarkHider(Sst *self) {
    switch (self->routine) {
    case 0:
        self->routine += 2;
        self->mappings = Mappings_SegaSprites;
        self->mapping_frame = 4;
        self->art_tile = TILE_MAP(0, 0, 0, 0, 3); // (tile 3 of the logo's art: blank)
        self->render_flags = 0;
        self->priority = 2 * 0x80;
        self->width_pixels = 8;
        self->x_pos.f.u = 0x174 + (PLANE_WIDEADD * 4);
        self->y_pos.f.u = 0xD8 + SCREEN_TALLADD2;
        break;
    }
    Sst_Draw(self);
}
