// Hidden Palace's badniks for Sonic 2 (Nick Arcade's objects 4C and 4F): the BBat, a bat that hangs about and swoops at Sonic in an arc, and the Redz, a dinosaur that walks to the edge of its floor and back.
#include "Object/HPZBadniks.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Level.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Object/Sonic.h"

#include "Macros.h"

#include "Resource/Animation/BBat.h"
#include "Resource/Animation/Redz.h"
#include "Resource/Animation/Rhinobot.h"
#include "Resource/Mappings/BBat.h"
#include "Resource/Mappings/Redz.h"
#include "Resource/Mappings/Rhinobot.h"

#define ArtTile_Redz 0x500
#define ArtTile_Rhinobot 0x3B2 // ($7640)
#define ArtTile_BBat 0x5A8 // (the alpha's is $530: the bubbles' art of Hidden Palace's water has that now, and the animals' $580 to $5A7, see Sonic2PLC.c)

// ---------------------------------------------------------------------------------------------------------------------------------------
// The BBat (object 4C)
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;      // 0x28
    uint8_t pad0;         // 0x29
    uint16_t timer;       // 0x2A (the flapping counts its high byte: as the original does)
    int16_t detect_timer; // 0x2C: time until it notices Sonic
    int16_t home_y;       // 0x2E: the middle of its hovering
    uint8_t pad1[0xD];    // 0x30-0x3C
    uint8_t turned;       // 0x3D: it swoops to the right
    uint8_t first;        // 0x3E: which way it faces next while it looks about
    uint8_t arc;          // 0x3F: its angle on the arc
} Own_BBat;

enum { BBatRoutine_Init = 0, BBatRoutine_Main = 2, BBatRoutine_Attack = 4 };
enum { BBatSub_Wait = 0, BBatSub_Flap = 2, BBatSub_Seek = 4 };

static void BBat_SetFlip(Sst *self, bool flip) {
    SST_RENDER_SET(self, SST_RENDER_X_FLIP, flip);
    SST_STATUS_SET(self, SST_STATUS_X_FLIP, flip);
}

// Sonic within 128 pixels wakes it (BBat_WaitForPlayer): true if it was woken
static bool BBat_WaitForPlayer(Sst *self, Own_BBat *own) {
    int16_t d0 = self->x_pos.f.u - player->x_pos.f.u;
    if (d0 > 0x80 || d0 < -0x80)
        return false;
    self->routine_secondary = BBatSub_Seek;
    self->anim = 2;
    own->timer = 8;
    own->first = 0;
    return true;
}

// It notices Sonic within 96 pixels and swoops (BBat_AttackPlayer). Returns what the original leaves in its zero flag: true if the rest of the frame's looking about is to be skipped
static bool BBat_AttackPlayer(Sst *self, Own_BBat *own) {
    own->detect_timer--;
    if (own->detect_timer >= 0)
        return own->detect_timer == 0;

    int16_t d0 = self->x_pos.f.u - player->x_pos.f.u;
    if (d0 > 0x60 || d0 < -0x60) {
        if (d0 <= 0x80 && d0 > -0x80)
            return false; // (between 96 and 128 pixels: nothing happens)
        self->anim = 1;
        self->routine_secondary = BBatSub_Wait;
        own->timer = 0x18;
        return false;
    }
    if (d0 < 0)
        own->turned = 0xFF;
    own->arc = 0x40;
    self->ground_vel = 0x400;
    self->routine = BBatRoutine_Attack;
    self->anim = 3;
    own->timer = 0xC;
    own->first = 1;
    return true;
}

static void BBat_Hover(Sst *self, Own_BBat *own) {
    switch (self->routine_secondary) {
    case BBatSub_Wait:
        if ((int16_t)--own->timer >= 0)
            return;
        if (BBat_WaitForPlayer(self, own))
            return;
        if ((uint8_t)RandomNumber() != 0) // (one frame in 256)
            return;
        own->timer = 0x18;
        own->detect_timer = 0x1E;
        self->routine_secondary = BBatSub_Flap;
        self->anim = 1;
        own->first = 0;
        break;
    case BBatSub_Flap: { // (counts the high byte of its timer, which is 0: one frame)
        uint8_t high = (uint8_t)(own->timer >> 8);
        high--;
        own->timer = (uint16_t)((own->timer & 0xFF) | (high << 8));
        if (high & 0x80)
            self->routine_secondary = BBatSub_Wait;
        break;
    }
    case BBatSub_Seek:
        if (BBat_AttackPlayer(self, own))
            return;
        if (--own->timer != 0)
            return;
        if (own->first) {
            own->first = 0;
            own->timer = 8;
            BBat_SetFlip(self, true);
        } else {
            own->first = 1;
            own->timer = 0xC;
            BBat_SetFlip(self, false);
        }
        break;
    }
}

void Obj_BBat(Sst *self) {
    Own_BBat *own = (Own_BBat *)self->own.u8;

    switch (self->routine) {
    case BBatRoutine_Init:
        self->mappings = Mappings_BBat;
        self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_BBat); // (palette line 2: Nick Arcade\x27s mistake, its flame and ears look odd)
        self->render_flags |= SST_RENDER_LEVEL;
        self->collision_flags = 0xA;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x10;
        self->y_radius = 0x10;
        self->x_radius = 8;
        self->routine += 2;
        own->home_y = self->y_pos.f.u;
        break;
    case BBatRoutine_Main: {
        BBat_Hover(self, own);
        // its arc
        int16_t sin, cos;
        CalcSine(own->arc, &sin, &cos);
        self->y_pos.f.u = (int16_t)(own->home_y + (sin >> 6));
        own->arc += 4;
        Sst_Animate(self, Animation_BBat);
        SstOnScreenTest(self);
        break;
    }
    case BBatRoutine_Attack: {
        int16_t sin, cos;
        // BBat_Move: its speed from the angle on the arc
        CalcSine(own->arc, &sin, &cos);
        self->x_vel = (int16_t)(((int32_t)cos * self->ground_vel) >> 8);
        self->y_vel = (int16_t)(((int32_t)sin * self->ground_vel) >> 8);
        // BBat_ArcMotion
        bool done;
        if (!own->turned) {
            done = own->arc >= 0xC0;
            if (!done)
                own->arc += 2;
        } else {
            done = own->arc == 0xC0;
            if (!done)
                own->arc -= 2;
        }
        if (done) {
            own->turned = 0;
            self->routine = BBatRoutine_Main;
            self->routine_secondary = BBatSub_Wait;
            own->timer = 0x18;
            self->anim = 1;
            BBat_SetFlip(self, false);
        }
        if (own->turned)
            BBat_SetFlip(self, true);
        Sst_MoveSprite2(self);
        Sst_Animate(self, Animation_BBat);
        SstOnScreenTest(self);
        break;
    }
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The Redz (object 4F)
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;     // 0x28
    uint8_t pad0[7];     // 0x29-0x2F
    int16_t wait_timer;  // 0x30
} Own_Redz;

enum { RedzRoutine_Init = 0, RedzRoutine_Main = 2, RedzRoutine_Delete = 4 };

void Obj_Redz(Sst *self) {
    Own_Redz *own = (Own_Redz *)self->own.u8;

    switch (self->routine) {
    case RedzRoutine_Init: {
        self->mappings = Mappings_Redz;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Redz);
        self->render_flags = SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x10;
        self->y_radius = 0x10;
        self->x_radius = 6;
        self->collision_flags = 0xC;
        Sst_MoveSprite(self);
        int16_t d1 = SstFloorDist(self, self->x_pos.f.u);
        if (d1 < 0) {
            self->y_pos.f.u += d1;
            self->y_vel = 0;
            self->routine += 2;
            self->status ^= SST_STATUS_X_FLIP;
        }
        break;
    }
    case RedzRoutine_Main:
        if (self->routine_secondary == 0) { // waits, then walks off
            if (--own->wait_timer < 0) {
                self->routine_secondary += 2;
                self->x_vel = -0x80;
                self->anim = 1;
                bool was_set = ((self->status & SST_STATUS_X_FLIP) != 0);
                self->status ^= SST_STATUS_X_FLIP;
                if (!was_set)
                    self->x_vel = -self->x_vel;
            }
        } else { // walks until the floor ends
            Sst_MoveSprite2(self);
            int16_t d1 = SstFloorDist(self, self->x_pos.f.u);
            if (d1 < -8 || d1 >= 0xC) {
                self->routine_secondary -= 2;
                own->wait_timer = 59;
                self->x_vel = 0;
                self->anim = 0;
            } else {
                self->y_pos.f.u += d1;
            }
        }
        Sst_Animate(self, Animation_Redz);
        SstOnScreenTest(self);
        break;
    case RedzRoutine_Delete:
        Sst_Delete(self);
        break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The Rhinobot (object 4D): a rhinoceros that walks to the edge of its floor and back, and charges at Sonic when he comes near in front of it (the alpha has it in Hidden Palace's art list and object table, and
// no layout places it)
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;     // 0x28
    uint8_t pad0[7];     // 0x29-0x2F
    int16_t wait_timer;  // 0x30
} Own_Rhinobot;

enum { RhinoAnim_Walk = 0, RhinoAnim_Still = 1, RhinoAnim_Charge = 2 };

// It looks for Sonic: behind it, it walks on; within $60 pixels in front of it, it charges (Offset_0x0229DC)
static void Rhinobot_Chase(Sst *self) {
    const int16_t d0 = (int16_t)(self->x_pos.f.u - player->x_pos.f.u);
    const bool facing_right = ((self->status & SST_STATUS_X_FLIP) != 0);
    if (d0 < 0) { // Sonic is to its right
        if (d0 < -0x60)
            return;
        if (facing_right) {
            self->anim = RhinoAnim_Charge;
            self->x_vel = 0x200;
        } else {
            self->anim = RhinoAnim_Walk;
            self->x_vel = -0x80;
        }
    } else { // to its left
        if (d0 > 0x60)
            return;
        if (facing_right) {
            self->anim = RhinoAnim_Walk;
            self->x_vel = 0x80;
        } else {
            self->anim = RhinoAnim_Charge;
            self->x_vel = -0x200;
        }
    }
}

void Obj_Rhinobot(Sst *self) {
    Own_Rhinobot *own = (Own_Rhinobot *)self->own.u8;

    switch (self->routine) {
    case 0: {
        self->mappings = Mappings_Rhinobot;
        self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_Rhinobot);
        self->render_flags |= SST_RENDER_LEVEL;
        self->collision_flags = 0x0A;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x18;
        self->y_radius = 0x10;
        self->x_radius = 0x18;
        Sst_MoveSprite(self);
        const int16_t d1 = SstFloorDist(self, self->x_pos.f.u);
        if (d1 < 0) {
            self->y_pos.f.u += d1;
            self->y_vel = 0;
            self->routine += 2;
        }
        break;
    }
    case 2:
        if (self->routine_secondary == 0) { // waits, then walks off
            if (--own->wait_timer < 0) {
                self->routine_secondary += 2;
                self->x_vel = -0x80;
                self->anim = RhinoAnim_Walk;
                const bool was_set = ((self->status & SST_STATUS_X_FLIP) != 0);
                self->status ^= SST_STATUS_X_FLIP;
                if (!was_set)
                    self->x_vel = -self->x_vel;
            }
        } else { // walks or charges until the floor ends
            Rhinobot_Chase(self);
            Sst_MoveSprite(self);
            const int16_t d1 = SstFloorDist(self, self->x_pos.f.u);
            if (d1 < -8) { // the floor ends: back a step, and stands a while
                self->routine_secondary -= 2;
                own->wait_timer = 0x3B;
                self->x_pos.v -= (int32_t)self->x_vel << 8;
                self->x_vel = 0;
                self->anim = RhinoAnim_Still;
            } else if (d1 < 0xC) {
                self->y_vel = 0;
                self->y_pos.f.u += d1;
            }
        }
        Sst_Animate(self, Animation_Rhinobot);
        SstOnScreenTest(self);
        break;
    }
}
