// The Crocobot (object 4E) of Sand Shower: see Crocobot.h
#include "Object/Crocobot.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Level.h"
#include "LevelCollision.h"

#include "Macros.h"

#include "Resource/Mappings/Crocobot.h"

#define ARTTILE_CROCOBOT 0x400 // (the staged ani.asm's, art tile $2411: palette line 1)

typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad0;      // 0x29
    uint8_t pad1[6];   // 0x2A-0x2F
    int16_t wait;      // 0x30
} Own_Crocobot;

// loc_1F406: the animations: 0 walks (3 ticks a frame), 1 stands (15), 2 has its mouth open (3)
static const uint8_t crocobot_animations[] = {
    0x00, 0x06, 0x00, 0x0E, 0x00, 0x11,
    0x03, 0x00, 0x04, 0x02, 0x03, 0x01, 0x05, 0xFF,
    0x0F, 0x00, 0xFF,
    0x03, 0x06, 0x0A, 0x08, 0x09, 0x07, 0x0B, 0xFF, 0x00,
};

enum { CrocobotRoutine_Init = 0, CrocobotRoutine_Main = 2 };
enum { CrocobotState_Wait = 0, CrocobotState_Walk = 2 };

static void Crocobot_Face(Sst *self, bool flip) {
    SST_STATUS_SET(self, SST_STATUS_X_FLIP, flip);
    SST_RENDER_SET(self, SST_RENDER_X_FLIP, flip);
}

// loc_1F3CE: its mouth opens when Sonic is within $40 pixels in the way it faces
static void Crocobot_Look(Sst *self) {
    const int16_t d = (int16_t)(self->x_pos.f.u - player->x_pos.f.u);
    const bool facing_right = ((self->status & SST_STATUS_X_FLIP) != 0);
    if (d >= 0) { // Sonic is to its left
        if (d > 0x40) {
            self->anim = 0;
        } else if (!facing_right) {
            self->anim = 2;
        }
        return;
    }
    if (d < -0x40 || !facing_right) { // to its right, and too far or it faces the other way
        self->anim = 0;
        return;
    }
    self->anim = 2;
}

void Obj_Crocobot(Sst *self) {
    Own_Crocobot *own = (Own_Crocobot *)self->own.u8;

    if (self->routine == CrocobotRoutine_Init) {
        self->mappings = Mappings_Crocobot;
        self->art_tile = TILE_MAP(0, 1, 0, 0, ARTTILE_CROCOBOT);
        self->render_flags |= 4 | SPRITE_CAM_FIELD;
        self->collision_flags = 0x0A;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x10;
        self->y_radius = 0x10;
        self->x_radius = 8;
        Sst_MoveSprite(self);
        const int16_t dist = SstFloorDist(self, self->x_pos.f.u);
        if (dist < 0) { // it has met the floor
            self->y_pos.f.u += dist;
            self->y_vel = 0;
            self->routine += 2;
        }
        return;
    }

    switch (self->routine_secondary) {
    case CrocobotState_Wait:
        if (--own->wait < 0) { // turns round and sets off
            self->routine_secondary += 2;
            self->x_vel = (int16_t)0xFF40;
            self->anim = 0;
            const bool was_flipped = ((self->status & SST_STATUS_X_FLIP) != 0);
            Crocobot_Face(self, !was_flipped);
            if (!was_flipped) // (it faces right now)
                self->x_vel = (int16_t)-self->x_vel;
        }
        break;

    case CrocobotState_Walk: {
        Crocobot_Look(self);
        Sst_MoveSprite2(self);
        const int16_t dist = SstFloorDist(self, self->x_pos.f.u);
        if (dist >= -8 && dist < 0x0C) {
            self->y_pos.f.u += dist;
        } else { // the floor ends: back to waiting, $3B frames
            self->routine_secondary -= 2;
            own->wait = 0x3B;
            self->x_vel = 0;
            self->anim = 1;
        }
        break;
    }
    }

    Sst_Animate(self, crocobot_animations);
    SstOnScreenTest(self);
}
