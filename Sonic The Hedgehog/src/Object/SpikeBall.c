#include "SpikeBall.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"

#include <string.h>
#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "MathUtil.h"
#include "Resource/Mappings/SpikeBallLZ.h"
#include "Resource/Mappings/SpikeBallSYZ.h"

// Object 57 - spiked balls twirling on a chain (SYZ, LZ). One parent object
// (the twirl centre) spawns up to 7 child link/ball objects; all of them
// (children plus the parent itself, stored as the last entry) get
// repositioned every frame along a circle by Sball_Twirl, using each
// object's own radius to place it along the chain.

static void Sball_Twirl(Sst *self, Own_SpikeBall *own) {
    // Real hardware does `add.w speed,obAngle(a0)` -- a 16-bit add into a
    // word straddling obAngle and the following (otherwise-unused) byte,
    // with only the high byte (self->angle) ever read back for CalcSine.
    // That makes the effective turn rate speed/256 per frame rather than
    // the raw speed value.
    uint16_t accum = (uint16_t)((self->angle << 8) | own->angle_frac);
    accum = (uint16_t)(accum + (uint16_t)own->speed);
    self->angle = (uint8_t)(accum >> 8);
    own->angle_frac = (uint8_t)accum;

    int16_t sin, cos;
    CalcSine(self->angle, &sin, &cos);

    for (int i = 0; i <= own->children; i++) {
        Sst *link = &sst[own->child_idx[i]];
        Own_SpikeBall *link_scratch = (Own_SpikeBall *)link->own.u8;

        int16_t radius = link_scratch->radius;
        link->y_pos.f.u = (int16_t)(own->orig_y + (((int32_t)radius * sin) >> 8));
        link->x_pos.f.u = (int16_t)(own->orig_x + (((int32_t)radius * cos) >> 8));
    }
}

static void SBall_ChkDel(Sst *self, Own_SpikeBall *own) {
    if (SstOutOfRange(own->orig_x)) {
        for (int i = 0; i <= own->children; i++) {
            Sst *link = &sst[own->child_idx[i]];
            if (link != self)
                Sst_Delete(link);
        }
        SstOnScreenTest2(self, own->orig_x); // (forgets its place and goes)
        return;
    }
    Sst_Draw(self);
}

static void SBall_Main(Sst *self, Own_SpikeBall *own) {
    self->routine += 2;
    self->mappings = Mappings_SpikeBallSYZ;
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_SYZ_Spikeball_Chain);
    self->render_flags = SST_RENDER_LEVEL;
    self->priority = 4 * 0x80;
    self->width_pixels = 16 / 2;
    own->orig_x = self->x_pos.f.u;
    own->orig_y = self->y_pos.f.u;

    self->collision_flags = 0x18 | 0x80; // col_8x8 | col_hurt -- SYZ: chain hurts Sonic
    bool is_lz = LEVEL_ZONE(level_id) == ZoneId_LZ;
    if (is_lz) {
        self->collision_flags = 0; // col_none -- LZ: chain doesn't hurt Sonic
        self->mappings = Mappings_SpikeBallLZ;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_LZ_Spikeball_Chain);
    }

    uint8_t subtype = self->subtype;
    int8_t speed = (int8_t)(subtype & 0xF0);
    own->speed = (int16_t)(speed << 3);

    self->angle = (uint8_t)(((((self->status & SST_STATUS_X_FLIP) != 0) ? 1 : 0) << 6) | ((((self->status & SST_STATUS_Y_FLIP) != 0) ? 1 : 0) << 7));

    int count = subtype & 7;
    int radius = count << 4;
    own->children = 0;
    own->radius = (uint8_t)radius;

    int loop_count = count - 1;
    if (loop_count >= 0 && (subtype & 8))
        loop_count--;

    for (int i = 0; loop_count >= 0 && i <= loop_count; i++) {
        // The original's default is a free slot anywhere. Its optional FixBugs variant (the slots
        // after the ball's own) drops the whole chain when none is free there -- a bare ball. A
        // link placed before the ball only costs a stray frame if it is deleted after being queued
        // for display, which BuildSprites skips safely.
        Sst *link = Sst_Allocate();
        if (link == NULL)
            break;

        // (The original zeroes a slot on delete; a leftover frame/anim made the chain links
        // draw nothing, so a link starts from a clean slot.)
        memset(link, 0, sizeof(Sst));

        own->children++;
        own->child_idx[own->children - 1] = (uint8_t)(link - sst);

        link->routine = 4; // SBall_Child
        link->code = self->code;
        link->mappings = self->mappings;
        link->art_tile = self->art_tile;
        link->render_flags = self->render_flags;
        link->priority = self->priority;
        link->width_pixels = self->width_pixels;
        link->collision_flags = self->collision_flags;

        radius -= 0x10;
        Own_SpikeBall *link_scratch = (Own_SpikeBall *)link->own.u8;
        link_scratch->radius = (uint8_t)radius;

        if (is_lz && radius == 0)
            link->mapping_frame = 2; // LZ stem attached to floor
    }

    own->child_idx[own->children] = (uint8_t)(self - sst);

    if (is_lz) {
        self->collision_flags = 0x0B | 0x80; // col_16x16 | col_hurt -- tip of chain is harmful
        self->mapping_frame = 1; // larger spikeball frame for tip
    }
}

void Obj_SpikeBall(Sst *self) {
    Own_SpikeBall *own = (Own_SpikeBall *)self->own.u8;

    switch (self->routine) {
    case 0:
        SBall_Main(self, own);
        __attribute__((fallthrough));
    case 2:
        Sball_Twirl(self, own);
        SBall_ChkDel(self, own);
        break;
    case 4: // SBall_Child -- rotation/deletion handled by parent, just display
        Sst_Draw(self);
        break;
    }
}
