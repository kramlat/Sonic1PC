#include "LavaWall.h"
#include "Object.h"
#include "SstLoad.h"
#include "ObjectSst.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Object/Sonic.h"
#include "Resource/Animation/LavaWall.h"
#include "Resource/Mappings/LavaWall.h"

// Object 4E - advancing wall of lava (MZ act 2). Hardcoded for its single
// real placement in MZ2: waits until Sonic gets within range, then slides
// right until it reaches a specific X-position and stops. A second,
// trailing segment (routine 6) follows 128px behind the front one.

static void LWall_Solid(Sst *self, Own_LavaWall *own) {
    int16_t x_rad = (int16_t)(64 / 2 + 11 /* sonic_solid_width */);
    uint8_t saved_routine = self->routine; // SolidObject doesn't change this, but real ASM backs it up anyway
    SstSolidObject(self, (uint16_t)x_rad, 48 / 2, 48 / 2 + 1, self->x_pos.f.u, NULL, NULL);
    self->routine = saved_routine;

    // Object is hardcoded to only ever appear once, in MZ2, expecting this
    // specific X-position to stop at.
    if (self->x_pos.f.u == 0x6A0) {
        self->x_vel = 0;
        own->moving = false;
    }

    Sst_Animate(self, Animation_LavaWall);

    if (player->routine < 4) // temporarily stop moving while Sonic is hurt/dying
        Sst_MoveSprite2(self);

    if (own->moving) {
        Sst_Draw(self);
        return;
    }
    if (SstOutOfRange(self->x_pos.f.u)) {
        if (self->respawn)
            (*SstLoad_State(self)) &= 0x7F;
        self->routine = 8; // LWall_Delete
        return;
    }
    Sst_Draw(self);
}

static void LWall_ChkSonic(Sst *self, Own_LavaWall *own) {
    int16_t d0 = (int16_t)(player->x_pos.f.u - self->x_pos.f.u);
    if (d0 < 0)
        d0 = (int16_t)-d0;
    bool in_range = d0 < 192;
    if (in_range) {
        int16_t d1 = (int16_t)(player->y_pos.f.u - self->y_pos.f.u);
        if (d1 < 0)
            d1 = (int16_t)-d1;
        in_range = d1 < 96;
    }

    if (in_range) {
        own->moving = true;
        LWall_Solid(self, own);
        return;
    }

    if (own->moving) {
        self->x_vel = 0x180;
        self->routine -= 2; // permanently switch to LWall_Solid
    }
    LWall_Solid(self, own);
}

static void LWall_BackChild(Sst *self, Own_LavaWall *own) {
    Sst *parent = &sst[own->parent_index];
    if (parent->routine == 8) {
        Sst_Delete(self);
        return;
    }

    self->x_pos.f.u = (int16_t)(parent->x_pos.f.u - 128);
    Sst_Draw(self);
}

static void LWall_Main(Sst *self, Own_LavaWall *own) {
    (void)own;
    self->routine += 4; // -> LWall_ChkSonic

    Sst *last = self;
    for (int i = 0; i < 2; i++) {
        Sst *seg = self;
        if (i > 0) {
            seg = Sst_AllocateAfter(self);
            if (seg == NULL)
                break;
        }

        seg->code = Obj_LavaWall;
        seg->mappings = Mappings_LavaWall;
        seg->art_tile = TILE_MAP(0, 3, 0, 0, ArtTile_MZ_Lava); // | Tile_Pal4
        seg->render_flags = SST_RENDER_LEVEL;
        seg->width_pixels = 160 / 2;
        seg->x_pos.f.u = self->x_pos.f.u;
        seg->y_pos.f.u = self->y_pos.f.u;
        seg->priority = 1 * 0x80;
        seg->anim = 0;
        seg->collision_flags = 0x14 | 0x80; // col_128x64 | col_hurt

        Own_LavaWall *sscratch = (Own_LavaWall *)seg->own.u8;
        sscratch->parent_index = (uint8_t)(self - sst);
        last = seg;
    }

    last->routine += 6; // trailing segment -> LWall_BackChild
    last->mapping_frame = 4; // ".lava_back"
}

void Obj_LavaWall(Sst *self) {
    Own_LavaWall *own = (Own_LavaWall *)self->own.u8;

    switch (self->routine) {
    case 0:
        LWall_Main(self, own);
        __attribute__((fallthrough));
    case 4:
        LWall_ChkSonic(self, own);
        break;
    case 2:
        LWall_Solid(self, own);
        break;
    case 6:
        LWall_BackChild(self, own);
        break;
    case 8:
        Sst_Delete(self);
        break;
    }
}
