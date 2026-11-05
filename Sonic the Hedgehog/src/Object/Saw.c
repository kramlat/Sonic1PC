#include "Saw.h"
#include "Object.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Constants.h"

#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Object/Sonic.h"
#include "Resource/Mappings/SawsPizzaCutters.h"
#include "Sound.h"

// Object 6A - pizza cutters and speeding saws (SBZ)

static void Saw_Animate(Sst *self) {
    if (--self->anim_frame_timer >= 0)
        return;
    self->anim_frame_timer = 3 - 1;
    self->mapping_frame ^= 1;
}

// Type 1 - pizza cutter that moves left and right
static void Saw_Type1_PizzaLeftRight(Sst *self, Own_Saw *own) {
    // state[N][0] is a 16-bit accumulator -- only the high byte is the
    // slow-moving integer value (the low byte is a fast-changing
    // sub-pixel remainder); reading the whole thing as uint8_t truncates
    // to the noisy low byte instead.
    int16_t d0 = (int16_t)(uint8_t)(oscillatory.state[3][0] >> 8); // frequency 2, middle value $30
    if (((self->status & SST_STATUS_X_FLIP) != 0))
        d0 = (int16_t)(-d0 + 0x60); // keep flipped saws in the same $60px range
    self->x_pos.f.u = (int16_t)(own->orig_x - d0);

    Saw_Animate(self);

    if ((self->render_flags & SST_RENDER_ON_SCREEN) && ((uint16_t)frame_count & 0xF) == 0)
        QueueSound2(sfx_Saw);
}

// Type 2 - pizza cutter that moves up and down
static void Saw_Type2_PizzaUpDown(Sst *self, Own_Saw *own) {
    uint8_t osc = (uint8_t)(oscillatory.state[1][0] >> 8); // frequency 2, middle value $18
    int16_t d0 = (int16_t)osc;
    if (((self->status & SST_STATUS_X_FLIP) != 0))
        d0 = (int16_t)(-d0 + 0x60 + 0x20);
    self->y_pos.f.u = (int16_t)(own->orig_y - d0);

    Saw_Animate(self);

    if ((self->render_flags & SST_RENDER_ON_SCREEN) && osc == 0x18)
        QueueSound2(sfx_Saw);
}

// Type 3 - speeding saw shot from the left once Sonic gets close. Returns
// true while hidden/not yet launched (caller skips display this frame).
static bool Saw_Type3_SpeedingFromLeft(Sst *self, Own_Saw *own) {
    if (own->shot) {
        Sst_MoveSprite2(self);
        own->orig_x = self->x_pos.f.u; // update for the offscreen check
        Saw_Animate(self);
        return false;
    }

    uint16_t px = (uint16_t)player->x_pos.f.u;
    if (px < 192)
        return true; // Sonic close to the left level edge
    uint16_t d0 = (uint16_t)(px - 192);
    uint16_t saw_x = (uint16_t)self->x_pos.f.u;
    if (d0 < saw_x)
        return true; // Sonic not in horizontal range

    uint16_t d1 = (uint16_t)((uint16_t)player->y_pos.f.u - 128);
    uint16_t saw_y = (uint16_t)self->y_pos.f.u;
    if (d1 >= saw_y)
        return false; // not in upper trigger zone (not hidden, just not launched)
    if ((uint16_t)(d1 + 256) < saw_y)
        return false; // not in lower trigger zone

    own->shot = 1;
    self->x_vel = 0x600; // fast, to the right
    self->collision_flags = 0x22 | 0x80; // col_48x48_alt | col_hurt
    self->mapping_frame = 2; // speeding saw frames
    QueueSound2(sfx_Saw);
    return false;
}

// Type 4 - speeding saw shot from the right once Sonic gets close.
static bool Saw_Type4_SpeedingFromRight(Sst *self, Own_Saw *own) {
    if (own->shot) {
        Sst_MoveSprite2(self);
        own->orig_x = self->x_pos.f.u;
        Saw_Animate(self);
        return false;
    }

    uint16_t d0 = (uint16_t)((uint16_t)player->x_pos.f.u + 224);
    uint16_t saw_x = (uint16_t)self->x_pos.f.u;
    if (d0 >= saw_x)
        return true; // Sonic not in horizontal range

    uint16_t d1 = (uint16_t)((uint16_t)player->y_pos.f.u - 128);
    uint16_t saw_y = (uint16_t)self->y_pos.f.u;
    if (d1 >= saw_y)
        return false;
    if ((uint16_t)(d1 + 256) < saw_y)
        return false;

    own->shot = 1;
    self->x_vel = (int16_t)-0x600; // fast, to the left
    self->collision_flags = 0x22 | 0x80; // col_48x48_alt | col_hurt
    self->mapping_frame = 2;
    QueueSound2(sfx_Saw);
    return false;
}

static void Saw_Action(Sst *self, Own_Saw *own) {
    bool hide = false;
    switch (self->subtype & 7) {
    case 0: break; // stationary pizza cutter
    case 1: Saw_Type1_PizzaLeftRight(self, own); break;
    case 2: Saw_Type2_PizzaUpDown(self, own); break;
    case 3: hide = Saw_Type3_SpeedingFromLeft(self, own); break;
    case 4: hide = Saw_Type4_SpeedingFromRight(self, own); break;
    }

    // FixBugs: real hardware skips the offscreen/delete check entirely
    // while hidden (an unshot speeding saw), which can leak the object if
    // Sonic never gets close enough before leaving the area. Still check
    // out-of-range here either way, but never display while hidden.
    if (SstOutOfRange(own->orig_x)) {
        SstLoad_Forget(self);
        Sst_Delete(self);
        return;
    }
    if (!hide)
        Sst_Draw(self);
}

static void Saw_Main(Sst *self, Own_Saw *own) {
    self->routine = 2; // advance to Saw_Action
    self->mappings = Mappings_SawsPizzaCutters;
    self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_SBZ_Saw); // | Tile_Pal3
    self->render_flags = SST_RENDER_LEVEL;
    self->priority = 4 * 0x80;
    self->width_pixels = 64 / 2;
    own->orig_x = self->x_pos.f.u;
    own->orig_y = self->y_pos.f.u;

    if (self->subtype < 3) // pizza cutter (subtype 0-2), not a speeding saw
        self->collision_flags = 0x22 | 0x80; // col_48x48_alt | col_hurt

    Saw_Action(self, own);
}

void Obj_Saw(Sst *self) {
    Own_Saw *own = (Own_Saw *)self->own.u8;

    if (self->routine == 0)
        Saw_Main(self, own);
    else
        Saw_Action(self, own);
}
