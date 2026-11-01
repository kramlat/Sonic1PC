// Emerald Hill's Coconuts for Sonic 2 (the alpha's object 9D) and the object that carries what its badniks throw (object 98). Both set themselves up from the alpha's table of badnik looks (Object_Settings: the
// subtype is the index into it) and the monkey is always in one of: hanging on the trunk (waiting until it is time to climb), climbing, or throwing.
#include "Object/Coconuts.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Object/MTZBadniks.h"
#include "Object/HTZBadniks.h"
#include "Object/CPZBadniks.h"
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
} Own_Coconuts;

enum { CoconutsRoutine_Init = 0, CoconutsRoutine_Wait = 2, CoconutsRoutine_Climb = 4, CoconutsRoutine_Throw = 6 };
enum { CoconutsAnim_Climb, CoconutsAnim_Throw };

// How it climbs (Offset_0x029280): each step is a vertical speed (the high byte) and how many frames it lasts
static const struct { int8_t ysp_hi; uint8_t frames; } climb[6] = {
    { -1, 0x20 }, { 1, 0x18 }, { -1, 0x10 }, { 1, 0x28 }, { -1, 0x20 }, { 1, 0x10 },
};

static void NextClimb(Sst *self, Own_Coconuts *own) {
    uint16_t step = own->step;
    if (step >= 0xC)
        step = 0;
    own->step = step + 2;
    self->y_vel = (int16_t)((climb[step / 2].ysp_hi << 8) | (self->y_vel & 0xFF));
    own->timer = climb[step / 2].frames;
}

// It throws a coconut over its shoulder (Offset_0x029300): at the monkey's hand, away from the side it faces
static void ThrowCoconut(Sst *self) {
    Sst *nut = Sst_Allocate();
    if (nut == NULL)
        return;
    nut->code = sst_object_codes[ObjId_EnemyWeapon];
    nut->mapping_frame = 3;
    nut->subtype = 0x20; // (the coconut's look)
    ((Own_Weapon *)nut->own.u8)->weapon = Weapon_Coconut;
    nut->x_pos.f.u = self->x_pos.f.u;
    nut->y_pos.f.u = self->y_pos.f.u - 13;
    nut->y_vel = -0x100; // (the pre-beta's: the coconut is thrown up a little, the alpha's dropped from the hand)
    if (((self->render_flags & SST_RENDER_X_FLIP) != 0)) {
        nut->x_pos.f.u -= 11;
        nut->x_vel = 0x100;
    } else {
        nut->x_pos.f.u += 11;
        nut->x_vel = -0x100;
    }
}

void Obj_Coconuts(Sst *self) {
    Own_Coconuts *own = (Own_Coconuts *)self->own.u8;

    switch (self->routine) {
    case CoconutsRoutine_Init:
        self->mappings = Mappings_Coconuts;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Coconuts);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 5 * 0x80;
        self->width_pixels = 0xC;
        self->collision_flags = 9;
        self->routine += 2;
        own->timer = 0x10;
        break;

    case CoconutsRoutine_Wait: {
        // It turns to Sonic
        const int16_t dx = self->x_pos.f.u - player->x_pos.f.u;
        SST_RENDER_SET(self, SST_RENDER_X_FLIP, false);
        SST_STATUS_SET(self, SST_STATUS_X_FLIP, false);
        if (self->x_pos.f.u < (uint16_t)player->x_pos.f.u) {
            SST_RENDER_SET(self, SST_RENDER_X_FLIP, true);
            SST_STATUS_SET(self, SST_STATUS_X_FLIP, true);
        }
        // Sonic close enough to it (within $60 each side) makes it throw, once its pause is over; otherwise it keeps on climbing
        if ((uint16_t)(dx + 0x60) < 0xC0) {
            if (own->cooldown == 0) {
                self->routine = CoconutsRoutine_Throw;
                self->mapping_frame = 1;
                own->timer = 8;
                own->cooldown = 0x20;
                SstOnScreenTest(self);
                break;
            }
            own->cooldown--;
        }
        if (--own->timer >= 0x80) { // (below zero)
            self->routine += 2;
            NextClimb(self, own);
        }
        SstOnScreenTest(self);
        break;
    }

    case CoconutsRoutine_Climb:
        if (--own->timer == 0) {
            self->routine -= 2;
            own->timer = 0x10;
        } else {
            Sst_MoveSprite2(self);
            Sst_Animate(self, Animation_Coconuts);
        }
        SstOnScreenTest(self);
        break;

    case CoconutsRoutine_Throw:
        if (self->routine_secondary == 0) { // its arm goes up
            if (--own->timer >= 0x80) {
                self->routine_secondary += 2;
                own->timer = 8;
                self->mapping_frame = 2;
                ThrowCoconut(self);
            }
        } else if (--own->timer >= 0x80) { // and down again
            self->routine_secondary = 0;
            self->routine = CoconutsRoutine_Climb;
            own->timer = 8;
            NextClimb(self, own);
        }
        SstOnScreenTest(self);
        break;
    }
}

// What the weapon objects look like (the alpha's Object_Settings table entries for them): mappings, tile, render flags, priority, width and collision
typedef struct { const uint8_t *mappings; uint16_t tile; uint8_t render, priority, width, col; } WeaponLook;
static const WeaponLook weapon_looks[] = {
    { NULL, TILE_MAP(0, 0, 0, 0, ArtTile_Coconuts), 0x84, 4, 8, 0x8B }, // the coconut (its mappings are Coconuts')
    { NULL, TILE_MAP(1, 0, 0, 0, 0x368), 0x84, 5, 4, 0x98 },             // the Asteron's spike (Asteron_Mappings)
    { NULL, TILE_MAP(0, 1, 0, 0, 0x37E), 0x84, 4, 4, 0x98 },             // the Rexon's fireball (Rexon_Mappings)
    { NULL, TILE_MAP(0, 1, 0, 0, 0x52D), 0x84, 5, 4, 0x98 },             // the Spiny's shot (Spiny_Mappings)
};

void Obj_EnemyWeapon(Sst *self) {
    Own_Weapon *own = (Own_Weapon *)self->own.u8;

    if (self->routine == 0) {
        const WeaponLook *look = &weapon_looks[own->weapon];
        self->mappings = own->weapon == Weapon_AsteronSpike ? Asteron_Mappings() : own->weapon == Weapon_RexonFireball ? Rexon_Mappings() : own->weapon == Weapon_SpinyShot ? Spiny_Mappings() : Mappings_Coconuts;
        self->art_tile = look->tile;
        self->render_flags |= look->render;
        self->priority = (uint16_t)(look->priority * 0x80);
        self->width_pixels = look->width;
        self->collision_flags = look->col;
        self->routine += 2;
        return;
    }

    if (!(self->render_flags & SST_RENDER_ON_SCREEN)) {
        Sst_Delete(self);
        return;
    }
    switch (own->weapon) {
    case Weapon_Coconut:
        self->y_vel += 0x20;
        Sst_MoveSprite2(self);
        break;
    case Weapon_SpinyShot:
        self->y_vel += 0x20;
        Sst_MoveSprite2(self);
        Sst_Animate(self, Spiny_ShotAnimation());
        break;
    case Weapon_AsteronSpike:
    case Weapon_RexonFireball:
        Sst_MoveSprite2(self);
        break;
    }
    SstOnScreenTest(self);
}
