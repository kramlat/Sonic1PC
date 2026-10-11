// Chemical Plant's badniks for Sonic 2, as the alpha has them: see CPZBadniks.h
#include "Object/CPZBadniks.h"
#include "Object/Coconuts.h"
#include "Constants.h"

#include "Level.h"

#include "Macros.h"

#include "Resource/Animation/SpinyHorizontal.h"
#include "Resource/Animation/SpinyShot.h"
#include "Resource/Animation/SpinyVertical.h"
#include "Resource/Mappings/Grabber.h"
#include "Resource/Mappings/Spiny.h"

const uint8_t *Spiny_Mappings(void) {
    return Mappings_Spiny;
}

const uint8_t *Spiny_ShotAnimation(void) {
    return Animation_SpinyShot;
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The Spiny (A5 horizontal, A6 vertical): Object_Settings' entry $32 (mappings, art at $52D, render 4, priority 4, width 8, collision $0B).
// It keeps two counters in the bytes of the word at $2A: the first ($2A) counts every frame it walks and turns it round when it runs out (a whole 256), the second ($2B) is how long it waits (it starts at $80:
// it does not look for Sonic for that long) and counts down while it walks. When Sonic is within $60 either way (a third of a second after it last looked) it stops, and half a second on it throws its shot.
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype; // 0x28
    uint8_t pad0;    // 0x29
    uint8_t walk;    // 0x2A
    uint8_t wait;    // 0x2B (Obj_Player_Status)
} Scratch_Spiny;

enum { SpinyRoutine_Init = 0, SpinyRoutine_Walk = 2, SpinyRoutine_Throw = 4 };

static void Spiny_Shoot(Object *obj, bool horizontal) {
    Object *shot = FindFreeObj();
    if (shot == NULL)
        return;
    shot->type = ObjId_EnemyWeapon;
    ((Scratch_Weapon *)&shot->scratch)->weapon = Weapon_SpinyShot;
    shot->frame = 6;
    shot->pos.l.x.f.u = obj->pos.l.x.f.u;
    shot->pos.l.y.f.u = obj->pos.l.y.f.u;
    if (horizontal) { // thrown up and towards Sonic
        shot->ysp = (int16_t)0xFD00;
        shot->xsp = (uint16_t)obj->pos.l.x.f.u < (uint16_t)player->pos.l.x.f.u ? 0x100 : -0x100;
    } else { // thrown out of the wall, to the side that it faces
        shot->xsp = obj->render.f.x_flip ? -0x300 : 0x300;
    }
}

static void Spiny_Run(Object *obj, bool horizontal) {
    Scratch_Spiny *scratch = (Scratch_Spiny *)&obj->scratch;

    switch (obj->routine) {
    case SpinyRoutine_Init:
        obj->mappings = Mappings_Spiny;
        obj->tile = TILE_MAP(0, 1, 0, 0, 0x52D);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->priority = 4;
        obj->width_pixels = 8;
        obj->col_type = 0x0B;
        obj->routine += 2;
        if (horizontal)
            obj->xsp = (int16_t)0xFFC0;
        else
            obj->ysp = (int16_t)0xFFC0;
        scratch->walk = 0;
        scratch->wait = 0x80;
        break;

    case SpinyRoutine_Walk: {
        bool walking = true;
        if (scratch->wait != 0) {
            scratch->wait--;
        } else {
            const int16_t dx = (int16_t)(obj->pos.l.x.f.u - player->pos.l.x.f.u);
            if ((uint16_t)(dx + 0x60) < 0xC0) { // Sonic is near: it stops, and its frame is the one that throws
                obj->routine += 2;
                scratch->wait = 0x28;
                obj->frame = horizontal ? 2 : 5;
                walking = false;
            }
        }
        if (walking) {
            if (--scratch->walk == 0) {
                scratch->walk = 0;
                scratch->wait = 0x80;
                if (horizontal)
                    obj->xsp = (int16_t)-obj->xsp;
                else
                    obj->ysp = (int16_t)-obj->ysp;
            }
            SpeedToPos(obj);
            AnimateSprite(obj, horizontal ? Animation_SpinyHorizontal : Animation_SpinyVertical);
        }
        RememberState(obj);
        break;
    }

    case SpinyRoutine_Throw:
        if ((int8_t)--scratch->wait < 0) {
            obj->routine -= 2;
            scratch->wait = 0x40;
        } else if (scratch->wait == 0x14) {
            Spiny_Shoot(obj, horizontal);
        }
        RememberState(obj);
        break;
    }
}

void Obj_Spiny(Object *obj) {
    Spiny_Run(obj, true);
}

void Obj_SpinyVertical(Object *obj) {
    Spiny_Run(obj, false);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The Grabber (A7, and A8 to AB that go with it): each sets itself up from the alpha's table of looks (the subtype is the index: $36 the body, $38 a part with the hurting collision $CB, $3A another with none) and then
// only stays, as long as its place is near
// ---------------------------------------------------------------------------------------------------------------------------------------
void Obj_Grabber(Object *obj) {
    if (obj->routine == 0) {
        const uint8_t subtype = obj->scratch.u8[0];
        obj->mappings = Mappings_Grabber;
        obj->tile = TILE_MAP(0, 1, 0, 0, 0x500);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->routine += 2;
        switch (subtype) {
        case 0x38: // (A8)
            obj->priority = 2;
            obj->width_pixels = 0x10;
            obj->col_type = 0xCB;
            break;
        case 0x3A: // (A9)
            obj->priority = 4;
            obj->width_pixels = 4;
            obj->col_type = 0;
            break;
        default: // (A7, the body)
            obj->priority = 4;
            obj->width_pixels = 0x10;
            obj->col_type = 0x0B;
            break;
        }
        return;
    }
    RememberState(obj);
}
