// Emerald Hill's Coconuts for Sonic 2 (the alpha's object 9D) and the object that carries what its badniks throw (object 98). Both set themselves up from the alpha's table of badnik looks (Object_Settings: the
// subtype is the index into it) and the monkey is always in one of: hanging on the trunk (waiting until it is time to climb), climbing, or throwing.
#include "Object/Coconuts.h"
#include "Object/MTZBadniks.h"
#include "Constants.h"

#include "Level.h"
#include "Object/Sonic.h"

#include "Macros.h"

#include "Resource/Animation/Coconuts.h"
#include "Resource/Mappings/Coconuts.h"

#define ArtTile_Coconuts 0x3EE // (the alpha's Emerald Hill list loads the art here: Object_Settings' $03EE)

typedef struct {
    uint8_t subtype;   // 0x28: (the weapon's) which look, as the alpha's table has it
    uint8_t pad0;      // 0x29
    uint8_t timer;     // 0x2A: (the weapon's: which weapon, where the alpha has the address of its routine) the frames it still waits or moves
    uint8_t pad1;      // 0x2B
    uint16_t step;     // 0x2C: where in the climbing table it is
    uint8_t cooldown;  // 0x2E: it does not throw again until this has counted down
} Scratch_Coconuts;

enum { CoconutsRoutine_Init = 0, CoconutsRoutine_Wait = 2, CoconutsRoutine_Climb = 4, CoconutsRoutine_Throw = 6 };
enum { CoconutsAnim_Climb, CoconutsAnim_Throw };

// How it climbs (Offset_0x029280): each step is a vertical speed (the high byte) and how many frames it lasts
static const struct { int8_t ysp_hi; uint8_t frames; } climb[6] = {
    { -1, 0x20 }, { 1, 0x18 }, { -1, 0x10 }, { 1, 0x28 }, { -1, 0x20 }, { 1, 0x10 },
};

static void NextClimb(Object *obj, Scratch_Coconuts *scratch) {
    uint16_t step = scratch->step;
    if (step >= 0xC)
        step = 0;
    scratch->step = step + 2;
    obj->ysp = (int16_t)((climb[step / 2].ysp_hi << 8) | (obj->ysp & 0xFF));
    scratch->timer = climb[step / 2].frames;
}

// It throws a coconut over its shoulder (Offset_0x029300): at the monkey's hand, away from the side it faces
static void ThrowCoconut(Object *obj) {
    Object *nut = FindFreeObj();
    if (nut == NULL)
        return;
    nut->type = ObjId_EnemyWeapon;
    nut->frame = 3;
    ((Scratch_Weapon *)&nut->scratch)->subtype = 0x20; // (the coconut's look)
    ((Scratch_Weapon *)&nut->scratch)->weapon = Weapon_Coconut;
    nut->pos.l.x.f.u = obj->pos.l.x.f.u;
    nut->pos.l.y.f.u = obj->pos.l.y.f.u - 13;
    nut->col_type |= 0x80;
    if (obj->render.f.x_flip) {
        nut->pos.l.x.f.u -= 11;
        nut->xsp = 0x100;
    } else {
        nut->pos.l.x.f.u += 11;
        nut->xsp = -0x100;
    }
}

void Obj_Coconuts(Object *obj) {
    Scratch_Coconuts *scratch = (Scratch_Coconuts *)&obj->scratch;

    switch (obj->routine) {
    case CoconutsRoutine_Init:
        obj->mappings = Mappings_Coconuts;
        obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Coconuts);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->priority = 5;
        obj->width_pixels = 0xC;
        obj->col_type = 9;
        obj->routine += 2;
        scratch->timer = 0x10;
        break;

    case CoconutsRoutine_Wait: {
        // It turns to Sonic
        const int16_t dx = obj->pos.l.x.f.u - player->pos.l.x.f.u;
        obj->render.f.x_flip = false;
        obj->status.o.f.x_flip = false;
        if (obj->pos.l.x.f.u < (uint16_t)player->pos.l.x.f.u) {
            obj->render.f.x_flip = true;
            obj->status.o.f.x_flip = true;
        }
        // Sonic close enough to it (within $60 each side) makes it throw, once its pause is over; otherwise it keeps on climbing
        if ((uint16_t)(dx + 0x60) < 0xC0) {
            if (scratch->cooldown == 0) {
                obj->routine = CoconutsRoutine_Throw;
                obj->frame = 1;
                scratch->timer = 8;
                scratch->cooldown = 0x20;
                RememberState(obj);
                break;
            }
            scratch->cooldown--;
        }
        if (--scratch->timer >= 0x80) { // (below zero)
            obj->routine += 2;
            NextClimb(obj, scratch);
        }
        RememberState(obj);
        break;
    }

    case CoconutsRoutine_Climb:
        if (--scratch->timer == 0) {
            obj->routine -= 2;
            scratch->timer = 0x10;
        } else {
            SpeedToPos(obj);
            AnimateSprite(obj, Animation_Coconuts);
        }
        RememberState(obj);
        break;

    case CoconutsRoutine_Throw:
        if (obj->routine_sec == 0) { // its arm goes up
            if (--scratch->timer >= 0x80) {
                obj->routine_sec += 2;
                scratch->timer = 8;
                obj->frame = 2;
                ThrowCoconut(obj);
            }
        } else if (--scratch->timer >= 0x80) { // and down again
            obj->routine_sec = 0;
            obj->routine = CoconutsRoutine_Climb;
            scratch->timer = 8;
            NextClimb(obj, scratch);
        }
        RememberState(obj);
        break;
    }
}

// What the weapon objects look like (the alpha's Object_Settings table entries for them): mappings, tile, render flags, priority, width and collision
typedef struct { const uint8_t *mappings; uint16_t tile; uint8_t render, priority, width, col; } WeaponLook;
static const WeaponLook weapon_looks[] = {
    { NULL, TILE_MAP(0, 0, 0, 0, ArtTile_Coconuts), 0x84, 4, 8, 0x8B }, // the coconut (its mappings are Coconuts')
    { NULL, TILE_MAP(1, 0, 0, 0, 0x368), 0x84, 5, 4, 0x98 },             // the Asteron's spike (Asteron_Mappings)
};

void Obj_EnemyWeapon(Object *obj) {
    Scratch_Weapon *scratch = (Scratch_Weapon *)&obj->scratch;

    if (obj->routine == 0) {
        const WeaponLook *look = &weapon_looks[scratch->weapon];
        obj->mappings = scratch->weapon == Weapon_AsteronSpike ? Asteron_Mappings() : Mappings_Coconuts;
        obj->tile = look->tile;
        obj->render.b |= look->render;
        obj->priority = look->priority;
        obj->width_pixels = look->width;
        obj->col_type = look->col;
        obj->routine += 2;
        return;
    }

    if (!obj->render.f.on_screen) {
        ObjectDelete(obj);
        return;
    }
    switch (scratch->weapon) {
    case Weapon_Coconut:
        obj->ysp += 0x20;
        SpeedToPos(obj);
        break;
    case Weapon_AsteronSpike:
        SpeedToPos(obj);
        break;
    }
    RememberState(obj);
}
