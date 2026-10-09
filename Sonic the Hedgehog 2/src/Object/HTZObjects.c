// Hill Top's own objects for Sonic 2 (Nick Arcade's 14 and 16): the seesaw with its ball (14), and the lift that carries whoever stands on it down the slope (16). Both carry Sonic and Tails.
// The Simon Wai prototype adds the breakable floor (2F) and the lava boxes (31).
#include "Object/HTZObjects.h"
#include "Constants.h"

#include "Level.h"
#include "LevelScroll.h"
#include "GameInterface.h"
#include "Object/CPZObjects.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Solid.h"
#include "Sound.h"

#include "Macros.h"

extern const uint8_t Mappings_HTZLift[]; // (defined with Scenery.c, which draws its poles)
#include "LevelCollision.h"
#include "Resource/Animation/HTZFireball.h"
#include "Resource/Mappings/HTZBreakFloor.h"
#include "Resource/Mappings/HTZFireball.h"
#include "Resource/Mappings/HTZFireFlames.h"
#include "Resource/Mappings/HTZSeesaw.h"
#include "Resource/Mappings/HTZSeesawBall.h"

static Object *Character(int who) {
    return who == SolidChar_Sonic ? player : (TAILS_OBJ->type != 0 ? TAILS_OBJ : NULL);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 14: the seesaw, and (the same object in other routines) its ball
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28: 0 for a seesaw with a ball
    uint8_t pad0[7];   // 0x29-0x2F
    int16_t orig_x;    // 0x30
    int16_t pad1;      // 0x32
    int16_t orig_y;    // 0x34 (the ball's)
    int16_t pad2;      // 0x36
    int16_t speed;     // 0x38: how fast Sonic was falling when he landed on it
    int8_t state;      // 0x3A: seesaw: which end is down (0 left, 1 flat, 2 right); ball: which side it is on (0 or 2)
    uint8_t pad3;      // 0x3B
    uint8_t parent;    // 0x3C: (the ball's) the slot of its seesaw: an index, not a pointer, so it cannot dangle
} Scratch_Seesaw;

enum { SeesawRoutine_Init = 0, SeesawRoutine_Main = 2, SeesawRoutine_Ball = 6, SeesawRoutine_MoveBall = 8, SeesawRoutine_BallFall = 0xA };

// How high the ball sits on each end, by the seesaw's frame and which end it is on (Seesaw_YOffsets): low, balanced, high, balanced, low
static const int16_t seesaw_y_offsets[5] = { -8, -0x1C, -0x2F, -0x1C, -8 };

// The top of the seesaw, a height for each 2 pixels of its width (Seesaw_SlopeData and Seesaw_FlatData)
static const uint8_t seesaw_slope[0x30] = {
    0x14, 0x14, 0x16, 0x18, 0x1A, 0x1C, 0x1A, 0x18, 0x16, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E,
    0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0xFF, 0xFE,
    0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8, 0xF7, 0xF6, 0xF5, 0xF4, 0xF3, 0xF2, 0xF2, 0xF2, 0xF2, 0xF2,
};
static const uint8_t seesaw_flat[0x30] = {
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
};

// Which end of the seesaw a character standing on it weighs down: 2 if he is on its left, 0 if on its right, 1 if he is in the middle (within 8 pixels of it)
static int Seesaw_Side(const Object *obj, const Object *chr) {
    uint16_t d0 = (uint16_t)(obj->pos.l.x.f.u - chr->pos.l.x.f.u);
    int side = 2;
    if ((uint16_t)obj->pos.l.x.f.u < (uint16_t)chr->pos.l.x.f.u) { // (he is on its right)
        d0 = (uint16_t)-(int16_t)d0;
        side = 0;
    }
    return d0 < 8 ? 1 : side;
}

// The frame moves a step toward the one the weight asks for (Seesaw_ChangeFrame), and the seesaw is mirrored for the frames on its other side
static void Seesaw_ChangeFrame(Object *obj, Scratch_Seesaw *scratch, int wanted) {
    int frame = obj->frame;
    if (frame == wanted)
        return;
    frame += frame < wanted ? 1 : -1;
    obj->frame = (uint8_t)frame;
    scratch->state = (int8_t)wanted;
    obj->render.f.x_flip = (frame & 2) != 0;
}

// The seesaw's own object: from `Seesaw_Init` on
static void Seesaw_Init(Object *obj, Scratch_Seesaw *scratch) {
    obj->routine += 2;
    obj->mappings = Mappings_HTZSeesaw;
    obj->tile = TILE_MAP(0, 0, 0, 0, 0x3C6);
    obj->render.b |= SPRITE_CAM_FIELD;
    obj->priority = 4;
    obj->width_pixels = 0x30;
    scratch->orig_x = obj->pos.l.x.f.u;

    if (scratch->subtype == 0) { // with a ball
        Object *ball = FindNextFreeObj(obj + 1);
        if (ball != NULL) {
            Scratch_Seesaw *ball_scratch = (Scratch_Seesaw *)&ball->scratch;
            ball->type = obj->type;
            ball->routine = SeesawRoutine_Ball;
            ball->pos.l.x.f.u = obj->pos.l.x.f.u;
            ball->pos.l.y.f.u = obj->pos.l.y.f.u;
            ball->status.b = obj->status.b;
            ball_scratch->parent = (uint8_t)(obj - objects);
        }
    }

    if (obj->status.o.f.x_flip)
        obj->frame = 2; // (a duplicate of frame 0, mirrored)
    scratch->state = (int8_t)obj->frame;
}

static void Seesaw_Main(Object *obj, Scratch_Seesaw *scratch) {
    int wanted = scratch->state;
    bool sonic_on = (obj->status.b & 8) != 0, tails_on = (obj->status.b & 0x10) != 0;
    if (sonic_on) {
        wanted = Seesaw_Side(obj, player);
        if (tails_on) {
            wanted += Seesaw_Side(obj, TAILS_OBJ);
            if (wanted == 3)
                wanted++;
            wanted >>= 1;
        }
    } else if (tails_on) {
        wanted = Seesaw_Side(obj, TAILS_OBJ);
    }
    Seesaw_ChangeFrame(obj, scratch, wanted);

    const uint8_t *slope = (obj->frame & 1) ? seesaw_flat : seesaw_slope;
    scratch->speed = player->ysp; // (how fast Sonic comes down on it, before he lands and his speed is lost)
    int16_t x = obj->pos.l.x.f.u;
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
        Object *chr = Character(who);
        if (chr != NULL)
            Solid_SlopedPlatform(obj, chr, who, obj->width_pixels, slope, x);
    }
}

// The seesaw of a ball: still the same object, of the same routine (Main), and not one that has been replaced by another id's object or a ball
static Object *Seesaw_Parent(const Object *ball, const Scratch_Seesaw *scratch) {
    Object *parent = &objects[scratch->parent];
    if (scratch->parent == 0 || parent == ball || parent->type != ball->type || parent->routine != SeesawRoutine_Main)
        return NULL;
    return parent;
}

static void Seesaw_Ball_Init(Object *obj, Scratch_Seesaw *scratch) {
    obj->routine += 2;
    obj->mappings = Mappings_HTZSeesawBall;
    obj->tile = TILE_MAP(0, 0, 0, 0, 0x3DE);
    obj->render.f.level_fg = true;
    obj->priority = 4;
    obj->col_type = 0x8B;
    obj->width_pixels = 0xC;
    scratch->orig_x = obj->pos.l.x.f.u;
    obj->pos.l.x.f.u += 0x28;
    obj->pos.l.y.f.u += 0x10;
    scratch->orig_y = obj->pos.l.y.f.u;
    obj->frame = 1;
    if (obj->status.o.f.x_flip) { // the seesaw is flipped: the ball starts on the other side
        obj->pos.l.x.f.u -= 0x50;
        scratch->state = 2;
    }
}

// The height of the ball when it is resting on the seesaw's end it is at
static int16_t Ball_RestY(const Object *ball, const Scratch_Seesaw *scratch, const Object *parent) {
    int index = parent->frame;
    if ((uint16_t)ball->pos.l.x.f.u < (uint16_t)scratch->orig_x)
        index += 2;
    return (int16_t)(scratch->orig_y + seesaw_y_offsets[index]);
}

static void Seesaw_BallFall(Object *obj, Scratch_Seesaw *scratch);

static void Seesaw_MoveBall(Object *obj, Scratch_Seesaw *scratch) {
    Object *parent = Seesaw_Parent(obj, scratch);
    if (parent == NULL) {
        ObjectDelete(obj);
        return;
    }
    Scratch_Seesaw *pscratch = (Scratch_Seesaw *)&parent->scratch;

    int diff = scratch->state - pscratch->state;
    if (diff == 0) { // it is where the seesaw says: it rides on its end
        int16_t x_off = 0x28;
        int index = parent->frame;
        if ((uint16_t)obj->pos.l.x.f.u < (uint16_t)scratch->orig_x) {
            x_off = -x_off;
            index += 2;
        }
        obj->pos.l.y.f.u = (int16_t)(scratch->orig_y + seesaw_y_offsets[index]);
        obj->pos.l.x.f.u = (int16_t)(scratch->orig_x + x_off);
        obj->pos.l.y.f.l = 0;
        obj->pos.l.x.f.l = 0;
        return;
    }

    // the seesaw has been tipped under it: the ball is thrown, the harder the further it tipped and the faster Sonic came down
    if (diff < 0)
        diff = -diff;
    int16_t ysp = (int16_t)0xF7E8, xsp = (int16_t)0xFEEC;
    if (diff != 1) {
        ysp = (int16_t)0xF510;
        xsp = (int16_t)0xFF34;
        if (pscratch->speed >= 0xA00) {
            ysp = (int16_t)0xF200;
            xsp = (int16_t)0xFF60;
        }
    }
    obj->ysp = ysp;
    obj->xsp = xsp;
    if ((uint16_t)obj->pos.l.x.f.u < (uint16_t)scratch->orig_x)
        obj->xsp = (int16_t)-obj->xsp;
    obj->routine += 2;
    Seesaw_BallFall(obj, scratch);
}

// The ball thrown by the seesaw launches whoever is standing on the end that it comes down on
static void Seesaw_Launch(const Object *ball, Object *chr) {
    chr->ysp = (int16_t)-ball->ysp;
    chr->status.p.f.in_air = true;
    chr->status.p.f.object_stand = false;
    ((Scratch_Sonic *)&chr->scratch)->jumping = 0;
    chr->anim = SonAnimId_Spring;
    chr->routine = 2;
    PlaySound(sfx_Spring);
}

static void Seesaw_BallFall(Object *obj, Scratch_Seesaw *scratch) {
    if (obj->ysp < 0) { // going up
        ObjectFall(obj);
        if ((int16_t)(scratch->orig_y - 0x2F) <= obj->pos.l.y.f.u)
            ObjectFall(obj); // (Nick Arcade moves it a second time once it is higher than that)
        return;
    }

    ObjectFall(obj);
    Object *parent = Seesaw_Parent(obj, scratch);
    if (parent == NULL) {
        ObjectDelete(obj);
        return;
    }
    Scratch_Seesaw *pscratch = (Scratch_Seesaw *)&parent->scratch;
    if (Ball_RestY(obj, scratch, parent) > obj->pos.l.y.f.u)
        return; // not down yet

    int8_t side = obj->xsp < 0 ? 2 : 0;
    pscratch->state = side;
    scratch->state = side;
    if (side != parent->frame) { // the ball came down on an end that was up: the seesaw throws up whoever is on it
        if (parent->status.b & 8) {
            parent->status.b &= (uint8_t)~8;
            Seesaw_Launch(obj, player);
        }
        if (parent->status.b & 0x10) {
            parent->status.b &= (uint8_t)~0x10;
            Seesaw_Launch(obj, TAILS_OBJ);
        }
    }
    obj->xsp = 0;
    obj->ysp = 0;
    obj->routine -= 2;
}

// Obj14_Animate (the Sol badnik): every fourth frame its palette line alternates (the flash of red and yellow), and it faces Sonic (flipped when he is at or to its right)
static void Seesaw_BallAnimate(Object *obj) {
    if ((frame_count & 3) == 0)
        obj->tile ^= 0x2000;
    obj->render.f.x_flip = (uint16_t)player->pos.l.x.f.u >= (uint16_t)obj->pos.l.x.f.u;
}

void Obj_HTZSeesaw(Object *obj) {
    Scratch_Seesaw *scratch = (Scratch_Seesaw *)&obj->scratch;

    switch (obj->routine) {
    case SeesawRoutine_Init:
        Seesaw_Init(obj, scratch);
        // fallthrough
    case SeesawRoutine_Main:
        Seesaw_Main(obj, scratch);
        break;
    case SeesawRoutine_Ball:
        Seesaw_Ball_Init(obj, scratch);
        // fallthrough
    case SeesawRoutine_MoveBall:
        Seesaw_BallAnimate(obj);
        Seesaw_MoveBall(obj, scratch);
        break;
    case SeesawRoutine_BallFall:
        Seesaw_BallAnimate(obj);
        Seesaw_BallFall(obj, scratch);
        break;
    }

    if (obj->type == 0) // (a ball that has lost its seesaw has deleted itself)
        return;
    if (IS_OFFSCREEN(scratch->orig_x))
        ObjectDelete(obj);
    else
        DisplaySprite(obj);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 16: the lift
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28: how long it slides, in eighths of a frame (the prototype's lsl #3)
    uint8_t pad0[7];   // 0x29-0x2F
    int16_t pad1[2];   // 0x30, 0x32: (where it began, kept by the original but not used)
    int16_t timer;     // 0x34: how long it still slides
} Scratch_HTZLift;

enum { LiftState_Wait = 0, LiftState_Slide = 2, LiftState_Fall = 4 }; // (routine_sec, $25)

// The Simon Wai prototype's lift (Obj16): it waits until it is stood on, slides down the slope (to the left if it is flipped) for as many frames as its subtype times eight, and then stops, leaves a
// broken pole behind (a scenery object, subtype 6) and falls
void Obj_HTZLift(Object *obj) {
    Scratch_HTZLift *scratch = (Scratch_HTZLift *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        obj->mappings = Mappings_HTZLift;
        obj->tile = TILE_MAP(0, 2, 0, 0, 0x3E6);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->width_pixels = 0x20;
        obj->frame = 0;
        obj->priority = 1;
        scratch->timer = (int16_t)(scratch->subtype << 3);
    }

    int16_t old_x = obj->pos.l.x.f.u;
    switch (obj->routine_sec) {
    case LiftState_Wait: // when it is stood on it starts to slide
        if (obj->status.b & 0x18) {
            obj->routine_sec += 2;
            obj->xsp = obj->status.o.f.x_flip ? -0x200 : 0x200;
            obj->ysp = 0x100;
        }
        break;
    case LiftState_Slide:
        SpeedToPos(obj);
        if (--scratch->timer == 0) {
            obj->routine_sec += 2;
            obj->frame = 2;
            obj->xsp = 0;
            obj->ysp = 0;
            Object *pole = FindNextFreeObj(obj + 1);
            if (pole != NULL) {
                pole->type = 0x1C; // (scenery)
                pole->pos.l.x.f.u = obj->pos.l.x.f.u;
                pole->pos.l.y.f.u = obj->pos.l.y.f.u;
                pole->render.b = obj->render.b;
                pole->scratch.u8[0] = 6;
            }
        }
        break;
    default: // it falls, and goes when it is below the bottom of the level
        SpeedToPos(obj);
        obj->ysp += 0x38;
        if ((uint16_t)(limit_btm2 + 0xE0) < (uint16_t)obj->pos.l.y.f.u) {
            ObjectDelete(obj);
            return;
        }
        break;
    }

    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
        Object *chr = Character(who);
        if (chr != NULL)
            Solid_Platform(obj, chr, who, obj->width_pixels, -0x28, old_x);
    }
    RememberState(obj);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 2F: the breakable floor (Obj_0x2F_Breakable_Floor, loc_1747C): a solid slab (a stack of five heights, by subtype) that breaks into its pieces, row by row from the top, when it is stood on and
// rolled on from the secondary collision path (or by anyone rolling, with subtype bit 7), and throws no one: they fall with it. Whoever is not rolling on it is put back on the primary path.
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype; // 0x28: bits 1-3 the height (bit 7: breaks for any roller)
} Scratch_BreakFloor;

// loc_17490: the half height and the frame of each height
static const uint8_t floor_heights[5][2] = { { 0x24, 0 }, { 0x20, 2 }, { 0x18, 4 }, { 0x10, 6 }, { 0x08, 8 } };
// loc_17662: the speed of each piece (x, y), a piece of the frame after the one that was there taking the pair at its frame number (the table goes on into zeros past its end)
static const int16_t floor_pieces[32][2] = {
    { -0x100, -0x800 }, { 0x100, -0x800 }, { -0xE0, -0x700 }, { 0xE0, -0x700 }, { -0xC0, -0x600 }, { 0xC0, -0x600 }, { -0xA0, -0x500 }, { 0xA0, -0x500 }, { -0x80, -0x400 }, { 0x80, -0x400 },
};

// loc_175AE and loc_175B4: a roller keeps rolling as he falls (loc_175CC: he is in the air and off the floor, whatever he was doing)
static void BreakFloor_Drop(Object *chr, bool rolling) {
    if (rolling) {
        chr->status.p.f.in_ball = true;
        chr->y_rad = 0xE;
        chr->x_rad = 7;
        chr->anim = SonAnimId_Roll;
    }
    chr->status.p.f.in_air = true;
    chr->status.p.f.object_stand = false;
    chr->routine = 2;
}

// $3E and $3F of the character, the solid bits of the first path ($C, $D)
static void BreakFloor_PrimaryPath(Object *chr) {
    Scratch_Sonic *scratch = (Scratch_Sonic *)&chr->scratch;
    scratch->top_solid_bit = 0xC;
    scratch->lrb_solid_bit = 0xD;
}

void Obj_HTZBreakFloor(Object *obj) {
    Scratch_BreakFloor *scratch = (Scratch_BreakFloor *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        obj->mappings = Mappings_HTZBreakFloor;
        obj->tile = TILE_MAP(1, 2, 0, 0, 0);
        obj->render.b = 0;
        obj->render.f.level_fg = true;
        obj->width_pixels = 0x10;
        obj->priority = 4;
        const int height = ((scratch->subtype & 0x1E) >> 1) % 5;
        obj->y_rad = (int8_t)floor_heights[height][0];
        obj->frame = floor_heights[height][1];
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
    const bool any = scratch->subtype & 0x80;
    // (a character's collision path is the solid bits he carries, the swappers' work: Game_CollisionPath; the floor puts whoever stands on it and does not break it back on the first path)
    const bool sonic_secondary = Game_CollisionPath(player) != 0, tails_secondary = sidekick != NULL && Game_CollisionPath(sidekick) != 0;
    if (obj->render.f.on_screen) {
        for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
            Object *chr = Character(who);
            if (chr != NULL)
                Solid_Character(obj, chr, who, (int16_t)(obj->width_pixels + 0xB), obj->y_rad, (int16_t)(obj->y_rad + 1), obj->pos.l.x.f.u, NULL);
        }
    }
    const uint8_t stand = obj->status.b & 0x18;
    bool broken = false;
    const bool sonic_breaks = sonic_rolls && (any || sonic_secondary), tails_breaks = tails_rolls && (any || tails_secondary);
    if (stand == 0x18) {
        if (sonic_breaks || tails_breaks) {
            BreakFloor_Drop(player, sonic_rolls);
            if (sidekick != NULL)
                BreakFloor_Drop(sidekick, tails_rolls);
            broken = true;
        }
    } else if (stand & 0x08) {
        if (sonic_breaks) {
            BreakFloor_Drop(player, true);
            broken = true;
        }
    } else if (stand & 0x10) {
        if (tails_breaks && sidekick != NULL) {
            BreakFloor_Drop(sidekick, true);
            broken = true;
        }
    }
    if (!broken) {
        if (stand & 0x08)
            BreakFloor_PrimaryPath(player);
        if ((stand & 0x10) && sidekick != NULL)
            BreakFloor_PrimaryPath(sidekick);
        RememberState(obj);
        return;
    }

    obj->status.b &= 0xE7;
    const int old_frame = obj->frame;
    obj->frame++;
    const uint8_t *piece;
    const int count = Mappings_FramePieces((const uint8_t *)obj->mappings, obj->frame, &piece);
    ObjectBreakToPieces(obj, floor_pieces + old_frame, count);
    ObjectChainScore(obj);
    SpeedToPos(obj); // (it goes on as its first piece, this very frame)
    obj->ysp += 0x18;
    DisplaySprite(obj);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 31: the lava boxes (Obj_0x31_Lava_Attributes): an invisible box, $80 wide, that hurts (col $96, $94, $95 by subtype, none for the fourth)
// ---------------------------------------------------------------------------------------------------------------------------------------
void Obj_HTZLavaBox(Object *obj) {
    static const uint8_t collision[4] = { 0x96, 0x94, 0x95, 0x00 };

    if (obj->routine == 0) {
        obj->routine += 2;
        obj->col_type = collision[obj->scratch.u8[0] & 3];
        obj->width_pixels = 0x80;
        obj->priority = 4;
        obj->frame = obj->scratch.u8[0];
    }
    obj->render.b = 0x84; // (never drawn, but the touch response has to find it "on screen")
    if (IS_OFFSCREEN(obj->pos.l.x.f.u))
        ObjectDelete(obj); // (the prototype does not forget its mark here either)
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 20: the fireball (Obj_0x20_Fireball): a bubble in the lava that throws two fireballs, one to each side, when its animation is at the right moment; they fall, burn where they land and spread three
// steps along the ground. The subtype's low nibble times 16 is how long it waits between throws, and its next three bits give the speed (up and out) of the balls.
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;  // 0x28
    uint8_t pad0[7];
    int16_t base_y;   // 0x30
    int16_t timer;    // 0x32
    int16_t reload;   // 0x34
    uint8_t spread;   // 0x36: how many more steps the fire goes on
    uint8_t pad1[1];
} Scratch_Fireball;

#define ARTTILE_LAVA_BUBBLE 0x416 // ($82C0 in Hill Top's second list)
#define ARTTILE_FIRE 0x39E        // ($73C0 in its first)

enum { FireRoutine_Init = 0, FireRoutine_Bubble = 2, FireRoutine_Throw = 4, FireRoutine_Wait = 6, FireRoutine_Fly = 8, FireRoutine_Burn = 0xA, FireRoutine_Gone = 0xC };

// loc_1723E: one fireball
static void Fireball_Throw(Object *obj, Object *ball) {
    ball->type = obj->type;
    ball->routine = FireRoutine_Fly;
    ball->pos.l.x.f.u = obj->pos.l.x.f.u;
    ball->pos.l.y.f.u = obj->pos.l.y.f.u;
    ball->xsp = obj->xsp;
    ball->ysp = obj->ysp;
    ball->y_rad = ball->x_rad = 8;
    ball->mappings = obj->mappings;
    ball->tile = obj->tile;
    ball->render.b = 4;
    ball->priority = 3;
    ball->width_pixels = 8;
    ball->col_type = 0x8B;
    ((Scratch_Fireball *)&ball->scratch)->base_y = ball->pos.l.y.f.u;
}

void Obj_HTZFireball(Object *obj) {
    Scratch_Fireball *scratch = (Scratch_Fireball *)&obj->scratch;

    switch (obj->routine) {
    case FireRoutine_Init: {
        obj->routine += 2;
        obj->y_rad = obj->x_rad = 8;
        obj->mappings = Mappings_HTZFireball;
        obj->tile = TILE_MAP(1, 0, 0, 0, ARTTILE_LAVA_BUBBLE);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->priority = 3;
        obj->width_pixels = 8;
        scratch->base_y = obj->pos.l.y.f.u;
        const int16_t speed = (int16_t)-(((int)scratch->subtype << 3) & 0x780);
        obj->xsp = obj->ysp = speed;
        scratch->timer = scratch->reload = (int16_t)((scratch->subtype & 0xF) << 4);
    }
        // Fallthrough
    case FireRoutine_Bubble:
        AnimateSprite(obj, Animation_HTZFireball);
        RememberState(obj);
        break;
    case FireRoutine_Throw:
        if (obj->frame_time.b == 5) { // the moment of the animation: two fireballs, one the other way
            Object *first = FindNextFreeObj(obj + 1);
            if (first != NULL) {
                Fireball_Throw(obj, first);
                Object *second = FindNextFreeObj(obj + 1);
                if (second != NULL) {
                    Fireball_Throw(obj, second);
                    second->xsp = (int16_t)-second->xsp;
                    second->render.f.x_flip = true;
                }
            }
            PlaySound(sfx_Fireball);
            obj->routine += 2;
        }
        AnimateSprite(obj, Animation_HTZFireball);
        RememberState(obj);
        break;
    case FireRoutine_Wait:
        if (--scratch->timer < 0) {
            scratch->timer = scratch->reload;
            obj->routine = FireRoutine_Bubble;
            obj->anim = 0;
            obj->prev_anim = 1; // (the animation starts again)
        }
        AnimateSprite(obj, Animation_HTZFireball);
        RememberState(obj);
        break;
    case FireRoutine_Fly:
        if (--obj->frame_time.b < 0) {
            obj->frame_time.b = 7;
            obj->frame = (uint8_t)((obj->frame + 1) & 1);
        }
        SpeedToPos(obj);
        obj->ysp += 0x18;
        if ((uint16_t)(limit_btm2 + 0xE0) < (uint16_t)obj->pos.l.y.f.u) {
            ObjectDelete(obj);
            return;
        }
        obj->render.f.y_flip = false;
        if (obj->ysp >= 0) { // falling: it burns where it lands
            obj->render.f.y_flip = true;
            const int16_t d1 = ObjFloorDist(obj, obj->pos.l.x.f.u);
            if (d1 < 0) {
                obj->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u + d1);
                obj->routine += 2;
                obj->anim = 2;
                obj->ysp = 0;
                obj->mappings = Mappings_HTZFireFlames;
                obj->tile = TILE_MAP(1, 0, 0, 0, ARTTILE_FIRE);
                obj->frame = 0;
                scratch->timer = 9;
                scratch->spread = 3;
            }
        }
        RememberState(obj);
        break;
    case FireRoutine_Burn:
        if (--scratch->timer < 0) {
            scratch->timer = 0x7F;
            if ((int8_t)--scratch->spread >= 0) { // the fire goes on one step along the ground
                Object *next = FindNextFreeObj(obj + 1);
                if (next != NULL) {
                    *next = *obj;
                    Scratch_Fireball *ns = (Scratch_Fireball *)&next->scratch;
                    ns->timer = 9;
                    next->anim = 2;
                    next->prev_anim = 0;
                    next->pos.l.x.f.u = (int16_t)(next->pos.l.x.f.u + (next->xsp < 0 ? -0x0E : 0x0E));
                    next->pos.l.y.f.u = (int16_t)(next->pos.l.y.f.u + ObjFloorDist(next, next->pos.l.x.f.u));
                }
            }
        }
        AnimateSprite(obj, Animation_HTZFireball);
        RememberState(obj);
        break;
    default:
        ObjectDelete(obj);
        break;
    }
}
