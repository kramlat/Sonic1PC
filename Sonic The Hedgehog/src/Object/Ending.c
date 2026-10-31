#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object/Credits.h"
#include "Object.h"
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
// byte, the low byte is the fraction the spin speed accumulates into): own.u16[0] here, self->angle mirrors the top.
static void SetAngle16(Sst *self, uint16_t angle) {
    self->own.u16[0] = angle;
    self->angle = (uint8_t)(angle >> 8);
}

// ---------------------------------------------------------------------------
// Object 88 - the chaos emeralds Sonic holds up: six of them spin up into a widening, rising circle.
// ---------------------------------------------------------------------------

#define echa_origX  own.u16[8]  // 0x38: centre of the circle
#define echa_origY  own.u16[9]  // 0x3A
#define echa_radius own.u16[10] // 0x3C: radius is its high byte
#define echa_angle  own.u16[11] // 0x3E: spin speed, added to the 16-bit angle each frame

static void ECha_Move(Sst *self) {
    SetAngle16(self, (uint16_t)(self->own.u16[0] + self->echa_angle));
    int16_t sin, cos;
    CalcSine(self->angle, &sin, &cos);
    int32_t radius = self->echa_radius >> 8;
    self->x_pos.f.u = (int16_t)(((radius * cos) >> 8) + (int16_t)self->echa_origX);
    self->y_pos.f.u = (int16_t)(((radius * sin) >> 8) + (int16_t)self->echa_origY);

    if (self->echa_radius != 0x20 * 0x100)
        self->echa_radius += 0x20; // widen
    if (self->echa_angle != 0x20 * 0x100)
        self->echa_angle += 0x20; // spin faster
    if (self->echa_origY != 0x140)
        self->echa_origY--; // rise
}

void Obj_EndChaos(Sst *self) {
    if (self->routine == 0) {
        if (player->mapping_frame != 2) // wait until Sonic's "hold" animation has reached its last frame
            return;
        self->x_pos.f.u = player->x_pos.f.u;
        self->y_pos.f.u = player->y_pos.f.u;
        uint8_t angle = 0;
        Sst *e = self;
        for (int i = 0; i < EMERALDS_ALL; i++) {
            if (i > 0) {
                e = Sst_AllocateAfter(e);
                if (e == NULL)
                    break;
            }
            e->code = Obj_EndChaos;
            e->routine += 2;
            e->mappings = Mappings_EndingEmeralds;
            e->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Ending_Emeralds);
            e->render_flags = SST_RENDER_LEVEL;
            e->priority = 1 * 0x80; // above Sonic
            e->echa_origX = (uint16_t)self->x_pos.f.u;
            e->echa_origY = (uint16_t)self->y_pos.f.u;
            e->anim = (uint8_t)(1 + i); // frame 0 is the white flash
            e->mapping_frame = (uint8_t)(1 + i);
            SetAngle16(e, (uint16_t)(angle << 8));
            angle += 0x100 / EMERALDS_ALL;
        }
    }
    ECha_Move(self);
    Sst_Draw(self);
}

// ---------------------------------------------------------------------------
// Object 89 - "SONIC THE HEDGEHOG" text: slides in, waits, then ends the sequence (the credits are next).
// ---------------------------------------------------------------------------

#define esth_time own.u16[4] // 0x30: how long to stay before going to the credits

void Obj_EndSTH(Sst *self) {
    switch (self->routine) {
    case 0:
        self->routine += 2;
        self->x_pos.f.u = (int16_t)(0x80 - 0xA0 + SCREEN_WIDEADD2); // starts off the left edge
        self->y_pos.f.u = (int16_t)(0x80 + 0x58 + SCREEN_TALLADD2);
        self->mappings = Mappings_EndingSTH;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Ending_STH);
        self->render_flags = 0;
        self->priority = 0 * 0x80;
        // fallthrough
    case 2:
        if (self->x_pos.f.u < 0x80 + 0x40 + SCREEN_WIDEADD2) {
            self->x_pos.f.u += 0x10;
            break;
        }
        self->routine += 2;
        self->esth_time = 5 * 60;
        // fallthrough
    case 4:
        if ((int16_t)--self->esth_time < 0)
            gamemode = GameMode_Credits; // the trigger End_MainLoop watches for
        break;
    }
    Sst_Draw(self);
}

// ---------------------------------------------------------------------------
// Object 8B - Eggman on the "TRY AGAIN" (not every emerald) and "END" (all six) screens.
// ---------------------------------------------------------------------------

#define eegg_time own.u16[4] // 0x30: time between juggle motions

static void EEgg_Animate(Sst *self) { Sst_Animate(self, Animation_TryAgainEggman); }

// The emeralds Eggman juggles (object 8C), in the order they were made
static Sst *tcha_list[EMERALDS_ALL];
static int tcha_count;

static void EEgg_Juggle(Sst *self) {
    self->routine += 2;
    int16_t dir = (self->anim & 1) ? -2 : 2;
    for (int i = 0; i < tcha_count; i++) {
        Sst *e = tcha_list[i];
        e->own.s16[11] = (int16_t)(dir * 0x100); // tcha_juggledir: the byte sits in the word's high half
        SetAngle16(e, (uint16_t)(e->own.u16[0] + (int8_t)(dir * 8) * 0x100));
    }
    self->mapping_frame++; // the raised-hand frame after juggling
    self->eegg_time = (2 * 60) - 8;
}

static void EEgg_Wait(Sst *self) {
    if ((int16_t)--self->eegg_time >= 0)
        return;
    self->anim ^= 1; // alternate the two juggle animations
    self->routine = 2;
}

void Obj_EndEggman(Sst *self) {
    switch (self->routine) {
    case 0:
        self->routine += 2;
        self->x_pos.f.u = (int16_t)(0x80 + 0xA0 + SCREEN_WIDEADD2);
        self->y_pos.f.u = (int16_t)(0x80 + 0x74 + SCREEN_TALLADD2);
        self->mappings = Mappings_TryAgainEggman;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Try_Again_Eggman);
        self->render_flags = 0;
        self->priority = 2 * 0x80; // behind the emeralds
        self->anim = 2;     // the "END" tantrum, for a good ending
        if (emeralds != EMERALDS_ALL) {
            Sst_SpawnCode(Obj_Credits, 0, 0);
            credits_num = 9; // the "TRY AGAIN" text page
            Sst_SpawnCode(Obj_TryChaos, 0, 0);
            self->anim = 0;
        }
        EEgg_Animate(self);
        break;
    case 2: EEgg_Animate(self); break;
    case 4: EEgg_Juggle(self); break;
    case 6: EEgg_Wait(self); break;
    }
    Sst_Draw(self);
}

// ---------------------------------------------------------------------------
// Object 8C - the emeralds Eggman juggles on the "TRY AGAIN" screen: one for every emerald Sonic is missing.
// ---------------------------------------------------------------------------

#define tcha_origX    own.u16[8]  // 0x38: centre of the juggle circle
#define tcha_origY    own.u16[9]  // 0x3A
#define tcha_radius   own.u16[10] // 0x3C: radius is its high byte
#define tcha_juggledir own.s16[11] // 0x3E: +-0x200 while juggling, 0 while resting in a hand
#define tcha_delay    own.u8[2]   // 0x2A: how long this emerald rests in a hand (obDelayAni)

static void TCha_Load(Sst *self) {
    int d2 = 0, d3 = 0;
    int count = EMERALDS_ALL - emeralds; // the ones Sonic doesn't have
    Sst *e = self;
    tcha_count = 0;
    for (int i = 0; i < count; i++) {
        if (i > 0) {
            e = Sst_AllocateAfter(e);
            if (e == NULL)
                break;
        }
        tcha_list[tcha_count++] = e;
        e->code = Obj_TryChaos;
        e->routine += 2;
        e->mappings = Mappings_EndingEmeralds; // the ending's own emerald frames
        e->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Try_Again_Emeralds);
        e->render_flags = 0;
        e->priority = 1 * 0x80; // above Eggman
        e->x_pos.f.u = (int16_t)(0x80 + 0x84 + SCREEN_WIDEADD2);
        e->tcha_origX = (uint16_t)(0x80 + 0xA0 + SCREEN_WIDEADD2);
        e->y_pos.f.u = (int16_t)(0x80 + 0x6C + SCREEN_TALLADD2);
        e->tcha_origY = (uint16_t)e->y_pos.f.u;
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
        e->mapping_frame = (uint8_t)(d2 + 1); // frame 0 is the white flash
        d2++;
        SetAngle16(e, 0x8000); // in Eggman's right hand
        e->anim_frame_timer = (int8_t)d3;
        e->tcha_delay = (uint8_t)d3;
        d3 += 10;
    }
}

static void TCha_Juggle(Sst *self) {
    if (self->tcha_juggledir == 0)
        return;

    bool move = true;
    if (self->anim_frame_timer != 0) {
        self->anim_frame_timer--;
        move = (self->anim_frame_timer == 0);
    }
    if (move)
        SetAngle16(self, (uint16_t)(self->own.u16[0] + self->tcha_juggledir));

    if (self->angle == 0 || self->angle == 0x80) { // landed in a hand
        self->tcha_juggledir = 0;
        self->anim_frame_timer = (int8_t)self->tcha_delay;
    }

    int16_t sin, cos;
    CalcSine(self->angle, &sin, &cos);
    int32_t radius = self->tcha_radius >> 8;
    self->x_pos.f.u = (int16_t)(((radius * cos) >> 8) + (int16_t)self->tcha_origX);
    self->y_pos.f.u = (int16_t)(((radius * sin) >> 8) + (int16_t)self->tcha_origY);
}

void Obj_TryChaos(Sst *self) {
    switch (self->routine) {
    case 0:
        TCha_Load(self);
        // fallthrough
    case 2: TCha_Juggle(self); break;
    }
    Sst_Draw(self);
}
