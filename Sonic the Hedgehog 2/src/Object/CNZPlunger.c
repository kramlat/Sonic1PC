// Casino Night's pinball plunger of the Sonic 2 pre-beta (object $85, Data/Objects/Obj_0x85.asm). With the subtype 0 it stands upright (the pre-beta's routine at $0257DA): a character who lands on it is
// held, rolled up, on its head, and while he holds a button (A, B or C) the spring draws back (up to $20 pixels, by one every 4 frames, its head flashing the faster the further it is); when he lets go
// he is thrown straight up at ($38 + $10) * $80. With another subtype it lies at 45 degrees ($02597A: the character sits on it, a button draws it back up to $1C and he is thrown up and sideways); with the
// subtype's top bit set he is thrown along the floor instead. It is drawn as the head (its mapping frame, 1 or flashing 5) and the base (a sub-sprite, frame 2, or 3 when it is far enough back).
#include "Object/CNZPlunger.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Game.h"
#include "Level.h"
#include "MathUtil.h"
#include "Object/CharControl.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Solid.h"
#include "Sound.h"

#include "Macros.h"

#include "Resource/Mappings/CNZPlunger.h"
#include "Resource/Mappings/CNZPlungerDiagonal.h"

#define FOR_EACH_CHARACTER(chr, who) \
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) \
        for (Sst *chr = (who == SolidChar_Sonic) ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL); chr != NULL; chr = NULL)

typedef struct {
    uint8_t subtype;
    uint8_t pad0[5];
    int16_t base_x;   // 0x2E
    uint8_t pad1[2];
    uint8_t slow;     // 0x32: frames to the next step of the draw back
    uint8_t flicker;  // 0x33: frames to the head's next change
    int16_t base_y;   // 0x34
    uint8_t state[2]; // 0x36, 0x37: 0 free, 1 a character on it, 2 let go (Sonic's, Tails')
    int16_t back;     // 0x38: how far it is drawn back
    uint8_t handled;  // 0x3A: a button has been counted this frame
} Own_Plunger;

static uint8_t Held(int who) {
    return (uint8_t)((who == SolidChar_Sonic ? jpad1_hold2 : jpad2_hold) & 0x70);
}

static void Plunger_Capture(Sst *chr, int16_t x) {
    OBJ_CONTROL(chr) = 0x81;
    chr->x_pos.f.u = x;
    chr->x_vel = 0;
    chr->y_vel = 0;
    chr->ground_vel = 0;
    chr->status |= SST_PSTATUS_IN_BALL;
    chr->y_radius = 0xE;
    chr->x_radius = 7;
    chr->anim = SonAnimId_Roll;
}

// The draw back and the head's flashing (what the character who is holding does to it): `limit` is $20 for the upright plunger and $1C for the lying one
static void Plunger_Pull(Sst *self, Own_Plunger *own, int16_t limit) {
    if (own->handled)
        return;
    own->handled = 1;
    if ((int8_t)--own->slow < 0) {
        own->slow = 3;
        if (own->back != limit)
            own->back++;
    }
    if ((int8_t)--own->flicker >= 0) {
        self->mapping_frame = 1;
    } else {
        own->flicker = (uint8_t)(((limit - own->back) & 0xFF) >> 1);
        self->mapping_frame ^= 4;
    }
}

static void Plunger_Release(Sst *self, Own_Plunger *own, Sst *chr, int who, bool upright) {
    const uint8_t stand_bit = (uint8_t)(1 << (3 + who));
    own->state[who] = 0;
    if (!(self->status & stand_bit))
        return;
    self->status &= (uint8_t)~stand_bit;
    if (upright) {
        chr->y_vel = (int16_t)-((own->back + 0x10) << 7);
        chr->x_vel = 0;
        chr->ground_vel = 0x800;
    } else {
        int16_t d0 = (int16_t)((own->back + 4) << 7);
        chr->x_vel = d0;
        d0 = (int16_t)-d0;
        chr->y_vel = d0;
        chr->ground_vel = 0x800;
        if (self->subtype & 0x80) { // (along the floor)
            chr->ground_vel = (int16_t)-d0;
            chr->status &= (uint8_t) ~(SST_PSTATUS_X_FLIP | SST_PSTATUS_IN_AIR);
            chr->angle = 0xE0;
        }
    }
    if (upright || !(self->subtype & 0x80)) {
        chr->status |= SST_PSTATUS_IN_AIR;
    }
    chr->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
    chr->routine = 2;
    OBJ_CONTROL(chr) = 0;
    PlaySound(sfx_AlphaE2);
}

// One character (Offset_0x02587C, and Offset_0x025A2A for the lying one)
static void Plunger_Character(Sst *self, Own_Plunger *own, Sst *chr, int who, bool upright) {
    const uint8_t stand_bit = (uint8_t)(1 << (3 + who));
    const uint8_t push_bit = (uint8_t)(1 << (5 + who));
    const int16_t seat_x = upright ? self->x_pos.f.u : (int16_t)(self->x_pos.f.u + 0x13);
    const int16_t seat_y = (int16_t)(self->y_pos.f.u - (upright ? 0x16 : 0x13));

    for (;;) {
        if (own->state[who] == 0) {
            if (debug_use || chr->y_vel < 0)
                return;
            Solid_Character(self, chr, who, upright ? 0x1B : 0x23, 8, 5, self->x_pos.f.u, NULL);
            if (!(self->status & stand_bit)) {
                if (upright || !(self->status & push_bit))
                    return;
                self->status |= stand_bit; // (the lying one takes him when he pushes at it, too)
            }
            Plunger_Capture(chr, seat_x);
            if (!upright)
                chr->y_pos.f.u = seat_y;
            own->state[who]++;
            return;
        }
        if (own->state[who] == 1) {
            if (Held(who))
                Plunger_Pull(self, own, upright ? 0x20 : 0x1C);
            chr->y_pos.f.u = seat_y;
            if (!upright)
                chr->x_pos.f.u = seat_x;
            return;
        }
        // let go
        own->state[who] = 0;
        if (!(self->status & stand_bit)) {
            continue; // (not on it after all: it is free for another to take)
        }
        self->status |= stand_bit;
        Plunger_Release(self, own, chr, who, upright);
        return;
    }
}

void Obj_CNZPlunger(Sst *self) {
    Own_Plunger *own = (Own_Plunger *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_CNZPlunger;
        self->art_tile = TILE_MAP(0, 0, 0, 0, 0x422);
        if (self->subtype != 0) {
            self->mappings = Mappings_CNZPlungerDiagonal;
            self->art_tile = TILE_MAP(0, 0, 0, 0, 0x402);
        }
        self->render_flags |= SST_RENDER_LEVEL | SST_RENDER_MULTI_DRAW;
        self->priority = 4 * 0x80;
        self->mapping_frame = 1;
        self->child_count = 1;
        if (self->subtype != 0) {
            self->routine += 2;
            self->height_pixels = 0x20;
            self->width_pixels = 0x18;
            own->base_x = self->x_pos.f.u;
            own->base_y = self->y_pos.f.u;
            self->children[0].x = self->x_pos.f.u;
            self->children[0].y = self->y_pos.f.u;
            self->children[0].frame = 2;
        } else {
            self->height_pixels = 0x10;
            self->width_pixels = 0x10;
            self->y_pos.f.u -= 0x18;
            own->base_y = self->y_pos.f.u;
            self->children[0].x = self->x_pos.f.u;
            self->children[0].y = (int16_t)(self->y_pos.f.u + 0x20);
            self->children[0].frame = 2;
        }
    }
    const bool vertical = self->subtype == 0;

    own->handled = 0;
    if (vertical) {
        self->y_pos.f.u = (int16_t)(own->base_y + own->back);
    } else {
        const int16_t half = (int16_t)(own->back >> 1);
        self->x_pos.f.u = (int16_t)(own->base_x - half);
        self->y_pos.f.u = (int16_t)(own->base_y + half);
    }
    self->children[0].frame = own->back >= 0x10 ? 3 : 2;

    Plunger_Character(self, own, player, SolidChar_Sonic, vertical);
    if (TAILS_OBJ->code != NULL)
        Plunger_Character(self, own, TAILS_OBJ, SolidChar_Tails, vertical);

    if (*(uint16_t *)own->state != 0) { // (someone is on it, or has let go)
        if (own->back != 0) {
            uint16_t held = 0;
            if (own->state[0] == 1)
                held |= Held(SolidChar_Sonic);
            if (own->state[1] == 1)
                held |= Held(SolidChar_Tails);
            if (held == 0) { // nobody holds: both are let go
                own->state[0] = 2;
                own->state[1] = 2;
            }
        }
    } else {
        self->mapping_frame = 1;
        own->back -= 4;
        if (own->back < 0)
            own->back = 0;
    }
    SstOnScreenTest(self);
}
