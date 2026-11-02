// Casino Night's flippers of the Sonic 2 pre-beta (object $86, Data/Objects/Obj_0x86.asm). With the subtype 0 a flipper is a slope that a character is rolled onto: it carries him along it, and a press of a
// button throws him away by where he is on it (further out: harder and flatter). With another subtype it is a wall flipper (frame 4) that throws a character who pushes against it back at $1000.
#include "Object/CNZFlippers.h"
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

#include "Resource/Animation/CNZFlippers.h"
#include "Resource/Mappings/CNZFlippers.h"

#define ArtTile_CNZFlippers 0x3B2 // ($7640: the zone's list)

#define FOR_EACH_CHARACTER(chr, who) \
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) \
        for (Sst *chr = (who == SolidChar_Sonic) ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL); chr != NULL; chr = NULL)

// the tops of the three positions, a height for each two pixels (Offset_0x025EB2, ...D6 and ...FA)
static const int8_t flipper_rest[36] = {
    7, 7, 7, 7, 7, 7, 7, 8, 9, 10, 11, 10,
    9, 8, 7, 6, 5, 4, 3, 2, 1, 0, -1, -2,
    -3, -4, -5, -6, -7, -8, -9, -10, -11, -12, -13, -14,
};
static const int8_t flipper_mid[36] = {
    6, 6, 6, 6, 6, 6, 7, 8, 9, 9, 9, 9,
    9, 9, 8, 8, 8, 8, 8, 8, 7, 7, 7, 7,
    6, 6, 6, 6, 5, 5, 4, 4, 4, 4, 4, 4,
};
static const int8_t flipper_up[36] = {
    5, 5, 5, 5, 5, 6, 7, 8, 9, 10, 11, 11,
    12, 12, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17,
    18, 18, 17, 17, 16, 16, 16, 16, 16, 16, 16, 16,
};

typedef struct {
    uint8_t subtype;
    uint8_t pad0[13];
    uint8_t state[2]; // 0x36, 0x37: a character has been on it a frame (Sonic's, Tails')
    uint8_t launch;   // 0x38: a button was pressed, and he is thrown after the frame's checks
} Own_Flipper;

enum { FlipperRoutine_Init = 0, FlipperRoutine_Slope = 2, FlipperRoutine_Wall = 4 };

static void Flipper_Roll(Sst *chr) { // (the character becomes a ball; he sinks 5 pixels into the new, shorter shape the first time)
    chr->y_radius = 0xE;
    chr->x_radius = 7;
    chr->anim = SonAnimId_Roll;
    if (!(chr->status & SST_PSTATUS_IN_BALL)) {
        chr->status |= SST_PSTATUS_IN_BALL;
        chr->y_pos.f.u += 5;
    }
}

// Offset_0x025D84: the throw
static void Flipper_Throw(Sst *self, Own_Flipper *own, Sst *chr, int who) {
    const uint8_t stand_bit = (uint8_t)(1 << (3 + who));
    if (!(self->status & stand_bit))
        return;
    self->status &= (uint8_t)~stand_bit;
    const bool flipped = (self->status & SST_STATUS_X_FLIP) != 0;
    int16_t d0 = (int16_t)(chr->x_pos.f.u - self->x_pos.f.u);
    if (flipped)
        d0 = (int16_t)-d0;
    d0 = (int16_t)(d0 + 0x23);
    int16_t d2 = ((uint16_t)d0 < 0x40) ? d0 : 0x40;
    d2 = (int16_t)-((d2 << 5) + 0x800);
    const uint8_t angle = (uint8_t)((d0 >> 2) + 0x40);
    int16_t sin, cos;
    CalcSine(angle, &sin, &cos);
    chr->y_vel = (int16_t)(((int32_t)sin * d2) >> 8);
    int16_t x = (int16_t)(((int32_t)cos * d2) >> 8);
    if (flipped)
        x = (int16_t)-x;
    chr->x_vel = x;
    chr->status |= SST_PSTATUS_IN_AIR;
    chr->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
    chr->routine = 2;
    OBJ_CONTROL(chr) = 0;
    self->anim = 1;
    own->state[who] = 0;
    PlaySound(sfx_AlphaE3);
}

// Offset_0x025CFE: what a character on the flipper does each frame
static void Flipper_Character(Sst *self, Own_Flipper *own, Sst *chr, int who, uint8_t press) {
    const uint8_t stand_bit = (uint8_t)(1 << (3 + who));
    if (!own->state[who]) {
        if (!(self->status & stand_bit))
            return;
        OBJ_CONTROL(chr) = 1;
        Flipper_Roll(chr);
        own->state[who]++;
        return;
    }
    if (press & 0x70) {
        own->launch = 1;
        return;
    }
    if (!(self->status & stand_bit)) {
        OBJ_CONTROL(chr) = 0;
        own->state[who] = 0;
        return;
    }
    int16_t d1 = (int16_t)(self->mapping_frame - 1); // (it carries him down the slope)
    chr->status |= SST_PSTATUS_X_FLIP;
    if (!(self->status & SST_STATUS_X_FLIP)) {
        d1 = (int16_t)-d1;
        chr->status &= (uint8_t)~SST_PSTATUS_X_FLIP;
    }
    chr->x_pos.f.u += d1;
    chr->x_vel = (int16_t)(d1 << 8);
    chr->ground_vel = chr->x_vel;
    chr->y_vel = 0;
}

// Offset_0x025E50: a character pushes at the wall flipper's side
static void Flipper_Push(Sst *self, Sst *chr) {
    self->anim = 3;
    self->prev_anim = 0;
    chr->x_vel = -0x1000;
    chr->x_pos.f.u += 8;
    chr->status |= SST_PSTATUS_X_FLIP;
    if ((uint16_t)self->x_pos.f.u < (uint16_t)chr->x_pos.f.u) {
        chr->status &= (uint8_t)~SST_PSTATUS_X_FLIP;
        chr->x_pos.f.u -= 0x10;
        chr->x_vel = 0x1000;
        self->anim = 4;
        self->prev_anim = 0;
    }
    ((Scratch_Sonic *)chr->own.u8)->control_lock = 0xF;
    chr->ground_vel = chr->x_vel;
    Flipper_Roll(chr);
}

void Obj_CNZFlipper(Sst *self) {
    Own_Flipper *own = (Own_Flipper *)self->own.u8;

    if (self->routine == FlipperRoutine_Init) {
        self->routine += 2;
        self->mappings = Mappings_CNZFlippers;
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_CNZFlippers);
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 0x18;
        self->priority = 4 * 0x80;
        if (self->subtype != 0) {
            self->routine += 2;
            self->anim = 2;
        }
    }

    if (self->routine == FlipperRoutine_Slope) {
        if (!debug_use) {
            const int8_t *slope = self->mapping_frame == 0 ? flipper_rest : self->mapping_frame == 1 ? flipper_mid : flipper_up;
            FOR_EACH_CHARACTER(chr, who)
                Solid_Character(self, chr, who, 0x23, 6, 6, self->x_pos.f.u, slope);
            Flipper_Character(self, own, player, SolidChar_Sonic, jpad1_press2);
            if (TAILS_OBJ->code != NULL)
                Flipper_Character(self, own, TAILS_OBJ, SolidChar_Tails, jpad2_press);
            if (own->launch) {
                own->launch = 0;
                if (TAILS_OBJ->code != NULL)
                    Flipper_Throw(self, own, TAILS_OBJ, SolidChar_Tails);
                Flipper_Throw(self, own, player, SolidChar_Sonic);
            }
            Sst_Animate(self, Animation_CNZFlippers);
        }
    } else {
        FOR_EACH_CHARACTER(chr, who) {
            Solid_Character(self, chr, who, 0x13, 0x18, 0x19, self->x_pos.f.u, NULL);
            if (self->status & (1 << (5 + who)))
                Flipper_Push(self, chr);
        }
        Sst_Animate(self, Animation_CNZFlippers);
    }
    SstOnScreenTest(self);
}
