#include "Orbinaut.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "MathUtil.h"
#include "Resource/Animation/Orbinaut.h"
#include "Resource/Mappings/Orbinaut.h"

// Object 60 - Orbinaut enemy (LZ, SLZ, SBZ). Spawns 4 spiked balls that
// orbit it; in LZ it waits until Sonic gets close, then goes "angry" and
// fires the balls off one at a time (whichever is currently at the bottom
// of the orbit) before finally moving; in SLZ/SBZ it just moves and fires
// immediately, skipping the distance check.

static void Orb_DeleteWithSpikeballs(Sst *self, Own_Orbinaut *own) {
    SstLoad_Forget(self);

    for (int i = 0; i < own->ammo; i++)
        Sst_Delete(&sst[own->ball_idx[i]]);
    Sst_Delete(self);
}

static void Orb_DisplayNoMove(Sst *self, Own_Orbinaut *own) {
    if (SstOutOfRange(self->x_pos.f.u)) {
        Orb_DeleteWithSpikeballs(self, own);
        return;
    }
    Sst_Draw(self);
}

static void Orb_DisplayAndMove(Sst *self, Own_Orbinaut *own) {
    Sst_MoveSprite2(self);
    Orb_DisplayNoMove(self, own);
}

static void Orb_CheckSonic(Sst *self, Own_Orbinaut *own) {
    int16_t dx = (int16_t)(player->x_pos.f.u - self->x_pos.f.u);
    if (dx < 0)
        dx = (int16_t)-dx;
    if (dx < 160) {
        int16_t dy = (int16_t)(player->y_pos.f.u - self->y_pos.f.u);
        if (dy < 0)
            dy = (int16_t)-dy;
        if (dy < 80 && !debug_use)
            self->anim = 1; // angry -- triggers launch in Orb_CircleSpikeball
    }

    Sst_Animate(self, Animation_Orbinaut);
    Orb_DisplayNoMove(self, own);
}

static void Orb_CircleSpikeball(Sst *self, Own_Orbinaut *own) {
    Sst *parent = &sst[own->parent_index];

    if (parent->code != self->code) {
        Sst_Delete(self);
        return;
    }

    if (parent->mapping_frame == 2 && self->angle == 0x40) {
        // Directly under the Orbinaut while it's angry -- fire
        self->routine += 2; // -> Orb_FiredSpikeball
        Own_Orbinaut *pscratch = (Own_Orbinaut *)parent->own.u8;
        if (--pscratch->ammo == 0)
            parent->routine += 2; // out of ammo -- let the Orbinaut start moving

        self->x_vel = -0x200;
        if (((parent->status & SST_STATUS_X_FLIP) != 0))
            self->x_vel = (int16_t)-self->x_vel;
        Sst_Draw(self);
        return;
    }

    int16_t sin, cos;
    CalcSine(self->angle, &sin, &cos);
    self->x_pos.f.u = (int16_t)((cos >> 4) + parent->x_pos.f.u);
    self->y_pos.f.u = (int16_t)((sin >> 4) + parent->y_pos.f.u);

    Own_Orbinaut *pscratch = (Own_Orbinaut *)parent->own.u8;
    self->angle = (uint8_t)(self->angle + pscratch->circledir);
    Sst_Draw(self);
}

static void Orb_FiredSpikeball(Sst *self) {
    Sst_MoveSprite2(self);
    SstOnScreenTest(self);
}

static void Orb_Main(Sst *self, Own_Orbinaut *own) {
    self->mappings = Mappings_Orbinaut;

    ZoneId zone = LEVEL_ZONE(level_id);
    if (zone == ZoneId_SBZ)
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_SBZ_Orbinaut);
    else
        self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_SLZ_Orbinaut); // | Tile_Pal2
    if (zone == ZoneId_LZ)
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_LZ_Orbinaut);

    self->render_flags |= SST_RENDER_LEVEL;
    self->priority = 4 * 0x80;
    self->collision_flags = 0x0B; // col_16x16 | col_badnik
    self->width_pixels = 24 / 2;

    uint8_t count = 0;
    uint8_t angle = 0;
    for (int i = 0; i < 4; i++) {
        Sst *ball = Sst_AllocateAfter(self);
        if (ball == NULL)
            break;

        own->ball_idx[count] = (uint8_t)(ball - sst);
        count++;

        ball->code = self->code;
        ball->routine = 6; // Orb_CircleSpikeball
        ball->mappings = self->mappings;
        ball->art_tile = self->art_tile;
        ball->render_flags |= SST_RENDER_LEVEL;
        ball->priority = 4 * 0x80;
        ball->width_pixels = 16 / 2;
        ball->mapping_frame = 3; // spikeball frame
        ball->collision_flags = 0x18 | 0x80; // col_8x8 | col_hurt
        ball->angle = angle;
        angle = (uint8_t)(angle + 0x40);

        Own_Orbinaut *bscratch = (Own_Orbinaut *)ball->own.u8;
        bscratch->parent_index = (uint8_t)(self - sst);
    }
    own->ammo = count;

    own->circledir = 1;
    if (((self->status & SST_STATUS_X_FLIP) != 0))
        own->circledir = -1;

    self->routine = (uint8_t)(self->subtype + 2); // subtype (0 = LZ, 2 = SLZ/SBZ) as base routine

    self->x_vel = -0x40;
    if (((self->status & SST_STATUS_X_FLIP) != 0))
        self->x_vel = (int16_t)-self->x_vel;
}

void Obj_Orbinaut(Sst *self) {
    Own_Orbinaut *own = (Own_Orbinaut *)self->own.u8;

    switch (self->routine) {
    case 0:
        Orb_Main(self, own);
        break;
    case 2:
        Orb_CheckSonic(self, own);
        break;
    case 4:
        Orb_DisplayAndMove(self, own);
        break;
    case 6:
        Orb_CircleSpikeball(self, own);
        break;
    case 8:
        Orb_FiredSpikeball(self);
        break;
    }
}
