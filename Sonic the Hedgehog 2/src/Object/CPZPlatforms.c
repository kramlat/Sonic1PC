// Chemical Plant's platforms for Sonic 2 (Nick Arcade's objects 0C and 19): the floating platform that bobs and now and then hangs and shakes at the top of its swing (0C), and the platform
// that slides, rises, falls or shuttles by its subtype (19). Both carry Sonic and Tails.
#include "Object/CPZPlatforms.h"
#include "Constants.h"

#include "Level.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Object/Tails.h"
#include "Solid.h"

#include "Macros.h"

#include "Resource/Mappings/CPZPlatform.h"
#include "Resource/Mappings/CPZPlatform2.h"

static void Platforms_Carry(Object *obj, int16_t x_rad, int16_t y_walk, int16_t old_x) {
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
        Object *chr = (who == SolidChar_Sonic) ? player : (TAILS_OBJ->type != 0 ? TAILS_OBJ : NULL);
        if (chr != NULL)
            Solid_Platform(obj, chr, who, x_rad, y_walk, old_x);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 0C: the floating platform
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28: bits 4-7 (unused, kept), bits 0-3 how many extra shakes
    uint8_t pad0[1];   // 0x29
    uint8_t pad1[0x10];// 0x2A-0x39
    int16_t centre_y;  // 0x3A: the middle of its swing
    uint8_t swing;     // 0x3C: its angle in the swing (a byte: a swing is 256 frames)
    uint8_t shake;     // 0x3D: the angle of its shaking at the top
    uint8_t shakes;    // 0x3E: how many shakes are left
    uint8_t shakes_max;// 0x3F
} Scratch_CPZPlatform;

void Obj_CPZPlatform(Object *obj) {
    Scratch_CPZPlatform *scratch = (Scratch_CPZPlatform *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        obj->mappings = Mappings_CPZPlatform;
        obj->tile = TILE_MAP(1, 3, 0, 0, 0x418);
        obj->render.f.level_fg = true;
        obj->width_pixels = 0x10;
        obj->priority = 4;
        scratch->centre_y = obj->pos.l.y.f.u - 0x10;
        scratch->shakes = scratch->shakes_max = scratch->subtype & 0xF;
    }

    int16_t sin, cos;
    uint8_t angle = scratch->swing;
    bool shaking = false;
    uint8_t shake_angle = 0;
    if (angle == 0) {
        if ((frame_count & 0x3FF) == 0) { // (every 1024 frames the swing begins)
            scratch->shake = 1;
            scratch->swing++;
        }
    } else if (angle == 0x80) { // at the top of the swing: a moment's shaking, a number of times
        uint8_t shake = scratch->shake;
        if (shake == 0) {
            if ((int8_t)--scratch->shakes < 0) {
                scratch->shakes = scratch->shakes_max;
                scratch->swing++;
            } else {
                shaking = true;
            }
        } else {
            shaking = true;
        }
        if (shaking) {
            shake_angle = scratch->shake;
            scratch->shake++;
        }
    } else {
        scratch->swing++;
    }

    if (shaking) {
        CalcSine(shake_angle, &sin, &cos);
        obj->pos.l.y.f.u = (int16_t)(scratch->centre_y + ((sin + 8) >> 6) - 0x10);
    } else {
        CalcSine(angle, &sin, &cos);
        obj->pos.l.y.f.u = (int16_t)(scratch->centre_y + ((cos + 8) >> 4));
    }

    Platforms_Carry(obj, obj->width_pixels, 9, obj->pos.l.x.f.u);
    RememberState(obj);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 19: the platform that slides, rises, falls or shuttles
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28: the movement (bits 0-3); the size (bits 4-7) is used at the start
    uint8_t pad0[7];   // 0x29-0x2F
    int16_t base_x;    // 0x30: where it is (its centre, or where it began)
    int16_t base_y;    // 0x32: and for the up and down
    int16_t wait;      // 0x34: (the shuttle) how long it waits at the far end
    int16_t back;      // 0x36: (the shuttle) it is coming back
} Scratch_CPZPlatform2;

static const uint8_t platform2_sizes[5][2] = { { 0x20, 0 }, { 0x20, 1 }, { 0x20, 2 }, { 0x40, 3 }, { 0x30, 4 } };

// The movement (Obj19_Modes). False if the platform is to be taken no further this frame (the one that waits hidden for a button)
static bool Platform2_Move(Object *obj, Scratch_CPZPlatform2 *scratch) {
    switch (scratch->subtype & 0xF) {
    case 1: { // slides to and fro
        int16_t d0 = (uint8_t)(oscillatory.state[3][0] >> 8);
        if (obj->status.o.f.x_flip)
            d0 = (int16_t)(0x60 - d0);
        obj->pos.l.x.f.u = (int16_t)(scratch->base_x - d0);
        break;
    }
    case 2: case 4: case 9: // waits for someone to stand on it, then the next movement
        if (obj->status.b & 0x18)
            scratch->subtype++;
        break;
    case 3: case 5: // slides right until a wall
        if (ObjHitWallRight(obj, obj->width_pixels) < 0) {
            if ((scratch->subtype & 0xF) == 3)
                scratch->subtype = 0;
            else
                scratch->subtype++;
        } else {
            obj->pos.l.x.f.u++;
            scratch->base_x = obj->pos.l.x.f.u;
        }
        break;
    case 6: { // falls to the floor
        SpeedToPos(obj);
        obj->ysp += 0x18;
        int16_t d1 = ObjFloorDist(obj, obj->pos.l.x.f.u);
        if (d1 < 0) {
            obj->pos.l.y.f.u += d1;
            obj->ysp = 0;
            scratch->subtype = 0;
        }
        break;
    }
    case 7: // hidden until a button is pressed
        if (f_switch[2])
            scratch->subtype -= 3;
        return false;
    case 8: { // up and down
        int16_t d0 = (uint8_t)(oscillatory.state[7][0] >> 8);
        if (obj->status.o.f.x_flip)
            d0 = (int16_t)(0x80 - d0);
        obj->pos.l.y.f.u = (int16_t)(scratch->base_y - d0);
        break;
    }
    case 10: { // shuttles: out to twice its width, waits, and back
        int16_t reach = (int16_t)(obj->width_pixels * 2), step = 8;
        if (obj->status.o.f.x_flip) {
            step = -step;
            reach = -reach;
        }
        if (scratch->back == 0) {
            int16_t d0 = obj->pos.l.x.f.u - scratch->base_x;
            if (d0 != reach) {
                obj->pos.l.x.f.u += step;
                scratch->wait = 0x12C;
            } else if (--scratch->wait == 0) {
                scratch->back = 1;
            }
        } else {
            int16_t d0 = obj->pos.l.x.f.u - scratch->base_x;
            if (d0 != 0) {
                obj->pos.l.x.f.u -= step;
            } else {
                scratch->back = 0;
                scratch->subtype--;
            }
        }
        break;
    }
    }
    return true;
}

void Obj_CPZPlatform2(Object *obj) {
    Scratch_CPZPlatform2 *scratch = (Scratch_CPZPlatform2 *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        obj->mappings = Mappings_CPZPlatform2;
        obj->tile = TILE_MAP(0, 3, 0, 0, 0x400);
        obj->render.f.level_fg = true;
        const uint8_t *size = platform2_sizes[((scratch->subtype >> 4) & 0xF) % 5];
        obj->width_pixels = size[0];
        obj->frame = 0; // (frames 1 to 4 do not exist in its mappings)
        (void)size[1];
        obj->priority = 4;
        scratch->base_x = obj->pos.l.x.f.u;
        scratch->base_y = obj->pos.l.y.f.u;
        scratch->subtype &= 0xF;
    }

    int16_t old_x = obj->pos.l.x.f.u;
    if (!Platform2_Move(obj, scratch)) {
        if (IS_OFFSCREEN(scratch->base_x))
            ObjectDelete(obj);
        return;
    }
    Platforms_Carry(obj, obj->width_pixels, 0x10, old_x);
    if (IS_OFFSCREEN(scratch->base_x)) {
        ObjectDelete(obj);
        return;
    }
    DisplaySprite(obj);
}
