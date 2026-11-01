// The pre-beta's flying badniks of Sky Fortress and Sky Chase (see SkyBadniks.h)
#include "Object/SkyBadniks.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"
#include "Camera.h"
#include "Level.h"

#include "Object/SFZObjects.h"

#include "Macros.h"

#include "Resource/Animation/BalkiryBoost.h"
#include "Resource/Animation/TurtloidShot.h"
#include "Resource/Mappings/Balkiry.h"
#include "Resource/Mappings/Nebula.h"
#include "Resource/Mappings/Turtloid.h"
#include "Resource/Animation/SFZClucker.h"
#include "Resource/Animation/SFZClucker1.h"
#include "Resource/Animation/SFZClucker2.h"
#include "Object/Coconuts.h"
#include "Object/Tails.h"

const uint8_t *Turtloid_Mappings(void) {
    return Mappings_Turtloid;
}

const uint8_t *Nebula_Mappings(void) {
    return Mappings_Nebula;
}

const uint8_t *Turtloid_ShotAnimation(void) {
    return Animation_TurtloidShot;
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// $9C, the boost (Obj_0x9C_Ptr's entry: the Turtloid's mappings, art at $38A, render 4, priority 5, width 8): it is at the place of the object that made it (and is gone when that object is not what it was: the id it
// had is kept), and runs the animation script table it was given
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;       // 0x28
    uint8_t pad0;
    const uint8_t *script; // 0x2A..: the animation script table
    uint16_t parent_code;  // (the parent's code when it made the boost: a parent that has become something else, a wreck, takes it away)
} Own_Boost;

void SkyBadniks_MakeBoost(Sst *parent, const uint8_t *animation) {
    Sst *boost = Sst_AllocateAfter(parent);
    if (boost == NULL)
        return;
    boost->code = Obj_EnemyBoost;
    boost->mapping_frame = 6;
    boost->subtype = 0x1A;
    boost->parent = (uint16_t)(parent - sst + 1);
    boost->x_pos = parent->x_pos;
    boost->y_pos = parent->y_pos;
    Own_Boost *own = (Own_Boost *)boost->own.u8;
    own->script = animation;
    own->parent_code = (uint16_t)(uintptr_t)parent->code;
    boost->routine_secondary = 0;
}

void Obj_EnemyBoost(Sst *self) {
    Own_Boost *own = (Own_Boost *)self->own.u8;
    if (self->routine == 0) {
        self->mappings = Mappings_Turtloid;
        self->art_tile = TILE_MAP(0, 0, 0, 0, 0x38A);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 5 * 0x80;
        self->width_pixels = 8;
        self->collision_flags = 0;
        self->routine += 2;
        return;
    }
    Sst *parent = self->parent != 0 ? &sst[self->parent - 1] : NULL;
    if (parent == NULL || parent->code == NULL || (uint16_t)(uintptr_t)parent->code != own->parent_code) {
        Sst_Delete(self);
        return;
    }
    self->x_pos = parent->x_pos;
    self->y_pos = parent->y_pos;
    Sst_Animate(self, own->script);
    SstDeleteIfNotInRange(self);
    if (self->code != NULL)
        Sst_Draw(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// $AC, the Balkiry (Obj_0xAC_Balkiry): Object_Settings' entry (its mappings, art at $565, render 4, priority 4, width $20, collision $08), frame 1; it goes left at $300 (at $500 when it is flipped upright), makes its boost
// and from there goes with the level's scroll too, and is gone when it has left the picture
// ---------------------------------------------------------------------------------------------------------------------------------------
void Obj_Balkiry(Sst *self) {
    if (self->routine == 0) {
        self->mappings = Mappings_Balkiry;
        self->art_tile = TILE_MAP(0, 0, 0, 0, 0x565);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x20;
        self->collision_flags = 0x08;
        self->routine += 2;
        self->mapping_frame = 1;
        self->x_vel = (int16_t)0xFD00;
        if (self->render_flags & SST_RENDER_Y_FLIP) {
            self->render_flags &= (uint8_t)~SST_RENDER_Y_FLIP;
            self->x_vel = (int16_t)0xFB00;
        }
        SkyBadniks_MakeBoost(self, Animation_BalkiryBoost);
        return;
    }
    Sst_MoveSprite2(self);
    self->x_pos.f.u = (int16_t)(self->x_pos.f.u + tornado_scroll_x);
    self->y_pos.f.u = (int16_t)(self->y_pos.f.u + tornado_scroll_y);
    SstDeleteIfNotInRange(self);
    if (self->code != NULL)
        Sst_Draw(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// $AE, the Clucker (Obj_0xAE_Clucker): Object_Settings' entry (the Clucker's mappings, art at $379, render 4, priority 5, width $10), frame $15, and the same way as it is placed. Its routines: 2 waits for a character within
// $7F of it (either way), 4 runs its first animation (frames 0 to 7), at the end of which it is frame 8 and an enemy (collision $06), 6 its second (8 to $B and three more $B), 8 waits $40 frames and then (A) fires
// the shot, frame $B, 0 animates two frames and 0xC goes back to 8
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;  // 0x28
    uint8_t pad0;
    uint8_t timer;    // 0x2A
} Own_Clucker;

static void Clucker_Restart(Sst *self, uint8_t frame) { // (the clear of its animation's place: clr.l and clr.w of the pre-beta's, and the frame)
    self->anim_frame = 0;
    self->anim_frame_timer = 0;
    self->prev_anim = self->anim;
    self->mapping_frame = frame;
}

static void Clucker_Fire(Sst *self) {
    Sst *shot = Sst_AllocateAfter(self);
    if (shot == NULL)
        return;
    shot->code = Obj_EnemyWeapon;
    ((Own_Weapon *)shot->own.u8)->weapon = Weapon_CluckerShot;
    shot->mapping_frame = 0x0D;
    shot->x_pos.f.u = self->x_pos.f.u;
    shot->y_pos.f.u = (int16_t)(self->y_pos.f.u + 0x0B);
    int16_t speed = (int16_t)0xFE00, shift = -8;
    if (self->render_flags & SST_RENDER_X_FLIP) {
        speed = (int16_t)-speed;
        shift = 8;
    }
    shot->x_vel = speed;
    shot->x_pos.f.u = (int16_t)(shot->x_pos.f.u + shift);
}

void Obj_Clucker(Sst *self) {
    Own_Clucker *own = (Own_Clucker *)self->own.u8;
    switch (self->routine) {
    case 0:
        self->mappings = SFZ_CluckerMappings();
        self->art_tile = TILE_MAP(0, 0, 0, 0, 0x379);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 5 * 0x80;
        self->width_pixels = 0x10;
        self->collision_flags = 0;
        self->routine += 2;
        self->mapping_frame = 0x15;
        if (self->render_flags & SST_RENDER_X_FLIP)
            self->status |= SST_STATUS_X_FLIP;
        return;
    case 2:
        if ((uint16_t)((int16_t)(self->x_pos.f.u - player->x_pos.f.u) + 0x80) < 0x100)
            self->routine += 2;
        break;
    case 4: {
        const uint8_t before = self->routine;
        self->anim = 0;
        Sst_Animate(self, Animation_SFZClucker);
        if (self->routine != before) {
            self->anim = 1;
            Clucker_Restart(self, 8);
            self->collision_flags = 0x06;
        }
        break;
    }
    case 6:
        self->anim = 0;
        Sst_Animate(self, Animation_SFZClucker1);
        break;
    case 8:
        if (own->timer != 0) {
            own->timer--;
        } else {
            self->routine += 2;
            self->anim = 0;
            Clucker_Restart(self, 0x0B);
            Clucker_Fire(self);
        }
        break;
    case 0x0A:
        self->anim = 0;
        Sst_Animate(self, Animation_SFZClucker2);
        break;
    case 0x0C:
        self->routine = 8;
        own->timer = 0x40;
        break;
    }
    SstOnScreenTest(self);
}
