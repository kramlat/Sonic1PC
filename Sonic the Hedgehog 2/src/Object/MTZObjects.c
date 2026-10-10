// Metropolis' objects for Sonic 2 (the Simon Wai prototype's 42, 64, 65, 66, 68, 69 and 6D; the rest of the zone's are in MTZObjects2.c, the boxes (6A), the switch (47) and the others it shares are with other zones):
// the steam vent that throws you up (42), the pistons (64), the platforms that slide out (65) with the cog that turns beside them, the springs in the walls (66), the block with an arrow of spikes that turns (68),
// the screw nut that you spin up and down (69) and the harpoon in the floor (6D). All carry Sonic and Tails.
#include "Object/MTZObjects.h"
#include "Constants.h"

#include "Game.h"
#include "Level.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "Object/CharThrow.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Solid.h"
#include "Sound.h"

#include "Macros.h"

#include "Resource/Mappings/MTZBlockArrow.h"
#include "Resource/Mappings/MTZCog.h"
#include "Resource/Mappings/MTZPiston.h"
#include "Resource/Mappings/MTZPlatformA.h"
#include "Resource/Mappings/MTZScrewNut.h"
#include "Resource/Mappings/MTZSpringWall.h"
#include "Resource/Mappings/MTZSteamVent.h"

#define FOR_EACH_CHARACTER(chr, who) \
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) \
        for (Object *chr = Character(who); chr != NULL; chr = NULL)

static Object *Character(int who) {
    return who == SolidChar_Sonic ? player : (TAILS_OBJ->type != 0 ? TAILS_OBJ : NULL);
}

static void ForgetIfRemembered(Object *obj) {
    if (obj->respawn_index)
        objstate[obj->respawn_index] &= 0x7F;
}

// MarkObjGone2: when x is out of sight the object is gone and forgotten, otherwise it is drawn
static void GoneIfOffscreen(Object *obj, int16_t x) {
    if (IS_OFFSCREEN(x)) {
        ForgetIfRemembered(obj);
        ObjectDelete(obj);
        return;
    }
    DisplaySprite(obj);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 42: the steam vent. A flat top you stand on that, every $80 frames, sinks and springs back up (two frames), throwing whoever is on it up at $600 (the subtype's bits as a spring's) and letting out steam
// to either side (child objects that hurt for a few frames). It waits $80 frames at each end
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad0[3];   // 0x29-0x2B
    uint8_t pad1[6];   // 0x2C-0x31
    int16_t wait;      // 0x32
    int16_t base_y;    // 0x34
    int16_t dip;       // 0x36: how far it is down (the top goes from 0x10 down to 0)
} Scratch_SteamVent;

enum { SteamRoutine_Init = 0, SteamRoutine_Vent = 2, SteamRoutine_Puff = 4 };
enum { SteamState_Wait = 0, SteamState_Rise = 2, SteamState_Pause = 4, SteamState_Sink = 6 };

static void SteamVent_Puff(Object *obj, Scratch_SteamVent *scratch, int16_t dx, bool flip) {
    Object *puff = FindFreeObj();
    if (puff == NULL)
        return;
    puff->type = obj->type;
    puff->routine = SteamRoutine_Puff;
    puff->pos.l.x.f.u = (int16_t)(obj->pos.l.x.f.u + dx);
    puff->pos.l.y.f.u = scratch->base_y;
    puff->frame_time.b = 7;
    puff->mappings = obj->mappings;
    puff->tile = TILE_MAP(0, 1, 0, 0, 0x405);
    puff->render.b = SPRITE_CAM_FIELD;
    puff->render.f.x_flip = flip;
    puff->width_pixels = 0x18;
    puff->priority = 4;
}

static void SteamVent_Throw(Object *obj, Scratch_SteamVent *scratch, Object *chr) {
    if (obj->routine_sec != SteamState_Rise)
        return;
    chr->ysp = -0xA00; // (Nick Arcade's was -$600: the alpha's vent throws harder)
    chr->status.p.f.in_air = true;
    chr->status.p.f.object_stand = false;
    chr->anim = SonAnimId_Spring;
    chr->routine = 2;
    if (scratch->subtype & 0x80)
        chr->xsp = 0;
    CharThrow_Tumble(chr, scratch->subtype, 4, 0, 1);
    PlaySound(sfx_Spring);
}

void Obj_MTZSteamVent(Object *obj) {
    Scratch_SteamVent *scratch = (Scratch_SteamVent *)&obj->scratch;

    if (obj->routine == SteamRoutine_Puff) { // a puff of steam: hurts from its fourth frame on (the alpha's: Nick Arcade's was the third, with a smaller hitbox), and goes after its seventh
        if (--obj->frame_time.b >= 0) {
            DisplaySprite(obj);
            return;
        }
        obj->frame_time.b = 7;
        obj->col_type = 0;
        obj->frame++;
        if (obj->frame == 3)
            obj->col_type = 0xA6;
        if (obj->frame == 7) {
            ObjectDelete(obj);
            return;
        }
        DisplaySprite(obj);
        return;
    }

    if (obj->routine == SteamRoutine_Init) {
        obj->routine += 2;
        obj->mappings = Mappings_MTZSteamVent;
        obj->tile = TILE_MAP(0, 3, 0, 0, 0);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->width_pixels = 0x10;
        obj->priority = 4;
        obj->frame = 7;
        scratch->base_y = obj->pos.l.y.f.u;
        scratch->dip = 0x10;
        obj->pos.l.y.f.u += 0x10;
    }

    FOR_EACH_CHARACTER(chr, who) {
        Solid_Character(obj, chr, who, 0x1B, 0x10, 0x10, obj->pos.l.x.f.u, NULL);
        if (obj->status.b & (8 << who))
            SteamVent_Throw(obj, scratch, chr);
    }

    switch (obj->routine_sec) {
    case SteamState_Wait:
        if (--scratch->wait < 0) {
            scratch->wait = 0x7F;
            obj->routine_sec += 2;
        }
        break;
    case SteamState_Rise:
        scratch->dip -= 8;
        if (scratch->dip == 0) {
            obj->routine_sec += 2;
            SteamVent_Puff(obj, scratch, 0x28, false);
            SteamVent_Puff(obj, scratch, -0x28, true);
        }
        obj->pos.l.y.f.u = (int16_t)(scratch->dip + scratch->base_y);
        break;
    case SteamState_Pause:
        if (--scratch->wait < 0) {
            scratch->wait = 0x7F;
            obj->routine_sec += 2;
        }
        break;
    default: // sinking again
        scratch->dip += 8;
        if (scratch->dip == 0x10)
            obj->routine_sec = SteamState_Wait;
        obj->pos.l.y.f.u = (int16_t)(scratch->dip + scratch->base_y);
        break;
    }
    RememberState(obj);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 64: the piston. A solid block (a big or a small one, by the subtype's high nibble) that, with subtype 1, moves in and out by its reach ($40) 8 pixels a frame and waits $3C frames at each end
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28: 0 still, 1 moving (the high nibble was the size)
    uint8_t pad0[5];   // 0x29-0x2D
    uint8_t height;    // 0x2E: its half height, for the solid
    uint8_t pad1;      // 0x2F
    int16_t base_y;    // 0x30
    uint8_t pad2[2];   // 0x32-0x33
    int16_t base_x;    // 0x34
    int16_t wait;      // 0x36
    uint8_t out;       // 0x38: it is going out (1) or in (0)
    uint8_t pad3;      // 0x39
    int16_t offset;    // 0x3A: how far it has moved
    int16_t reach;     // 0x3C
} Scratch_Piston;

// The sizes (loc_1A8C6): half width, half height and reach, by the subtype's high nibble
static const uint8_t piston_sizes[2][3] = { { 0x40, 0x0C, 0x40 }, { 0x10, 0x20, 0x40 } };

void Obj_MTZPiston(Object *obj) {
    Scratch_Piston *scratch = (Scratch_Piston *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        const int size = (scratch->subtype >> 4) & 1; // (the prototype reads past its two entries for the rest: no layout has them)
        obj->width_pixels = piston_sizes[size][0];
        scratch->height = piston_sizes[size][1];
        obj->frame = (uint8_t)size;
        if (size == 0) {
            obj->y_rad = 0x6C;
            obj->render.f.explicit_height = true;
        }
        obj->mappings = Mappings_MTZPiston;
        obj->tile = TILE_MAP(0, 1, 0, 0, 0);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->priority = 4;
        scratch->base_x = obj->pos.l.x.f.u;
        scratch->base_y = obj->pos.l.y.f.u;
        scratch->reach = piston_sizes[size][2];
        scratch->subtype &= 0xF;
    }

    const int16_t old_x = obj->pos.l.x.f.u;
    if (scratch->subtype == 1) {
        if (!scratch->out) {
            if (scratch->offset != 0) {
                scratch->offset -= 8;
            } else if (--scratch->wait < 0) {
                scratch->wait = 0x3C;
                scratch->out = 1;
            }
        }
        if (scratch->out) {
            if (scratch->offset != scratch->reach) {
                scratch->offset += 8;
            } else if (--scratch->wait < 0) {
                scratch->wait = 0x3C;
                scratch->out = 0;
            }
        }
        int16_t d0 = scratch->offset;
        if (obj->status.o.f.x_flip)
            d0 = (int16_t)(0x40 - d0);
        obj->pos.l.y.f.u = (int16_t)(scratch->base_y + d0);
    }

    if (obj->render.f.on_screen) {
        FOR_EACH_CHARACTER(chr, who)
            Solid_Character(obj, chr, who, (int16_t)(obj->width_pixels + 0xB), scratch->height, (int16_t)(scratch->height + 1), old_x, NULL);
    }
    if (IS_OFFSCREEN(scratch->base_x)) {
        ObjectDelete(obj);
        return;
    }
    DisplaySprite(obj);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 65: the platform. Four kinds by the subtype's high nibble (0: a wide one $80 out, 1: a narrower one that comes out when someone is near, 2: only the cog of one, 3: a wide one that is out and goes in) and
// modes by its low (or, with bit 7, the kind's own): 0 still, 1 slides out when its switch is down (and then back $B4 frames later), 2 the way back, 3 slides out as someone comes near, 4 waits to be stood on
// and then 5 goes to and fro between $1800 and $1B40, 6 and 7 the same as 1 and 2 the other way round. A cog (a child object) turns beside it with its position
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28: the mode
    uint8_t pad0[7];   // 0x29-0x2F
    int16_t base_y;    // 0x30
    uint8_t pad1[2];   // 0x32-0x33
    int16_t base_x;    // 0x34
    int16_t wait;      // 0x36
    uint8_t out;       // 0x38: it is going out (the switch has been seen)
    uint8_t pad2;      // 0x39
    int16_t offset;    // 0x3A: how far out it is
    int16_t reach;     // 0x3C: (the cog's: the slot of the platform, in 0x3C as a byte)
    uint8_t switch_id; // 0x3E
    uint8_t pad3;      // 0x3F
} Scratch_MTZPlatform;

typedef struct {
    uint8_t pad0[0x14]; // 0x28-0x3B
    uint8_t platform;   // 0x3C: the slot of the platform
} Scratch_MTZCog;

enum { PlatformRoutine_Init = 0, PlatformRoutine_Main = 2, PlatformRoutine_Cog = 4, PlatformRoutine_FreeCog = 6 };

static int16_t mtz_platform_cog_x; // MTZ_Platform_Cog_X: where the platform that goes to and fro is, which the loose cog follows

// The platforms' sizes (loc_1AA8A): half width, half height, reach and mode, by the subtype's high nibble
static const uint8_t platform_sizes[4][4] = { { 0x40, 0x0C, 0x80, 0x01 }, { 0x20, 0x0C, 0x40, 0x03 }, { 0x10, 0x10, 0x20, 0x00 }, { 0x40, 0x0C, 0x80, 0x07 } };
// The cog's frame by how far it is along (loc_1AE1E)
static const uint8_t cog_frames[8] = { 0, 0, 2, 2, 2, 1, 1, 1 };

static void Platform_SetX(Object *obj, Scratch_MTZPlatform *scratch, int16_t flip_total) {
    int16_t d0 = scratch->offset;
    if (obj->status.o.f.x_flip)
        d0 = (int16_t)(flip_total - d0);
    obj->pos.l.x.f.u = (int16_t)(scratch->base_x - d0);
}

static void Platform_Remember(Object *obj, bool set) {
    if (!obj->respawn_index)
        return;
    if (set)
        objstate[obj->respawn_index] |= 1;
    else
        objstate[obj->respawn_index] &= (uint8_t)~1;
}

void Obj_MTZPlatform(Object *obj) {
    Scratch_MTZPlatform *scratch = (Scratch_MTZPlatform *)&obj->scratch;

    if (obj->routine == PlatformRoutine_Cog) { // the cog beside a platform turns with how far out the platform is
        Object *platform = &objects[((Scratch_MTZCog *)&obj->scratch)->platform];
        const int16_t offset = ((Scratch_MTZPlatform *)&platform->scratch)->offset;
        obj->frame = cog_frames[offset & 7];
        RememberState(obj);
        return;
    }
    if (obj->routine == PlatformRoutine_FreeCog) { // ... and a loose one with the platform that goes to and fro
        obj->frame = cog_frames[mtz_platform_cog_x & 7];
        RememberState(obj);
        return;
    }

    if (obj->routine == PlatformRoutine_Init) {
        obj->routine += 2;
        obj->mappings = Mappings_MTZPlatformA;
        obj->tile = TILE_MAP(0, 3, 0, 0, 0);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->priority = 4;
        const int kind = (scratch->subtype >> 4) & 3;
        obj->width_pixels = platform_sizes[kind][0];
        obj->y_rad = platform_sizes[kind][1];
        obj->frame = (uint8_t)kind;
        if (kind == 1)
            obj->status.b |= 0x80;
        if (kind == 2) { // only the cog
            obj->routine += 4;
            obj->mappings = Mappings_MTZCog;
            obj->tile = TILE_MAP(0, 3, 0, 0, 0x55F);
            obj->frame = cog_frames[mtz_platform_cog_x & 7];
            RememberState(obj);
            return;
        }
        scratch->base_x = obj->pos.l.x.f.u;
        scratch->base_y = obj->pos.l.y.f.u;
        scratch->reach = platform_sizes[kind][2];
        if (scratch->subtype & 0x80) {
            scratch->switch_id = scratch->subtype & 0xF;
            scratch->subtype = platform_sizes[kind][3];
            if (scratch->subtype == 7)
                scratch->offset = scratch->reach;
            Object *cog = FindNextFreeObj(obj + 1);
            if (cog != NULL) {
                cog->type = obj->type;
                cog->routine = PlatformRoutine_Cog;
                cog->pos.l.x.f.u = (int16_t)(obj->pos.l.x.f.u - 0x4C);
                cog->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u + 0x14);
                cog->render.b = 0;
                if (!obj->status.o.f.x_flip) {
                    cog->pos.l.x.f.u = (int16_t)(cog->pos.l.x.f.u + 0x18);
                    cog->render.f.x_flip = true;
                }
                cog->mappings = Mappings_MTZCog;
                cog->tile = TILE_MAP(0, 3, 0, 0, 0x55F);
                cog->render.b |= SPRITE_CAM_FIELD;
                cog->width_pixels = 0x10;
                cog->priority = 4;
                ((Scratch_MTZCog *)&cog->scratch)->platform = (uint8_t)(obj - objects);
            }
            ForgetIfRemembered(obj);
        }
        scratch->subtype &= 0xF;
    }

    const int16_t old_x = obj->pos.l.x.f.u;
    Object *sonic = player, *tails = TAILS_OBJ;
    switch (scratch->subtype) {
    case 0:
        break;
    case 1: // slides out when its switch is down
    case 6: // (6: after its wait)
    case 7: // (7: out, goes in when its switch is down)
    case 2: { // (2: out, goes in after its wait)
        const int mode = scratch->subtype;
        const bool out_direction = (mode == 1 || mode == 6);
        if (mode == 6) {
            if (!scratch->out) {
                if (--scratch->wait == 0)
                    scratch->out = 1;
                else {
                    Platform_SetX(obj, scratch, 0x80);
                    break;
                }
            }
        } else if (mode == 1 || mode == 7) {
            if (!scratch->out) {
                if (!(f_switch[scratch->switch_id] & 1)) {
                    Platform_SetX(obj, scratch, 0x80);
                    break;
                }
                scratch->out = 1;
            }
        } else if (mode == 2) {
            if (!scratch->out) {
                if (--scratch->wait == 0)
                    scratch->out = 1;
                else {
                    Platform_SetX(obj, scratch, 0x80);
                    break;
                }
            }
        }
        if (out_direction) {
            if (scratch->reach == scratch->offset) { // all the way out: the mode changes and it waits
                scratch->subtype++;
                scratch->wait = 0xB4;
                scratch->out = 0;
                Platform_Remember(obj, true);
            } else {
                scratch->offset += 2;
            }
        } else if (scratch->offset == 0) { // all the way in
            scratch->subtype--;
            scratch->wait = 0xB4;
            scratch->out = 0;
            Platform_Remember(obj, false);
        } else {
            scratch->offset -= 2;
        }
        Platform_SetX(obj, scratch, 0x80);
        break;
    }
    case 3: { // slides out as a character comes near (to the right of it; the left if flipped), and back
        int16_t x0 = scratch->base_x, x1 = scratch->base_x;
        if (!obj->status.o.f.x_flip) {
            x0 -= 0x20;
            x1 += 0x60;
        } else {
            x0 -= 0xA0;
            x1 -= 0x20;
        }
        const int16_t y0 = (int16_t)(obj->pos.l.y.f.u - 0x10), y1 = (int16_t)(obj->pos.l.y.f.u + 0x40);
        bool near = false;
        for (int i = 0; i < 2; i++) {
            Object *chr = i == 0 ? sonic : tails;
            if ((uint16_t)(chr->pos.l.x.f.u - x0) < (uint16_t)(x1 - x0) && (uint16_t)(chr->pos.l.y.f.u - y0) < (uint16_t)(y1 - y0))
                near = true;
        }
        if (near) {
            if (scratch->reach == scratch->offset)
                break; // (it stays where it is)
            scratch->offset += 0x10;
        } else if (scratch->offset != 0) {
            scratch->offset -= 0x10;
        }
        Platform_SetX(obj, scratch, 0x40);
        break;
    }
    case 4: // waits to be stood on
        if (obj->status.b & 8)
            scratch->subtype++;
        break;
    case 5: // goes to and fro (the alpha's: out to $1BC0 and back to $1880; in the third act it goes right to $2940 and stays)
        if (!scratch->out) {
            obj->pos.l.x.f.u += 2;
            if (LEVEL_ZONE(level_id) == ZoneId_MTZ3) {
                if (obj->pos.l.x.f.u == 0x2940)
                    scratch->subtype = 0;
            } else if (obj->pos.l.x.f.u == 0x1BC0) {
                scratch->out = 1;
            }
        } else {
            obj->pos.l.x.f.u -= 2;
            if (obj->pos.l.x.f.u == 0x1880)
                scratch->out = 0;
        }
        scratch->base_x = obj->pos.l.x.f.u;
        mtz_platform_cog_x = obj->pos.l.x.f.u;
        break;
    }

    FOR_EACH_CHARACTER(chr, who)
        Solid_Character(obj, chr, who, (int16_t)(obj->width_pixels + 0xB), obj->y_rad, (int16_t)(obj->y_rad + 1), old_x, NULL);
    if (IS_OFFSCREEN(scratch->base_x)) {
        ForgetIfRemembered(obj);
        ObjectDelete(obj);
        return;
    }
    DisplaySprite(obj);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 66: the spring in the wall. Solid and not drawn (the prototype only draws it in debug mode): a character in the air that pushes against its open side (the right, or the left if flipped) is thrown away
// from it and up, at $800 each way, rolling
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28
} Scratch_WallSpring;

static void WallSpring_Throw(Object *obj, Scratch_WallSpring *scratch, Object *chr, int who) {
    chr->xsp = -0x800;
    chr->ysp = -0x800;
    chr->status.p.f.x_flip = true;
    if (!obj->status.o.f.x_flip) {
        chr->status.p.f.x_flip = false;
        chr->xsp = (int16_t)-chr->xsp;
    }
    ((Scratch_Sonic *)&chr->scratch)->control_lock = 0xF;
    chr->inertia = chr->xsp;
    if (!chr->status.p.f.in_ball)
        chr->anim = SonAnimId_Walk;
    if (scratch->subtype & 0x80)
        chr->ysp = 0;
    CharThrow_Tumble(chr, scratch->subtype, 8, 1, 3);
    obj->status.b &= (uint8_t)~0x60;
    chr->status.p.f.pushing = false;
    PlaySound(sfx_Spring);
    (void)who;
}

void Obj_MTZSpringWall(Object *obj) {
    Scratch_WallSpring *scratch = (Scratch_WallSpring *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        obj->mappings = Mappings_MTZSpringWall;
        obj->tile = TILE_MAP(1, 0, 0, 0, 0x680);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->width_pixels = 8;
        obj->priority = 4;
        obj->y_rad = 0x40;
        obj->frame = (scratch->subtype >> 4) & 7;
        if (obj->frame != 0)
            obj->y_rad = (int8_t)0x80;
    }

    FOR_EACH_CHARACTER(chr, who) {
        const uint8_t height = (uint8_t)obj->y_rad;
        if (Solid_Character(obj, chr, who, 0x13, height, (int16_t)(height + 1), obj->pos.l.x.f.u, NULL) != 1)
            continue;
        if (!chr->status.p.f.in_air)
            continue;
        uint8_t d1 = obj->status.b;
        if (obj->pos.l.x.f.u >= chr->pos.l.x.f.u)
            d1 ^= 1;
        if (!(d1 & 1))
            WallSpring_Throw(obj, scratch, chr, who);
    }
    if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
        ObjectDelete(obj);
        return;
    }
    if (debug_use)
        DisplaySprite(obj);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 68: the block with an arrow. A solid block (frame 4) that makes a spiked arrow (a child object that is the same object) which slides out to $20 pixels and back, in the direction (up, right, down, left) of
// its frame, and then turns a quarter to the next, a long breath between. Object 6D (the harpoon in the floor) is the same spike
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad0[7];   // 0x29-0x2F
    int16_t base_x;    // 0x30
    int16_t base_y;    // 0x32
    uint16_t slide;    // 0x34: how far it has slid out (16.8: the high byte is pixels)
    int16_t going_in;  // 0x36
    int16_t waiting;   // 0x38
    int16_t pause;     // 0x3A: (the harpoon's)
} Scratch_BlockArrow;

enum { ArrowRoutine_Init = 0, ArrowRoutine_Block = 2, ArrowRoutine_Arrow = 4 };

static const uint8_t arrow_cols[4] = { 0x84, 0xA6, 0x84, 0xA6 };

void Obj_MTZBlockArrow(Object *obj) {
    Scratch_BlockArrow *scratch = (Scratch_BlockArrow *)&obj->scratch;

    if (obj->routine == ArrowRoutine_Arrow) {
        // loc_1B656: the slide
        bool slide = true;
        if (scratch->waiting != 0) {
            if ((frame_count & 0x3F) != 0)
                slide = false;
            else {
                scratch->waiting = 0;
                if (obj->render.f.on_screen)
                    PlaySound(sfx_SpikesMove);
            }
        }
        if (slide) {
            if (scratch->going_in) {
                scratch->slide = (uint16_t)(scratch->slide - 0x800);
                if (scratch->slide > 0x2000) { // (it went below 0: a borrow)
                    scratch->slide = 0;
                    scratch->going_in = 0;
                    scratch->waiting = 1;
                    obj->routine_sec = (uint8_t)((obj->routine_sec + 1) & 3);
                    obj->frame = obj->routine_sec;
                    obj->col_type = arrow_cols[obj->routine_sec];
                }
            } else {
                scratch->slide = (uint16_t)(scratch->slide + 0x800);
                if (scratch->slide >= 0x2000) {
                    scratch->slide = 0x2000;
                    scratch->going_in = 1;
                    scratch->waiting = 1;
                }
            }
        }
        const int16_t hi = (int16_t)(scratch->slide >> 8);
        switch (obj->routine_sec) {
        case 0: obj->pos.l.y.f.u = (int16_t)(scratch->base_y - hi); break;
        case 1: obj->pos.l.x.f.u = (int16_t)(scratch->base_x + hi); break;
        case 2: obj->pos.l.y.f.u = (int16_t)(scratch->base_y + hi); break;
        default: obj->pos.l.x.f.u = (int16_t)(scratch->base_x - hi); break;
        }
        GoneIfOffscreen(obj, scratch->base_x);
        return;
    }

    if (obj->routine == ArrowRoutine_Init) {
        obj->routine += 2;
        obj->mappings = Mappings_MTZBlockArrow;
        obj->tile = TILE_MAP(0, 3, 0, 0, 0x414);
        obj->render.b = SPRITE_CAM_FIELD;
        obj->width_pixels = 0x10;
        obj->priority = 4;
        Object *arrow = FindNextFreeObj(obj + 1);
        if (arrow != NULL) {
            arrow->type = obj->type;
            arrow->routine = ArrowRoutine_Arrow;
            arrow->pos.l.x.f.u = obj->pos.l.x.f.u;
            arrow->pos.l.y.f.u = obj->pos.l.y.f.u;
            Scratch_BlockArrow *other = (Scratch_BlockArrow *)&arrow->scratch;
            other->base_x = arrow->pos.l.x.f.u;
            other->base_y = arrow->pos.l.y.f.u;
            arrow->mappings = obj->mappings;
            arrow->tile = TILE_MAP(0, 1, 0, 0, 0x41C);
            arrow->render.b |= SPRITE_CAM_FIELD;
            arrow->width_pixels = 0x10;
            arrow->priority = 4;
            const uint16_t t = (uint16_t)(frame_count >> 6);
            other->going_in = (int16_t)(t & 1);
            const int direction = (((t >> 1) + scratch->subtype) & 3);
            arrow->routine_sec = (uint8_t)direction;
            arrow->frame = (uint8_t)direction;
            arrow->col_type = arrow_cols[direction];
        }
        obj->frame = 4;
    }

    FOR_EACH_CHARACTER(chr, who)
        Solid_Character(obj, chr, who, 0x1B, 0x10, 0x11, obj->pos.l.x.f.u, NULL);
    RememberState(obj);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 6D: the harpoon in the floor. A spike (the block arrow's frame 0, hurting) that comes up $20 pixels, and goes down, on a beat the subtype is the offset of: it waits the other half of its beat
// ---------------------------------------------------------------------------------------------------------------------------------------
void Obj_MTZHarpoon(Object *obj) {
    Scratch_BlockArrow *scratch = (Scratch_BlockArrow *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        obj->mappings = Mappings_MTZBlockArrow;
        obj->tile = TILE_MAP(0, 1, 0, 0, 0x41C);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->width_pixels = 4;
        obj->priority = 4;
        scratch->base_x = obj->pos.l.x.f.u;
        scratch->base_y = obj->pos.l.y.f.u;
        obj->col_type = 0x84;
    }

    // loc_1B788
    if (scratch->pause != 0) {
        scratch->pause--;
    } else {
        bool move = true;
        if (scratch->waiting != 0) {
            if (((frame_count - scratch->subtype) & 0x7F) != 0)
                move = false;
            else
                scratch->waiting = 0;
        }
        if (move) {
            if (scratch->going_in) {
                scratch->slide = (uint16_t)(scratch->slide - 0x400);
                if (scratch->slide > 0x2000) {
                    scratch->slide = 0;
                    scratch->going_in = 0;
                    scratch->waiting = 1;
                }
            } else {
                scratch->slide = (uint16_t)(scratch->slide + 0x400);
                if (scratch->slide >= 0x2000) {
                    scratch->slide = 0x2000;
                    scratch->going_in = 1;
                    scratch->pause = 3;
                }
            }
        }
    }
    obj->pos.l.y.f.u = (int16_t)(scratch->base_y - (int16_t)(scratch->slide >> 8));
    GoneIfOffscreen(obj, scratch->base_x);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 69: the screw nut. Stand on it and roll to its middle, and walking across it turns the screw: the nut goes down (or up) as the frames turn, by the subtype's range x 8; at the bottom of a subtype of bit 7 it
// falls to the floor. Each character has a state of his own (the prototype keeps them at 0x38 and 0x3C)
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad0[3];   // 0x29-0x2B
    uint8_t pad1[4];   // 0x2C-0x2F
    uint8_t pad2[2];   // 0x30-0x31
    int16_t base_y;    // 0x32
    int16_t turn;      // 0x34: how far the screw has been turned
    int16_t range;     // 0x36
    uint8_t state[2];  // 0x38 and 0x3A? (the prototype's are 0x38 and 0x3C: here each is a state byte and a side byte)
    uint8_t side[2];
} Scratch_ScrewNut;

enum { NutRoutine_Init = 0, NutRoutine_Screw = 2, NutRoutine_Fall = 4, NutRoutine_Solid = 6 };

static void ScrewNut_Frame(Object *obj, Scratch_ScrewNut *scratch) {
    const int16_t d1 = (int16_t)(scratch->turn >> 3);
    obj->frame = (uint8_t)(((d1 >> 1) & 3));
    obj->pos.l.y.f.u = (int16_t)(scratch->base_y - d1);
}

static void ScrewNut_Char(Object *obj, Scratch_ScrewNut *scratch, Object *chr, int who) {
    uint8_t *state = &scratch->state[who];
    uint8_t *side = &scratch->side[who];
    if (!(obj->status.b & (8 << who)))
        *state = 0;
    switch (*state) {
    case 0:
        if (!(obj->status.b & (8 << who)))
            return;
        *state += 2;
        *side = (uint8_t)(obj->pos.l.x.f.u < chr->pos.l.x.f.u);
        // Fallthrough
    case 2: {
        int16_t d0 = (int16_t)(chr->pos.l.x.f.u - obj->pos.l.x.f.u);
        if (*side)
            d0 += 0xF;
        if ((uint16_t)d0 < 0x10) {
            chr->pos.l.x.f.u = obj->pos.l.x.f.u;
            *state += 2;
        }
        return;
    }
    default: {
        const int16_t d0 = (int16_t)(chr->pos.l.x.f.u - obj->pos.l.x.f.u);
        if (d0 < 0) { // walking left of the middle turns it down
            scratch->turn += d0;
            chr->pos.l.x.f.u = obj->pos.l.x.f.u;
            ScrewNut_Frame(obj, scratch);
            const int16_t depth = (int16_t)(-(scratch->turn >> 3));
            if (depth < scratch->range)
                return;
            obj->pos.l.y.f.u = (int16_t)(scratch->base_y + scratch->range);
            scratch->turn = (int16_t)(-(scratch->range << 3));
            obj->frame = 0;
            if (!(scratch->subtype & 0x80))
                *state = 0;
            else
                obj->routine = NutRoutine_Fall;
        } else { // ... and right turns it up
            scratch->turn += d0;
            chr->pos.l.x.f.u = obj->pos.l.x.f.u;
            ScrewNut_Frame(obj, scratch);
        }
        return;
    }
    }
}

void Obj_MTZScrewNut(Object *obj) {
    Scratch_ScrewNut *scratch = (Scratch_ScrewNut *)&obj->scratch;

    switch (obj->routine) {
    case NutRoutine_Init:
        obj->routine += 2;
        obj->mappings = Mappings_MTZScrewNut;
        obj->tile = TILE_MAP(0, 1, 0, 0, 0x500);
        obj->render.b = SPRITE_CAM_FIELD;
        obj->width_pixels = 0x20;
        obj->y_rad = 0x0B;
        obj->priority = 4;
        scratch->base_y = obj->pos.l.y.f.u;
        scratch->range = (int16_t)((scratch->subtype & 0x7F) << 3);
        // Fallthrough
    case NutRoutine_Screw:
        FOR_EACH_CHARACTER(chr, who)
            ScrewNut_Char(obj, scratch, chr, who);
        break;
    case NutRoutine_Fall: {
        SpeedToPos(obj);
        obj->ysp += 0x38;
        const int16_t d1 = ObjFloorDist(obj, obj->pos.l.x.f.u);
        if (d1 < 0) {
            obj->pos.l.y.f.u += d1;
            obj->ysp = 0;
            obj->routine += 2;
        }
        break;
    }
    default:
        break;
    }
    FOR_EACH_CHARACTER(chr, who)
        Solid_Character(obj, chr, who, 0x2B, 0x0C, 0x0D, obj->pos.l.x.f.u, NULL);
    RememberState(obj);
}
