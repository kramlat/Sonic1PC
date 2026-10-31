// The corkscrews of Emerald Hill and the rotating cylinders of Metropolis for Sonic 2 (Nick Arcade's object 06, "sloop"; the cylinders are the ones with bit 7 in their subtype). The corkscrew is an invisible path, 416 pixels long, that Sonic or Tails run onto at the speed to be carried along it. While they are on it their
// height follows the corkscrew's curve and they turn about as they go (the flip angle that makes them tumble through the frames of a spin). Slower than top speed, or in the air, and they fall off.
#include "Object/Spiral.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Solid.h"

#include "MathUtil.h"
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

typedef struct {
    uint8_t subtype; // 0x28: bit 7 makes it a rotating cylinder
} Own_Spiral;

// One character against the corkscrew (sub_149BC)
static void Spiral_Character(Sst *self, Sst *chr, int who) {
    Scratch_Sonic *sscratch = (Scratch_Sonic *)chr->own.u8;
    const uint8_t on_bit = (uint8_t)(1 << (3 + who));

    if (!(self->status & on_bit)) {
        // Not on it yet: running at it on the ground, near enough its end and at the right height (a little less near if he is on another object)
        if (((chr->status & SST_PSTATUS_IN_AIR) != 0))
            return;
        int16_t d0 = chr->x_pos.f.u - self->x_pos.f.u;
        int16_t near = ((chr->status & SST_PSTATUS_OBJECT_STAND) != 0) ? 0xB0 : 0xC0, far = near + 0x10;
        if (chr->x_vel < 0) {
            if (d0 < near || d0 > far)
                return;
        } else {
            if (d0 > -near || d0 < -far)
                return;
        }
        int16_t d1 = chr->y_pos.f.u - self->y_pos.f.u - 0x10;
        if ((uint16_t)d1 >= 0x30)
            return;
        Solid_Ride(self, chr, who);
        return;
    }

    // On it: carried along while at top speed on the ground, within its length
    int16_t speed = chr->ground_vel < 0 ? -chr->ground_vel : chr->ground_vel;
    int16_t d0 = chr->x_pos.f.u - self->x_pos.f.u + 0xD0;
    if (speed < 0x600 || ((chr->status & SST_PSTATUS_IN_AIR) != 0) || d0 < 0 || d0 >= 0x1A0) {
        chr->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
        self->status &= (uint8_t)~on_bit;
        sscratch->flips_remaining = 0;
        sscratch->flip_speed = 4;
        return;
    }
    if (!((chr->status & SST_PSTATUS_OBJECT_STAND) != 0))
        return;
    chr->y_pos.f.u = self->y_pos.f.u + heights[d0] - (chr->y_radius - 0x13);
    sscratch->flip_angle = flip_angles[(d0 >> 3) & 0x3F];
}

// The rotating cylinder of Metropolis (the alpha's routine at $0166E6): a character that runs into it, level with its middle, is taken round it, higher and lower as the cosine of an angle that goes on by 4 a frame and with
// his flip angle the same (so he turns as the cylinder does); at its ends, or if he jumps, he is let go (in the air, and a jump up from it is not kept: the speed is halved or lost by where on the cylinder he was). One angle
// for each character, in memory of their own (the alpha's $F7B2 and $F7B3)
static uint8_t cylinder_angle[2];

static void Cylinder_Release(Sst *self, Sst *chr, uint8_t on_bit) {
    Scratch_Sonic *sscratch = (Scratch_Sonic *)chr->own.u8;
    chr->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
    self->status &= (uint8_t)~on_bit;
    sscratch->flips_remaining = 0;
    sscratch->flip_speed = 4;
    chr->status |= SST_PSTATUS_IN_AIR;
}

static void Cylinder_Character(Sst *self, Sst *chr, int who) {
    const uint8_t on_bit = (uint8_t)(1 << (3 + who));
    uint8_t *angle = &cylinder_angle[who];
    Scratch_Sonic *sscratch = (Scratch_Sonic *)chr->own.u8;

    if (!(self->status & on_bit)) {
        // Not on it: level with its middle, at the right height
        const int16_t dx = (int16_t)(chr->x_pos.f.u - self->x_pos.f.u);
        if (dx < -0xC0 || dx >= 0xC0)
            return;
        const uint16_t top = (uint16_t)(self->y_pos.f.u + 0x3C);
        const uint16_t feet = (uint16_t)(chr->y_pos.f.u + (int8_t)chr->y_radius + 4);
        if (top > feet)
            return;
        const uint16_t d0 = (uint16_t)(top - feet);
        if (d0 < 0xFFF0)
            return;
        if (chr->routine >= 6)
            return;
        chr->y_pos.f.u = (int16_t)(chr->y_pos.f.u + (int16_t)d0 + 3);
        Solid_Ride(self, chr, who);
        chr->anim = 0;
        chr->prev_anim = 1;
        *angle = 0;
        if (chr->ground_vel == 0)
            chr->ground_vel = 1;
        return;
    }

    if (((chr->status & SST_PSTATUS_IN_AIR) != 0)) { // jumped off it: how much of his upward speed he keeps depends on where on it he was
        const uint8_t d0 = (uint8_t)(*angle + 0x20);
        if (d0 < 0x40)
            chr->y_vel = (int16_t)(chr->y_vel >> 1);
        else
            chr->y_vel = 0;
        Cylinder_Release(self, chr, on_bit);
        return;
    }
    const int16_t span = (int16_t)(chr->x_pos.f.u - self->x_pos.f.u + 0xC0);
    if (span < 0 || span >= 0x180) { // off an end
        Cylinder_Release(self, chr, on_bit);
        return;
    }
    if (!((chr->status & SST_PSTATUS_OBJECT_STAND) != 0))
        return;
    int16_t sin, cos;
    CalcSine(*angle, &sin, &cos);
    chr->y_pos.f.u = (int16_t)(self->y_pos.f.u + ((cos * 0x2800) >> 16) - ((int8_t)chr->y_radius - 0x13));
    sscratch->flip_angle = *angle;
    *angle = (uint8_t)(*angle + 4);
    if (chr->ground_vel == 0)
        chr->ground_vel = 1;
}

void Obj_Spiral(Sst *self) {
    Own_Spiral *own = (Own_Spiral *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->width_pixels = 0xD0;
        if (self->subtype & 0x80) // a rotating cylinder (Metropolis)
            self->routine += 2;
    }

    if (self->routine == 4) {
        Cylinder_Character(self, player, SolidChar_Sonic);
        if (TAILS_OBJ->code != NULL)
            Cylinder_Character(self, TAILS_OBJ, SolidChar_Tails);
    } else {
        Spiral_Character(self, player, SolidChar_Sonic);
        if (TAILS_OBJ->code != NULL)
            Spiral_Character(self, TAILS_OBJ, SolidChar_Tails);
    }

    SstDeleteIfNotInRange2(self, self->x_pos.f.u);
}
