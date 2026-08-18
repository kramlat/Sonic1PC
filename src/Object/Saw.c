#include "Saw.h"

#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Object/Sonic.h"
#include "Resource/Mappings/SawsPizzaCutters.h"
#include "Sound.h"

// Object 6A - pizza cutters and speeding saws (SBZ)

static bool Saw_OutOfRange(int16_t x) {
    uint16_t obj_pos = (uint16_t)x & 0xFF80;
    uint16_t cam_pos = (uint16_t)(scrpos_x.f.u - 128) & 0xFF80;
    return (uint16_t)(obj_pos - cam_pos) > (128 + 320 + 192);
}

static void Saw_Animate(Object *obj) {
    if (--obj->frame_time.b >= 0)
        return;
    obj->frame_time.b = 3 - 1;
    obj->frame ^= 1;
}

// Type 1 - pizza cutter that moves left and right
static void Saw_Type1_PizzaLeftRight(Object *obj, Scratch_Saw *scratch) {
    // state[N][0] is a 16-bit accumulator -- only the high byte is the
    // slow-moving integer value (the low byte is a fast-changing
    // sub-pixel remainder); reading the whole thing as uint8_t truncates
    // to the noisy low byte instead.
    int16_t d0 = (int16_t)(uint8_t)(oscillatory.state[3][0] >> 8); // frequency 2, middle value $30
    if (obj->status.o.f.x_flip)
        d0 = (int16_t)(-d0 + 0x60); // keep flipped saws in the same $60px range
    obj->pos.l.x.f.u = (int16_t)(scratch->orig_x - d0);

    Saw_Animate(obj);

    if (obj->render.f.on_screen && ((uint16_t)frame_count & 0xF) == 0)
        QueueSound2(sfx_Saw);
}

// Type 2 - pizza cutter that moves up and down
static void Saw_Type2_PizzaUpDown(Object *obj, Scratch_Saw *scratch) {
    uint8_t osc = (uint8_t)(oscillatory.state[1][0] >> 8); // frequency 2, middle value $18
    int16_t d0 = (int16_t)osc;
    if (obj->status.o.f.x_flip)
        d0 = (int16_t)(-d0 + 0x60 + 0x20);
    obj->pos.l.y.f.u = (int16_t)(scratch->orig_y - d0);

    Saw_Animate(obj);

    if (obj->render.f.on_screen && osc == 0x18)
        QueueSound2(sfx_Saw);
}

// Type 3 - speeding saw shot from the left once Sonic gets close. Returns
// true while hidden/not yet launched (caller skips display this frame).
static bool Saw_Type3_SpeedingFromLeft(Object *obj, Scratch_Saw *scratch) {
    if (scratch->shot) {
        SpeedToPos(obj);
        scratch->orig_x = obj->pos.l.x.f.u; // update for the offscreen check
        Saw_Animate(obj);
        return false;
    }

    uint16_t px = (uint16_t)player->pos.l.x.f.u;
    if (px < 192)
        return true; // Sonic close to the left level edge
    uint16_t d0 = (uint16_t)(px - 192);
    uint16_t saw_x = (uint16_t)obj->pos.l.x.f.u;
    if (d0 < saw_x)
        return true; // Sonic not in horizontal range

    uint16_t d1 = (uint16_t)((uint16_t)player->pos.l.y.f.u - 128);
    uint16_t saw_y = (uint16_t)obj->pos.l.y.f.u;
    if (d1 >= saw_y)
        return false; // not in upper trigger zone (not hidden, just not launched)
    if ((uint16_t)(d1 + 256) < saw_y)
        return false; // not in lower trigger zone

    scratch->shot = 1;
    obj->xsp = 0x600; // fast, to the right
    obj->col_type = 0x22 | 0x80; // col_48x48_alt | col_hurt
    obj->frame = 2; // speeding saw frames
    QueueSound2(sfx_Saw);
    return false;
}

// Type 4 - speeding saw shot from the right once Sonic gets close.
static bool Saw_Type4_SpeedingFromRight(Object *obj, Scratch_Saw *scratch) {
    if (scratch->shot) {
        SpeedToPos(obj);
        scratch->orig_x = obj->pos.l.x.f.u;
        Saw_Animate(obj);
        return false;
    }

    uint16_t d0 = (uint16_t)((uint16_t)player->pos.l.x.f.u + 224);
    uint16_t saw_x = (uint16_t)obj->pos.l.x.f.u;
    if (d0 >= saw_x)
        return true; // Sonic not in horizontal range

    uint16_t d1 = (uint16_t)((uint16_t)player->pos.l.y.f.u - 128);
    uint16_t saw_y = (uint16_t)obj->pos.l.y.f.u;
    if (d1 >= saw_y)
        return false;
    if ((uint16_t)(d1 + 256) < saw_y)
        return false;

    scratch->shot = 1;
    obj->xsp = (int16_t)-0x600; // fast, to the left
    obj->col_type = 0x22 | 0x80; // col_48x48_alt | col_hurt
    obj->frame = 2;
    QueueSound2(sfx_Saw);
    return false;
}

static void Saw_Action(Object *obj, Scratch_Saw *scratch) {
    bool hide = false;
    switch (obj->scratch.u8[0] & 7) {
    case 0: break; // stationary pizza cutter
    case 1: Saw_Type1_PizzaLeftRight(obj, scratch); break;
    case 2: Saw_Type2_PizzaUpDown(obj, scratch); break;
    case 3: hide = Saw_Type3_SpeedingFromLeft(obj, scratch); break;
    case 4: hide = Saw_Type4_SpeedingFromRight(obj, scratch); break;
    }

    // FixBugs: real hardware skips the offscreen/delete check entirely
    // while hidden (an unshot speeding saw), which can leak the object if
    // Sonic never gets close enough before leaving the area. Still check
    // out-of-range here either way, but never display while hidden.
    if (Saw_OutOfRange(scratch->orig_x)) {
        ObjectDelete(obj);
        return;
    }
    if (!hide)
        DisplaySprite(obj);
}

static void Saw_Main(Object *obj, Scratch_Saw *scratch) {
    obj->routine = 2; // advance to Saw_Action
    obj->mappings = Mappings_SawsPizzaCutters;
    obj->tile = TILE_MAP(0, 2, 0, 0, ArtTile_SBZ_Saw); // | Tile_Pal3
    obj->render.f.align_fg = true;
    obj->priority = 4;
    obj->width_pixels = 64 / 2;
    scratch->orig_x = obj->pos.l.x.f.u;
    scratch->orig_y = obj->pos.l.y.f.u;

    if (obj->scratch.u8[0] < 3) // pizza cutter (subtype 0-2), not a speeding saw
        obj->col_type = 0x22 | 0x80; // col_48x48_alt | col_hurt

    Saw_Action(obj, scratch);
}

void Obj_Saw(Object *obj) {
    Scratch_Saw *scratch = (Scratch_Saw *)&obj->scratch;

    if (obj->routine == 0)
        Saw_Main(obj, scratch);
    else
        Saw_Action(obj, scratch);
}
