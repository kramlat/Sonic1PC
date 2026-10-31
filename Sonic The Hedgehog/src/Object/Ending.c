#include "SstLoad.h"
#include "Ending.h"
#include "Constants.h"

#include <string.h>

#include "Game.h"
#include "Level.h"
#include "MathUtil.h"
#include "SpecialStage.h"

#include "Resource/Animation/TryAgainEggman.h"
#include "Resource/Mappings/EndingEmeralds.h"
#include "Resource/Mappings/EndingSTH.h"
#include "Resource/Mappings/TryAgainEggman.h"

#define EMERALDS_ALL 6

// The ending emeralds and the juggled ones keep their angle in 16 bits at the object's 0x26 (the angle byte is its high
// byte, the low byte is the fraction the spin speed accumulates into): scratch.u16[0] here, obj->angle mirrors the top.
static void SetAngle16(Object *obj, uint16_t angle) {
    obj->scratch.u16[0] = angle;
    obj->angle = (uint8_t)(angle >> 8);
}

// ---------------------------------------------------------------------------
// Object 88 - the chaos emeralds Sonic holds up: six of them spin up into a widening, rising circle.
// ---------------------------------------------------------------------------

#define echa_origX  scratch.u16[8]  // 0x38: centre of the circle
#define echa_origY  scratch.u16[9]  // 0x3A
#define echa_radius scratch.u16[10] // 0x3C: radius is its high byte
#define echa_angle  scratch.u16[11] // 0x3E: spin speed, added to the 16-bit angle each frame

static void ECha_Move(Object *obj) {
    SetAngle16(obj, (uint16_t)(obj->scratch.u16[0] + obj->echa_angle));
    int16_t sin, cos;
    CalcSine(obj->angle, &sin, &cos);
    int32_t radius = obj->echa_radius >> 8;
    obj->pos.l.x.f.u = (int16_t)(((radius * cos) >> 8) + (int16_t)obj->echa_origX);
    obj->pos.l.y.f.u = (int16_t)(((radius * sin) >> 8) + (int16_t)obj->echa_origY);

    if (obj->echa_radius != 0x20 * 0x100)
        obj->echa_radius += 0x20; // widen
    if (obj->echa_angle != 0x20 * 0x100)
        obj->echa_angle += 0x20; // spin faster
    if (obj->echa_origY != 0x140)
        obj->echa_origY--; // rise
}

void Obj_EndChaos(Object *obj) {
    if (obj->routine == 0) {
        if (player->mapping_frame != 2) // wait until Sonic's "hold" animation has reached its last frame
            return;
        obj->pos.l.x.f.u = player->x_pos.f.u;
        obj->pos.l.y.f.u = player->y_pos.f.u;
        uint8_t angle = 0;
        for (int i = 0; i < EMERALDS_ALL; i++) {
            Object *e = obj + i;
            e->type = ObjId_88;
            e->routine += 2;
            e->mappings = Mappings_EndingEmeralds;
            e->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Ending_Emeralds);
            e->render.b = 0;
            e->render.f.level_fg = true;
            e->priority = 1; // above Sonic
            e->echa_origX = (uint16_t)obj->pos.l.x.f.u;
            e->echa_origY = (uint16_t)obj->pos.l.y.f.u;
            e->anim = (uint8_t)(1 + i); // frame 0 is the white flash
            e->frame = (uint8_t)(1 + i);
            SetAngle16(e, (uint16_t)(angle << 8));
            angle += 0x100 / EMERALDS_ALL;
        }
    }
    ECha_Move(obj);
    DisplaySprite(obj);
}

// ---------------------------------------------------------------------------
// Object 89 - "SONIC THE HEDGEHOG" text: slides in, waits, then ends the sequence (the credits are next).
// ---------------------------------------------------------------------------

#define esth_time scratch.u16[4] // 0x30: how long to stay before going to the credits

void Obj_EndSTH(Object *obj) {
    switch (obj->routine) {
    case 0:
        obj->routine += 2;
        obj->pos.s.x = (int16_t)(0x80 - 0xA0 + SCREEN_WIDEADD2); // starts off the left edge
        obj->pos.s.y = (int16_t)(0x80 + 0x58 + SCREEN_TALLADD2);
        obj->mappings = Mappings_EndingSTH;
        obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Ending_STH);
        obj->render.b = 0;
        obj->priority = 0;
        // fallthrough
    case 2:
        if (obj->pos.s.x < 0x80 + 0x40 + SCREEN_WIDEADD2) {
            obj->pos.s.x += 0x10;
            break;
        }
        obj->routine += 2;
        obj->esth_time = 5 * 60;
        // fallthrough
    case 4:
        if ((int16_t)--obj->esth_time < 0)
            gamemode = GameMode_Credits; // the trigger End_MainLoop watches for
        break;
    }
    DisplaySprite(obj);
}

// ---------------------------------------------------------------------------
// Object 8B - Eggman on the "TRY AGAIN" (not every emerald) and "END" (all six) screens.
// ---------------------------------------------------------------------------

#define eegg_time scratch.u16[4] // 0x30: time between juggle motions

static void EEgg_Animate(Object *obj) { AnimateSprite(obj, Animation_TryAgainEggman); }

static void EEgg_Juggle(Object *obj) {
    obj->routine += 2;
    int16_t dir = (obj->anim & 1) ? -2 : 2;
    for (int i = 0; i < EMERALDS_ALL; i++) {
        Object *e = &objects[CREDITS_SLOT_CHAOS + i];
        e->scratch.s16[11] = (int16_t)(dir * 0x100); // tcha_juggledir: the byte sits in the word's high half
        SetAngle16(e, (uint16_t)(e->scratch.u16[0] + (int8_t)(dir * 8) * 0x100));
    }
    obj->frame++; // the raised-hand frame after juggling
    obj->eegg_time = (2 * 60) - 8;
}

static void EEgg_Wait(Object *obj) {
    if ((int16_t)--obj->eegg_time >= 0)
        return;
    obj->anim ^= 1; // alternate the two juggle animations
    obj->routine = 2;
}

void Obj_EndEggman(Object *obj) {
    switch (obj->routine) {
    case 0:
        obj->routine += 2;
        obj->pos.s.x = (int16_t)(0x80 + 0xA0 + SCREEN_WIDEADD2);
        obj->pos.s.y = (int16_t)(0x80 + 0x74 + SCREEN_TALLADD2);
        obj->mappings = Mappings_TryAgainEggman;
        obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Try_Again_Eggman);
        obj->render.b = 0;
        obj->priority = 2; // behind the emeralds
        obj->anim = 2;     // the "END" tantrum, for a good ending
        if (emeralds != EMERALDS_ALL) {
            Sst_Spawn(ObjId_Credits, 0, 0);
            credits_num = 9; // the "TRY AGAIN" text page
            objects[CREDITS_SLOT_CHAOS].type = ObjId_8C;
            obj->anim = 0;
        }
        EEgg_Animate(obj);
        break;
    case 2: EEgg_Animate(obj); break;
    case 4: EEgg_Juggle(obj); break;
    case 6: EEgg_Wait(obj); break;
    }
    DisplaySprite(obj);
}

// ---------------------------------------------------------------------------
// Object 8C - the emeralds Eggman juggles on the "TRY AGAIN" screen: one for every emerald Sonic is missing.
// ---------------------------------------------------------------------------

#define tcha_origX    scratch.u16[8]  // 0x38: centre of the juggle circle
#define tcha_origY    scratch.u16[9]  // 0x3A
#define tcha_radius   scratch.u16[10] // 0x3C: radius is its high byte
#define tcha_juggledir scratch.s16[11] // 0x3E: +-0x200 while juggling, 0 while resting in a hand
#define tcha_delay    scratch.u8[2]   // 0x2A: how long this emerald rests in a hand (obDelayAni)

static void TCha_Load(Object *obj) {
    int d2 = 0, d3 = 0;
    int count = EMERALDS_ALL - emeralds; // the ones Sonic doesn't have
    for (int i = 0; i < count; i++) {
        Object *e = obj + i;
        e->type = ObjId_8C;
        e->routine += 2;
        e->mappings = Mappings_EndingEmeralds; // the ending's own emerald frames
        e->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Try_Again_Emeralds);
        e->render.b = 0;
        e->priority = 1; // above Eggman
        e->pos.s.x = (int16_t)(0x80 + 0x84 + SCREEN_WIDEADD2);
        e->tcha_origX = (uint16_t)(0x80 + 0xA0 + SCREEN_WIDEADD2);
        e->pos.s.y = (int16_t)(0x80 + 0x6C + SCREEN_TALLADD2);
        e->tcha_origY = (uint16_t)e->pos.s.y;
        e->tcha_radius = 0x1C * 0x100;

        // The first frame (colour) not yet collected: skip the ones in the list.
        for (;;) {
            bool collected = false;
            for (int k = (int)emeralds - 1; k >= 0; k--)
                if (emerald_list[k] == d2)
                    collected = true;
            if (!collected)
                break;
            d2++;
        }
        e->frame = (uint8_t)(d2 + 1); // frame 0 is the white flash
        d2++;
        SetAngle16(e, 0x8000); // in Eggman's right hand
        e->frame_time.b = (int8_t)d3;
        e->tcha_delay = (uint8_t)d3;
        d3 += 10;
    }
}

static void TCha_Juggle(Object *obj) {
    if (obj->tcha_juggledir == 0)
        return;

    bool move = true;
    if (obj->frame_time.b != 0) {
        obj->frame_time.b--;
        move = (obj->frame_time.b == 0);
    }
    if (move)
        SetAngle16(obj, (uint16_t)(obj->scratch.u16[0] + obj->tcha_juggledir));

    if (obj->angle == 0 || obj->angle == 0x80) { // landed in a hand
        obj->tcha_juggledir = 0;
        obj->frame_time.b = (int8_t)obj->tcha_delay;
    }

    int16_t sin, cos;
    CalcSine(obj->angle, &sin, &cos);
    int32_t radius = obj->tcha_radius >> 8;
    obj->pos.s.x = (int16_t)(((radius * cos) >> 8) + (int16_t)obj->tcha_origX);
    obj->pos.s.y = (int16_t)(((radius * sin) >> 8) + (int16_t)obj->tcha_origY);
}

void Obj_TryChaos(Object *obj) {
    switch (obj->routine) {
    case 0:
        TCha_Load(obj);
        // fallthrough
    case 2: TCha_Juggle(obj); break;
    }
    DisplaySprite(obj);
}
