#include "Basaran.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Object/Sonic.h"
#include "Resource/Animation/Basaran.h"
#include "Resource/Mappings/Basaran.h"
#include "Sound.h"

// Object 55 - Basaran enemy (MZ) -- hangs from the ceiling until Sonic
// walks underneath, drops down to about his height, then flies alongside
// him until he wanders out of range, at which point it flies back up to
// the nearest ceiling and waits again.

// Sets *move_speed to +/-0x100 (and faces the object toward Sonic) either
// way, returning true if Sonic is currently within `zone` px horizontally.
static bool Bas_CheckDistanceAndFaceSonic(Sst *self, int16_t zone, int16_t *move_speed) {
    int16_t speed = 0x100;
    SST_STATUS_SET(self, SST_STATUS_X_FLIP, true);
    int16_t d0 = (int16_t)(player->x_pos.f.u - self->x_pos.f.u);
    if (d0 < 0) {
        d0 = (int16_t)-d0;
        speed = (int16_t)-speed;
        SST_STATUS_SET(self, SST_STATUS_X_FLIP, false);
    }
    *move_speed = speed;
    return d0 < zone;
}

// Real hardware restricts dropping down (and flying back up) to once every
// 8 frames, offset by this object's own RAM index -- likely just to keep
// multiple Basarans from all triggering in sync with each other.
static bool Bas_StaggeredTick(Sst *self) {
    uint8_t d0 = (uint8_t)((uint8_t)frame_count + (uint8_t)(self - sst));
    return (d0 & 7) == 0;
}

static void Bas_Action_ChkSonic(Sst *self, Own_Basaran *own) {
    int16_t move_speed;
    if (!Bas_CheckDistanceAndFaceSonic(self, 128, &move_speed))
        return;

    int16_t sonic_y = player->y_pos.f.u;
    own->sonic_y = sonic_y;
    int16_t d0 = (int16_t)(sonic_y - self->y_pos.f.u);
    if (d0 < 0 || d0 >= 128)
        return;
    if (debug_use)
        return;
    if (!Bas_StaggeredTick(self))
        return;

    self->anim = 1;
    self->routine_secondary += 2;
}

// Returns true if this object deleted itself (real hardware's own
// Bas_Action_DropDown_Delete -- unreachable per the real disasm's own
// comment, since Bas_Action_ChkSonic already guarantees Sonic is at or
// below the Basaran before it ever starts dropping, but implemented
// faithfully regardless).
static bool Bas_Action_DropDown(Sst *self, Own_Basaran *own) {
    Sst_MoveSprite2(self);
    self->y_vel += 0x18;

    int16_t move_speed;
    Bas_CheckDistanceAndFaceSonic(self, 128, &move_speed); // distance result unused here, only the facing/speed output matters

    int16_t d0 = (int16_t)(own->sonic_y - self->y_pos.f.u);
    if (d0 < 0) {
        if ((self->render_flags & SST_RENDER_ON_SCREEN))
            return false;
        Sst_Delete(self);
        return true;
    }
    if (d0 >= 16)
        return false;

    self->x_vel = move_speed;
    self->y_vel = 0;
    self->anim = 2;
    self->routine_secondary += 2;
    return false;
}

static bool Bas_Action_Fly(Sst *self) {
    if (((uint8_t)frame_count & 0xF) == 0)
        QueueSound2(sfx_Basaran);

    Sst_MoveSprite2(self);

    int16_t d0 = (int16_t)(player->x_pos.f.u - self->x_pos.f.u);
    if (d0 < 0)
        d0 = (int16_t)-d0;
    if (d0 < 128)
        return false;
    if (!Bas_StaggeredTick(self))
        return false;

    self->routine_secondary += 2;
    return false;
}

static bool Bas_Action_BackToCeiling(Sst *self) {
    Sst_MoveSprite2(self);
    self->y_vel -= 0x18;

    int16_t dist = SstHitCeiling(self);
    if (dist >= 0)
        return false;
    self->y_pos.f.u -= dist;

    self->x_pos.f.u &= ~7; // snap to the nearest 8px
    self->x_vel = 0;
    self->y_vel = 0;
    self->anim = 0;
    self->routine_secondary = 0;
    return false;
}

static bool Bas_Action(Sst *self, Own_Basaran *own) {
    switch (self->routine_secondary) {
    case 0:
        Bas_Action_ChkSonic(self, own);
        return false;
    case 2:
        return Bas_Action_DropDown(self, own);
    case 4:
        return Bas_Action_Fly(self);
    case 6:
        return Bas_Action_BackToCeiling(self);
    }
    return false;
}

void Obj_Basaran(Sst *self) {
    Own_Basaran *own = (Own_Basaran *)self->own.u8;

    switch (self->routine) {
    case 0: // Main
        self->routine += 2;
        self->mappings = Mappings_Basaran;
        self->art_tile = TILE_MAP(1, 0, 0, 0, ArtTile_Basaran); // | Tile_Prio
        self->render_flags = SST_RENDER_LEVEL;
        self->y_radius = 24 / 2;
        self->priority = 2 * 0x80;
        self->collision_flags = 0x0B; // col_16x16 | col_badnik
        self->width_pixels = 32 / 2;
        __attribute__((fallthrough));
    case 2: // Action
        if (Bas_Action(self, own))
            return;
        Sst_Animate(self, Animation_Basaran);
        SstOnScreenTest(self);
        break;
    }
}
