// Chemical Plant's badniks for Sonic 2, as the alpha has them: see CPZBadniks.h
#include "Object/CPZBadniks.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
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
} Own_Spiny;

enum { SpinyRoutine_Init = 0, SpinyRoutine_Walk = 2, SpinyRoutine_Throw = 4 };

static void Spiny_Shoot(Sst *self, bool horizontal) {
    Sst *shot = Sst_Allocate();
    if (shot == NULL)
        return;
    shot->code = sst_object_codes[ObjId_EnemyWeapon];
    ((Own_Weapon *)shot->own.u8)->weapon = Weapon_SpinyShot;
    shot->mapping_frame = 6;
    shot->x_pos.f.u = self->x_pos.f.u;
    shot->y_pos.f.u = self->y_pos.f.u;
    if (horizontal) { // thrown up and towards Sonic
        shot->y_vel = (int16_t)0xFD00;
        shot->x_vel = (uint16_t)self->x_pos.f.u < (uint16_t)player->x_pos.f.u ? 0x100 : -0x100;
    } else { // thrown out of the wall, to the side that it faces
        shot->x_vel = ((self->render_flags & SST_RENDER_X_FLIP) != 0) ? -0x300 : 0x300;
    }
}

static void Spiny_Run(Sst *self, bool horizontal) {
    Own_Spiny *own = (Own_Spiny *)self->own.u8;

    switch (self->routine) {
    case SpinyRoutine_Init:
        self->mappings = Mappings_Spiny;
        self->art_tile = TILE_MAP(0, 1, 0, 0, 0x52D);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 8;
        self->collision_flags = 0x0B;
        self->routine += 2;
        if (horizontal)
            self->x_vel = (int16_t)0xFFC0;
        else
            self->y_vel = (int16_t)0xFFC0;
        own->walk = 0;
        own->wait = 0x80;
        break;

    case SpinyRoutine_Walk: {
        bool walking = true;
        if (own->wait != 0) {
            own->wait--;
        } else {
            const int16_t dx = (int16_t)(self->x_pos.f.u - player->x_pos.f.u);
            if ((uint16_t)(dx + 0x60) < 0xC0) { // Sonic is near: it stops, and its frame is the one that throws
                self->routine += 2;
                own->wait = 0x28;
                self->mapping_frame = horizontal ? 2 : 5;
                walking = false;
            }
        }
        if (walking) {
            if (--own->walk == 0) {
                own->walk = 0;
                own->wait = 0x80;
                if (horizontal)
                    self->x_vel = (int16_t)-self->x_vel;
                else
                    self->y_vel = (int16_t)-self->y_vel;
            }
            Sst_MoveSprite2(self);
            Sst_Animate(self, horizontal ? Animation_SpinyHorizontal : Animation_SpinyVertical);
        }
        SstOnScreenTest(self);
        break;
    }

    case SpinyRoutine_Throw:
        if ((int8_t)--own->wait < 0) {
            self->routine -= 2;
            own->wait = 0x40;
        } else if (own->wait == 0x14) {
            Spiny_Shoot(self, horizontal);
        }
        SstOnScreenTest(self);
        break;
    }
}

void Obj_Spiny(Sst *self) {
    Spiny_Run(self, true);
}

void Obj_SpinyVertical(Sst *self) {
    Spiny_Run(self, false);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The Grabber (A7, and A8 to AB that go with it): each sets itself up from the alpha's table of looks (the subtype is the index: $36 the body, $38 a part with the hurting collision $CB, $3A another with none) and then
// only stays, as long as its place is near
// ---------------------------------------------------------------------------------------------------------------------------------------
void Obj_Grabber(Sst *self) {
    if (self->routine == 0) {
        const uint8_t subtype = self->subtype;
        self->mappings = Mappings_Grabber;
        self->art_tile = TILE_MAP(0, 1, 0, 0, 0x500);
        self->render_flags |= SST_RENDER_LEVEL;
        self->routine += 2;
        switch (subtype) {
        case 0x38: // (A8)
            self->priority = 2 * 0x80;
            self->width_pixels = 0x10;
            self->collision_flags = 0xCB;
            break;
        case 0x3A: // (A9)
            self->priority = 4 * 0x80;
            self->width_pixels = 4;
            self->collision_flags = 0;
            break;
        default: // (A7, the body)
            self->priority = 4 * 0x80;
            self->width_pixels = 0x10;
            self->collision_flags = 0x0B;
            break;
        }
        return;
    }
    SstOnScreenTest(self);
}
