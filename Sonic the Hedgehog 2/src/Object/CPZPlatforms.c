// Chemical Plant's platforms for Sonic 2 (Nick Arcade's objects 0C and 19): the floating platform that bobs and now and then hangs and shakes at the top of its swing (0C), and the platform
// that slides, rises, falls or shuttles by its subtype (19). Both carry Sonic and Tails.
#include "Object/CPZPlatforms.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
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

static void Platforms_Carry(Sst *self, int16_t x_rad, int16_t y_walk, int16_t old_x) {
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
        Sst *chr = (who == SolidChar_Sonic) ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL);
        if (chr != NULL)
            Solid_Platform(self, chr, who, x_rad, y_walk, old_x);
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
} Own_CPZPlatform;

void Obj_CPZPlatform(Sst *self) {
    Own_CPZPlatform *own = (Own_CPZPlatform *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_CPZPlatform;
        self->art_tile = TILE_MAP(1, 3, 0, 0, 0x418);
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 0x10;
        self->priority = 4 * 0x80;
        own->centre_y = self->y_pos.f.u - 0x10;
        own->shakes = own->shakes_max = self->subtype & 0xF;
    }

    int16_t sin, cos;
    uint8_t angle = own->swing;
    bool shaking = false;
    uint8_t shake_angle = 0;
    if (angle == 0) {
        if ((frame_count & 0x3FF) == 0) { // (every 1024 frames the swing begins)
            own->shake = 1;
            own->swing++;
        }
    } else if (angle == 0x80) { // at the top of the swing: a moment's shaking, a number of times
        uint8_t shake = own->shake;
        if (shake == 0) {
            if ((int8_t)--own->shakes < 0) {
                own->shakes = own->shakes_max;
                own->swing++;
            } else {
                shaking = true;
            }
        } else {
            shaking = true;
        }
        if (shaking) {
            shake_angle = own->shake;
            own->shake++;
        }
    } else {
        own->swing++;
    }

    if (shaking) {
        CalcSine(shake_angle, &sin, &cos);
        self->y_pos.f.u = (int16_t)(own->centre_y + ((sin + 8) >> 6) - 0x10);
    } else {
        CalcSine(angle, &sin, &cos);
        self->y_pos.f.u = (int16_t)(own->centre_y + ((cos + 8) >> 4));
    }

    Platforms_Carry(self, self->width_pixels, 9, self->x_pos.f.u);
    SstOnScreenTest(self);
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
} Own_CPZPlatform2;

static const uint8_t platform2_sizes[5][2] = { { 0x20, 0 }, { 0x20, 1 }, { 0x20, 2 }, { 0x40, 3 }, { 0x30, 4 } };

// The movement (Obj19_Modes). False if the platform is to be taken no further this frame (the one that waits hidden for a button)
static bool Platform2_Move(Sst *self, Own_CPZPlatform2 *own) {
    switch (self->subtype & 0xF) {
    case 1: { // slides to and fro
        int16_t d0 = (uint8_t)(oscillatory.state[3][0] >> 8);
        if (((self->status & SST_STATUS_X_FLIP) != 0))
            d0 = (int16_t)(0x60 - d0);
        self->x_pos.f.u = (int16_t)(own->base_x - d0);
        break;
    }
    case 2: case 4: case 9: // waits for someone to stand on it, then the next movement
        if (self->status & 0x18)
            self->subtype++;
        break;
    case 3: case 5: // slides right until a wall
        if (SstHitWallRight(self, self->width_pixels) < 0) {
            if ((self->subtype & 0xF) == 3)
                self->subtype = 0;
            else
                self->subtype++;
        } else {
            self->x_pos.f.u++;
            own->base_x = self->x_pos.f.u;
        }
        break;
    case 6: { // falls to the floor
        Sst_MoveSprite2(self);
        self->y_vel += 0x18;
        int16_t d1 = SstFloorDist(self, self->x_pos.f.u);
        if (d1 < 0) {
            self->y_pos.f.u += d1;
            self->y_vel = 0;
            self->subtype = 0;
        }
        break;
    }
    case 7: // hidden until a button is pressed
        if (f_switch[2])
            self->subtype -= 3;
        return false;
    case 8: { // up and down
        int16_t d0 = (uint8_t)(oscillatory.state[7][0] >> 8);
        if (((self->status & SST_STATUS_X_FLIP) != 0))
            d0 = (int16_t)(0x80 - d0);
        self->y_pos.f.u = (int16_t)(own->base_y - d0);
        break;
    }
    case 10: { // shuttles: out to twice its width, waits, and back
        int16_t reach = (int16_t)(self->width_pixels * 2), step = 8;
        if (((self->status & SST_STATUS_X_FLIP) != 0)) {
            step = -step;
            reach = -reach;
        }
        if (own->back == 0) {
            int16_t d0 = self->x_pos.f.u - own->base_x;
            if (d0 != reach) {
                self->x_pos.f.u += step;
                own->wait = 0x12C;
            } else if (--own->wait == 0) {
                own->back = 1;
            }
        } else {
            int16_t d0 = self->x_pos.f.u - own->base_x;
            if (d0 != 0) {
                self->x_pos.f.u -= step;
            } else {
                own->back = 0;
                self->subtype--;
            }
        }
        break;
    }
    }
    return true;
}

void Obj_CPZPlatform2(Sst *self) {
    Own_CPZPlatform2 *own = (Own_CPZPlatform2 *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_CPZPlatform2;
        self->art_tile = TILE_MAP(0, 3, 0, 0, 0x400);
        self->render_flags |= SST_RENDER_LEVEL;
        const uint8_t *size = platform2_sizes[((self->subtype >> 4) & 0xF) % 5];
        self->width_pixels = size[0];
        self->mapping_frame = 0; // (frames 1 to 4 do not exist in its mappings)
        (void)size[1];
        self->priority = 4 * 0x80;
        own->base_x = self->x_pos.f.u;
        own->base_y = self->y_pos.f.u;
        self->subtype &= 0xF;
    }

    int16_t old_x = self->x_pos.f.u;
    if (!Platform2_Move(self, own)) {
        SstDeleteIfNotInRange2(self, own->base_x);
        return;
    }
    Platforms_Carry(self, self->width_pixels, 0x10, old_x);
    SstOnScreenTest2(self, own->base_x);
}
