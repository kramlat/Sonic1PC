// Metropolis' other objects for Sonic 2 (the Simon Wai prototype's 67, 6C, 6E, 6F, 70 and 72; the first batch is in MTZObjects.c): the teleporter that lifts a character and sends him along a path (67), the platforms that
// go round a path of points (6C), the machinery (6E) and the gears (70) that go round to the oscillators, the parallelogram elevators (6F) and the conveyor belts (72).
#include "Object/MTZObjects.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Game.h"
#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Object/CharControl.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Oscillatory Routines.h"
#include "Solid.h"
#include "Sound.h"

#include "Macros.h"

#include "Resource/Mappings/MTZElevator.h"
#include "Resource/Mappings/MTZGear.h"
#include "Resource/Mappings/MTZMachine.h"
#include "Resource/Mappings/MTZMovingPlatform.h"
#include "Resource/Mappings/MTZTeleport.h"

extern Oscillatory oscillatory;

#define FOR_EACH_CHARACTER(chr, who) \
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) \
        for (Sst *chr = Character(who); chr != NULL; chr = NULL)

static Sst *Character(int who) {
    return who == SolidChar_Sonic ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL);
}

// The oscillators of the prototype's Oscillating_Data, by their offset (4 bytes each): the high byte of the value
static int16_t Osc(int offset) {
    return (int16_t)(uint8_t)(oscillatory.state[offset >> 2][0] >> 8);
}

static void ForgetIfRemembered(Sst *self) {
    if (self->respawn)
        (*SstLoad_State(self)) &= 0x7F;
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 67: the teleporter. A character who walks into it (in the strip of $10 pixels at its place, anywhere $20 above or below) is taken: he is rolled up, lifted out of the ground a few pixels and let drop, and then
// carried along the path the subtype's low nibble names (a negative subtype goes it backwards, then with the nibble negated) at $10 pixels a frame along the longer axis. Subtype bit 4 keeps his speed when he lands.
// Each character has a state of his own: 0 free, 2 being lifted (his angle runs up to $80), 4 on the path
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t state;
    uint8_t angle;  // how far through the lift he is
    int8_t time;    // frames left on this part of the path
    uint8_t pad;
    int16_t count;  // bytes of the path still to travel
    uint16_t pos;   // the word of the path the next point is at
    uint8_t pad1[2];
} TeleportChar;

typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad0[3];   // 0x29-0x2B
    TeleportChar chr[2]; // 0x2C and 0x36
} Own_Teleport;

enum { TeleportState_Free = 0, TeleportState_Lift = 2, TeleportState_Path = 4 };

// The paths (loc_1B35A): each point as x and y, and the bytes of each
static const uint8_t gear_sizes[32] = {
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0C, 0x10, 0x08, 0x10, 0x0C, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
};

static const struct { int8_t x, y; uint8_t frame; } gear_positions[32] = {
    { 0, -72, 0 }, { 50, -50, 4 }, { 72, 0, 8 }, { 50, 50, 12 },
    { 0, 72, 16 }, { -50, 50, 20 }, { -72, 0, 24 }, { -50, -50, 28 },
    { 13, -72, 1 }, { 63, -38, 5 }, { 72, 12, 9 }, { 39, 60, 13 },
    { -13, 72, 17 }, { -63, 38, 21 }, { -72, -12, 25 }, { -39, -60, 29 },
    { 25, -68, 2 }, { 70, -23, 6 }, { 70, 23, 10 }, { 25, 68, 14 },
    { -25, 68, 18 }, { -70, 23, 22 }, { -70, -23, 26 }, { -25, -68, 30 },
    { 39, -60, 3 }, { 72, -12, 7 }, { 63, 38, 11 }, { 13, 72, 15 },
    { -39, 60, 19 }, { -72, 12, 23 }, { -63, -38, 27 }, { -13, -72, 31 },
};

static const int16_t moving_paths[3][20] = {
    { 0, 0, -22, 10, -32, 32, -32, 224, -22, 246, 0, 256, 22, 246, 32, 224, 32, 32, 22, 10 },
    { 0, 0, -22, 10, -32, 32, -32, 352, -22, 374, 0, 384, 22, 374, 32, 352, 32, 32, 22, 10 },
    { 0, 0, -22, 10, -32, 32, -32, 480, -22, 502, 0, 512, 22, 502, 32, 480, 32, 32, 22, 10 },
};

static const int16_t moving_spawns[3][8][3] = {
    { { 0, 0, 0x01 }, { -32, 58, 0x03 }, { -32, 128, 0x03 }, { -32, 198, 0x03 }, { 0, 256, 0x06 }, { 32, 198, 0x08 }, { 32, 128, 0x08 }, { 32, 58, 0x08 } },
    { { 0, 0, 0x11 }, { -32, 90, 0x13 }, { -32, 192, 0x13 }, { -32, 294, 0x13 }, { 0, 384, 0x16 }, { 32, 294, 0x18 }, { 32, 192, 0x18 }, { 32, 90, 0x18 } },
    { { 0, 0, 0x21 }, { -32, 122, 0x23 }, { -32, 256, 0x23 }, { -32, 390, 0x23 }, { 0, 512, 0x26 }, { 32, 390, 0x28 }, { 32, 256, 0x28 }, { 32, 122, 0x28 } },
};

static const int16_t elevator_paths[5][8][3] = {
    { { 256, -128, 256 }, { -256, 128, 256 } },
    { { 256, -128, 384 }, { -256, 128, 384 } },
    { { -256, 128, 128 }, { -256, 0, 384 }, { 256, -128, 128 }, { 256, 0, 384 } },
    { { 256, -128, 512 }, { 256, 0, 256 }, { -256, 128, 256 }, { 256, 0, 384 }, { -256, 0, 384 }, { 256, -128, 256 }, { -256, 0, 256 }, { -256, 128, 512 } },
    { { -256, 128, 384 }, { 256, 0, 512 }, { -256, 0, 512 }, { 256, -128, 384 } },
};
static const uint8_t elevator_path_sizes[5] = { 12, 12, 24, 48, 24 };

static const int8_t elevator_slope[256] = {
    -31, 1, -30, 2, -29, 3, -28, 4, -27, 5, -26, 6, -25, 7, -24, 8,
    -23, 9, -22, 10, -21, 11, -20, 12, -19, 13, -18, 14, -17, 15, -16, 16,
    -15, 17, -14, 18, -13, 19, -12, 20, -11, 21, -10, 22, -9, 23, -8, 24,
    -7, 25, -6, 26, -5, 27, -4, 28, -3, 29, -2, 30, -1, 31, 0, 32,
    1, 33, 2, 34, 3, 35, 4, 36, 5, 37, 6, 38, 7, 39, 8, 40,
    9, 41, 10, 42, 11, 43, 12, 44, 13, 45, 14, 46, 15, 47, 16, 48,
    17, 49, 18, 50, 19, 51, 20, 52, 21, 53, 22, 54, 23, 55, 24, 56,
    25, 57, 26, 58, 27, 59, 28, 60, 29, 61, 30, 62, 31, 63, 32, 64,
    32, 64, 32, 63, 32, 62, 32, 61, 32, 60, 32, 59, 32, 58, 32, 57,
    32, 56, 32, 55, 32, 54, 32, 53, 32, 52, 32, 51, 32, 50, 32, 49,
    32, 48, 32, 47, 32, 46, 32, 45, 32, 44, 32, 43, 32, 42, 32, 41,
    32, 40, 32, 39, 32, 38, 32, 37, 32, 36, 32, 35, 32, 34, 32, 33,
    32, 32, 32, 31, 32, 30, 32, 29, 32, 28, 32, 27, 32, 26, 32, 25,
    32, 24, 32, 23, 32, 22, 32, 21, 32, 20, 32, 19, 32, 18, 32, 17,
    32, 16, 32, 15, 32, 14, 32, 13, 32, 12, 32, 11, 32, 10, 32, 9,
    32, 8, 32, 7, 32, 6, 32, 5, 32, 4, 32, 3, 32, 2, 32, 1,
};

static const uint16_t teleport_paths[13][12] = { // (the alpha's Teleport_From_To_Data, $01C192: Metropolis's paths were laid out again with its levels)
    { 0x07A8, 0x0270, 0x0750, 0x0270, 0x0740, 0x0280, 0x0740, 0x03E0, 0x0750, 0x03F0, 0x07A8, 0x03F0 },
    { 0x0C58, 0x05F0, 0x0E28, 0x05F0 },
    { 0x1828, 0x06B0, 0x17D0, 0x06B0, 0x17C0, 0x06C0, 0x17C0, 0x07E0, 0x17B0, 0x07F0, 0x1758, 0x07F0 },
    { 0x05D8, 0x0370, 0x0780, 0x0370 },
    { 0x05D8, 0x05F0, 0x0700, 0x05F0 },
    { 0x0BD8, 0x01F0, 0x0C30, 0x01F0, 0x0C40, 0x01E0, 0x0C40, 0x00C0, 0x0C50, 0x00B0, 0x0CA8, 0x00B0 },
    { 0x1728, 0x0330, 0x15D0, 0x0330, 0x15C0, 0x0320, 0x15C0, 0x0240, 0x15D0, 0x0230, 0x1628, 0x0230 },
    { 0x06D8, 0x01F0, 0x0730, 0x01F0, 0x0740, 0x01E0, 0x0740, 0x0100, 0x0750, 0x00F0, 0x07A8, 0x00F0 },
    { 0x07D8, 0x0330, 0x0828, 0x0330, 0x0840, 0x0340, 0x0840, 0x0458, 0x0828, 0x0470, 0x07D8, 0x0470 },
    { 0x0FD8, 0x03B0, 0x1028, 0x03B0, 0x1040, 0x0398, 0x1040, 0x02C4, 0x1058, 0x02B0, 0x10A8, 0x02B0 },
    { 0x0FD8, 0x04B0, 0x1028, 0x04B0, 0x1040, 0x04C0, 0x1040, 0x05D8, 0x1058, 0x05F0, 0x10A8, 0x05F0 },
    { 0x2058, 0x0430, 0x20A8, 0x0430, 0x20C0, 0x0418, 0x20C0, 0x02C0, 0x20D0, 0x02B0, 0x2128, 0x02B0 },
    { 0x2328, 0x05B0, 0x22D0, 0x05B0, 0x22C0, 0x05A0, 0x22C0, 0x04C0, 0x22D0, 0x04B0, 0x2328, 0x04B0 },
};
static const uint8_t teleport_path_bytes[13] = { 24, 8, 24, 8, 8, 24, 24, 24, 24, 24, 24, 24, 24 };

static const uint8_t Animation_MTZTeleport[] = {
    0x00, 0x04, 0x00, 0x07,
    0x1F, 0x00, 0xFF,
    0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0xFE, 0x02,
};

#define TELEPORT_SPEED 0x1000

// loc_1B2DC: sets the character's speed to go from where he is to (tx, ty) at TELEPORT_SPEED along the longer axis, and how many frames it takes
static void Teleport_Aim(Sst *chr, TeleportChar *tc, int16_t tx, int16_t ty) {
    int16_t sx = TELEPORT_SPEED, sy = TELEPORT_SPEED;
    int16_t dx = (int16_t)(tx - chr->x_pos.f.u), dy = (int16_t)(ty - chr->y_pos.f.u);
    int16_t ax = dx, ay = dy;
    if (dx < 0) {
        ax = (int16_t)-dx;
        sx = (int16_t)-sx;
    }
    if (dy < 0) {
        ay = (int16_t)-dy;
        sy = (int16_t)-sy;
    }
    if ((uint16_t)ay >= (uint16_t)ax) { // along y
        int16_t frames = (int16_t)(((int32_t)dy * 65536) / sy);
        int16_t vx = 0;
        if (dx != 0 && frames != 0)
            vx = (int16_t)(((int32_t)dx * 65536) / frames);
        chr->x_vel = vx;
        chr->y_vel = sy;
        tc->time = (int8_t)((uint16_t)(frames < 0 ? -frames : frames) >> 8);
    } else { // along x
        int16_t frames = (int16_t)(((int32_t)dx * 65536) / sx);
        int16_t vy = 0;
        if (dy != 0 && frames != 0)
            vy = (int16_t)(((int32_t)dy * 65536) / frames);
        chr->y_vel = vy;
        chr->x_vel = sx;
        tc->time = (int8_t)((uint16_t)(frames < 0 ? -frames : frames) >> 8);
    }
}

// loc_1B278: the path starts at its first point (or, for a negative subtype, its last)
static void Teleport_Start(Sst *self, Own_Teleport *own, Sst *chr, TeleportChar *tc) {
    const bool back = (int8_t)self->subtype < 0;
    const int number = (back ? -(int8_t)self->subtype : self->subtype) & 0xF;
    const uint16_t *p = teleport_paths[number];
    const int words = teleport_path_bytes[number] / 2;
    tc->count = (int16_t)(teleport_path_bytes[number] - 4);
    if (back) {
        chr->x_pos.f.u = (int16_t)p[words - 2];
        chr->y_pos.f.u = (int16_t)p[words - 1];
        tc->pos = (uint16_t)(words - 4);
    } else {
        chr->x_pos.f.u = (int16_t)p[0];
        chr->y_pos.f.u = (int16_t)p[1];
        tc->pos = 2;
    }
    Teleport_Aim(chr, tc, (int16_t)p[tc->pos], (int16_t)p[tc->pos + 1]);
}

static void Teleport_Step(Sst *self, Own_Teleport *own, Sst *chr, TeleportChar *tc) {
    switch (tc->state) {
    case TeleportState_Free: { // loc_1B138
        if (debug_use)
            return;
        int16_t d0 = (int16_t)(chr->x_pos.f.u - self->x_pos.f.u + 3);
        if (self->status & 1)
            d0 += 0x0A;
        if ((uint16_t)d0 >= 0x10)
            return;
        int16_t d1 = (int16_t)(chr->y_pos.f.u - self->y_pos.f.u + 0x20);
        if ((uint16_t)d1 >= 0x40)
            return;
        if (OBJ_CONTROL(chr) != 0)
            return;
        tc->state += 2;
        OBJ_CONTROL(chr) = 0x81;
        chr->anim = SonAnimId_Roll;
        chr->ground_vel = 0x800;
        chr->x_vel = 0;
        chr->y_vel = 0;
        self->status &= (uint8_t)~0x20;
        chr->status &= (uint8_t)~SST_PSTATUS_PUSHING;
        chr->status |= SST_PSTATUS_IN_AIR;
        chr->x_pos.f.u = self->x_pos.f.u;
        chr->y_pos.f.u = self->y_pos.f.u;
        tc->angle = 0;
        PlaySound(sfx_Roll);
        self->anim = 1;
        self->prev_anim = 0;
        return;
    }
    case TeleportState_Lift: { // loc_1B1C8: up a few pixels and back down
        int16_t sin, cos;
        CalcSine(tc->angle, &sin, &cos);
        tc->angle = (uint8_t)(tc->angle + 2);
        chr->y_pos.f.u = (int16_t)(self->y_pos.f.u - (sin >> 5));
        if (tc->angle == 0x80) {
            Teleport_Start(self, own, chr, tc);
            tc->state += 2;
            PlaySound(sfx_Teleport);
        }
        return;
    }
    case TeleportState_Path: { // loc_1B1FC
        if (--tc->time >= 0) {
            chr->x_pos.v += chr->x_vel * 256;
            chr->y_pos.v += chr->y_vel * 256;
            return;
        }
        const bool back = (int8_t)self->subtype < 0;
        const int number = (back ? -(int8_t)self->subtype : self->subtype) & 0xF;
        const uint16_t *p = teleport_paths[number];
        chr->x_pos.f.u = (int16_t)p[tc->pos];
        chr->y_pos.f.u = (int16_t)p[tc->pos + 1];
        tc->pos = (uint16_t)(tc->pos + (back ? -2 : 2));
        tc->count -= 4;
        if (tc->count == 0) { // loc_1B256: the end
            chr->y_pos.f.u &= 0x07FF;
            tc->state = TeleportState_Free;
            OBJ_CONTROL(chr) = 0;
            if (!(self->subtype & 0x10)) {
                chr->x_vel = 0;
                chr->y_vel = 0;
            }
            return;
        }
        Teleport_Aim(chr, tc, (int16_t)p[tc->pos], (int16_t)p[tc->pos + 1]);
        return;
    }
    }
}

void Obj_MTZTeleport(Sst *self) {
    Own_Teleport *own = (Own_Teleport *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_MTZTeleport;
        self->art_tile = TILE_MAP(0, 3, 0, 0, 0x33C);
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 0x10;
        self->priority = 5 * 0x80;
    } else {
        Teleport_Step(self, own, player, &own->chr[0]);
        if (TAILS_OBJ->code != NULL)
            Teleport_Step(self, own, TAILS_OBJ, &own->chr[1]);
    }

    if (own->chr[0].state == 0 && own->chr[1].state == 0) { // (MarkObjGone3: nothing is drawn while it is idle)
        if (SstOutOfRange(self->x_pos.f.u)) {
            ForgetIfRemembered(self);
            Sst_Delete(self);
        }
        return;
    }
    Sst_Animate(self, Animation_MTZTeleport);
    Sst_Draw(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 6C: the moving platforms. A platform goes from point to point of a path of ten (relative to where it was placed), one pixel a frame along the longer axis, one way round or, flipped, the other; the subtype's low
// nibble is the point it starts at and the high bits the path. A subtype with bit 7 is no platform but makes a ring of eight of them (a list of places and subtypes for each of three), itself the first
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad0[7];   // 0x29-0x2F
    int16_t base_x;    // 0x30
    int16_t base_y;    // 0x32
    int16_t goal_x;    // 0x34
    int16_t goal_y;    // 0x36
    uint8_t index;     // 0x38: the byte of the path the goal is at
    uint8_t size;      // 0x39: bytes of the path
    int8_t step;       // 0x3A: 4 or -4
    uint8_t pad1;      // 0x3B
    uint8_t path;      // 0x3C: which of the three
} Own_MovingPlatform;

// loc_1C112: sets the speed to go to the goal at a pixel a frame along the longer axis; the shorter axis is given the fraction (of 256) it moves each frame, and the remainder as where in the pixel it starts
static void MovingPlatform_Aim(Sst *self, Own_MovingPlatform *own) {
    int16_t dx = (int16_t)(self->x_pos.f.u - own->goal_x);
    int16_t dy = (int16_t)(self->y_pos.f.u - own->goal_y);
    int16_t sx = -0x100, sy = -0x100;
    uint16_t ax = (uint16_t)dx, ay = (uint16_t)dy;
    if ((uint16_t)self->x_pos.f.u < (uint16_t)own->goal_x) { // (the carry of the subtraction: it is to the left of its goal)
        ax = (uint16_t)-dx;
        sx = 0x100;
    }
    if ((uint16_t)self->y_pos.f.u < (uint16_t)own->goal_y) {
        ay = (uint16_t)-dy;
        sy = 0x100;
    }
    if (ay >= ax) { // along y
        int16_t vx = 0;
        uint16_t sub = 0;
        if (dx != 0) {
            const int32_t dividend = (int32_t)dx * 256;
            const int32_t quotient = dividend / (int16_t)ay, remainder = dividend % (int16_t)ay;
            vx = (int16_t)-quotient;
            sub = (uint16_t)remainder;
        }
        self->x_vel = vx;
        self->y_vel = sy;
        self->x_pos.v = (int32_t)(((uint32_t)self->x_pos.v & 0xFFFF0000u) | sub);
        self->y_pos.v = (int32_t)((uint32_t)self->y_pos.v & 0xFFFF0000u);
    } else { // along x
        int16_t vy = 0;
        uint16_t sub = 0;
        if (dy != 0) {
            const int32_t dividend = (int32_t)dy * 256;
            const int32_t quotient = dividend / (int16_t)ax, remainder = dividend % (int16_t)ax;
            vy = (int16_t)-quotient;
            sub = (uint16_t)remainder;
        }
        self->y_vel = vy;
        self->x_vel = sx;
        self->y_pos.v = (int32_t)(((uint32_t)self->y_pos.v & 0xFFFF0000u) | sub);
        self->x_pos.v = (int32_t)((uint32_t)self->x_pos.v & 0xFFFF0000u);
    }
}

// loc_1C0B6: at its goal it picks the next point; and always it moves
static void MovingPlatform_Move(Sst *self, Own_MovingPlatform *own) {
    if (self->x_pos.f.u == own->goal_x && self->y_pos.f.u == own->goal_y) {
        uint8_t next = (uint8_t)(own->index + own->step);
        if (next >= own->size)
            next = (next & 0x80) ? (uint8_t)(own->size - 4) : 0;
        own->index = next;
        const int16_t *point = &moving_paths[own->path][next / 2];
        own->goal_x = (int16_t)(point[0] + own->base_x);
        own->goal_y = (int16_t)(point[1] + own->base_y);
        MovingPlatform_Aim(self, own);
    }
    Sst_MoveSprite2(self);
}

void Obj_MTZMovingPlatform(Sst *self) {
    Own_MovingPlatform *own = (Own_MovingPlatform *)self->own.u8;

    if (self->routine == 0) {
        if (self->subtype & 0x80) { // loc_1C04A: a ring of platforms
            const int list = self->subtype & 0x7F;
            const int16_t x = self->x_pos.f.u, y = self->y_pos.f.u;
            Sst *platform = self;
            for (int i = 0; i < 8; i++) {
                if (i > 0) {
                    platform = Sst_Allocate();
                    if (platform == NULL)
                        break;
                }
                const int16_t *entry = moving_spawns[list % 3][i];
                platform->code = sst_object_codes[0x6C];
                platform->x_pos.f.u = (int16_t)(x + entry[0]);
                platform->y_pos.f.u = (int16_t)(y + entry[1]);
                Own_MovingPlatform *other = (Own_MovingPlatform *)platform->own.u8;
                other->base_x = x;
                other->base_y = y;
                platform->subtype = (uint8_t)entry[2];
                platform->status = self->status;
            }
            return; // (and nothing is drawn)
        }
        self->routine += 2;
        self->mappings = Mappings_MTZMovingPlatform;
        self->art_tile = TILE_MAP(0, 3, 0, 0, 0x3F9);
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 0x10;
        self->priority = 4 * 0x80;
        self->mapping_frame = 0;
        own->path = (uint8_t)((self->subtype >> 4) % 3);
        own->size = 0x28;
        uint8_t index = (uint8_t)((self->subtype & 0xF) << 2);
        own->step = 4;
        if (self->status & 1) {
            own->step = -4;
            uint8_t next = (uint8_t)(index + own->step);
            if (next >= own->size)
                next = (next & 0x80) ? (uint8_t)(own->size - 4) : 0;
            index = next;
        }
        own->index = index;
        const int16_t *point = &moving_paths[own->path][index / 2];
        own->goal_x = (int16_t)(point[0] + own->base_x);
        own->goal_y = (int16_t)(point[1] + own->base_y);
        MovingPlatform_Aim(self, own);
    }

    const int16_t old_x = self->x_pos.f.u;
    MovingPlatform_Move(self, own);
    FOR_EACH_CHARACTER(chr, who)
        Solid_Platform(self, chr, who, 0x10, 8, old_x);

    if (SstOutOfRange(own->base_x)) { // (it is not forgotten: it is gone for good)
        Sst_Delete(self);
        return;
    }
    Sst_Draw(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 6E: the machine. A solid block (three sizes by the subtype's high bits) or, in the fourth, a part that is only for show, that goes round a square: the oscillators say where, the subtype's bit 0 turns it
// the other way up and bit 1 mirrors it
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad0[7];   // 0x29-0x2F
    int16_t base_y;    // 0x30
    uint8_t pad1[2];   // 0x32-0x33
    int16_t base_x;    // 0x34
} Own_Machine;

enum { MachineRoutine_Init = 0, MachineRoutine_Solid = 2, MachineRoutine_Show = 4 };

// loc_1C2F8: the width and height of each (and the frame, 0-3)
static const uint8_t machine_sizes[4][2] = { { 0x10, 0x0C }, { 0x28, 0x08 }, { 0x60, 0x18 }, { 0x0C, 0x0C } };

static void Machine_Place(Sst *self, Own_Machine *own, int16_t d1, int16_t d2) {
    if (self->subtype & 1) {
        d1 = (int16_t)-d1;
        d2 = (int16_t)-d2;
    }
    if (self->subtype & 2) {
        d1 = (int16_t)-d1;
        const int16_t swap = d1;
        d1 = d2;
        d2 = swap;
    }
    self->x_pos.f.u = (int16_t)(own->base_x + d1);
    self->y_pos.f.u = (int16_t)(own->base_y + d2);
}

void Obj_MTZMachine(Sst *self) {
    Own_Machine *own = (Own_Machine *)self->own.u8;

    if (self->routine == MachineRoutine_Init) {
        self->routine += 2;
        self->mappings = Mappings_MTZMachine;
        self->art_tile = TILE_MAP(0, 3, 0, 0, 0);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        const int size = (self->subtype >> 4) & 3;
        self->width_pixels = machine_sizes[size][0];
        self->y_radius = (int8_t)machine_sizes[size][1];
        self->mapping_frame = (uint8_t)size;
        own->base_x = self->x_pos.f.u;
        own->base_y = self->y_pos.f.u;
        if (size == 3) {
            self->routine = MachineRoutine_Show;
            self->art_tile = TILE_MAP(0, 3, 0, 0, 0x3F0);
            self->priority = 5 * 0x80;
        }
    }

    if (self->routine == MachineRoutine_Solid) {
        const int16_t old_x = self->x_pos.f.u;
        Machine_Place(self, own, (int16_t)(int8_t)(Osc(0x20) - 0x38), (int16_t)(int8_t)(Osc(0x24) - 0x38));
        if ((self->render_flags & SST_RENDER_ON_SCREEN)) {
            FOR_EACH_CHARACTER(chr, who)
                Solid_Character(self, chr, who, (int16_t)(self->width_pixels + 0xB), self->y_radius, (int16_t)(self->y_radius + 1), old_x, NULL);
        }
    } else {
        Machine_Place(self, own, (int16_t)(int8_t)((uint8_t)(Osc(0x20) >> 1) - 0x1C), (int16_t)(int8_t)((uint8_t)(Osc(0x24) >> 1) - 0x1C));
    }

    if (SstOutOfRange(own->base_x)) {
        ForgetIfRemembered(self);
        Sst_Delete(self);
        return;
    }
    Sst_Draw(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 6F: the parallelogram elevators. A very wide ($100) slab with a sloping top and bottom that stands still (mode 0), waits to be stood on (1) and then goes along a path of steps (a speed and how many
// frames) for ever (2): the subtype's high nibble is the path, the low one the mode
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28: the mode
    uint8_t pad0[7];   // 0x29-0x2F
    int16_t base_y;    // 0x30
    int16_t base_x;    // 0x32
    int16_t timer;     // 0x34: frames left of the step
    uint8_t pad1[2];   // 0x36-0x37
    uint8_t step;      // 0x38: the byte of the path the next step is at
    uint8_t pad2[3];   // 0x39-0x3B
    uint8_t path;      // 0x3C
} Own_Elevator;

// loc_1C604: the next step
static void Elevator_NextStep(Sst *self, Own_Elevator *own) {
    const int16_t *step = elevator_paths[own->path][own->step / 6];
    self->x_vel = step[0];
    self->y_vel = step[1];
    own->timer = step[2];
    own->step = (uint8_t)(own->step + 6);
    if (!(elevator_path_sizes[own->path] > own->step))
        own->step = 0;
}

void Obj_MTZElevator(Sst *self) {
    Own_Elevator *own = (Own_Elevator *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_MTZElevator;
        self->art_tile = TILE_MAP(0, 3, 0, 0, 0x53F);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x80;
        self->y_radius = 0x20;
        own->base_x = self->x_pos.f.u;
        own->base_y = self->y_pos.f.u;
        own->path = (uint8_t)(((self->subtype >> 4) & 7) % 5);
        Elevator_NextStep(self, own);
        self->subtype &= 0x0F;
    }

    const int16_t old_x = self->x_pos.f.u;
    switch (self->subtype) {
    case 1:
        if (self->status & 0x18)
            self->subtype++;
        break;
    case 2:
        Sst_MoveSprite2(self);
        if (--own->timer == 0)
            Elevator_NextStep(self, own);
        break;
    }
    FOR_EACH_CHARACTER(chr, who)
        Solid_CharacterDouble(self, chr, who, self->width_pixels, old_x, elevator_slope);

    if (!SstOutOfRange(self->x_pos.f.u)) {
        Sst_Draw(self);
        return;
    }
    if (!SstOutOfRange(own->base_x))
        return;
    ForgetIfRemembered(self);
    Sst_Delete(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 70: the rotating gears. Eight of them (the object itself the first) go round a ring, each a step ahead of the one before, one place round every $10 frames; the subtype's flip makes them go the other way.
// They are solid, and as big as their frame (loc_1C996)
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad0[7];   // 0x29-0x2F
    int16_t base_y;    // 0x30
    int16_t base_x;    // 0x32
    uint16_t column;   // 0x34: which gear it is of the eight, in threes
    uint16_t row;      // 0x36: how far round the ring, in $18
} Own_Gear;

void Obj_MTZGear(Sst *self) {
    Own_Gear *own = (Own_Gear *)self->own.u8;

    if (self->routine == 0) {
        const int16_t x = self->x_pos.f.u, y = self->y_pos.f.u;
        uint16_t column = 0;
        Sst *gear = self;
        self->status |= 0x80;
        for (int i = 0; i < 8; i++) {
            if (i > 0) {
                gear = Sst_AllocateAfter(self);
                if (gear == NULL)
                    break;
            }
            gear->code = self->code;
            gear->routine += 2;
            gear->mappings = Mappings_MTZGear;
            gear->art_tile = TILE_MAP(0, 3, 0, 0, 0x378);
            gear->render_flags = SST_RENDER_LEVEL;
            gear->priority = 4 * 0x80;
            gear->width_pixels = 0x10;
            Own_Gear *other = (Own_Gear *)gear->own.u8;
            other->base_x = x;
            other->base_y = y;
            gear->x_pos.f.u = (int16_t)(x + gear_positions[i].x);
            gear->y_pos.f.u = (int16_t)(y + gear_positions[i].y);
            gear->mapping_frame = gear_positions[i].frame;
            other->column = column;
            column += 3;
            gear->status = self->status;
        }
    }

    const int16_t old_x = self->x_pos.f.u;
    if ((frame_count & 0xF) == 0) {
        int16_t row = (int16_t)own->row;
        if (self->status & 1) {
            row -= 0x18;
            if (row < 0) {
                row = 0x48;
                own->column -= 3;
                if ((int16_t)own->column < 0)
                    own->column = 0x15;
            }
        } else {
            row += 0x18;
            if (row >= 0x60) {
                row = 0;
                own->column += 3;
                if (own->column >= 0x18)
                    own->column = 0;
            }
        }
        own->row = (uint16_t)row;
        const int index = (row + own->column) / 3;
        self->x_pos.f.u = (int16_t)(own->base_x + gear_positions[index].x);
        self->y_pos.f.u = (int16_t)(own->base_y + gear_positions[index].y);
        self->mapping_frame = gear_positions[index].frame;
    }
    const int size = (self->mapping_frame * 2) & 0x1E;
    const int16_t width = gear_sizes[size], height = gear_sizes[size + 1];
    FOR_EACH_CHARACTER(chr, who)
        Solid_Character(self, chr, who, width, height, height, old_x, NULL);

    SstOnScreenTest2(self, own->base_x);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 72: the conveyor belts. Invisible; a character on the ground within the strip it covers (the subtype times $10 either side of it) and the $30 above it is moved two pixels a frame, to the right or, flipped,
// the left
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad0[13];  // 0x29-0x35
    int16_t speed;     // 0x36
    uint8_t range;     // 0x38
} Own_Conveyor;

static void Conveyor_Carry(Sst *self, Own_Conveyor *own, Sst *chr) {
    const int16_t range = own->range;
    int16_t d0 = (int16_t)(chr->x_pos.f.u - self->x_pos.f.u + range);
    if ((uint16_t)d0 >= (uint16_t)(range * 2))
        return;
    int16_t d1 = (int16_t)(chr->y_pos.f.u - self->y_pos.f.u + 0x30);
    if ((uint16_t)d1 >= 0x30)
        return;
    if (((chr->status & SST_PSTATUS_IN_AIR) != 0))
        return;
    chr->x_pos.f.u = (int16_t)(chr->x_pos.f.u + own->speed);
}

void Obj_MTZConveyor(Sst *self) {
    Own_Conveyor *own = (Own_Conveyor *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        own->range = (uint8_t)(self->subtype << 4);
        own->speed = (self->status & 1) ? -2 : 2;
    }
    Conveyor_Carry(self, own, player);
    if (TAILS_OBJ->code != NULL)
        Conveyor_Carry(self, own, TAILS_OBJ);

    if (SstOutOfRange(self->x_pos.f.u)) { // MarkObjGone3
        ForgetIfRemembered(self);
        Sst_Delete(self);
    }
}
