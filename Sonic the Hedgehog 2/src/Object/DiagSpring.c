// The Simon Wai prototype's object 40: its diagonal springs (the "lever spring" art, a wide flat spring set on a slope: Chemical Plant's, Casino Night's...). Unlike the diagonal springs of object 41 it
// launches by where along its slope the character stands, and only after it has been compressed: Obj_0x40_Diagonal_Springs (loc_1A30C) in its disassembly.
// Subtype: bit 0 the character tumbles, bit 1 the weaker spring (a tumble of one flip, not three), bits 2-3 the collision path he takes (4: the first path's, 8: the second's). The object may be flipped.
#include "Object/DiagSpring.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Level.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Solid.h"
#include "Sound.h"

#include "Macros.h"

#include "Resource/Mappings/DiagSpring.h"

typedef struct {
    uint8_t subtype; // 0x28
} Own_DiagSpring;

#define ARTTILE_LEVER_SPRING 0x440 // ($8800 in the zones' art lists)

// loc_1A57E: animation 0 holds the frame 0; animation 1 shows the compressed frame 1 for 4 ticks and goes back to 0
static const uint8_t Animation_DiagSpring[] = {
    0x00, 0x04, 0x00, 0x07,
    0x0F, 0x00, 0xFF,
    0x03, 0x01, 0x00, 0xFD, 0x00,
};

// loc_1A4E6: how much harder the spring throws the character who stands on it at a given distance along it (the far end throws up to 4 more)
static const uint8_t launch_boost[0x48] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

// loc_1A52E and loc_1A556: the height of the spring's top for each 2 pixel column, for the open frame and the compressed one
static const int8_t slope_open[0x28] = {
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x10,
    0x11, 0x12, 0x13, 0x14, 0x14, 0x15, 0x15, 0x16, 0x17, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
    0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
};
static const int8_t slope_pressed[0x28] = {
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0C, 0x0C, 0x0C, 0x0D, 0x0D,
    0x0D, 0x0D, 0x0D, 0x0D, 0x0E, 0x0E, 0x0F, 0x0F, 0x10, 0x10, 0x10, 0x10, 0x0F, 0x0F, 0x0E, 0x0E,
    0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D,
};

// The tumble and the collision path the subtype asks for (the same as the other springs')
static void DiagSpring_Tumble(Sst *chr, uint8_t subtype) {
    Scratch_Sonic *sscratch = (Scratch_Sonic *)chr->own.u8;

    if (subtype & 1) {
        chr->ground_vel = 1;
        sscratch->flip_angle = 1;
        chr->anim = SonAnimId_Walk;
        sscratch->flips_remaining = 1;
        sscratch->flip_speed = 8;
        if (!(subtype & 2))
            sscratch->flips_remaining = 3;
        if (((chr->status & SST_PSTATUS_X_FLIP) != 0)) {
            sscratch->flip_angle = (uint8_t)-sscratch->flip_angle;
            chr->ground_vel = -chr->ground_vel;
        }
    }
    switch (subtype & 0xC) {
    case 4:
        sscratch->top_solid_bit = 0xC;
        sscratch->lrb_solid_bit = 0xD;
        break;
    case 8:
        sscratch->top_solid_bit = 0xE;
        sscratch->lrb_solid_bit = 0xF;
        break;
    }
}

// The character stands on the spring (loc_1A3BA): on its springy side (past $10 pixels from the middle towards its low end) it starts to compress; once compressed back to the open frame it throws him
static void DiagSpring_Stand(Sst *self, Sst *chr, uint8_t subtype) {
    const bool flipped = ((self->status & SST_STATUS_X_FLIP) != 0);
    if (!flipped) {
        if (!((uint16_t)(self->x_pos.f.u - 0x10) < (uint16_t)chr->x_pos.f.u))
            return;
    } else {
        if (!((uint16_t)(self->x_pos.f.u + 0x10) >= (uint16_t)chr->x_pos.f.u))
            return;
    }

    if (self->anim != 1) { // compress first
        self->anim = 1;
        self->prev_anim = 0;
        return;
    }
    if (self->mapping_frame != 0)
        return;

    // The throw (loc_1A3FA): further along the slope throws harder
    int16_t d0 = (int16_t)(chr->x_pos.f.u - (self->x_pos.f.u - 0x1C));
    if (flipped)
        d0 = (int16_t)(0x26 - d0);
    if (d0 < 0)
        d0 = 0;
    int boost = d0 < 0x48 ? launch_boost[d0] : 0;

    chr->y_vel = (int16_t)((0xFC - boost) << 8);
    chr->status |= SST_PSTATUS_X_FLIP;
    if (!flipped) {
        chr->status &= (uint8_t)~SST_PSTATUS_X_FLIP;
        boost = -boost;
    }
    int16_t xs = chr->x_vel < 0 ? (int16_t)-chr->x_vel : chr->x_vel;
    if (xs >= 0x400)
        chr->x_vel = (int16_t)(chr->x_vel - (boost << 8));
    chr->status |= SST_PSTATUS_IN_AIR;
    chr->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
    chr->anim = 0x10;
    chr->routine = 2;
    DiagSpring_Tumble(chr, subtype);
    PlaySound(sfx_Spring);
}

void Obj_DiagSpring(Sst *self) {
    Own_DiagSpring *own = (Own_DiagSpring *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_DiagSpring;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ARTTILE_LEVER_SPRING);
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 0x1C;
        self->priority = 4 * 0x80;
    }

    Sst_Animate(self, Animation_DiagSpring);
    const int8_t *slope = self->mapping_frame == 0 ? slope_open : slope_pressed;
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
        Sst *chr = who == SolidChar_Sonic ? player : TAILS_OBJ;
        if (who == SolidChar_Tails && chr->code == NULL)
            continue;
        Solid_Character(self, chr, who, 0x27, 8, 8, self->x_pos.f.u, slope);
        if (self->status & (1 << (3 + who)))
            DiagSpring_Stand(self, chr, self->subtype);
    }

    SstOnScreenTest(self); // (MarkObjGone: drawn, or gone when out of range, letting it come back)
}
