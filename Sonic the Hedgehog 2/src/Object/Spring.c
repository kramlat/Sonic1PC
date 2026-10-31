// Springs for Sonic 2 (Nick Arcade's object 41): up, sideways, down and the diagonal pair. Sonic 1's spring (Sonic 1's Spring.c) only knows Sonic and straight springs; this one launches both Sonic and Tails,
// can send them tumbling (subtype bit 0, flips counted by bit 1), cut their x speed (bit 7) and switch their collision path (bits 2 and 3, kept for the path swappers to come).
// Subtype: bits 4-5 the direction (0 up, 1 sideways, 2 down, 3 diagonal up, 4 diagonal down), bit 1 the weaker, yellow spring.
#include "Object/Spring.h"
#include "SstLoad.h"
#include "Resource/Animation/Spring.h"
#include "Resource/Mappings/Spring.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "SolidSst.h"
#include "Sound.h"

#include "Macros.h"

#include "Resource/Mappings/SpringGHZ.h"
#include "Resource/Mappings/SpringYellow.h"

// The art: Green Hill's own (ArtTile_Spring_Horizontal, the one facing up, and _Vertical, the sideways one) and everywhere else's at Nick Arcade's places (ArtTile_SpringUp, Side, Diag): its zone PLCs load them

enum { SpringAnim_Idle, SpringAnim_Up, SpringAnim_SideIdle, SpringAnim_Side, SpringAnim_DiagIdle, SpringAnim_Diag };
enum { SpringRoutine_Init = 0, SpringRoutine_Up = 2, SpringRoutine_Side = 4, SpringRoutine_Down = 6, SpringRoutine_DiagUp = 8, SpringRoutine_DiagDown = 10 };

static const int16_t spring_power[] = { -0x1000, -0xA00 };

// The slope of a diagonal spring's top (byte_E918's two tables: Obj41_SlopeData_DiagUp and _DiagDown), a height for each 2 pixel column
static const int8_t slope_diag_up[] = {
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0xE, 0xC, 0xA, 8,
    6, 4, 2, 0, -2, -4, -4, -4, -4, -4, -4, -4,
};
static const int8_t slope_diag_down[] = {
    -0xC, -0x10, -0x10, -0x10, -0x10, -0x10, -0x10, -0x10, -0x10, -0x10, -0x10, -0x10, -0xE, -0xC, -0xA, -8,
    -6, -4, -2, 0, 2, 4, 4, 4, 4, 4, 4, 4,
};

// What a launch does to the character besides moving it: the tumble the subtype asks for, and the collision path
static void Spring_FlipAndPath(Sst *chr, uint8_t subtype, uint8_t flip_speed, uint8_t flips_weak, uint8_t flips_strong) {
    Scratch_Sonic *sscratch = (Scratch_Sonic *)chr->own.u8;

    if (subtype & 1) {
        chr->ground_vel = 1;
        sscratch->flip_angle = 1;
        chr->anim = SonAnimId_Walk;
        sscratch->flips_remaining = flips_weak;
        sscratch->flip_speed = flip_speed;
        if (!(subtype & 2))
            sscratch->flips_remaining = flips_strong;
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

static void Spring_SetAnim(Sst *self, uint8_t anim) {
    self->anim = anim;
    self->prev_anim = 0;
}

// Launches the character up (sub_E34E)
static void Spring_LaunchUp(Sst *self, Sst *chr, int16_t power, uint8_t subtype) {
    Spring_SetAnim(self, SpringAnim_Up);
    chr->y_pos.f.u += 8;
    chr->y_vel = power;
    chr->status |= SST_PSTATUS_IN_AIR;
    chr->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
    chr->anim = SonAnimId_Spring;
    chr->routine = 2;
    if (subtype & 0x80)
        chr->x_vel = 0;
    Spring_FlipAndPath(chr, subtype, 4, 0, 1);
    PlaySound(sfx_Spring);
}

// ... down (sub_E64E)
static void Spring_LaunchDown(Sst *self, Sst *chr, int16_t power, uint8_t subtype) {
    Spring_SetAnim(self, SpringAnim_Up);
    chr->y_pos.f.u -= 8;
    chr->y_vel = -power;
    if (subtype & 0x80)
        chr->x_vel = 0;
    Spring_FlipAndPath(chr, subtype, 4, 0, 1);
    chr->status |= SST_PSTATUS_IN_AIR;
    chr->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
    chr->routine = 2;
    PlaySound(sfx_Spring);
}

// ... sideways (sub_E474)
static void Spring_LaunchSide(Sst *self, Sst *chr, int16_t power, uint8_t subtype) {
    Scratch_Sonic *sscratch = (Scratch_Sonic *)chr->own.u8;

    Spring_SetAnim(self, SpringAnim_Side);
    chr->x_vel = power;
    chr->x_pos.f.u += 8;
    chr->status |= SST_PSTATUS_X_FLIP;
    if (!((self->status & SST_STATUS_X_FLIP) != 0)) {
        chr->status &= (uint8_t)~SST_PSTATUS_X_FLIP;
        chr->x_pos.f.u -= 0x10;
        chr->x_vel = -chr->x_vel;
    }
    sscratch->control_lock = 0xF;
    chr->ground_vel = chr->x_vel;
    if (!((chr->status & SST_PSTATUS_IN_BALL) != 0))
        chr->anim = SonAnimId_Walk;
    if (subtype & 0x80)
        chr->y_vel = 0;
    Spring_FlipAndPath(chr, subtype, 8, 1, 3);
    self->status &= (uint8_t)~((1 << 5) | (1 << 6));
    chr->status &= (uint8_t)~SST_PSTATUS_PUSHING;
    PlaySound(sfx_Spring);
}

// ... diagonally up (sub_E73E), if the character is on the springy half; it takes off along the diagonal
static void Spring_LaunchDiagUp(Sst *self, Sst *chr, int16_t power, uint8_t subtype) {
    int16_t x = self->x_pos.f.u;
    if (!((self->status & SST_STATUS_X_FLIP) != 0)) {
        if (!((uint16_t)(x - 4) < (uint16_t)chr->x_pos.f.u))
            return;
    } else {
        if (!((uint16_t)(x + 4) >= (uint16_t)chr->x_pos.f.u))
            return;
    }

    Spring_SetAnim(self, SpringAnim_Diag);
    chr->y_vel = power;
    chr->x_vel = power;
    chr->y_pos.f.u += 6;
    chr->x_pos.f.u += 6;
    chr->status |= SST_PSTATUS_X_FLIP;
    if (!((self->status & SST_STATUS_X_FLIP) != 0)) {
        chr->status &= (uint8_t)~SST_PSTATUS_X_FLIP;
        chr->x_pos.f.u -= 0xC;
        chr->x_vel = -chr->x_vel;
    }
    chr->status |= SST_PSTATUS_IN_AIR;
    chr->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
    chr->anim = SonAnimId_Spring;
    chr->routine = 2;
    Spring_FlipAndPath(chr, subtype, 8, 1, 3);
    PlaySound(sfx_Spring);
}

// ... diagonally down (sub_E870), from underneath
static void Spring_LaunchDiagDown(Sst *self, Sst *chr, int16_t power, uint8_t subtype) {
    Spring_SetAnim(self, SpringAnim_Diag);
    chr->y_vel = -power;
    chr->x_vel = power;
    chr->y_pos.f.u -= 6;
    chr->x_pos.f.u += 6;
    chr->status |= SST_PSTATUS_X_FLIP;
    if (!((self->status & SST_STATUS_X_FLIP) != 0)) {
        chr->status &= (uint8_t)~SST_PSTATUS_X_FLIP;
        chr->x_pos.f.u -= 0xC;
        chr->x_vel = -chr->x_vel;
    }
    chr->status |= SST_PSTATUS_IN_AIR;
    chr->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
    chr->routine = 2;
    Spring_FlipAndPath(chr, subtype, 8, 1, 3);
    PlaySound(sfx_Spring);
}

// A sideways spring also springs a character that runs at its face without having touched it (sub_E54C): in a box in front of it, running toward it
static void Spring_RunInto(Sst *self, Sst *chr, int16_t power, uint8_t subtype) {
    int16_t x0 = self->x_pos.f.u;
    int16_t x1 = x0 + 0x28;
    if (((self->status & SST_STATUS_X_FLIP) != 0)) {
        x1 = x0;
        x0 -= 0x28;
    }
    int16_t y0 = self->y_pos.f.u - 0x18, y1 = self->y_pos.f.u + 0x18;

    if (((chr->status & SST_PSTATUS_IN_AIR) != 0))
        return;
    int16_t speed = chr->ground_vel;
    if (((self->status & SST_STATUS_X_FLIP) != 0))
        speed = -speed;
    if (speed < 0)
        return;
    uint16_t cx = (uint16_t)chr->x_pos.f.u, cy = (uint16_t)chr->y_pos.f.u;
    if (cx < (uint16_t)x0 || cx >= (uint16_t)x1 || cy < (uint16_t)y0 || cy >= (uint16_t)y1)
        return;
    Spring_LaunchSide(self, chr, power, subtype);
}

// The two characters, Sonic and then Tails if he is here: `who` is which, and `chr` the object to act on
static Sst *Spring_Character(int who) {
    if (who == SolidChar_Sonic)
        return player;
    return TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL;
}
#define FOR_CHARACTERS(chr, who) \
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) \
        for (Sst *chr = Spring_Character(who); chr != NULL; chr = NULL)

void Obj_Spring(Sst *self) {
    Own_Spring *own = (Own_Spring *)self->own.u8;
    const bool ghz = LEVEL_ZONE(level_id) == ZoneId_GHZ;

    switch (self->routine) {
    case SpringRoutine_Init: {
        self->routine += 2;
        self->mappings = ghz ? Mappings_SpringGHZ : Mappings_Spring;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ghz ? ArtTile_Spring_Horizontal : ArtTile_SpringUp);
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 0x10;
        self->priority = 4 * 0x80;

        uint8_t subtype = self->subtype;
        switch ((subtype >> 3) & 0xE) {
        case 2: // sideways
            self->routine = SpringRoutine_Side;
            self->anim = SpringAnim_SideIdle;
            self->mapping_frame = 3;
            self->art_tile = TILE_MAP(0, 0, 0, 0, ghz ? ArtTile_Spring_Vertical : ArtTile_SpringSide);
            self->width_pixels = 8;
            break;
        case 4: // down
            self->routine = SpringRoutine_Down;
            self->mapping_frame = 6;
            SST_STATUS_SET(self, SST_STATUS_Y_FLIP, true);
            break;
        case 6: // diagonally up
            self->routine = SpringRoutine_DiagUp;
            self->anim = SpringAnim_DiagIdle;
            self->mapping_frame = 7;
            self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_SpringDiag);
            break;
        case 8: // diagonally down
            self->routine = SpringRoutine_DiagDown;
            self->anim = SpringAnim_DiagIdle;
            self->mapping_frame = 0xA;
            self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_SpringDiag);
            SST_STATUS_SET(self, SST_STATUS_Y_FLIP, true);
            break;
        }

        own->power = spring_power[(subtype & 2) >> 1];
        if (subtype & 2) {
            self->art_tile |= TILE_MAP(0, 1, 0, 0, 0); // yellow: palette line 1
            if (!ghz)
                self->mappings = Mappings_SpringYellow;
        }
        break;
    }
    case SpringRoutine_Up:
        FOR_CHARACTERS(chr, who) {
            SolidSst_Character(self, chr, who, 0x1B, 8, 0x10, self->x_pos.f.u, NULL);
            if (self->status & (1 << (3 + who)))
                Spring_LaunchUp(self, chr, own->power, self->subtype);
        }
        Sst_Animate(self, Animation_Spring);
        break;
    case SpringRoutine_Side:
        FOR_CHARACTERS(chr, who) {
            SolidSst_Character(self, chr, who, 0x13, 0xE, 0xF, self->x_pos.f.u, NULL);
            if (self->status & (1 << (5 + who))) {
                // (only from the side it faces)
                uint8_t flip = ((self->status & SST_STATUS_X_FLIP) != 0);
                if (!(self->x_pos.f.u < chr->x_pos.f.u))
                    flip ^= 1;
                if (!(flip & 1))
                    Spring_LaunchSide(self, chr, own->power, self->subtype);
            }
        }
        if (self->anim != SpringAnim_Side) {
            FOR_CHARACTERS(chr, who) {
                (void)who;
                Spring_RunInto(self, chr, own->power, self->subtype);
            }
        }
        Sst_Animate(self, Animation_Spring);
        break;
    case SpringRoutine_Down:
        FOR_CHARACTERS(chr, who) {
            if (SolidSst_Character(self, chr, who, 0x1B, 8, 0x10, self->x_pos.f.u, NULL) == -2)
                Spring_LaunchDown(self, chr, own->power, self->subtype);
        }
        Sst_Animate(self, Animation_Spring);
        break;
    case SpringRoutine_DiagUp:
        FOR_CHARACTERS(chr, who) {
            SolidSst_Character(self, chr, who, 0x1B, 0x10, 0x10, self->x_pos.f.u, slope_diag_up);
            if (self->status & (1 << (3 + who)))
                Spring_LaunchDiagUp(self, chr, own->power, self->subtype);
        }
        Sst_Animate(self, Animation_Spring);
        break;
    case SpringRoutine_DiagDown:
        FOR_CHARACTERS(chr, who) {
            if (SolidSst_Character(self, chr, who, 0x1B, 0x10, 0x10, self->x_pos.f.u, slope_diag_down) == -2)
                Spring_LaunchDiagDown(self, chr, own->power, self->subtype);
        }
        Sst_Animate(self, Animation_Spring);
        break;
    }

    // Draw, and delete once off-screen
    Sst_Draw(self);
    SstDeleteIfNotInRange2(self, self->x_pos.f.u);
}
