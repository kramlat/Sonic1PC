#include "Seesaw.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Object/Sonic.h"
#include "Resource/Mappings/Seesaw.h"
#include "Resource/Mappings/SeesawBall.h"
#include "Sound.h"

// Object 5E - seesaws (SLZ)
//
// Throughout these comments, "ascending" means sloping upwards from left to
// right, and "descending" means going down from left to right.
//
// Each seesaw spawns a spike ball child (unless its subtype marks it as a
// boss-fight seesaw). The ball independently catapults itself between the
// seesaw's two ends based on how the seesaw's own tilt state changes.

static const uint8_t See_DataSlope[48] = {
    0x24, 0x24,                                                             // flat
    0x26, 0x28, 0x2A, 0x2C,                                                 // ascending
    0x2A, 0x28, 0x26, 0x24,                                                 // descending
    0x23, 0x22, 0x21, 0x20, 0x1F, 0x1E, 0x1D, 0x1C, 0x1B, 0x1A, 0x19, 0x18, // descending
    0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E, 0x0D, 0x0C, // (steep)
    0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03,                  //
    0x02, 0x02, 0x02, 0x02, 0x02,                                          // flat
};

static const uint8_t See_DataFlat[48] = {
    0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
    0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
    0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
    0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
};

// Relative Y-distances to align the spikeball with the seesaw, indexed by
// (seesaw frame) + (2 if the ball is on the left side).
static const int16_t See_Spikeball_YOffsets[5] = { -8, -28, -47, -28, -8 };

static void See_Spikeball_Action(Sst *self, Own_Seesaw *own);
static void See_Spikeball_InAir(Sst *self, Own_Seesaw *own);

// Updates the seesaw's own frame/tilt-state (0=descending,1=flat,2=ascending).
// Also called (indirectly, via see_state_see being written into by the
// spikeball) to sync the seesaw's visuals to state changes the spikeball
// itself drove.
static void See_ChgFrame(Sst *self, Own_Seesaw *own, int8_t new_state) {
    int8_t frame = (int8_t)self->mapping_frame;
    if (frame == new_state)
        return;
    if (frame < new_state)
        frame = (int8_t)(frame + 2);
    frame = (int8_t)(frame - 1);
    self->mapping_frame = (uint8_t)frame;
    own->state = new_state;

    SST_RENDER_SET(self, SST_RENDER_X_FLIP, false);
    if (frame & 2)
        SST_RENDER_SET(self, SST_RENDER_X_FLIP, true);
}

// Sets the seesaw tilt based on what side of it Sonic is standing on.
static void See_ChkSide(Sst *self, Own_Seesaw *own) {
    int16_t d0 = (int16_t)(self->x_pos.f.u - player->x_pos.f.u);
    int8_t new_state;
    if (d0 >= 0) {
        new_state = 2; // Sonic on the left side -- ascending
    } else {
        d0 = (int16_t)-d0;
        new_state = 0; // Sonic on the right side -- descending
    }
    if ((uint16_t)d0 < 8)
        new_state = 1; // within 8px of center -- flat
    See_ChgFrame(self, own, new_state);
}

static void See_Seesaw_Platform(Sst *self, Own_Seesaw *own) {
    See_ChgFrame(self, own, own->state);

    const uint8_t *heightmap = (self->mapping_frame & 1) ? See_DataFlat : See_DataSlope;

    own->landspeed = player->y_vel;

    SstSlopeObject(self, 96 / 2, heightmap); // may set self->routine = 4
}

static void See_Seesaw_StoodOn(Sst *self, Own_Seesaw *own) {
    See_ChkSide(self, own);

    const uint8_t *heightmap = (self->mapping_frame & 1) ? See_DataFlat : See_DataSlope;

    SstExitPlatform(self, 96 / 2, 96 / 2, NULL); // may set self->routine = 2
    SstSlopeObject_AssumeStoodOn(self, 96 / 2, heightmap, self->x_pos.f.u);
}

static void See_Main(Sst *self, Own_Seesaw *own) {
    self->routine = 2; // advance to See_Seesaw_Platform
    self->mappings = Mappings_Seesaw;
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_SLZ_Seesaw);
    self->render_flags |= SST_RENDER_LEVEL;
    self->priority = 4 * 0x80;
    self->width_pixels = 96 / 2;
    own->orig_x = self->x_pos.f.u;

    uint8_t self_index = (uint8_t)(self - sst);

    if (self->subtype == 0) { // spawn a spikeball? (boss-fight seesaws don't)
        Sst *ball = Sst_AllocateAfter(self);
        if (ball != NULL) {
            ball->code = Obj_Seesaw;
            ball->routine = 6; // See_Spikeball_Setup
            ball->x_pos.f.u = self->x_pos.f.u;
            ball->y_pos.f.u = self->y_pos.f.u;
            ball->status = self->status;
            Own_Seesaw *bscratch = (Own_Seesaw *)ball->own.u8;
            bscratch->parent_index = self_index;
        }
    }

    if (((self->status & SST_STATUS_X_FLIP) != 0)) // never actually happens anywhere in the game
        self->mapping_frame = 2;

    own->state = (int8_t)self->mapping_frame;

    See_Seesaw_Platform(self, own);
}

static void See_Spikeball_Action_Align(Sst *self, Own_Seesaw *own, Sst *parent) {
    int idx = parent->mapping_frame;
    int16_t x_off = 40; // align to the right side
    if ((int16_t)(self->x_pos.f.u - own->orig_x) < 0) {
        x_off = -40; // align to the left side
        idx += 2;
    }

    self->y_pos.f.u = (int16_t)(own->orig_y + See_Spikeball_YOffsets[idx]);
    self->x_pos.f.u = (int16_t)(own->orig_x + x_off);
    self->y_pos.f.l = 0;
    self->x_pos.f.l = 0;
}

static void See_Spikeball_Setup(Sst *self, Own_Seesaw *own) {
    self->routine = 8; // advance to See_Spikeball_Action
    self->mappings = Mappings_SeesawBall;
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_SLZ_Spikeball);
    self->render_flags |= SST_RENDER_LEVEL;
    self->priority = 4 * 0x80;
    self->collision_flags = 0x0B | 0x80; // col_16x16 | col_hurt
    self->width_pixels = 24 / 2;

    own->orig_x = self->x_pos.f.u;
    self->x_pos.f.u = (int16_t)(self->x_pos.f.u + 40);
    own->orig_y = self->y_pos.f.u;
    self->mapping_frame = 1; // silver spikeball

    if (((self->status & SST_STATUS_X_FLIP) != 0)) {
        self->x_pos.f.u = (int16_t)(self->x_pos.f.u - 80); // move to the left side instead
        own->state = 2; // on right side (i.e. mirrored -- see comment on Own_Seesaw::state)
    }

    See_Spikeball_Action(self, own);
}

static void See_Spikeball_Action(Sst *self, Own_Seesaw *own) {
    Sst *parent = &sst[own->parent_index];
    Own_Seesaw *pscratch = (Own_Seesaw *)parent->own.u8;

    int8_t d0 = (int8_t)(own->state - pscratch->state);
    if (d0 == 0) {
        See_Spikeball_Action_Align(self, own, parent);
        return;
    }
    if (d0 < 0)
        d0 = (int8_t)-d0;

    int16_t vy, vx;
    if (d0 == 1) {
        vy = -0x818; vx = -0x114; // slow catapult (partial state change)
    } else if (pscratch->landspeed < (int16_t)0xA00) {
        vy = -0xAF0; vx = -0xCC; // faster catapult (opposite ends)
    } else {
        vy = -0xE00; vx = -0xA0; // fastest catapult (Sonic landed very fast)
    }

    self->y_vel = vy;
    self->x_vel = vx;
    if ((int16_t)(self->x_pos.f.u - own->orig_x) < 0)
        self->x_vel = (int16_t)-self->x_vel;

    self->routine = 0xA; // See_Spikeball_InAir
    See_Spikeball_InAir(self, own);
}

static void See_Spikeball_InAir_FallingDown(Sst *self, Own_Seesaw *own) {
    Sst_MoveSprite(self);

    Sst *parent = &sst[own->parent_index];
    Own_Seesaw *pscratch = (Own_Seesaw *)parent->own.u8;

    int idx = parent->mapping_frame;
    if ((int16_t)(self->x_pos.f.u - own->orig_x) < 0)
        idx += 2;

    int16_t landing_y = (int16_t)(own->orig_y + See_Spikeball_YOffsets[idx]);
    if (landing_y > self->y_pos.f.u)
        return; // hasn't reached landing height yet

    int8_t new_state = (self->x_vel < 0) ? 2 : 0; // bounce side based on ball's horizontal direction
    pscratch->state = new_state;
    own->state = new_state;

    if (new_state != parent->mapping_frame) { // is the seesaw NOT already lowered on this side?
        bool was_standing = ((parent->status & SST_STATUS_PLAYER_STAND) != 0);
        parent->status &= (uint8_t)~SST_STATUS_PLAYER_STAND;
        if (was_standing) {
            parent->routine = 2; // reset seesaw back to See_Seesaw_Platform
            player->y_vel = (int16_t)-self->y_vel; // bounce Sonic based on the ball's landing speed
            player->status |= SST_PSTATUS_IN_AIR;
            Sonic_CancelSpindash(); // reset the charge so landing doesn't release it like a drop dash
            player->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
            ((Scratch_Sonic *)player->own.u8)->jumping = 0;
            player->anim = SonAnimId_Spring;
            player->routine = 2; // Sonic_Control
            QueueSound2(sfx_Spring);
        }
    }

    self->x_vel = 0;
    self->y_vel = 0;
    self->routine = 8; // See_Spikeball_Action
}

static void See_Spikeball_InAir(Sst *self, Own_Seesaw *own) {
    if (self->y_vel >= 0) {
        See_Spikeball_InAir_FallingDown(self, own);
        return;
    }

    // Ball is still going up
    Sst_MoveSprite(self);
    int16_t threshold = (int16_t)(own->orig_y - 47);
    if (threshold > self->y_pos.f.u)
        return; // more than 47px above the seesaw -- single gravity only
    Sst_MoveSprite(self); // double gravity while within 47px of the seesaw
}

void Obj_Seesaw(Sst *self) {
    Own_Seesaw *own = (Own_Seesaw *)self->own.u8;

    switch (self->routine) {
    case 0: See_Main(self, own); break;
    case 2: See_Seesaw_Platform(self, own); break;
    case 4: See_Seesaw_StoodOn(self, own); break;
    case 6: See_Spikeball_Setup(self, own); break;
    case 8: See_Spikeball_Action(self, own); break;
    case 0xA: See_Spikeball_InAir(self, own); break;
    }

    if (SstOutOfRange(own->orig_x))
        Sst_Delete(self);
    else
        Sst_Draw(self);
}
