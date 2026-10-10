// Hidden Palace's badniks for Sonic 2 (Nick Arcade's objects 4C and 4F): the BBat, a bat that hangs about and swoops at Sonic in an arc, and the Redz, a dinosaur that walks to the edge of its floor and back.
#include "Object/HPZBadniks.h"
#include "Constants.h"

#include "Level.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Object/Sonic.h"

#include "Macros.h"

#include "Resource/Animation/BBat.h"
#include "Resource/Animation/Redz.h"
#include "Resource/Mappings/BBat.h"
#include "Resource/Mappings/Redz.h"

#define ArtTile_Redz 0x500
#define ArtTile_BBat 0x580 // (the alpha's is $530: the bubbles' art of Hidden Palace's water has that now, see Sonic2PLC.c)

// ---------------------------------------------------------------------------------------------------------------------------------------
// The BBat (object 4C)
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;      // 0x28
    uint8_t pad0;         // 0x29
    uint16_t timer;       // 0x2A (the flapping counts its high byte: as the original does)
    int16_t detect_timer; // 0x2C: time until it notices Sonic
    int16_t home_y;       // 0x2E: the middle of its hovering
    uint8_t pad1[0xD];    // 0x30-0x3C
    uint8_t turned;       // 0x3D: it swoops to the right
    uint8_t first;        // 0x3E: which way it faces next while it looks about
    uint8_t arc;          // 0x3F: its angle on the arc
} Scratch_BBat;

enum { BBatRoutine_Init = 0, BBatRoutine_Main = 2, BBatRoutine_Attack = 4 };
enum { BBatSub_Wait = 0, BBatSub_Flap = 2, BBatSub_Seek = 4 };

static void BBat_SetFlip(Object *obj, bool flip) {
    obj->render.f.x_flip = flip;
    obj->status.o.f.x_flip = flip;
}

// Sonic within 128 pixels wakes it (BBat_WaitForPlayer): true if it was woken
static bool BBat_WaitForPlayer(Object *obj, Scratch_BBat *scratch) {
    int16_t d0 = obj->pos.l.x.f.u - player->pos.l.x.f.u;
    if (d0 > 0x80 || d0 < -0x80)
        return false;
    obj->routine_sec = BBatSub_Seek;
    obj->anim = 2;
    scratch->timer = 8;
    scratch->first = 0;
    return true;
}

// It notices Sonic within 96 pixels and swoops (BBat_AttackPlayer). Returns what the original leaves in its zero flag: true if the rest of the frame's looking about is to be skipped
static bool BBat_AttackPlayer(Object *obj, Scratch_BBat *scratch) {
    scratch->detect_timer--;
    if (scratch->detect_timer >= 0)
        return scratch->detect_timer == 0;

    int16_t d0 = obj->pos.l.x.f.u - player->pos.l.x.f.u;
    if (d0 > 0x60 || d0 < -0x60) {
        if (d0 <= 0x80 && d0 > -0x80)
            return false; // (between 96 and 128 pixels: nothing happens)
        obj->anim = 1;
        obj->routine_sec = BBatSub_Wait;
        scratch->timer = 0x18;
        return false;
    }
    if (d0 < 0)
        scratch->turned = 0xFF;
    scratch->arc = 0x40;
    obj->inertia = 0x400;
    obj->routine = BBatRoutine_Attack;
    obj->anim = 3;
    scratch->timer = 0xC;
    scratch->first = 1;
    return true;
}

static void BBat_Hover(Object *obj, Scratch_BBat *scratch) {
    switch (obj->routine_sec) {
    case BBatSub_Wait:
        if ((int16_t)--scratch->timer >= 0)
            return;
        if (BBat_WaitForPlayer(obj, scratch))
            return;
        if ((uint8_t)RandomNumber() != 0) // (one frame in 256)
            return;
        scratch->timer = 0x18;
        scratch->detect_timer = 0x1E;
        obj->routine_sec = BBatSub_Flap;
        obj->anim = 1;
        scratch->first = 0;
        break;
    case BBatSub_Flap: { // (counts the high byte of its timer, which is 0: one frame)
        uint8_t high = (uint8_t)(scratch->timer >> 8);
        high--;
        scratch->timer = (uint16_t)((scratch->timer & 0xFF) | (high << 8));
        if (high & 0x80)
            obj->routine_sec = BBatSub_Wait;
        break;
    }
    case BBatSub_Seek:
        if (BBat_AttackPlayer(obj, scratch))
            return;
        if (--scratch->timer != 0)
            return;
        if (scratch->first) {
            scratch->first = 0;
            scratch->timer = 8;
            BBat_SetFlip(obj, true);
        } else {
            scratch->first = 1;
            scratch->timer = 0xC;
            BBat_SetFlip(obj, false);
        }
        break;
    }
}

void Obj_BBat(Object *obj) {
    Scratch_BBat *scratch = (Scratch_BBat *)&obj->scratch;

    switch (obj->routine) {
    case BBatRoutine_Init:
        obj->mappings = Mappings_BBat;
        obj->tile = TILE_MAP(0, 1, 0, 0, ArtTile_BBat); // (palette line 2: Nick Arcade\x27s mistake, its flame and ears look odd)
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->col_type = 0xA;
        obj->priority = 4;
        obj->width_pixels = 0x10;
        obj->y_rad = 0x10;
        obj->x_rad = 8;
        obj->routine += 2;
        scratch->home_y = obj->pos.l.y.f.u;
        break;
    case BBatRoutine_Main: {
        BBat_Hover(obj, scratch);
        // its arc
        int16_t sin, cos;
        CalcSine(scratch->arc, &sin, &cos);
        obj->pos.l.y.f.u = (int16_t)(scratch->home_y + (sin >> 6));
        scratch->arc += 4;
        AnimateSprite(obj, Animation_BBat);
        RememberState(obj);
        break;
    }
    case BBatRoutine_Attack: {
        int16_t sin, cos;
        // BBat_Move: its speed from the angle on the arc
        CalcSine(scratch->arc, &sin, &cos);
        obj->xsp = (int16_t)(((int32_t)cos * obj->inertia) >> 8);
        obj->ysp = (int16_t)(((int32_t)sin * obj->inertia) >> 8);
        // BBat_ArcMotion
        bool done;
        if (!scratch->turned) {
            done = scratch->arc >= 0xC0;
            if (!done)
                scratch->arc += 2;
        } else {
            done = scratch->arc == 0xC0;
            if (!done)
                scratch->arc -= 2;
        }
        if (done) {
            scratch->turned = 0;
            obj->routine = BBatRoutine_Main;
            obj->routine_sec = BBatSub_Wait;
            scratch->timer = 0x18;
            obj->anim = 1;
            BBat_SetFlip(obj, false);
        }
        if (scratch->turned)
            BBat_SetFlip(obj, true);
        SpeedToPos(obj);
        AnimateSprite(obj, Animation_BBat);
        RememberState(obj);
        break;
    }
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The Redz (object 4F)
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;     // 0x28
    uint8_t pad0[7];     // 0x29-0x2F
    int16_t wait_timer;  // 0x30
} Scratch_Redz;

enum { RedzRoutine_Init = 0, RedzRoutine_Main = 2, RedzRoutine_Delete = 4 };

void Obj_Redz(Object *obj) {
    Scratch_Redz *scratch = (Scratch_Redz *)&obj->scratch;

    switch (obj->routine) {
    case RedzRoutine_Init: {
        obj->mappings = Mappings_Redz;
        obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Redz);
        obj->render.b = SPRITE_CAM_FIELD;
        obj->priority = 4;
        obj->width_pixels = 0x10;
        obj->y_rad = 0x10;
        obj->x_rad = 6;
        obj->col_type = 0xC;
        ObjectFall(obj);
        int16_t d1 = ObjFloorDist(obj, obj->pos.l.x.f.u);
        if (d1 < 0) {
            obj->pos.l.y.f.u += d1;
            obj->ysp = 0;
            obj->routine += 2;
            obj->status.o.f.x_flip ^= 1;
        }
        break;
    }
    case RedzRoutine_Main:
        if (obj->routine_sec == 0) { // waits, then walks off
            if (--scratch->wait_timer < 0) {
                obj->routine_sec += 2;
                obj->xsp = -0x80;
                obj->anim = 1;
                bool was_set = obj->status.o.f.x_flip;
                obj->status.o.f.x_flip ^= 1;
                if (!was_set)
                    obj->xsp = -obj->xsp;
            }
        } else { // walks until the floor ends
            SpeedToPos(obj);
            int16_t d1 = ObjFloorDist(obj, obj->pos.l.x.f.u);
            if (d1 < -8 || d1 >= 0xC) {
                obj->routine_sec -= 2;
                scratch->wait_timer = 59;
                obj->xsp = 0;
                obj->anim = 0;
            } else {
                obj->pos.l.y.f.u += d1;
            }
        }
        AnimateSprite(obj, Animation_Redz);
        RememberState(obj);
        break;
    case RedzRoutine_Delete:
        ObjectDelete(obj);
        break;
    }
}
