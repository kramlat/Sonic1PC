#include "SpecialResult.h"

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

static Scratch_SpecialResult *SR(Object *o) { return (Scratch_SpecialResult *)&o->scratch; }

static void SSR_Display(Object *obj) {
    if (obj->pos.s.x >= 0 && obj->pos.s.x < (0x200 + SCREEN_WIDEADD))
        DisplaySprite(obj);
}

static void SSR_Main(Object *obj) {
    int count = (rings >= SSR_CONTINUE_RINGS) ? 5 : 4;
    Object *a1 = obj;
    for (int i = 0; i < count; i++, a1++) {
        a1->type = ObjId_SSResult;
        a1->pos.s.x = SSR_ItemData[i].x0;
        SR(a1)->main_x = SSR_ItemData[i].x1;
        a1->pos.s.y = SSR_ItemData[i].y;
        a1->routine = SSR_ItemData[i].routine;
        a1->frame = SSR_ItemData[i].frame;
        a1->mappings = Mappings_SpecialResult;
        a1->tile = TILE_MAP(1, 0, 0, 0, ArtTile_Title_Card);
        a1->render.b = 0; // positioned on screen, not in the level
        a1->frame_time.w = 0;
    }

    // The header text: "SPECIAL STAGE" with no emeralds, "CHAOS EMERALDS" with some, "SONIC GOT THEM ALL" with all.
    uint8_t frame = 7;
    if (emeralds != 0) {
        frame = 0;
        if (emeralds == 6) {
            frame = 8;
            obj->pos.s.x = 0x18;
            SR(obj)->main_x = 0x118;
        }
    }
    obj->frame = frame;
}

void Obj_SpecialResult(Object *obj) {
    switch (obj->routine) {
    case 0: // wait for the title card art
        if (plc_buffer[0].art != NULL)
            return;
        SSR_Main(obj);
        // fall through into Move, like the real routine 0
        __attribute__((fallthrough));
    case 2: { // Move in
        int16_t speed = 0x10;
        if (obj->pos.s.x == SR(obj)->main_x) { // reached its place
            if (obj->frame == 2) { // the ring bonus element drives the sequence
                obj->routine += 2;
                obj->frame_time.w = 3 * 60;
                objects[SSR_EMERALD_SLOT].type = ObjId_SSRChaos;
                // fall through into the wait (below)
                if (--obj->frame_time.w == 0)
                    obj->routine += 2;
                DisplaySprite(obj);
                return;
            }
        } else {
            if (obj->pos.s.x > SR(obj)->main_x)
                speed = -speed;
            obj->pos.s.x += speed;
        }
        SSR_Display(obj);
        break;
    }
    case 4: case 8: case 0xC: case 0x10: // waits
        if (--obj->frame_time.w == 0)
            obj->routine += 2;
        DisplaySprite(obj);
        break;
    case 6: // tally the ring bonus into the score
        DisplaySprite(obj);
        endact_bonus = true; // update the bonus numbers
        if (ring_bonus != 0) {
            ring_bonus -= 10;
            AddPoints(10);
            if ((vbla_count & 3) == 0)
                QueueSound2(sfx_Switch);
        } else {
            QueueSound2(sfx_Cash);
            obj->routine += 2; // wait, then exit
            obj->frame_time.w = 3 * 60;
            if (rings >= SSR_CONTINUE_RINGS) { // an extra continue: show it first
                obj->frame_time.w = 1 * 60;
                obj->routine += 4;
            }
        }
        break;
    case 0xA: case 0x12: // exit: tells the stage loop it can finish
        restart = true;
        DisplaySprite(obj);
        break;
    case 0xE: { // the continue icon appears
        Object *icon = &objects[SSR_CARD_SLOT + 4];
        icon->frame = 4; // mini Sonic
        icon->routine = 0x14;
        QueueSound2(sfx_Continue);
        obj->routine += 2;
        obj->frame_time.w = 6 * 60;
        DisplaySprite(obj);
        break;
    }
    case 0x14: // mini Sonic taps his foot
        if ((vbla_count & 0xF) == 0)
            obj->frame ^= 1;
        DisplaySprite(obj);
        break;
    }
}

void Obj_SpecialResultEmerald(Object *obj) {
    switch (obj->routine) {
    case 0: { // Main: this object and the next ones become the collected emeralds
        if (emeralds == 0) {
            ObjectDelete(obj);
            return;
        }
        Object *a1 = obj;
        for (int i = 0; i < emeralds && i < 6; i++, a1++) {
            a1->type = ObjId_SSRChaos;
            a1->pos.s.x = SSRC_PosData[i];
            a1->pos.s.y = 0xF0;
            uint8_t colour = emerald_list[i];
            a1->frame = colour;
            a1->anim = colour;
            a1->routine = 2;
            a1->mappings = Mappings_SSResultEmerald;
            a1->tile = TILE_MAP(1, 0, 0, 0, ArtTile_SS_Results_Emeralds);
            a1->render.b = 0;
        }
        // fall through into Flash for this first emerald
        __attribute__((fallthrough));
    }
    case 2: { // Flash: alternate between blank and the emerald every frame
        uint8_t frame = obj->frame;
        obj->frame = 6; // blank
        if (frame == 6)
            obj->frame = obj->anim;
        DisplaySprite(obj);
        break;
    }
    }
}
