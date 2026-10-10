// Chemical Plant's own objects, the Simon Wai prototype's: the elevator (19, Obj_0x19_Elevator at loc_1621C), the speed booster (1B, Obj1B), the section of pipe that tips you off (0B, Obj0B), the moving blocks
// that make its staircases (6B, Obj_0x6B_Mz_Platform at loc_1BCEC), the one way barrier (2D), the tube cover (32), the rotating platforms (78), the sliding platforms (7A), the invisible block (74)
// and the droplet chain badnik (1D). Each is a port of its routine in the prototype's disassembly.
#include "Object/CPZObjects.h"
#include "Constants.h"

#include "Camera.h"
#include "Game.h"
#include "Level.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "MathUtil.h"
#include "Oscillatory Routines.h"
#include "Solid.h"
#include "Sound.h"

#include "Macros.h"

#include "Resource/Mappings/Booster.h"
#include "Resource/Mappings/CPZBarrier.h"
#include "Resource/Mappings/CPZBlock.h"
#include "Resource/Mappings/CPZInvisibleBlock.h"
#include "Resource/Mappings/CPZSlider.h"
#include "Resource/Mappings/CPZWorm.h"
#include "Resource/Mappings/FloatingPlatform.h"
#include "Resource/Mappings/CPZElevator.h"
#include "Resource/Mappings/PipeTipper.h"
#include "Resource/Mappings/TubeCover.h"
#include "Resource/Mappings/HTZRock.h"

extern const uint8_t Mappings_MTZPlatformA[]; // (defined with MTZObjects.c, whose platforms they are)

extern Oscillatory oscillatory;

// What the object does for each of the two characters: `chr` is NULL for a Tails who is not there
#define FOR_EACH_CHARACTER(chr, who) \
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) \
        for (Object *chr = (who == SolidChar_Sonic) ? player : (TAILS_OBJ->type != 0 ? TAILS_OBJ : NULL); chr != NULL; chr = NULL)

// The object is out of range: it goes, and forgets that it was loaded so that it can come back (MarkObjGone)
static void GoneIfOffscreen(Object *obj, int16_t x) {
    if (IS_OFFSCREEN(x)) {
        if (obj->respawn_index)
            objstate[obj->respawn_index] &= 0x7F;
        ObjectDelete(obj);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 19: the elevator, a platform that moves by its subtype
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;  // 0x28: bits 4-7 which size, bits 0-3 how it moves (the low four are the movement's step too: some kinds hand on to the next)
    uint8_t pad[7];
    int16_t base_x;   // 0x30
    int16_t base_y;   // 0x32
} Scratch_Elevator;

// loc_1622E: the width and the mapping frame of each size
static const uint8_t elevator_sizes[5][2] = { { 0x20, 0 }, { 0x18, 1 }, { 0x20, 2 }, { 0x40, 3 }, { 0x30, 4 } };

// The oscillators of the prototype's Oscillating_Data, by their offset (4 bytes each): the high byte of the value
static int16_t Osc(int offset) {
    return (int16_t)(uint8_t)(oscillatory.state[offset >> 2][0] >> 8);
}

static void Elevator_Move(Object *obj, Scratch_Elevator *scratch) {
    const int kind = scratch->subtype & 0xF;
    switch (kind) {
    case 0:
    case 1: { // slides along x
        int16_t d0 = kind == 0 ? Osc(8) : Osc(0xC), d1 = kind == 0 ? 0x40 : 0x60;
        if (obj->status.o.f.x_flip)
            d0 = (int16_t)(-d0 + d1);
        obj->pos.l.x.f.u = (int16_t)(scratch->base_x - d0);
        break;
    }
    case 2: { // slides along y
        int16_t d0 = Osc(0x1C), d1 = 0x80;
        if (obj->status.o.f.x_flip)
            d0 = (int16_t)(-d0 + d1);
        obj->pos.l.y.f.u = (int16_t)(scratch->base_y - d0);
        break;
    }
    case 3: // waits for someone to stand on it
        if (obj->status.b & 0x18)
            scratch->subtype++;
        break;
    case 4: // then rises, slowing down to a stop at the top
    case 6:
    case 7: { // (6 and 7 never stop: they bob up and down above their start)
        SpeedToPos(obj);
        int16_t step = 8;
        int16_t top = (int16_t)(scratch->base_y - 0x60);
        if ((uint16_t)top < (uint16_t)obj->pos.l.y.f.u)
            step = -step;
        obj->ysp = (int16_t)(obj->ysp + step);
        if (kind == 4 && obj->ysp == 0)
            scratch->subtype++;
        break;
    }
    case 5: // (stopped)
        break;
    default: { // 8-15: round in a circle (the two oscillators a quarter of a turn apart), one way or the other and by which side it starts
        int16_t d1 = (int16_t)(Osc(0x38) - 0x40), d2 = (int16_t)(Osc(0x3C) - 0x40);
        if (kind & 4) {
            d1 = (int16_t)-d1;
            d2 = (int16_t)-d2;
        }
        if (kind & 2) {
            d1 = (int16_t)-d1;
            int16_t t = d1;
            d1 = d2;
            d2 = t;
        }
        if (kind >= 12)
            d1 = (int16_t)-d1;
        obj->pos.l.x.f.u = (int16_t)(scratch->base_x + d1);
        obj->pos.l.y.f.u = (int16_t)(scratch->base_y + d2);
        break;
    }
    }
}

void Obj_CPZElevator(Object *obj) {
    Scratch_Elevator *scratch = (Scratch_Elevator *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        obj->mappings = Mappings_CPZElevator;
        obj->tile = TILE_MAP(0, 3, 0, 0, LEVEL_ZONE(level_id) == ZoneId_OOZ ? 0x300 : 0x3A0);
        obj->render.b = SPRITE_CAM_FIELD;
        int size = (scratch->subtype >> 4) & 0xF;
        if (size > 4)
            size = 4;
        obj->width_pixels = elevator_sizes[size][0];
        obj->frame = elevator_sizes[size][1] > 2 ? 2 : elevator_sizes[size][1]; // (the mappings have three frames)
        obj->priority = 4;
        scratch->base_x = obj->pos.l.x.f.u;
        scratch->base_y = obj->pos.l.y.f.u;
        scratch->subtype &= 0xF;
        if (scratch->subtype == 7)
            obj->pos.l.y.f.u -= 0xC0;
    }

    int16_t old_x = obj->pos.l.x.f.u;
    Elevator_Move(obj, scratch);
    FOR_EACH_CHARACTER(chr, who)
        Solid_Platform(obj, chr, who, obj->width_pixels, 0x10, old_x);

    if (IS_OFFSCREEN(scratch->base_x)) {
        GoneIfOffscreen(obj, scratch->base_x);
        return;
    }
    DisplaySprite(obj);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 1B: the speed booster
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;  // 0x28: bit 1 the slower one
    uint8_t pad[7];
    int16_t speed;    // 0x30
} Scratch_Booster;

#define ARTTILE_BOOSTER 0x39C // ($7380 in the zone's art list)

static void Booster_Give(Object *obj, Object *chr, int16_t speed) {
    chr->xsp = speed; // (goes super fast)
    chr->status.p.f.x_flip = false;
    if (obj->status.o.f.x_flip) {
        chr->status.p.f.x_flip = true;
        chr->xsp = (int16_t)-chr->xsp;
    }
    ((Scratch_Sonic *)&chr->scratch)->control_lock = 0xF; // (not to turn round for a few frames)
    chr->inertia = chr->xsp;
    obj->status.b &= (uint8_t)~((1 << 5) | (1 << 6));
    chr->status.p.f.pushing = false;
    PlaySound(sfx_Spring);
}

void Obj_CPZBooster(Object *obj) {
    Scratch_Booster *scratch = (Scratch_Booster *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        obj->mappings = Mappings_Booster;
        obj->tile = TILE_MAP(1, 3, 0, 0, ARTTILE_BOOSTER);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->width_pixels = 0x20;
        obj->priority = 1;
        scratch->speed = (scratch->subtype & 2) ? 0xA00 : 0x1000;
    }

    obj->frame = (uint8_t)(frame_count & 2);
    const int16_t x0 = (int16_t)(obj->pos.l.x.f.u - 0x10), x1 = (int16_t)(obj->pos.l.x.f.u + 0x10);
    const int16_t y0 = (int16_t)(obj->pos.l.y.f.u - 0x10), y1 = (int16_t)(obj->pos.l.y.f.u + 0x10);
    FOR_EACH_CHARACTER(chr, who) {
        (void)who;
        if (chr->status.p.f.in_air)
            continue;
        uint16_t cx = (uint16_t)chr->pos.l.x.f.u, cy = (uint16_t)chr->pos.l.y.f.u;
        if (cx >= (uint16_t)x0 && cx < (uint16_t)x1 && cy >= (uint16_t)y0 && cy < (uint16_t)y1)
            Booster_Give(obj, chr, scratch->speed);
    }
    RememberState(obj);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 0B: the section of pipe that tips you off
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;  // 0x28: bits 4-7 how long it stays (in 8ths of a second), bits 0-3 when it first turns
    uint8_t pad[7];
    int16_t wait;     // 0x30
    int16_t wait2;    // 0x32
    uint8_t offset;   // 0x36
} Scratch_PipeTipper;

#define ARTTILE_PIPE_TIPPER 0x3B0 // ($7600 in the zone's art list)

// Ani_obj0B: it tips over to its fifth frame and back
static const uint8_t Animation_PipeTipper[] = {
    0x00, 0x04, 0x00, 0x0C,
    0x07, 0x00, 0x01, 0x02, 0x03, 0x04, 0xFE, 0x01,
    0x07, 0x04, 0x03, 0x02, 0x01, 0x00, 0xFE, 0x01,
};

void Obj_CPZPipeTipper(Object *obj) {
    Scratch_PipeTipper *scratch = (Scratch_PipeTipper *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        obj->mappings = Mappings_PipeTipper;
        obj->tile = TILE_MAP(1, 3, 0, 0, ARTTILE_PIPE_TIPPER);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->width_pixels = 0x10;
        obj->priority = 4;
        int d0 = (scratch->subtype & 0xF0) + 0x10;
        scratch->wait = (int16_t)(d0 - 1);
        scratch->wait2 = (int16_t)(d0 - 1);
        scratch->offset = (uint8_t)(((scratch->subtype & 0xF) + 1) << 4);
    }

    if (obj->routine == 2) { // waits until the frame counter and its offset make 0
        if ((uint8_t)(frame_count + scratch->offset) != 0) {
            RememberState(obj);
            return;
        }
        obj->routine += 2;
    }
    // Turn
    if (--scratch->wait < 0) {
        scratch->wait = 0x7F;
        if (obj->anim != 0)
            scratch->wait = scratch->wait2;
        obj->anim ^= 1;
        obj->prev_anim = 0xFF; // (the animation starts again)
    }
    AnimateSprite(obj, Animation_PipeTipper);

    if (obj->frame == 0) { // upright: a platform
        FOR_EACH_CHARACTER(chr, who)
            Solid_Platform(obj, chr, who, 0x10, 0x11, obj->pos.l.x.f.u);
    } else { // tipped: whoever stands on it falls
        FOR_EACH_CHARACTER(chr, who) {
            if (obj->status.b & (1 << (3 + who))) {
                chr->status.p.f.object_stand = false;
                obj->status.b &= (uint8_t)~(1 << (3 + who));
            }
        }
    }
    RememberState(obj);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 6B: the moving block (the staircase): a solid block that slides, waits to be stood on and then falls, or travels round a square, by its subtype
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;  // 0x28: bit 4 the size, bits 0-3 how it moves
    uint8_t pad[7];
    int16_t base_y;   // 0x30
    int16_t base_x;   // 0x34
    uint8_t started;  // 0x38: (kind 7) it has been stood on
} Scratch_CPZBlock;

#define ARTTILE_CPZ_BLOCK 0x418 // ($8300 in the zone's art list)

// What the block does (loc_1BDC8): 0 nothing (7 is the alpha's new one, below), 1 and 2 slide along x, 3 and 4 along y, 5 waits to be stood on and goes on to 6, which falls; 8-11 go round a square of side 2x$10, $30, $50 or $70, as the
// oscillator that the kind names goes
static void CPZBlock_Move(Object *obj, Scratch_CPZBlock *scratch) {
    const int kind = scratch->subtype & 0xF;
    switch (kind) {
    case 1:
    case 2:
    case 3:
    case 4: {
        int16_t d1 = (kind == 1 || kind == 3) ? 0x40 : 0x80;
        int16_t d0 = (kind == 1 || kind == 3) ? Osc(8) : Osc(0x1C);
        if (obj->status.o.f.x_flip)
            d0 = (int16_t)(-d0 + d1);
        if (kind <= 2)
            obj->pos.l.x.f.u = (int16_t)(scratch->base_x - d0);
        else
            obj->pos.l.y.f.u = (int16_t)(scratch->base_y - d0);
        break;
    }
    case 5:
        obj->pos.l.y.f.u = (int16_t)(scratch->base_y + (Osc(0) >> 1));
        if (obj->status.b & 0x18)
            scratch->subtype++;
        break;
    case 6:
        obj->pos.l.y.v += obj->ysp * 256;
        obj->ysp = (int16_t)(obj->ysp + 8);
        if (!((uint16_t)(limit_btm1 + 0xE0) >= (uint16_t)obj->pos.l.y.f.u))
            scratch->subtype = 0;
        break;
    case 7: // (the alpha's: waits to be stood on, then drops $70 pixels as a pendulum would, speeding up and slowing down by 8, and stops where its speed is 0 again)
        if (!scratch->started) {
            if (!(obj->status.b & 0x18))
                break;
            scratch->started = 1;
        }
        SpeedToPos(obj);
        {
            int16_t accel = 8;
            if ((uint16_t)(scratch->base_y + 0x70) < (uint16_t)obj->pos.l.y.f.u)
                accel = -8;
            obj->ysp = (int16_t)(obj->ysp + accel);
            if (obj->ysp == 0)
                scratch->subtype = 0;
        }
        break;
    case 8:
    case 9:
    case 10:
    case 11: {
        static const int16_t sides[4] = { 0x10, 0x30, 0x50, 0x70 };
        const int osc = 10 + (kind - 8) * 1; // the oscillator pairs (Oscillating_Data+$28, +$2C, +$30, +$34: entries 10, 11, 12, 13)
        int16_t d1 = sides[kind - 8];
        int16_t d0 = (int16_t)(oscillatory.state[osc][0] >> 8);
        if (kind == 8)
            d0 = (int16_t)(d0 >> 1);
        int16_t d3 = (int16_t)oscillatory.state[osc][1];
        if (d3 == 0)
            obj->status.b = (uint8_t)((obj->status.b & ~3) | ((obj->status.b + 1) & 3)); // (the phase: which side of the square)
        switch (obj->status.b & 3) {
        case 0:
            obj->pos.l.x.f.u = (int16_t)(scratch->base_x + d0 - d1);
            obj->pos.l.y.f.u = (int16_t)(scratch->base_y - d1);
            break;
        case 1:
            obj->pos.l.y.f.u = (int16_t)(scratch->base_y - (d0 - (d1 - 1)));
            obj->pos.l.x.f.u = (int16_t)(scratch->base_x + d1);
            break;
        case 2:
            obj->pos.l.x.f.u = (int16_t)(scratch->base_x - (d0 - (d1 - 1)));
            obj->pos.l.y.f.u = (int16_t)(scratch->base_y + d1);
            break;
        default:
            obj->pos.l.y.f.u = (int16_t)(scratch->base_y + d0 - d1);
            obj->pos.l.x.f.u = (int16_t)(scratch->base_x - d1);
            break;
        }
        break;
    }
    default:
        break;
    }
}

void Obj_CPZBlock(Object *obj) {
    Scratch_CPZBlock *scratch = (Scratch_CPZBlock *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        const bool cpz = LEVEL_ZONE(level_id) == ZoneId_CPZ; // (loc_1BD06: Metropolis's platforms are the slide-out platform's: object 65's mappings)
        obj->mappings = cpz ? Mappings_CPZBlock : Mappings_MTZPlatformA;
        obj->tile = TILE_MAP(0, 3, 0, 0, cpz ? ARTTILE_CPZ_BLOCK : 0);
        obj->render.b = SPRITE_CAM_FIELD;
        obj->priority = 3;
        const bool small = (scratch->subtype >> 4) & 1; // (loc_1BCFE: a wide block of 12 high, or a square one of 16)
        obj->width_pixels = small ? 0x10 : 0x20;
        obj->y_rad = small ? 0x10 : 0x0C;
        obj->frame = small ? 0 : 1;
        scratch->base_x = obj->pos.l.x.f.u;
        scratch->base_y = obj->pos.l.y.f.u;
        const int kind = scratch->subtype & 0xF;
        if (kind >= 8 && (int16_t)oscillatory.state[10 + (kind - 8)][1] < 0)
            obj->status.b ^= 1; // (it starts on the other side of its square when its oscillator is going down)
    }

    int16_t old_x = obj->pos.l.x.f.u;
    CPZBlock_Move(obj, scratch);
    if (obj->render.f.on_screen) {
        FOR_EACH_CHARACTER(chr, who)
            Solid_Character(obj, chr, who, (int16_t)(obj->width_pixels + 0xB), obj->y_rad, (int16_t)(obj->y_rad + 1), old_x, NULL);
    }
    if (IS_OFFSCREEN(scratch->base_x)) {
        GoneIfOffscreen(obj, scratch->base_x);
        return;
    }
    DisplaySprite(obj);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 2D: the one way barrier, a door that rises while a character is within reach of it on its open side, and shuts behind them
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;  // 0x28: the frame
    uint8_t pad0[7];
    int16_t height;   // 0x30: how far it has risen
    int16_t base_y;   // 0x32
    uint8_t pad1[4];
    int16_t left;     // 0x38: the strip in which it opens
    int16_t right;    // 0x3A
} Scratch_CPZBarrier;

#define ARTTILE_CPZ_STRIPES 0x394 // ($7280 in the zone's art list)
#define ARTTILE_HTZ_VALVE_BARRIER 0x426 // ($84C0 in Hill Top's second list)

void Obj_CPZBarrier(Object *obj) {
    Scratch_CPZBarrier *scratch = (Scratch_CPZBarrier *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        obj->mappings = Mappings_CPZBarrier;
        // (Hill Top's valve barrier has art and a width of its own: the default of Obj2D_Init; Chemical Plant's is the stripes)
        const bool cpz = LEVEL_ZONE(level_id) == ZoneId_CPZ;
        const bool mtz = LEVEL_ZONE(level_id) == ZoneId_MTZ || LEVEL_ZONE(level_id) == ZoneId_MTZ3; // (Metropolis's is the first tile of its art, and as wide as Chemical Plant's)
        obj->tile = mtz ? TILE_MAP(0, 3, 0, 0, 0) : TILE_MAP(0, 1, 0, 0, cpz ? ARTTILE_CPZ_STRIPES : ARTTILE_HTZ_VALVE_BARRIER);
        obj->width_pixels = (cpz || mtz) ? 0xC : 8;
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->priority = 4;
        scratch->base_y = obj->pos.l.y.f.u;
        obj->frame = scratch->subtype;
        int16_t d2 = (int16_t)(obj->pos.l.x.f.u - 0x200), d3 = (int16_t)(obj->pos.l.x.f.u + 0x18);
        if (obj->status.o.f.x_flip) {
            d2 = (int16_t)(d2 + 0x1E8);
            d3 = (int16_t)(d3 + 0x1E8);
        }
        scratch->left = d2;
        scratch->right = d3;
    }

    int16_t d2, d3;
    const int16_t x = obj->pos.l.x.f.u;
    if (!obj->status.o.f.x_flip) {
        d2 = scratch->left;
        d3 = obj->routine_sec ? scratch->right : x;
    } else {
        d2 = obj->routine_sec ? scratch->left : x;
        d3 = scratch->right;
    }
    obj->routine_sec = 0; // (down, unless a character is in the strip)
    FOR_EACH_CHARACTER(chr, who) {
        const uint16_t cx = (uint16_t)chr->pos.l.x.f.u;
        if (cx >= (uint16_t)d2 && cx < (uint16_t)d3)
            obj->routine_sec = 2;
    }
    if (obj->routine_sec) {
        if (scratch->height != 0x40) {
            scratch->height += 8;
            obj->pos.l.y.f.u = (int16_t)(scratch->base_y - scratch->height);
        }
    } else if (scratch->height != 0) {
        scratch->height -= 8;
        obj->pos.l.y.f.u = (int16_t)(scratch->base_y - scratch->height);
    }

    if (obj->render.f.on_screen) {
        FOR_EACH_CHARACTER(chr, who)
            Solid_Character(obj, chr, who, (int16_t)(obj->width_pixels + 0xB), 0x20, 0x21, x, NULL);
    }
    RememberState(obj);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 32: the tube cover (in Hill Top the rock): solid, until a character rolls into it from above: it breaks into four pieces and throws him up
// ---------------------------------------------------------------------------------------------------------------------------------------
#define ARTTILE_TUBE_COVER 0x430 // ($8600 in the zone's art list)
#define ARTTILE_HTZ_ROCK 0x3B2 // ($7640 in Hill Top's)

// loc_17808: the speed of each piece (x, y); and Hill Top's rock's (loc_177F0), which is in six
static const int16_t cover_pieces[4][2] = { { -0x100, -0x200 }, { 0x100, -0x200 }, { -0xC0, -0x1C0 }, { 0xC0, -0x1C0 } };
static const int16_t rock_pieces[6][2] = { { -0x200, -0x200 }, { 0, -0x280 }, { 0x200, -0x200 }, { -0x1C0, -0x1C0 }, { 0, -0x200 }, { 0x1C0, -0x1C0 } };

// loc_17778: a rolling character bounces off what he has broken, curled up
static void Cover_Bounce(Object *chr) {
    chr->status.p.f.in_ball = true;
    chr->y_rad = 0xE;
    chr->x_rad = 7;
    chr->anim = SonAnimId_Roll;
    chr->ysp = -0x300;
}

// loc_17796 (and loc_17772 above it): he is in the air, and no longer on the cover
static void Cover_Launch(Object *chr, bool rolling) {
    if (rolling)
        Cover_Bounce(chr);
    chr->status.p.f.in_air = true;
    chr->status.p.f.object_stand = false;
    chr->routine = 2;
}

// loc_17818: the score for it, as for a badnik (Obj29, 10, 20, 50 then 100 and, after fifteen in a row, 1000)
void ObjectChainScore(Object *obj) {
    Object *points = FindFreeObj();
    if (points == NULL)
        return;
    static const uint16_t scores[4] = { 10, 20, 50, 100 };
    points->type = ObjId_Points;
    points->pos.l.x.f.u = obj->pos.l.x.f.u;
    points->pos.l.y.f.u = obj->pos.l.y.f.u;
    uint16_t d2 = item_bonus;
    item_bonus += 2;
    if (d2 >= 6)
        d2 = 6;
    uint16_t score = scores[d2 >> 1];
    if (item_bonus >= 0x20) {
        score = 1000;
        d2 = 10;
    }
    AddPoints(score);
    points->frame = (uint8_t)(d2 >> 1);
}

// BreakObjectToPieces: the object is its first piece, and each other piece of the frame is a new object of the same id, in routine 4, that drifts away on the speed its piece is given
void ObjectBreakToPieces(Object *obj, const int16_t (*speeds)[2], int count) {
    const uint8_t *piece;
    Mappings_FramePieces((const uint8_t *)obj->mappings, obj->frame, &piece);
    obj->render.f.static_mappings = true;
    Object *part = obj;
    for (int i = 0; i < count; i++) {
        if (i > 0) {
            part = FindNextFreeObj(obj + 1);
            if (part == NULL)
                break;
            piece += SPRITE_PIECE_SIZE;
            part->type = obj->type;
            part->render.b = obj->render.b;
            part->tile = obj->tile;
            part->priority = obj->priority;
            part->width_pixels = obj->width_pixels;
            part->pos.l.x.f.u = obj->pos.l.x.f.u;
            part->pos.l.y.f.u = obj->pos.l.y.f.u;
        }
        part->routine = 4;
        part->mappings = piece;
        part->xsp = speeds[i][0];
        part->ysp = speeds[i][1];
    }
    PlaySound(sfx_WallSmash);
}

void Obj_TubeCover(Object *obj) {
    if (obj->routine == 0) {
        obj->routine += 2;
        const bool rock = LEVEL_ZONE(level_id) != ZoneId_CPZ; // (Hill Top's rock: Obj_0x32 is the same object there, with its own art, width and pieces)
        obj->mappings = rock ? Mappings_HTZRock : Mappings_TubeCover;
        obj->tile = rock ? TILE_MAP(0, 2, 0, 0, ARTTILE_HTZ_ROCK) : TILE_MAP(0, 3, 0, 0, ARTTILE_TUBE_COVER);
        obj->width_pixels = rock ? 0x18 : 0x10;
        obj->render.b = SPRITE_CAM_FIELD;
        obj->priority = 4;
    }

    if (obj->routine == 4) { // a piece
        SpeedToPos(obj);
        obj->ysp += 0x18;
        if (!obj->render.f.on_screen)
            ObjectDelete(obj);
        else
            DisplaySprite(obj);
        return;
    }

    Object *sidekick = TAILS_OBJ->type != 0 ? TAILS_OBJ : NULL;
    const bool sonic_rolls = player->anim == SonAnimId_Roll, tails_rolls = sidekick != NULL && sidekick->anim == SonAnimId_Roll;
    if (obj->render.f.on_screen) {
        FOR_EACH_CHARACTER(chr, who)
            Solid_Character(obj, chr, who, (int16_t)(obj->width_pixels + 0xB), 0x10, 0x11, obj->pos.l.x.f.u, NULL);
    }
    const uint8_t stand = obj->status.b & 0x18;
    bool broken = false;
    if (stand == 0x18) { // both are on it
        if (sonic_rolls || tails_rolls) {
            Cover_Launch(player, sonic_rolls);
            if (sidekick != NULL)
                Cover_Launch(sidekick, tails_rolls);
            broken = true;
        }
    } else if (stand & 0x08) {
        if (sonic_rolls) {
            Cover_Launch(player, true);
            broken = true;
        }
    } else if (stand & 0x10) {
        if (sidekick != NULL && tails_rolls) {
            Cover_Launch(sidekick, true);
            broken = true;
        }
    }
    if (!broken) {
        RememberState(obj);
        return;
    }
    obj->status.b &= 0xE7;
    if (obj->mappings == Mappings_HTZRock)
        ObjectBreakToPieces(obj, rock_pieces, 6);
    else
        ObjectBreakToPieces(obj, cover_pieces, 4);
    ObjectChainScore(obj);
    // (it goes on as its first piece, this very frame)
    SpeedToPos(obj);
    obj->ysp += 0x18;
    DisplaySprite(obj);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 78: the rotating platforms: four blocks side by side that hang from the first and sink in a staircase when they are stood on (or bumped from below)
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;  // 0x28: bits 0-2 what they do, and the next kind once they have started
    uint8_t pad0[3];
    int16_t wait;     // 0x2C
    uint8_t touched;  // 0x2E: what has touched the first (bit 4/5: stood on by Sonic/Tails, bit 2/3: bumped from below by them), it stays set
    uint8_t index;    // 0x2F: which of the four it is, as the offset into the first's table
    int16_t base_x;   // 0x30
    int16_t base_y;   // 0x32
    int16_t off[4];   // 0x34: (the first's) how far down each of the four is
    uint8_t parent;   // 0x3C: the slot of the first
    uint8_t pad1[3];
} Scratch_CPZRotor;

// loc_1D4BC .. loc_1D552: what the first does for each kind (0 and 4 wait to be stood on, 2 and 6 to be bumped, then 1, 3, 5, 7 sink or rise)
static void Rotor_Run(Scratch_CPZRotor *scratch) {
    int16_t *off = scratch->off;
    switch (scratch->subtype & 7) {
    case 0:
    case 4:
        if (scratch->wait == 0) {
            if (scratch->touched & 0x30)
                scratch->wait = 0x1E;
        } else if (--scratch->wait == 0) {
            scratch->subtype++;
        }
        break;
    case 2:
    case 6:
        if (scratch->wait == 0) {
            if (scratch->touched & 0x0C)
                scratch->wait = 0x3C;
        } else if (--scratch->wait == 0) {
            scratch->subtype++;
        } else { // shakes: the four go up and down a pixel, in turns
            int16_t d0 = (int16_t)((scratch->wait >> 2) & 1);
            off[0] = d0;
            off[1] = d0 ^ 1;
            off[2] = d0;
            off[3] = d0 ^ 1;
        }
        break;
    case 1:
    case 3:
        if (off[0] != 0x80) {
            off[0]++;
            off[1] = (int16_t)(off[0] * 3 >> 2);
            off[2] = (int16_t)(off[0] >> 1);
            off[3] = (int16_t)(off[0] >> 2);
        }
        break;
    default: // 5, 7
        if (off[0] != -0x80) {
            off[0]--;
            off[1] = (int16_t)(off[0] * 3 >> 2);
            off[2] = (int16_t)(off[0] >> 1);
            off[3] = (int16_t)(off[0] >> 2);
        }
        break;
    }
}

void Obj_CPZRotor(Object *obj) {
    Scratch_CPZRotor *scratch = (Scratch_CPZRotor *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        int d3 = 0x34, d4 = 2;
        if (obj->status.o.f.x_flip) {
            d3 = 0x3A;
            d4 = -2;
        }
        int16_t d2 = obj->pos.l.x.f.u;
        Object *part = obj;
        for (int i = 0; i < 4; i++) {
            if (i > 0) {
                part = FindNextFreeObj(obj + 1);
                if (part == NULL)
                    break;
                part->routine = 4;
            }
            Scratch_CPZRotor *ps = (Scratch_CPZRotor *)&part->scratch;
            part->type = obj->type;
            part->mappings = Mappings_CPZBlock;
            part->tile = TILE_MAP(0, 3, 0, 0, ARTTILE_CPZ_BLOCK);
            part->render.b = 4;
            part->priority = 3;
            part->width_pixels = 0x10;
            ps->subtype = scratch->subtype;
            part->pos.l.x.f.u = d2;
            part->pos.l.y.f.u = obj->pos.l.y.f.u;
            ps->base_x = obj->pos.l.x.f.u;
            ps->base_y = part->pos.l.y.f.u;
            d2 = (int16_t)(d2 + 0x20);
            ps->index = (uint8_t)d3;
            ps->parent = (uint8_t)(obj - objects);
            d3 += d4;
        }
    }

    if (obj->routine == 2) {
        Rotor_Run(scratch);
    }
    Object *first = &objects[scratch->parent];
    Scratch_CPZRotor *fs = (Scratch_CPZRotor *)&first->scratch;
    obj->pos.l.y.f.u = (int16_t)(scratch->base_y + fs->off[(scratch->index - 0x34) >> 1]);
    if (obj->render.f.on_screen) {
        FOR_EACH_CHARACTER(chr, who) {
            int32_t hit = Solid_Character(obj, chr, who, (int16_t)(obj->width_pixels + 0xB), 0x10, 0x11, obj->pos.l.x.f.u, NULL);
            if (hit == -1)
                fs->touched |= (uint8_t)(0x10 << who);
            else if (hit == -2)
                fs->touched |= (uint8_t)(4 << who);
        }
    }
    GoneIfOffscreen(obj, scratch->base_x);
    if (obj->type != 0)
        DisplaySprite(obj);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 7A: the sliding platforms: one platform, or two, that go to and fro along a row and turn round when they meet
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;  // 0x28: which row (the offset into loc_1D5A8)
    uint8_t pad0[7];
    int16_t base_x;   // 0x30
    int16_t left;     // 0x32: the ends of the row
    int16_t right;    // 0x34
    uint8_t going_left; // 0x36
    uint8_t pad1[5];
    uint8_t partner;  // 0x3C: the slot of the other platform
    uint8_t pad2[3];
} Scratch_CPZSlider;

#define ARTTILE_CPZ_SLIDER ARTTILE_CPZ_BLOCK

// loc_1D5A8: for each kind, how many platforms there are less one, how far the row reaches each side of its place, and where each platform starts in it
static const struct {
    uint8_t count, reach;
    int16_t start0, start1;
} slider_rows[3] = { { 0, 0x70, -0x70, 0 }, { 1, 0xB0, -0xB0, 0x40 }, { 1, 0xF0, -0x80, 0x80 } };

static void Slider_Move(Object *obj, Scratch_CPZSlider *scratch) {
    const int16_t old_x = obj->pos.l.x.f.u;
    int16_t x = old_x;
    if (scratch->going_left) {
        x--;
        if (x == scratch->left)
            scratch->going_left = 0;
    } else {
        x++;
        if (x == scratch->right)
            scratch->going_left = 1;
    }
    obj->pos.l.x.f.u = x;
    FOR_EACH_CHARACTER(chr, who)
        Solid_Platform(obj, chr, who, obj->width_pixels, 8, old_x);
}

void Obj_CPZSlider(Object *obj) {
    Scratch_CPZSlider *scratch = (Scratch_CPZSlider *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        const int row = scratch->subtype / 6 > 2 ? 2 : scratch->subtype / 6;
        Object *other = obj;
        for (int i = 0; i <= slider_rows[row].count; i++) {
            if (i > 0) {
                other = FindNextFreeObj(obj + 1);
                if (other == NULL)
                    break;
                other->type = obj->type;
                other->routine = 4;
                other->pos.l.x.f.u = obj->pos.l.x.f.u;
                other->pos.l.y.f.u = obj->pos.l.y.f.u;
            }
            other->mappings = Mappings_CPZSlider;
            other->tile = TILE_MAP(1, 3, 0, 0, ARTTILE_CPZ_SLIDER);
            other->render.b = 4;
            other->priority = 4;
            other->width_pixels = 0x10;
            ((Scratch_CPZSlider *)&other->scratch)->base_x = obj->pos.l.x.f.u;
        }
        ((Scratch_CPZSlider *)&other->scratch)->partner = (uint8_t)(obj - objects);
        scratch->partner = (uint8_t)(other - objects);
        if (scratch->subtype == 0xC)
            scratch->going_left = 1;
        const int16_t reach = slider_rows[row].reach;
        const int16_t left = (int16_t)(scratch->base_x - reach), right = (int16_t)(left + reach * 2);
        Scratch_CPZSlider *os = (Scratch_CPZSlider *)&other->scratch;
        scratch->left = os->left = left;
        scratch->right = os->right = right;
        obj->pos.l.x.f.u = (int16_t)(obj->pos.l.x.f.u + slider_rows[row].start0);
        other->pos.l.x.f.u = (int16_t)(other->pos.l.x.f.u + slider_rows[row].start1);
    }

    if (obj->routine == 2) { // the first: it is the one that goes when the row is out of range
        Slider_Move(obj, scratch);
        if (IS_OFFSCREEN(scratch->left) && IS_OFFSCREEN(scratch->right)) {
            Object *other = &objects[scratch->partner];
            if (other != obj && other->type == obj->type)
                ObjectDelete(other);
            if (obj->respawn_index)
                objstate[obj->respawn_index] &= 0x7F;
            ObjectDelete(obj);
            return;
        }
    } else { // the second: it turns the first round when they meet
        Slider_Move(obj, scratch);
        Object *other = &objects[scratch->partner];
        if (other->type == obj->type && (int16_t)(obj->pos.l.x.f.u - 0x10) == (int16_t)(other->pos.l.x.f.u + 0x10)) {
            scratch->going_left ^= 1;
            ((Scratch_CPZSlider *)&other->scratch)->going_left ^= 1;
        }
    }
    DisplaySprite(obj);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 74: the invisible block, a solid that is only there while it is on the screen (and only shows in debug mode)
// ---------------------------------------------------------------------------------------------------------------------------------------
void Obj_CPZInvisibleBlock(Object *obj) {
    if (obj->routine == 0) {
        obj->routine += 2;
        obj->mappings = Mappings_CPZInvisibleBlock;
        obj->tile = TILE_MAP(1, 0, 0, 0, 0x680);
        obj->render.b |= SPRITE_CAM_FIELD;
        const uint8_t subtype = obj->scratch.u8[0];
        obj->width_pixels = (uint8_t)((((subtype & 0xF0) + 0x10)) >> 1);
        obj->y_rad = (int8_t)(((subtype & 0xF) + 1) << 3);
    }

    const int16_t dx = (int16_t)(obj->pos.l.x.f.u - scrpos_x.f.u), dy = (int16_t)(obj->pos.l.y.f.u - scrpos_y.f.u);
    if (dx >= 0 && dx < 0x140 && dy >= 0 && dy < 0xE0) {
        FOR_EACH_CHARACTER(chr, who)
            Solid_Character(obj, chr, who, (int16_t)(obj->width_pixels + 0xB), obj->y_rad, (int16_t)(obj->y_rad + 1), obj->pos.l.x.f.u, NULL);
    }
    if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
        GoneIfOffscreen(obj, obj->pos.l.x.f.u);
        return;
    }
    if (debug_use)
        DisplaySprite(obj);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 1D: the droplet chain, a row of drops that jump up out of the floor one after another (it hurts), at the same place each time or, in
// the other kind, hop sideways at the top
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;  // 0x28: bits 0-3 how many drops, bit 4-7 the hopping kind
    uint8_t pad0[7];
    int16_t base_y;   // 0x30
    int16_t wait;     // 0x32: frames to wait before the jump
    int16_t jump;     // 0x34: the speed it jumps at
    int16_t drift;    // 0x36: the speed it adds to xsp each frame
    int16_t base_x;   // 0x38
    int16_t hop;      // 0x3A: how far it hops at the top of the jump (the hopping kind)
} Scratch_CPZWorm;

#define ARTTILE_CPZ_WORM 0x43C // ($8780 in the zone's second list)

void Obj_CPZWorm(Object *obj) {
    Scratch_CPZWorm *scratch = (Scratch_CPZWorm *)&obj->scratch;

    switch (obj->routine) {
    case 0: {
        const int count = scratch->subtype & 0xF;
        const uint8_t routine = (scratch->subtype & 0xF0) ? 6 : 2;
        const bool flip = obj->status.o.f.x_flip;
        Object *drop = obj;
        for (int i = 0; i <= count; i++) {
            if (i > 0) {
                drop = FindNextFreeObj(obj + 1);
                if (drop == NULL)
                    break;
            }
            Scratch_CPZWorm *ds = (Scratch_CPZWorm *)&drop->scratch;
            drop->type = obj->type;
            drop->routine = routine;
            drop->pos.l.x.f.u = obj->pos.l.x.f.u;
            drop->pos.l.y.f.u = obj->pos.l.y.f.u;
            drop->mappings = Mappings_CPZWorm;
            drop->tile = TILE_MAP(0, 3, 0, 0, ARTTILE_CPZ_WORM);
            drop->render.b = 4;
            drop->priority = 3;
            drop->col_type = 0x8B;
            ds->base_x = drop->pos.l.x.f.u;
            ds->base_y = drop->pos.l.y.f.u;
            drop->ysp = -0x480;
            ds->jump = drop->ysp;
            drop->width_pixels = 8;
            ds->hop = flip ? -0x60 : 0x60;
            ds->drift = flip ? -0x0B : 0x0B;
            ds->wait = (int16_t)(i * 3);
        }
        return;
    }
    case 2:
    case 6: // waiting to jump
        if (--scratch->wait < 0) {
            obj->routine += 2;
            scratch->wait = 0x3B;
            PlaySound(sfx_Fireball);
        }
        break;
    case 4: // jumping, and drifting to one side and then to the other
        SpeedToPos(obj);
        obj->xsp += scratch->drift;
        obj->ysp += 0x18;
        if (obj->ysp == 0)
            scratch->drift = (int16_t)-scratch->drift;
        if (!((uint16_t)scratch->base_y > (uint16_t)obj->pos.l.y.f.u)) {
            obj->ysp = scratch->jump;
            obj->xsp = 0;
            obj->routine -= 2;
        }
        break;
    default: // 8: jumping, and hopping sideways at the top
        SpeedToPos(obj);
        obj->ysp += 0x18;
        if (obj->ysp == 0)
            obj->pos.l.x.f.u = (int16_t)(scratch->hop + scratch->base_x);
        if (!((uint16_t)scratch->base_y > (uint16_t)obj->pos.l.y.f.u)) {
            obj->ysp = scratch->jump;
            obj->pos.l.x.f.u = scratch->base_x;
        }
        break;
    }
    RememberState(obj);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 0C: the small floating platform (Obj0C, Nick Arcade's Chemical Plant platform, kept in the prototype's object table but with none in its levels): it bobs on a sine,
// and every 1024 frames it starts a longer, slower swing, by its subtype
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;  // 0x28: bits 0-3 how many times it pauses at the bottom of the swing
    uint8_t pad0[0x11];
    int16_t base_y;   // 0x3A
    uint8_t phase;    // 0x3C
    uint8_t swing;    // 0x3D
    uint8_t pauses;   // 0x3E
    uint8_t reload;   // 0x3F
} Scratch_FloatingPlatform;

void Obj_FloatingPlatform(Object *obj) {
    Scratch_FloatingPlatform *scratch = (Scratch_FloatingPlatform *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        obj->mappings = Mappings_FloatingPlatform;
        obj->tile = TILE_MAP(1, 3, 0, 0, ARTTILE_CPZ_BLOCK);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->width_pixels = 0x10;
        obj->priority = 4;
        scratch->base_y = (int16_t)(obj->pos.l.y.f.u - 0x10);
        scratch->pauses = scratch->reload = scratch->subtype & 0xF;
    }

    int16_t sin, cos;
    const uint8_t angle = scratch->phase;
    bool bob = true; // (loc_14AC0) the usual bob; the swing (loc_14A8E) is the other
    if (angle == 0) {
        if ((frame_count & 0x3FF) == 0) {
            scratch->swing = 1;
            scratch->phase++;
        }
    } else if (angle != 0x80) {
        scratch->phase++;
    } else {
        const uint8_t d1 = scratch->swing;
        if (d1 != 0 || (int8_t)--scratch->pauses >= 0) {
            scratch->swing++;
            CalcSine(d1, &sin, &cos);
            obj->pos.l.y.f.u = (int16_t)(scratch->base_y + (((sin + 8) >> 6) - 0x10));
            bob = false;
        } else {
            scratch->pauses = scratch->reload;
            scratch->phase++;
        }
    }
    if (bob) {
        CalcSine(angle, &sin, &cos);
        obj->pos.l.y.f.u = (int16_t)(scratch->base_y + ((cos + 8) >> 4));
    }
    FOR_EACH_CHARACTER(chr, who)
        Solid_Platform(obj, chr, who, obj->width_pixels, 9, obj->pos.l.x.f.u);
    RememberState(obj);
}
