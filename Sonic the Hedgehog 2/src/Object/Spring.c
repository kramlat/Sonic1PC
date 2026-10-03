// Springs for Sonic 2 (Nick Arcade's object 41): up, sideways, down and the diagonal pair. Sonic 1's spring (Sonic 1's Spring.c) only knows Sonic and straight springs; this one launches both Sonic and Tails,
// can send them tumbling (subtype bit 0, flips counted by bit 1), cut their x speed (bit 7) and switch their collision path (bits 2 and 3, kept for the path swappers to come).
// Subtype: bits 4-5 the direction (0 up, 1 sideways, 2 down, 3 diagonal up, 4 diagonal down), bit 1 the weaker, yellow spring.
#include "Object/Spring.h"
#include "Constants.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Solid.h"
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
static void Spring_FlipAndPath(Object *chr, uint8_t subtype, uint8_t flip_speed, uint8_t flips_weak, uint8_t flips_strong) {
    Scratch_Sonic *sscratch = (Scratch_Sonic *)&chr->scratch;

    if (subtype & 1) {
        chr->inertia = 1;
        sscratch->flip_angle = 1;
        chr->anim = SonAnimId_Walk;
        sscratch->flips_remaining = flips_weak;
        sscratch->flip_speed = flip_speed;
        if (!(subtype & 2))
            sscratch->flips_remaining = flips_strong;
        if (chr->status.p.f.x_flip) {
            sscratch->flip_angle = (uint8_t)-sscratch->flip_angle;
            chr->inertia = -chr->inertia;
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

static void Spring_SetAnim(Object *obj, uint8_t anim) {
    obj->anim = anim;
    obj->prev_anim = 0;
}

// Launches the character up (sub_E34E)
static void Spring_LaunchUp(Object *obj, Object *chr, int16_t power, uint8_t subtype) {
    Spring_SetAnim(obj, SpringAnim_Up);
    chr->pos.l.y.f.u += 8;
    chr->ysp = power;
    chr->status.p.f.in_air = true;
    chr->status.p.f.object_stand = false;
    chr->anim = SonAnimId_Spring;
    chr->routine = 2;
    if (subtype & 0x80)
        chr->xsp = 0;
    Spring_FlipAndPath(chr, subtype, 4, 0, 1);
    PlaySound(sfx_Spring);
}

// ... down (sub_E64E)
static void Spring_LaunchDown(Object *obj, Object *chr, int16_t power, uint8_t subtype) {
    Spring_SetAnim(obj, SpringAnim_Up);
    chr->pos.l.y.f.u -= 8;
    chr->ysp = -power;
    if (subtype & 0x80)
        chr->xsp = 0;
    Spring_FlipAndPath(chr, subtype, 4, 0, 1);
    chr->status.p.f.in_air = true;
    chr->status.p.f.object_stand = false;
    chr->routine = 2;
    PlaySound(sfx_Spring);
}

// ... sideways (sub_E474)
static void Spring_LaunchSide(Object *obj, Object *chr, int16_t power, uint8_t subtype) {
    Scratch_Sonic *sscratch = (Scratch_Sonic *)&chr->scratch;

    Spring_SetAnim(obj, SpringAnim_Side);
    chr->xsp = power;
    chr->pos.l.x.f.u += 8;
    chr->status.p.f.x_flip = true;
    if (!obj->status.o.f.x_flip) {
        chr->status.p.f.x_flip = false;
        chr->pos.l.x.f.u -= 0x10;
        chr->xsp = -chr->xsp;
    }
    sscratch->control_lock = 0xF;
    chr->inertia = chr->xsp;
    if (!chr->status.p.f.in_ball)
        chr->anim = SonAnimId_Walk;
    if (subtype & 0x80)
        chr->ysp = 0;
    Spring_FlipAndPath(chr, subtype, 8, 1, 3);
    obj->status.b &= (uint8_t)~((1 << 5) | (1 << 6));
    chr->status.p.f.pushing = false;
    PlaySound(sfx_Spring);
}

// ... diagonally up (sub_E73E), if the character is on the springy half; it takes off along the diagonal
static void Spring_LaunchDiagUp(Object *obj, Object *chr, int16_t power, uint8_t subtype) {
    int16_t x = obj->pos.l.x.f.u;
    if (!obj->status.o.f.x_flip) {
        if (!((uint16_t)(x - 4) < (uint16_t)chr->pos.l.x.f.u))
            return;
    } else {
        if (!((uint16_t)(x + 4) >= (uint16_t)chr->pos.l.x.f.u))
            return;
    }

    Spring_SetAnim(obj, SpringAnim_Diag);
    chr->ysp = power;
    chr->xsp = power;
    chr->pos.l.y.f.u += 6;
    chr->pos.l.x.f.u += 6;
    chr->status.p.f.x_flip = true;
    if (!obj->status.o.f.x_flip) {
        chr->status.p.f.x_flip = false;
        chr->pos.l.x.f.u -= 0xC;
        chr->xsp = -chr->xsp;
    }
    chr->status.p.f.in_air = true;
    chr->status.p.f.object_stand = false;
    chr->anim = SonAnimId_Spring;
    chr->routine = 2;
    Spring_FlipAndPath(chr, subtype, 8, 1, 3);
    PlaySound(sfx_Spring);
}

// ... diagonally down (sub_E870), from underneath
static void Spring_LaunchDiagDown(Object *obj, Object *chr, int16_t power, uint8_t subtype) {
    Spring_SetAnim(obj, SpringAnim_Diag);
    chr->ysp = -power;
    chr->xsp = power;
    chr->pos.l.y.f.u -= 6;
    chr->pos.l.x.f.u += 6;
    chr->status.p.f.x_flip = true;
    if (!obj->status.o.f.x_flip) {
        chr->status.p.f.x_flip = false;
        chr->pos.l.x.f.u -= 0xC;
        chr->xsp = -chr->xsp;
    }
    chr->status.p.f.in_air = true;
    chr->status.p.f.object_stand = false;
    chr->routine = 2;
    Spring_FlipAndPath(chr, subtype, 8, 1, 3);
    PlaySound(sfx_Spring);
}

// A sideways spring also springs a character that runs at its face without having touched it (sub_E54C): in a box in front of it, running toward it
static void Spring_RunInto(Object *obj, Object *chr, int16_t power, uint8_t subtype) {
    int16_t x0 = obj->pos.l.x.f.u;
    int16_t x1 = x0 + 0x28;
    if (obj->status.o.f.x_flip) {
        x1 = x0;
        x0 -= 0x28;
    }
    int16_t y0 = obj->pos.l.y.f.u - 0x18, y1 = obj->pos.l.y.f.u + 0x18;

    if (chr->status.p.f.in_air)
        return;
    int16_t speed = chr->inertia;
    if (obj->status.o.f.x_flip)
        speed = -speed;
    if (speed < 0)
        return;
    uint16_t cx = (uint16_t)chr->pos.l.x.f.u, cy = (uint16_t)chr->pos.l.y.f.u;
    if (cx < (uint16_t)x0 || cx >= (uint16_t)x1 || cy < (uint16_t)y0 || cy >= (uint16_t)y1)
        return;
    Spring_LaunchSide(obj, chr, power, subtype);
}

// The two characters, Sonic and then Tails if he is here: `who` is which, and `chr` the object to act on
static Object *Spring_Character(int who) {
    if (who == SolidChar_Sonic)
        return player;
    return TAILS_OBJ->type != 0 ? TAILS_OBJ : NULL;
}
#define FOR_CHARACTERS(chr, who) \
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) \
        for (Object *chr = Spring_Character(who); chr != NULL; chr = NULL)

void Obj_Spring(Object *obj) {
    Scratch_Spring *scratch = (Scratch_Spring *)&obj->scratch;
    const bool ghz = LEVEL_ZONE(level_id) == ZoneId_GHZ;

    switch (obj->routine) {
    case SpringRoutine_Init: {
        obj->routine += 2;
        obj->mappings = ghz ? Mappings_SpringGHZ : Mappings_Spring;
        obj->tile = TILE_MAP(0, 0, 0, 0, ghz ? ArtTile_Spring_Horizontal : ArtTile_SpringUp);
        obj->render.f.level_fg = true;
        obj->width_pixels = 0x10;
        obj->priority = 4;

        uint8_t subtype = scratch->subtype;
        switch ((subtype >> 3) & 0xE) {
        case 2: // sideways
            obj->routine = SpringRoutine_Side;
            obj->anim = SpringAnim_SideIdle;
            obj->frame = 3;
            obj->tile = TILE_MAP(0, 0, 0, 0, ghz ? ArtTile_Spring_Vertical : ArtTile_SpringSide);
            obj->width_pixels = 8;
            break;
        case 4: // down
            obj->routine = SpringRoutine_Down;
            obj->frame = 6;
            obj->status.o.f.y_flip = true;
            break;
        case 6: // diagonally up
            obj->routine = SpringRoutine_DiagUp;
            obj->anim = SpringAnim_DiagIdle;
            obj->frame = 7;
            obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_SpringDiag);
            break;
        case 8: // diagonally down
            obj->routine = SpringRoutine_DiagDown;
            obj->anim = SpringAnim_DiagIdle;
            obj->frame = 0xA;
            obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_SpringDiag);
            obj->status.o.f.y_flip = true;
            break;
        }

        scratch->power = spring_power[(subtype & 2) >> 1];
        if (subtype & 2) {
            obj->tile |= TILE_MAP(0, 1, 0, 0, 0); // yellow: palette line 1
            if (!ghz)
                obj->mappings = Mappings_SpringYellow;
        }
        break;
    }
    case SpringRoutine_Up:
        FOR_CHARACTERS(chr, who) {
            Solid_Character(obj, chr, who, 0x1B, 8, 0x10, obj->pos.l.x.f.u, NULL);
            if (obj->status.b & (1 << (3 + who)))
                Spring_LaunchUp(obj, chr, scratch->power, scratch->subtype);
        }
        AnimateSprite(obj, Animation_Spring);
        break;
    case SpringRoutine_Side:
        FOR_CHARACTERS(chr, who) {
            Solid_Character(obj, chr, who, 0x13, 0xE, 0xF, obj->pos.l.x.f.u, NULL);
            if (obj->status.b & (1 << (5 + who))) {
                // (only from the side it faces)
                uint8_t flip = obj->status.o.f.x_flip;
                if (!(obj->pos.l.x.f.u < chr->pos.l.x.f.u))
                    flip ^= 1;
                if (!(flip & 1))
                    Spring_LaunchSide(obj, chr, scratch->power, scratch->subtype);
            }
        }
        if (obj->anim != SpringAnim_Side) {
            FOR_CHARACTERS(chr, who) {
                (void)who;
                Spring_RunInto(obj, chr, scratch->power, scratch->subtype);
            }
        }
        AnimateSprite(obj, Animation_Spring);
        break;
    case SpringRoutine_Down:
        FOR_CHARACTERS(chr, who) {
            if (Solid_Character(obj, chr, who, 0x1B, 8, 0x10, obj->pos.l.x.f.u, NULL) == -2)
                Spring_LaunchDown(obj, chr, scratch->power, scratch->subtype);
        }
        AnimateSprite(obj, Animation_Spring);
        break;
    case SpringRoutine_DiagUp:
        FOR_CHARACTERS(chr, who) {
            Solid_Character(obj, chr, who, 0x1B, 0x10, 0x10, obj->pos.l.x.f.u, slope_diag_up);
            if (obj->status.b & (1 << (3 + who)))
                Spring_LaunchDiagUp(obj, chr, scratch->power, scratch->subtype);
        }
        AnimateSprite(obj, Animation_Spring);
        break;
    case SpringRoutine_DiagDown:
        FOR_CHARACTERS(chr, who) {
            if (Solid_Character(obj, chr, who, 0x1B, 0x10, 0x10, obj->pos.l.x.f.u, slope_diag_down) == -2)
                Spring_LaunchDiagDown(obj, chr, scratch->power, scratch->subtype);
        }
        AnimateSprite(obj, Animation_Spring);
        break;
    }

    // Draw, and delete once off-screen
    DisplaySprite(obj);
    if (IS_OFFSCREEN(obj->pos.l.x.f.u))
        ObjectDelete(obj);
}
