// Solid objects for both characters (Nick Arcade's sub SolidObject.asm: SolidObject_Always_SingleCharacter and SlopedSolid_SingleCharacter, with MvSonicOnPtfm, MvSonicOnSlope and RideObject_SetRide),
// as one function: a flat top, or the top of a slope when a height table is given.
#include "Solid.h"

#include "Level.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"

// The character is carried along with the object it stands on (MvSonicOnPtfm / MvSonicOnSlope): put on its top, and moved along by what the object moved
static void MoveOnObject(Object *obj, Object *chr, int16_t x, int16_t y_top) {
    if ((lock_multi & 0x80) || chr->routine >= 6) // (an object holding the player: no carrying)
        return;
    chr->pos.l.y.f.u = y_top - chr->y_rad;
    chr->pos.l.x.f.u -= x - obj->pos.l.x.f.u;
}

// The height of a slope's top at the character's place across the object (the heights are relative to the first, flipped with the object)
static int16_t SlopeHeight(const Object *obj, const Object *chr, int16_t x_rad, const int8_t *slope) {
    uint16_t column = (uint16_t)(chr->pos.l.x.f.u - obj->pos.l.x.f.u + x_rad);
    if (obj->render.f.x_flip)
        column = (uint16_t)(~column + (x_rad << 1));
    column >>= 1;
    return slope[column] - slope[0];
}

// The character lands on the object (RideObject_SetRide): whatever it stood on before lets go, it takes this one, and its vertical speed becomes ground speed
static void RideObject(Object *obj, Object *chr, int who) {
    Scratch_Sonic *scratch = (Scratch_Sonic *)&chr->scratch;

    if (chr->status.p.f.object_stand) {
        Object *prv = &objects[scratch->standing_obj];
        prv->status.o.f.player_stand = false;
    }

    scratch->standing_obj = (uint8_t)(obj - objects);
    chr->angle = 0;
    chr->ysp = 0;
    chr->inertia = chr->xsp;
    if (chr->status.p.f.in_air) {
        if (who == SolidChar_Sonic)
            Sonic_ResetOnFloor(chr);
        else
            Tails_ResetOnFloor(chr);
    }
    chr->status.p.f.object_stand = true;
    obj->status.b |= (uint8_t)(1 << (3 + who));
}

int32_t Solid_Character(Object *obj, Object *chr, int who, int16_t x_rad, int16_t y_air, int16_t y_walk, int16_t x, const int8_t *slope) {
    const uint8_t stand_bit = (uint8_t)(1 << (3 + who));
    const uint8_t push_bit = (uint8_t)(1 << (5 + who));

    // Already standing on it: carry on until the character jumps or walks off
    if (obj->status.b & stand_bit) {
        int16_t d0 = chr->pos.l.x.f.u - obj->pos.l.x.f.u + x_rad;
        if (chr->status.p.f.in_air || d0 < 0 || (uint16_t)d0 >= (uint16_t)(x_rad << 1)) {
            chr->status.p.f.object_stand = false;
            obj->status.b &= (uint8_t)~stand_bit;
            return 0;
        }
        if (slope == NULL)
            MoveOnObject(obj, chr, x, obj->pos.l.y.f.u - y_walk);
        else if (chr->status.p.f.object_stand)
            MoveOnObject(obj, chr, x, obj->pos.l.y.f.u - SlopeHeight(obj, chr, x_rad, slope) + slope[0]); // (the height as the table says it: the top of the object, not relative to its first column)
        return 0;
    }

    // The part of the box test along x: d0 is the character's distance from the object's left edge
    int16_t d0 = chr->pos.l.x.f.u - obj->pos.l.x.f.u + x_rad;
    int16_t x_dia = x_rad << 1;
    int16_t y_top = obj->pos.l.y.f.u;
    int16_t d2 = y_air;
    int16_t d3, d4;
    bool miss = d0 < 0 || (uint16_t)d0 > (uint16_t)x_dia;
    if (!miss) {
        if (slope != NULL)
            y_top = obj->pos.l.y.f.u - SlopeHeight(obj, chr, x_rad, slope);

        // ... and along y
        d2 += chr->y_rad;
        d3 = chr->pos.l.y.f.u - y_top + 4 + d2;
        d4 = d2 << 1;
        miss = d3 < 0 || (uint16_t)d3 >= (uint16_t)d4;
    }
    if (lock_multi & 0x80) // an object has switched the player's object collisions off
        miss = true;

    if (!miss) {
        if (chr->routine >= 6 || (who == SolidChar_Sonic && debug_use))
            return 0;

        // Which way is the character coming in: d5 its distance past the nearest side, d1 past the nearest of top and bottom
        int16_t d5 = d0;
        int16_t d1 = x_rad;
        if ((uint16_t)d1 < (uint16_t)d0) {
            d0 -= (int16_t)(x_rad << 1);
            d5 = -d0;
        }
        d1 = d3;
        if ((uint16_t)d2 < (uint16_t)d3) {
            d3 -= 4;
            d3 -= d4;
            d1 = -d3;
        }

        if ((uint16_t)d5 <= (uint16_t)d1) {
            // The side
            if ((uint16_t)d1 > 4) {
                bool push = true;
                if (d0 != 0) {
                    bool toward = d0 < 0 ? chr->xsp < 0 : chr->xsp >= 0;
                    if (toward) {
                        chr->inertia = 0;
                        chr->xsp = 0;
                    }
                }
                chr->pos.l.x.f.u -= d0;
                if (chr->status.p.f.in_air)
                    push = false;
                if (push) {
                    obj->status.b |= push_bit;
                    chr->status.p.f.pushing = true;
                    return 1;
                }
            }
            obj->status.b &= (uint8_t)~push_bit;
            return 1;
        }

        if (d3 < 0) {
            // The underside: pushed down, or crushed if it can't move
            if (chr->ysp != 0) {
                if (chr->ysp < 0 && d3 < 0) {
                    chr->pos.l.y.f.u -= d3;
                    chr->ysp = 0;
                }
            } else if (!chr->status.p.f.in_air) {
                if (who == SolidChar_Sonic)
                    KillSonic(chr, obj);
                else
                    KillTails(chr);
            }
            return -2;
        }

        if (d3 < 0x10) {
            // The top: lands on it if it is over the object and not rising
            d3 -= 4;
            int16_t wid = obj->width_pixels;
            int16_t d1x = wid + chr->pos.l.x.f.u - obj->pos.l.x.f.u;
            if (d1x >= 0 && d1x < (wid << 1) && chr->ysp >= 0) {
                chr->pos.l.y.f.u -= d3;
                chr->pos.l.y.f.u -= 1;
                RideObject(obj, chr, who);
                return -1;
            }
            return 0;
        }
    }

    // Not touching: stop pushing
    obj->status.b &= (uint8_t)~push_bit;
    return 0;
}

// Landing on top of a platform (PlatformObject_cont / PlatformObject11_cont): `x_rad` is half the platform's width, `width` the whole span measured from its left edge (a bridge's logs are not as wide as its
// ends), `y_walk` how high above the object's centre the surface is. The character must not be rising and be within 16 pixels of the surface. True if he has just landed on it.
bool Solid_PlatformLand(Object *obj, Object *chr, int who, int16_t x_rad, int16_t width, int16_t y_walk) {
    if (chr->ysp < 0)
        return false;
    int16_t d0 = chr->pos.l.x.f.u - obj->pos.l.x.f.u + x_rad;
    if (d0 < 0 || (uint16_t)d0 >= (uint16_t)width)
        return false;

    int16_t dist = obj->pos.l.y.f.u - y_walk - (int16_t)(chr->pos.l.y.f.u + chr->y_rad + 4);
    if (dist > 0 || (uint16_t)dist < 0xFFF0u) // (the surface has to be within 16 pixels above the feet)
        return false;
    if ((lock_multi & 0x80) || chr->routine >= 6)
        return false;

    chr->pos.l.y.f.u += dist + 3;
    RideObject(obj, chr, who);
    return true;
}

// A platform for either character (PlatformObject_SingleCharacter): while he stands on it he is carried along (`x` is where the platform was before it moved this frame) until he jumps or walks off;
// otherwise he may land on it. `x_rad` is half its width and `y_walk` how high above its centre the surface is.
void Solid_Platform(Object *obj, Object *chr, int who, int16_t x_rad, int16_t y_walk, int16_t x) {
    const uint8_t stand_bit = (uint8_t)(1 << (3 + who));
    if (obj->status.b & stand_bit) {
        int16_t d0 = chr->pos.l.x.f.u - obj->pos.l.x.f.u + x_rad;
        if (chr->status.p.f.in_air || d0 < 0 || (uint16_t)d0 >= (uint16_t)(x_rad << 1)) {
            chr->status.p.f.object_stand = false;
            obj->status.b &= (uint8_t)~stand_bit;
            return;
        }
        MoveOnObject(obj, chr, x, obj->pos.l.y.f.u - y_walk);
        return;
    }
    Solid_PlatformLand(obj, chr, who, x_rad, x_rad << 1, y_walk);
}

// A character is put on an object without a solid test of its own (the corkscrew's path): it stands on it as if it had landed (RideObject_SetRide)
void Solid_Ride(Object *obj, Object *chr, int who) {
    RideObject(obj, chr, who);
}
