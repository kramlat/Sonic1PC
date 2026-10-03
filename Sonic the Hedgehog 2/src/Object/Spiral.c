// The corkscrews of Emerald Hill for Sonic 2 (Nick Arcade's object 06, "sloop"): an invisible path, 416 pixels long, that Sonic or Tails run onto at the speed to be carried along it. While they are on it their
// height follows the corkscrew's curve and they turn about as they go (the flip angle that makes them tumble through the frames of a spin). Slower than top speed, or in the air, and they fall off.
#include "Object/Spiral.h"
#include "Constants.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Solid.h"

#include "Macros.h"

static const uint8_t flip_angles[] = {
    0x00, 0x00, 0x01, 0x01, 0x16, 0x16, 0x16, 0x16, 0x2C, 0x2C, 0x2C, 0x2C, 0x42, 0x42, 0x42, 0x42, 0x58, 0x58, 0x58, 0x58, 0x6E, 0x6E, 0x6E, 0x6E, 0x84, 0x84, 0x84, 0x84, 0x9A, 0x9A, 0x9A, 0x9A, 0xB0, 0xB0, 0xB0, 0xB0, 0xC6, 0xC6, 0xC6, 0xC6, 0xDC, 0xDC, 0xDC, 0xDC, 0xF2, 0xF2, 0xF2, 0xF2, 0x01, 0x01, 0x00, 0x00
};

static const int8_t heights[] = {
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 31, 31,
    31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 30, 30, 30,
    30, 30, 30, 30, 30, 30, 29, 29, 29, 29, 29, 28, 28, 28, 28, 27,
    27, 27, 27, 26, 26, 26, 25, 25, 25, 24, 24, 24, 23, 23, 22, 22,
    21, 21, 20, 20, 19, 18, 18, 17, 16, 16, 15, 14, 14, 13, 12, 12,
    11, 10, 10, 9, 8, 8, 7, 6, 6, 5, 4, 4, 3, 2, 2, 1,
    0, -1, -2, -2, -3, -4, -4, -5, -6, -7, -7, -8, -9, -9, -10, -10,
    -11, -11, -12, -12, -13, -14, -14, -15, -15, -16, -16, -17, -17, -18, -18, -19,
    -19, -19, -20, -21, -21, -22, -22, -23, -23, -24, -24, -25, -25, -26, -26, -27,
    -27, -28, -28, -28, -29, -29, -30, -30, -30, -31, -31, -31, -32, -32, -32, -33,
    -33, -33, -33, -34, -34, -34, -35, -35, -35, -35, -35, -35, -35, -35, -36, -36,
    -36, -36, -36, -36, -36, -36, -36, -37, -37, -37, -37, -37, -37, -37, -37, -37,
    -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37, -37,
    -37, -37, -37, -37, -36, -36, -36, -36, -36, -36, -36, -35, -35, -35, -35, -35,
    -35, -35, -35, -34, -34, -34, -33, -33, -33, -33, -32, -32, -32, -31, -31, -31,
    -30, -30, -30, -29, -29, -28, -28, -28, -27, -27, -26, -26, -25, -25, -24, -24,
    -23, -23, -22, -22, -21, -21, -20, -19, -19, -18, -18, -17, -16, -16, -15, -14,
    -14, -13, -12, -11, -11, -10, -9, -8, -7, -7, -6, -5, -4, -3, -2, -1,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 8, 9, 10, 10, 11, 12, 13,
    13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21,
    21, 22, 22, 23, 23, 24, 24, 24, 25, 25, 25, 25, 26, 26, 26, 26,
    27, 27, 27, 27, 28, 28, 28, 28, 28, 28, 29, 29, 29, 29, 29, 29,
    29, 30, 30, 30, 30, 30, 30, 30, 31, 31, 31, 31, 31, 31, 31, 31,
    31, 31, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
};

// One character against the corkscrew (sub_149BC)
static void Spiral_Character(Object *obj, Object *chr, int who) {
    Scratch_Sonic *sscratch = (Scratch_Sonic *)&chr->scratch;
    const uint8_t on_bit = (uint8_t)(1 << (3 + who));

    if (!(obj->status.b & on_bit)) {
        // Not on it yet: running at it on the ground, near enough its end and at the right height (a little less near if he is on another object)
        if (chr->status.p.f.in_air)
            return;
        int16_t d0 = chr->pos.l.x.f.u - obj->pos.l.x.f.u;
        int16_t near = chr->status.p.f.object_stand ? 0xB0 : 0xC0, far = near + 0x10;
        if (chr->xsp < 0) {
            if (d0 < near || d0 > far)
                return;
        } else {
            if (d0 > -near || d0 < -far)
                return;
        }
        int16_t d1 = chr->pos.l.y.f.u - obj->pos.l.y.f.u - 0x10;
        if ((uint16_t)d1 >= 0x30)
            return;
        Solid_Ride(obj, chr, who);
        return;
    }

    // On it: carried along while at top speed on the ground, within its length
    int16_t speed = chr->inertia < 0 ? -chr->inertia : chr->inertia;
    int16_t d0 = chr->pos.l.x.f.u - obj->pos.l.x.f.u + 0xD0;
    if (speed < 0x600 || chr->status.p.f.in_air || d0 < 0 || d0 >= 0x1A0) {
        chr->status.p.f.object_stand = false;
        obj->status.b &= (uint8_t)~on_bit;
        sscratch->flips_remaining = 0;
        sscratch->flip_speed = 4;
        return;
    }
    if (!chr->status.p.f.object_stand)
        return;
    chr->pos.l.y.f.u = obj->pos.l.y.f.u + heights[d0] - (chr->y_rad - 0x13);
    sscratch->flip_angle = flip_angles[(d0 >> 3) & 0x3F];
}

void Obj_Spiral(Object *obj) {
    if (obj->routine == 0) {
        obj->routine += 2;
        obj->width_pixels = 0xD0;
    }

    Spiral_Character(obj, player, SolidChar_Sonic);
    if (TAILS_OBJ->type != 0)
        Spiral_Character(obj, TAILS_OBJ, SolidChar_Tails);

    if (IS_OFFSCREEN(obj->pos.l.x.f.u))
        ObjectDelete(obj);
}
