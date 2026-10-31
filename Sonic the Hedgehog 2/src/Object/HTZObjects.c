// Hill Top's own objects for Sonic 2 (Nick Arcade's 14 and 16): the seesaw with its ball (14), and the lift that carries whoever stands on it down the slope (16). Both carry Sonic and Tails.
// The Simon Wai prototype adds the breakable floor (2F) and the lava boxes (31).
#include "Object/HTZObjects.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Level.h"
#include "LevelScroll.h"
#include "GameInterface.h"
#include "Object/CPZObjects.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "SolidSst.h"
#include "Sound.h"

#include "Object/DebugMarkers.h"

#include "Macros.h"

extern const uint8_t Mappings_HTZLift[]; // (defined with Scenery.c, which draws its poles)
#include "LevelCollision.h"
#include "Resource/Animation/HTZFireball.h"
#include "Resource/Mappings/HTZBreakFloor.h"
#include "Resource/Mappings/HTZFireball.h"
#include "Resource/Mappings/HTZFireFlames.h"
#include "Resource/Mappings/HTZSeesaw.h"
#include "Resource/Mappings/HTZSeesawBall.h"

static Sst *Character(int who) {
    return who == SolidChar_Sonic ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL);
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
} Own_Seesaw;

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
static int Seesaw_Side(const Sst *self, const Sst *chr) {
    uint16_t d0 = (uint16_t)(self->x_pos.f.u - chr->x_pos.f.u);
    int side = 2;
    if ((uint16_t)self->x_pos.f.u < (uint16_t)chr->x_pos.f.u) { // (he is on its right)
        d0 = (uint16_t)-(int16_t)d0;
        side = 0;
    }
    return d0 < 8 ? 1 : side;
}

// The frame moves a step toward the one the weight asks for (Seesaw_ChangeFrame), and the seesaw is mirrored for the frames on its other side
static void Seesaw_ChangeFrame(Sst *self, Own_Seesaw *scratch, int wanted) {
    int frame = self->mapping_frame;
    if (frame == wanted)
        return;
    frame += frame < wanted ? 1 : -1;
    self->mapping_frame = (uint8_t)frame;
    scratch->state = (int8_t)wanted;
    SST_RENDER_SET(self, SST_RENDER_X_FLIP, (frame & 2) != 0);
}

// The seesaw's own object: from `Seesaw_Init` on
static void Seesaw_Init(Sst *self, Own_Seesaw *scratch) {
    self->routine += 2;
    self->mappings = Mappings_HTZSeesaw;
    self->art_tile = TILE_MAP(0, 0, 0, 0, 0x3C6);
    self->render_flags |= SST_RENDER_LEVEL;
    self->priority = 4 * 0x80;
    self->width_pixels = 0x30;
    scratch->orig_x = self->x_pos.f.u;

    if (self->subtype == 0) { // with a ball
        Sst *ball = Sst_AllocateAfter(self);
        if (ball != NULL) {
            Own_Seesaw *ball_scratch = (Own_Seesaw *)ball->own.u8;
            ball->code = self->code;
            ball->routine = SeesawRoutine_Ball;
            ball->x_pos.f.u = self->x_pos.f.u;
            ball->y_pos.f.u = self->y_pos.f.u;
            ball->status = self->status;
            ball_scratch->parent = (uint8_t)(self - sst);
        }
    }

    if (((self->status & SST_STATUS_X_FLIP) != 0))
        self->mapping_frame = 2; // (a duplicate of frame 0, mirrored)
    scratch->state = (int8_t)self->mapping_frame;
}

static void Seesaw_Main(Sst *self, Own_Seesaw *scratch) {
    int wanted = scratch->state;
    bool sonic_on = (self->status & 8) != 0, tails_on = (self->status & 0x10) != 0;
    if (sonic_on) {
        wanted = Seesaw_Side(self, player);
        if (tails_on) {
            wanted += Seesaw_Side(self, TAILS_OBJ);
            if (wanted == 3)
                wanted++;
            wanted >>= 1;
        }
    } else if (tails_on) {
        wanted = Seesaw_Side(self, TAILS_OBJ);
    }
    Seesaw_ChangeFrame(self, scratch, wanted);

    const uint8_t *slope = (self->mapping_frame & 1) ? seesaw_flat : seesaw_slope;
    scratch->speed = player->y_vel; // (how fast Sonic comes down on it, before he lands and his speed is lost)
    int16_t x = self->x_pos.f.u;
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
        Sst *chr = Character(who);
        if (chr != NULL)
            SolidSst_SlopedPlatform(self, chr, who, self->width_pixels, slope, x);
    }
}

// The seesaw of a ball: still the same object, of the same routine (Main), and not one that has been replaced by another id's object or a ball
static Sst *Seesaw_Parent(const Sst *ball, const Own_Seesaw *scratch) {
    Sst *parent = &sst[scratch->parent];
    if (scratch->parent == 0 || parent == ball || parent->code != ball->code || parent->routine != SeesawRoutine_Main)
        return NULL;
    return parent;
}

static void Seesaw_Ball_Init(Sst *self, Own_Seesaw *scratch) {
    self->routine += 2;
    self->mappings = Mappings_HTZSeesawBall;
    self->art_tile = TILE_MAP(0, 0, 0, 0, 0x3DE);
    self->render_flags |= SST_RENDER_LEVEL;
    self->priority = 4 * 0x80;
    self->collision_flags = 0x8B;
    self->width_pixels = 0xC;
    scratch->orig_x = self->x_pos.f.u;
    self->x_pos.f.u += 0x28;
    self->y_pos.f.u += 0x10;
    scratch->orig_y = self->y_pos.f.u;
    self->mapping_frame = 0; // (the alpha's mappings: frame 0 is on palette line 0, frame 1 on line 1, and the flash adds a line to it)
    if (((self->status & SST_STATUS_X_FLIP) != 0)) { // the seesaw is flipped: the ball starts on the other side
        self->x_pos.f.u -= 0x50;
        scratch->state = 2;
    }
}

// The height of the ball when it is resting on the seesaw's end it is at
static int16_t Ball_RestY(const Sst *ball, const Own_Seesaw *scratch, const Sst *parent) {
    int index = parent->mapping_frame;
    if ((uint16_t)ball->x_pos.f.u < (uint16_t)scratch->orig_x)
        index += 2;
    return (int16_t)(scratch->orig_y + seesaw_y_offsets[index]);
}

static void Seesaw_BallFall(Sst *self, Own_Seesaw *scratch);

static void Seesaw_MoveBall(Sst *self, Own_Seesaw *scratch) {
    Sst *parent = Seesaw_Parent(self, scratch);
    if (parent == NULL) {
        Sst_Delete(self);
        return;
    }
    Own_Seesaw *pscratch = (Own_Seesaw *)parent->own.u8;

    int diff = scratch->state - pscratch->state;
    if (diff == 0) { // it is where the seesaw says: it rides on its end
        int16_t x_off = 0x28;
        int index = parent->mapping_frame;
        if ((uint16_t)self->x_pos.f.u < (uint16_t)scratch->orig_x) {
            x_off = -x_off;
            index += 2;
        }
        self->y_pos.f.u = (int16_t)(scratch->orig_y + seesaw_y_offsets[index]);
        self->x_pos.f.u = (int16_t)(scratch->orig_x + x_off);
        self->y_pos.f.l = 0;
        self->x_pos.f.l = 0;
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
    self->y_vel = ysp;
    self->x_vel = xsp;
    if ((uint16_t)self->x_pos.f.u < (uint16_t)scratch->orig_x)
        self->x_vel = (int16_t)-self->x_vel;
    self->routine += 2;
    Seesaw_BallFall(self, scratch);
}

// The ball thrown by the seesaw launches whoever is standing on the end that it comes down on
static void Seesaw_Launch(const Sst *ball, Sst *chr) {
    chr->y_vel = (int16_t)-ball->y_vel;
    chr->status |= SST_PSTATUS_IN_AIR;
    chr->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
    ((Scratch_Sonic *)chr->own.u8)->jumping = 0;
    chr->anim = SonAnimId_Spring;
    chr->routine = 2;
    PlaySound(sfx_Spring);
}

static void Seesaw_BallFall(Sst *self, Own_Seesaw *scratch) {
    if (self->y_vel < 0) { // going up
        Sst_MoveSprite(self);
        if ((int16_t)(scratch->orig_y - 0x2F) <= self->y_pos.f.u)
            Sst_MoveSprite(self); // (Nick Arcade moves it a second time once it is higher than that)
        return;
    }

    Sst_MoveSprite(self);
    Sst *parent = Seesaw_Parent(self, scratch);
    if (parent == NULL) {
        Sst_Delete(self);
        return;
    }
    Own_Seesaw *pscratch = (Own_Seesaw *)parent->own.u8;
    if (Ball_RestY(self, scratch, parent) > self->y_pos.f.u)
        return; // not down yet

    int8_t side = self->x_vel < 0 ? 2 : 0;
    pscratch->state = side;
    scratch->state = side;
    if (side != parent->mapping_frame) { // the ball came down on an end that was up: the seesaw throws up whoever is on it
        if (parent->status & 8) {
            parent->status &= (uint8_t)~8;
            Seesaw_Launch(self, player);
        }
        if (parent->status & 0x10) {
            parent->status &= (uint8_t)~0x10;
            Seesaw_Launch(self, TAILS_OBJ);
        }
    }
    self->x_vel = 0;
    self->y_vel = 0;
    self->routine -= 2;
}

// Obj14_Animate (the Sol badnik): every fourth frame its palette line alternates (the flash of red and yellow), and it faces Sonic (flipped when he is at or to its right)
static void Seesaw_BallAnimate(Sst *self) {
    if ((frame_count & 3) == 0)
        self->art_tile ^= 0x2000;
    SST_RENDER_SET(self, SST_RENDER_X_FLIP, (uint16_t)player->x_pos.f.u >= (uint16_t)self->x_pos.f.u);
}

void Obj_HTZSeesaw(Sst *self) {
    Own_Seesaw *scratch = (Own_Seesaw *)self->own.u8;

    switch (self->routine) {
    case SeesawRoutine_Init:
        Seesaw_Init(self, scratch);
        // fallthrough
    case SeesawRoutine_Main:
        Seesaw_Main(self, scratch);
        break;
    case SeesawRoutine_Ball:
        Seesaw_Ball_Init(self, scratch);
        // fallthrough
    case SeesawRoutine_MoveBall:
        Seesaw_BallAnimate(self);
        Seesaw_MoveBall(self, scratch);
        break;
    case SeesawRoutine_BallFall:
        Seesaw_BallAnimate(self);
        Seesaw_BallFall(self, scratch);
        break;
    }

    if (self->code == NULL) // (a ball that has lost its seesaw has deleted itself)
        return;
    SstOnScreenTest2(self, scratch->orig_x);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 16: the lift
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28: how long it slides, in eighths of a frame (the prototype's lsl #3)
    uint8_t pad0[7];   // 0x29-0x2F
    int16_t pad1[2];   // 0x30, 0x32: (where it began, kept by the original but not used)
    int16_t timer;     // 0x34: how long it still slides
} Own_HTZLift;

enum { LiftState_Wait = 0, LiftState_Slide = 2, LiftState_Fall = 4 }; // (routine_sec, $25)

// The Simon Wai prototype's lift (Obj16): it waits until it is stood on, slides down the slope (to the left if it is flipped) for as many frames as its subtype times eight, and then stops, leaves a
// broken pole behind (a scenery object, subtype 6) and falls
void Obj_HTZLift(Sst *self) {
    Own_HTZLift *scratch = (Own_HTZLift *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_HTZLift;
        self->art_tile = TILE_MAP(0, 2, 0, 0, 0x3E6);
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 0x20;
        self->mapping_frame = 0;
        self->priority = 1 * 0x80;
        scratch->timer = (int16_t)(self->subtype << 3);
    }

    int16_t old_x = self->x_pos.f.u;
    switch (self->routine_secondary) {
    case LiftState_Wait: // when it is stood on it starts to slide
        if (self->status & 0x18) {
            self->routine_secondary += 2;
            self->x_vel = ((self->status & SST_STATUS_X_FLIP) != 0) ? -0x200 : 0x200;
            self->y_vel = 0x100;
        }
        break;
    case LiftState_Slide:
        if ((frame_count & 0xF) == 0)
            PlaySound(sfx_HTZLift); // (the alpha's $E4, every 16 frames)
        Sst_MoveSprite2(self);
        if (--scratch->timer == 0) {
            self->routine_secondary += 2;
            self->mapping_frame = 2;
            self->x_vel = 0;
            self->y_vel = 0;
            Sst *pole = Sst_AllocateAfter(self);
            if (pole != NULL) {
                pole->code = sst_object_codes[0x1C]; // (scenery)
                pole->x_pos.f.u = self->x_pos.f.u;
                pole->y_pos.f.u = self->y_pos.f.u;
                pole->render_flags = self->render_flags;
                pole->subtype = 6;
            }
        }
        break;
    default: // it falls, and goes when it is below the bottom of the level
        Sst_MoveSprite2(self);
        self->y_vel += 0x38;
        if ((uint16_t)(limit_btm2 + 0xE0) < (uint16_t)self->y_pos.f.u) {
            Sst_Delete(self);
            return;
        }
        break;
    }

    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
        Sst *chr = Character(who);
        if (chr != NULL)
            SolidSst_Platform(self, chr, who, self->width_pixels, -0x28, old_x);
    }
    SstOnScreenTest(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 2F: the breakable floor (Obj_0x2F_Breakable_Floor, loc_1747C): a solid slab (a stack of five heights, by subtype) that breaks into its pieces, row by row from the top, when it is stood on and
// rolled on from the secondary collision path (or by anyone rolling, with subtype bit 7), and throws no one: they fall with it. Whoever is not rolling on it is put back on the primary path.
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype; // 0x28: bits 1-3 the height (bit 7: breaks for any roller)
} Own_BreakFloor;

// loc_17490: the half height and the frame of each height
static const uint8_t floor_heights[5][2] = { { 0x24, 0 }, { 0x20, 2 }, { 0x18, 4 }, { 0x10, 6 }, { 0x08, 8 } };
// loc_17662: the speed of each piece (x, y), a piece of the frame after the one that was there taking the pair at its frame number (the table goes on into zeros past its end)
static const int16_t floor_pieces[32][2] = {
    { -0x100, -0x800 }, { 0x100, -0x800 }, { -0xE0, -0x700 }, { 0xE0, -0x700 }, { -0xC0, -0x600 }, { 0xC0, -0x600 }, { -0xA0, -0x500 }, { 0xA0, -0x500 }, { -0x80, -0x400 }, { 0x80, -0x400 },
};

// loc_175AE and loc_175B4: a roller keeps rolling as he falls (loc_175CC: he is in the air and off the floor, whatever he was doing)
static void BreakFloor_Drop(Sst *chr, bool rolling) {
    if (rolling) {
        chr->status |= SST_PSTATUS_IN_BALL;
        chr->y_radius = 0xE;
        chr->x_radius = 7;
        chr->anim = SonAnimId_Roll;
    }
    chr->status |= SST_PSTATUS_IN_AIR;
    chr->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
    chr->routine = 2;
}

// $3E and $3F of the character, the solid bits of the first path ($C, $D)
static void BreakFloor_PrimaryPath(Sst *chr) {
    Scratch_Sonic *scratch = (Scratch_Sonic *)chr->own.u8;
    scratch->top_solid_bit = 0xC;
    scratch->lrb_solid_bit = 0xD;
}

void Obj_HTZBreakFloor(Sst *self) {
    Own_BreakFloor *scratch = (Own_BreakFloor *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_HTZBreakFloor;
        self->art_tile = TILE_MAP(1, 2, 0, 0, 0);
        self->render_flags = SST_RENDER_LEVEL;
        self->width_pixels = 0x10;
        self->priority = 4 * 0x80;
        const int height = ((self->subtype & 0x1E) >> 1) % 5;
        self->y_radius = (int8_t)floor_heights[height][0];
        self->mapping_frame = floor_heights[height][1];
    }

    if (self->routine == 4) { // a piece
        Sst_MoveSprite2(self);
        self->y_vel += 0x18;
        if (!(self->render_flags & SST_RENDER_ON_SCREEN))
            Sst_Delete(self);
        else
            Sst_Draw(self);
        return;
    }

    Sst *sidekick = TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL;
    const bool sonic_rolls = player->anim == SonAnimId_Roll, tails_rolls = sidekick != NULL && sidekick->anim == SonAnimId_Roll;
    const bool any = self->subtype & 0x80;
    // (a character's collision path is the solid bits he carries, the swappers' work: Game_CollisionPath; the floor puts whoever stands on it and does not break it back on the first path)
    const bool sonic_secondary = Game_CollisionPath(player) != 0, tails_secondary = sidekick != NULL && Game_CollisionPath(sidekick) != 0;
    if ((self->render_flags & SST_RENDER_ON_SCREEN)) {
        for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
            Sst *chr = Character(who);
            if (chr != NULL)
                SolidSst_Character(self, chr, who, (int16_t)(self->width_pixels + 0xB), self->y_radius, (int16_t)(self->y_radius + 1), self->x_pos.f.u, NULL);
        }
    }
    const uint8_t stand = self->status & 0x18;
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
        SstOnScreenTest(self);
        return;
    }

    self->status &= 0xE7;
    const int old_frame = self->mapping_frame;
    self->mapping_frame++;
    const uint8_t *piece;
    const int count = Mappings_FramePieces((const uint8_t *)self->mappings, self->mapping_frame, &piece);
    ObjectBreakToPieces(self, floor_pieces + old_frame, count);
    ObjectChainScore(self);
    Sst_MoveSprite2(self); // (it goes on as its first piece, this very frame)
    self->y_vel += 0x18;
    Sst_Draw(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 31: the lava boxes (Obj_0x31_Lava_Attributes): an invisible box, $80 wide, that hurts (col $96, $94, $95 by subtype, none for the fourth)
// ---------------------------------------------------------------------------------------------------------------------------------------
void Obj_HTZLavaBox(Sst *self) {
    static const uint8_t collision[4] = { 0x96, 0x94, 0x95, 0x00 };

    if (self->routine == 0) {
        self->routine += 2;
        self->collision_flags = collision[self->subtype & 3];
        self->width_pixels = 0x80;
        self->priority = 4 * 0x80;
        self->mapping_frame = self->subtype;
    }
    self->render_flags = 0x84; // (never drawn, but the touch response has to find it "on screen")
    int16_t box_w, box_h;
    if (DebugMarkers_TouchBox(self->collision_flags, &box_w, &box_h))
        DebugMarkersSst_Show(self, box_w, box_h); // (the debug cheat shows its corners)
    SstDeleteIfNotInRange2(self, self->x_pos.f.u); // (the prototype does not forget its mark here either)
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
} Own_Fireball;

#define ARTTILE_LAVA_BUBBLE 0x416 // ($82C0 in Hill Top's second list)
#define ARTTILE_FIRE 0x39E        // ($73C0 in its first)

enum { FireRoutine_Init = 0, FireRoutine_Bubble = 2, FireRoutine_Throw = 4, FireRoutine_Wait = 6, FireRoutine_Fly = 8, FireRoutine_Burn = 0xA, FireRoutine_Gone = 0xC };

// loc_1723E: one fireball
static void Fireball_Throw(Sst *self, Sst *ball) {
    ball->code = self->code;
    ball->routine = FireRoutine_Fly;
    ball->x_pos.f.u = self->x_pos.f.u;
    ball->y_pos.f.u = self->y_pos.f.u;
    ball->x_vel = self->x_vel;
    ball->y_vel = self->y_vel;
    ball->y_radius = ball->x_radius = 8;
    ball->mappings = self->mappings;
    ball->art_tile = self->art_tile;
    ball->render_flags = 4;
    ball->priority = 3 * 0x80;
    ball->width_pixels = 8;
    ball->collision_flags = 0x8B;
    ((Own_Fireball *)ball->own.u8)->base_y = ball->y_pos.f.u;
}

void Obj_HTZFireball(Sst *self) {
    Own_Fireball *scratch = (Own_Fireball *)self->own.u8;

    switch (self->routine) {
    case FireRoutine_Init: {
        self->routine += 2;
        self->y_radius = self->x_radius = 8;
        self->mappings = Mappings_HTZFireball;
        self->art_tile = TILE_MAP(1, 0, 0, 0, ARTTILE_LAVA_BUBBLE);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 3 * 0x80;
        self->width_pixels = 8;
        scratch->base_y = self->y_pos.f.u;
        const int16_t speed = (int16_t)-(((int)self->subtype << 3) & 0x780);
        self->x_vel = self->y_vel = speed;
        scratch->timer = scratch->reload = (int16_t)((self->subtype & 0xF) << 4);
    }
        // Fallthrough
    case FireRoutine_Bubble:
        Sst_Animate(self, Animation_HTZFireball);
        SstOnScreenTest(self);
        break;
    case FireRoutine_Throw:
        if (self->anim_frame_timer == 5) { // the moment of the animation: two fireballs, one the other way
            Sst *first = Sst_AllocateAfter(self);
            if (first != NULL) {
                Fireball_Throw(self, first);
                Sst *second = Sst_AllocateAfter(self);
                if (second != NULL) {
                    Fireball_Throw(self, second);
                    second->x_vel = (int16_t)-second->x_vel;
                    SST_RENDER_SET(second, SST_RENDER_X_FLIP, true);
                }
            }
            PlaySound(sfx_Fireball);
            self->routine += 2;
        }
        Sst_Animate(self, Animation_HTZFireball);
        SstOnScreenTest(self);
        break;
    case FireRoutine_Wait:
        if (--scratch->timer < 0) {
            scratch->timer = scratch->reload;
            self->routine = FireRoutine_Bubble;
            self->anim = 0;
            self->prev_anim = 1; // (the animation starts again)
        }
        Sst_Animate(self, Animation_HTZFireball);
        SstOnScreenTest(self);
        break;
    case FireRoutine_Fly:
        if (--self->anim_frame_timer < 0) {
            self->anim_frame_timer = 7;
            self->mapping_frame = (uint8_t)((self->mapping_frame + 1) & 1);
        }
        Sst_MoveSprite2(self);
        self->y_vel += 0x18;
        if ((uint16_t)(limit_btm2 + 0xE0) < (uint16_t)self->y_pos.f.u) {
            Sst_Delete(self);
            return;
        }
        SST_RENDER_SET(self, SST_RENDER_Y_FLIP, false);
        if (self->y_vel >= 0) { // falling: it burns where it lands
            SST_RENDER_SET(self, SST_RENDER_Y_FLIP, true);
            const int16_t d1 = SstFloorDist(self, self->x_pos.f.u);
            if (d1 < 0) {
                self->y_pos.f.u = (int16_t)(self->y_pos.f.u + d1);
                self->routine += 2;
                self->anim = 2;
                self->y_vel = 0;
                self->mappings = Mappings_HTZFireFlames;
                self->art_tile = TILE_MAP(1, 0, 0, 0, ARTTILE_FIRE);
                self->mapping_frame = 0;
                scratch->timer = 9;
                scratch->spread = 3;
            }
        }
        SstOnScreenTest(self);
        break;
    case FireRoutine_Burn:
        if (--scratch->timer < 0) {
            scratch->timer = 0x7F;
            if ((int8_t)--scratch->spread >= 0) { // the fire goes on one step along the ground
                Sst *next = Sst_AllocateAfter(self);
                if (next != NULL) {
                    *next = *self;
                    Own_Fireball *ns = (Own_Fireball *)next->own.u8;
                    ns->timer = 9;
                    next->anim = 2;
                    next->prev_anim = 0;
                    next->x_pos.f.u = (int16_t)(next->x_pos.f.u + (next->x_vel < 0 ? -0x0E : 0x0E));
                    next->y_pos.f.u = (int16_t)(next->y_pos.f.u + SstFloorDist(next, next->x_pos.f.u));
                }
            }
        }
        Sst_Animate(self, Animation_HTZFireball);
        SstOnScreenTest(self);
        break;
    default:
        Sst_Delete(self);
        break;
    }
}

void HTZFireball_BossBallLands(Sst *self) {
    Own_Fireball *scratch = (Own_Fireball *)self->own.u8;
    self->code = sst_object_codes[0x20];
    self->routine = FireRoutine_Burn;
    self->anim = 2;
    self->y_vel = 0;
    self->mappings = Mappings_HTZFireFlames;
    self->art_tile = TILE_MAP(1, 0, 0, 0, ARTTILE_FIRE);
    self->mapping_frame = 0;
    scratch->timer = 9;
    scratch->spread = 3;
    Obj_HTZFireball(self);
}
