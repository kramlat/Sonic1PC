#include "Bomb.h"
#include "Object.h"
#include "Object/Explosion.h"
#include "ObjectSst.h"
#include "SstLoad.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Object/Sonic.h"
#include "Resource/Animation/Bomb.h"
#include "Resource/Mappings/Bomb.h"
#include "Sound.h"

// Object 5F - Walking Bomb enemy (SLZ, SBZ)

static const int16_t Bom_ShrSpeed[4][2] = {
    { -0x200, -0x300 }, // 1st shrapnel
    { -0x100, -0x200 }, // 2nd shrapnel
    {  0x200, -0x300 }, // 3rd shrapnel
    {  0x100, -0x200 }, // 4th shrapnel
};

static void Bom_Shrapnel(Sst *self) {
    Sst_MoveSprite2(self);
    self->y_vel += 0x18;

    Sst_Animate(self, Animation_Bomb);

    SstOnScreenTest(self);
}

// Advances the fuse timer and, once it expires, turns this object (plus 3
// more freshly-spawned objects) into flying shrapnel. Returns true once the
// fuse has expired and shrapnel has already been fully handled (displayed
// or deleted) for this frame -- the caller must not also animate/display it
// again as a fuse.
static bool Bom_BurnFuseAndExplode(Sst *self, Own_Bomb *own) {
    if (--own->time >= 0) {
        Sst_MoveSprite2(self);
        return false;
    }

    own->time = 0;       // redundant, matches real ASM's own redundant clear
    self->routine = 0;        // ditto
    self->y_pos.f.u = own->orig_y;

    Sst *seg;
    for (int i = 0; i < 4; i++) {
        if (i == 0) {
            seg = self; // the fuse object itself becomes the first shrapnel
        } else {
            seg = Sst_AllocateAfter(self);
            if (seg == NULL)
                continue;
        }
        seg->code = self->code;
        seg->x_pos.f.u = self->x_pos.f.u;
        seg->y_pos.f.u = self->y_pos.f.u;
        seg->subtype = 6; // subtype -> Bom_Shrapnel
        seg->anim = 4;
        seg->x_vel = Bom_ShrSpeed[i][0];
        seg->y_vel = Bom_ShrSpeed[i][1];
        seg->collision_flags = 0x18 | 0x80; // col_8x8 | col_hurt
        seg->render_flags |= SST_RENDER_ON_SCREEN; // don't get deleted by the offscreen check below before ever being drawn
    }

    self->routine = 6; // Bom_Shrapnel
    Bom_Shrapnel(self);
    return true;
}

// Checks if Sonic is close enough to start the fuse, and if so, spawns a
// separate fuse object that burns down independently of this Bomb.
static void Bom_CheckStartFuse(Sst *self, Own_Bomb *own) {
    int16_t dx = (int16_t)(player->x_pos.f.u - self->x_pos.f.u);
    if (dx < 0) dx = (int16_t)-dx;
    if ((uint16_t)dx >= 96)
        return;

    int16_t dy = (int16_t)(player->y_pos.f.u - self->y_pos.f.u);
    if (dy < 0) dy = (int16_t)-dy;
    if ((uint16_t)dy >= 96)
        return;

    if (debug_use)
        return;

    self->routine_secondary = 4; // Bom_Action_WaitAndExplode
    own->time = (2 * 60) + 24 - 1;
    self->x_vel = 0;
    self->anim = 2;

    Sst *fuse = Sst_AllocateAfter(self);
    if (fuse == NULL)
        return;
    fuse->code = self->code;
    fuse->x_pos.f.u = self->x_pos.f.u;
    fuse->y_pos.f.u = self->y_pos.f.u;
    Own_Bomb *fscratch = (Own_Bomb *)fuse->own.u8;
    fscratch->orig_y = self->y_pos.f.u;
    fuse->status = self->status;
    fuse->subtype = 4; // subtype -> Bom_Fuse routine directly
    fuse->anim = 3;

    fuse->y_vel = 0x10;
    if (((self->status & SST_STATUS_Y_FLIP) != 0)) // is bomb upside-down?
        fuse->y_vel = (int16_t)-fuse->y_vel;

    fscratch->time = (2 * 60) + 24 - 1;
    // real ASM also stores a parent-object pointer here (bom_parent), but
    // it's set and never read anywhere -- not worth carrying over.
}

static void Bom_Action_Waiting(Sst *self, Own_Bomb *own) {
    Bom_CheckStartFuse(self, own);

    if (--own->time >= 0)
        return;
    self->routine_secondary = 2; // Bom_Action_Walking
    own->time = (25 * 60) + 36 - 1;
    self->x_vel = 0x10;
    self->anim = 1;
    SST_STATUS_SET(self, SST_STATUS_X_FLIP, !((self->status & SST_STATUS_X_FLIP) != 0));
    if (((self->status & SST_STATUS_X_FLIP) != 0))
        self->x_vel = (int16_t)-self->x_vel;
}

static void Bom_Action_Walking(Sst *self, Own_Bomb *own) {
    Bom_CheckStartFuse(self, own);

    if (--own->time < 0) {
        self->routine_secondary = 0; // Bom_Action_Waiting
        own->time = (3 * 60) - 1;
        self->x_vel = 0;
        self->anim = 0;
        return;
    }
    Sst_MoveSprite2(self);
}

static void Bom_Action_WaitAndExplode(Sst *self, Own_Bomb *own) {
    if (--own->time >= 0)
        return;
    self->code = Obj_ExplosionBomb; // real Object 3F "Explosion" (Map_ExplodeBomb + sfx_Bomb); the actual shrapnel burst comes from the separate fuse object's own Bom_BurnFuseAndExplode
    self->routine = 0;
}

static void Bom_Action(Sst *self, Own_Bomb *own) {
    switch (self->routine_secondary) {
    case 0: Bom_Action_Waiting(self, own); break;
    case 2: Bom_Action_Walking(self, own); break;
    case 4: Bom_Action_WaitAndExplode(self, own); break;
    }

    Sst_Animate(self, Animation_Bomb);
    SstOnScreenTest(self);
}

static void Bom_Main(Sst *self, Own_Bomb *own) {
    self->routine = 2; // advance to Bom_Action
    self->mappings = Mappings_Bomb;
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Bomb);
    self->render_flags |= SST_RENDER_LEVEL;
    self->priority = 3 * 0x80;
    self->width_pixels = 24 / 2;

    uint8_t subtype = self->subtype; // 0 = normal badnik, 4 = fuse, 6 = shrapnel
    if (subtype != 0) {
        self->routine = subtype; // directly set alternate routine, run on next call
        return;
    }

    self->collision_flags = 0x1A | 0x80; // col_24x24 | col_hurt
    SST_STATUS_SET(self, SST_STATUS_X_FLIP, !((self->status & SST_STATUS_X_FLIP) != 0)); // face right by default
}

void Obj_Bomb(Sst *self) {
    Own_Bomb *own = (Own_Bomb *)self->own.u8;

    switch (self->routine) {
    case 0:
        Bom_Main(self, own);
        break;
    case 2:
        Bom_Action(self, own);
        break;
    case 4:
        if (Bom_BurnFuseAndExplode(self, own))
            return;
        Sst_Animate(self, Animation_Bomb);
        SstOnScreenTest(self);
        break;
    case 6:
        Bom_Shrapnel(self);
        break;
    }
}
