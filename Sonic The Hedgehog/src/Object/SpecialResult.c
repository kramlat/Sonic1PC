#include "SpecialResult.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Game.h"
#include "Level.h"
#include "PLC.h"
#include "SpecialStage.h"
#include "Sound.h"

// Mappings are owned by TitleCard.c (Mappings_SpecialResult) and SpecialStage.c (Mappings_SSResultEmerald).
extern const uint8_t Mappings_SpecialResult[];
extern const uint8_t Mappings_SSResultEmerald[];

#define SSR_CONTINUE_RINGS 50

// Card elements, from SSR_ItemData: start X, target X, Y, routine, frame.
static const struct { int16_t x0, x1, y; uint8_t routine, frame; } SSR_ItemData[5] = {
    { 0x020, 0x120, 0x0C4, 2, 0 }, // header text (frame chosen below)
    { 0x320, 0x120, 0x118, 2, 1 }, // score tally
    { 0x360, 0x120, 0x128, 2, 2 }, // ring bonus tally
    { 0x1EC, 0x11C, 0x0C4, 2, 3 }, // blue oval
    { 0x3A0, 0x120, 0x138, 2, 6 }, // continue tally (only with 50 rings)
};

// X positions of the Chaos Emeralds, in the order they were collected (interleaved so they stay centred).
static const int16_t SSRC_PosData[6] = { 0x110, 0x128, 0x0F8, 0x140, 0x0E0, 0x158 };

static Own_SpecialResult *SR(Sst *o) { return (Own_SpecialResult *)o->own.u8; }

static void SSR_Display(Sst *self) {
    if (self->x_pos.f.u >= 0 && self->x_pos.f.u < (0x200 + SCREEN_WIDEADD))
        Sst_Draw(self);
}

// The continue element, the fifth of the card (it comes up when 50 rings are collected)
static Sst *ssr_continue_element;

static void SSR_Main(Sst *self) {
    int count = (rings >= SSR_CONTINUE_RINGS) ? 5 : 4;
    Sst *a1 = self;
    ssr_continue_element = NULL;
    for (int i = 0; i < count; i++) {
        if (i > 0) {
            a1 = Sst_AllocateAfter(a1);
            if (a1 == NULL)
                break;
        }
        if (i == 4)
            ssr_continue_element = a1;
        a1->code = Obj_SpecialResult;
        a1->x_pos.f.u = SSR_ItemData[i].x0;
        SR(a1)->main_x = SSR_ItemData[i].x1;
        a1->y_pos.f.u = SSR_ItemData[i].y;
        a1->routine = SSR_ItemData[i].routine;
        a1->mapping_frame = SSR_ItemData[i].frame;
        a1->mappings = Mappings_SpecialResult;
        a1->art_tile = TILE_MAP(1, 0, 0, 0, ArtTile_Title_Card);
        a1->render_flags = 0; // positioned on screen, not in the level
        SR(a1)->timer = 0;
    }

    // The header text: "SPECIAL STAGE" with no emeralds, "CHAOS EMERALDS" with some, "SONIC GOT THEM ALL" with all.
    uint8_t frame = 7;
    if (emeralds != 0) {
        frame = 0;
        if (emeralds == 6) {
            frame = 8;
            self->x_pos.f.u = 0x18;
            SR(self)->main_x = 0x118;
        }
    }
    self->mapping_frame = frame;
}

void Obj_SpecialResult(Sst *self) {
    switch (self->routine) {
    case 0: // wait for the title card art
        if (plc_buffer[0].art != NULL)
            return;
        SSR_Main(self);
        // fall through into Move, like the real routine 0
        __attribute__((fallthrough));
    case 2: { // Move in
        int16_t speed = 0x10;
        if (self->x_pos.f.u == SR(self)->main_x) { // reached its place
            if (self->mapping_frame == 2) { // the ring bonus element drives the sequence
                self->routine += 2;
                SR(self)->timer = 3 * 60;
                Sst_SpawnCode(Obj_SpecialResultEmerald, 0, 0);
                // fall through into the wait (below)
                if (--SR(self)->timer == 0)
                    self->routine += 2;
                Sst_Draw(self);
                return;
            }
        } else {
            if (self->x_pos.f.u > SR(self)->main_x)
                speed = -speed;
            self->x_pos.f.u += speed;
        }
        SSR_Display(self);
        break;
    }
    case 4: case 8: case 0xC: case 0x10: // waits
        if (--SR(self)->timer == 0)
            self->routine += 2;
        Sst_Draw(self);
        break;
    case 6: // tally the ring bonus into the score
        Sst_Draw(self);
        endact_bonus = true; // update the bonus numbers
        if (ring_bonus != 0) {
            ring_bonus -= 10;
            AddPoints(10);
            if ((vbla_count & 3) == 0)
                QueueSound2(sfx_Switch);
        } else {
            QueueSound2(sfx_Cash);
            self->routine += 2; // wait, then exit
            SR(self)->timer = 3 * 60;
            if (rings >= SSR_CONTINUE_RINGS) { // an extra continue: show it first
                SR(self)->timer = 1 * 60;
                self->routine += 4;
            }
        }
        break;
    case 0xA: case 0x12: // exit: tells the stage loop it can finish
        restart = true;
        Sst_Draw(self);
        break;
    case 0xE: { // the continue icon appears
        Sst *icon = ssr_continue_element;
        if (icon != NULL) {
            icon->mapping_frame = 4; // mini Sonic
            icon->routine = 0x14;
        }
        QueueSound2(sfx_Continue);
        self->routine += 2;
        SR(self)->timer = 6 * 60;
        Sst_Draw(self);
        break;
    }
    case 0x14: // mini Sonic taps his foot
        if ((vbla_count & 0xF) == 0)
            self->mapping_frame ^= 1;
        Sst_Draw(self);
        break;
    }
}

void Obj_SpecialResultEmerald(Sst *self) {
    switch (self->routine) {
    case 0: { // Main: this object and the next ones become the collected emeralds
        if (emeralds == 0) {
            Sst_Delete(self);
            return;
        }
        Sst *a1 = self;
        for (int i = 0; i < emeralds && i < 6; i++) {
            if (i > 0) {
                a1 = Sst_AllocateAfter(a1);
                if (a1 == NULL)
                    break;
            }
            a1->code = Obj_SpecialResultEmerald;
            a1->x_pos.f.u = SSRC_PosData[i];
            a1->y_pos.f.u = 0xF0;
            uint8_t colour = emerald_list[i];
            a1->mapping_frame = colour;
            a1->anim = colour;
            a1->routine = 2;
            a1->mappings = Mappings_SSResultEmerald;
            a1->art_tile = TILE_MAP(1, 0, 0, 0, ArtTile_SS_Results_Emeralds);
            a1->render_flags = 0;
        }
        // fall through into Flash for this first emerald
        __attribute__((fallthrough));
    }
    case 2: { // Flash: alternate between blank and the emerald every frame
        uint8_t frame = self->mapping_frame;
        self->mapping_frame = 6; // blank
        if (frame == 6)
            self->mapping_frame = self->anim;
        Sst_Draw(self);
        break;
    }
    }
}
