// The boss of Chemical Plant for Sonic 2: the pre-beta's object $5D (see CPZBoss.h). Every part is an object of the boss's own id with a routine of its own; each keeps the slot of the object it belongs to (the
// pre-beta keeps its address) and asks it things by that. The routines, as the pre-beta numbers them:
//   0 starts the ship, and makes its parts          2 the ship (the object of the boss itself: the hits are counted on it)
//   4 makes the pipe's twelve segments, one a frame 6 the end of the pipe, which goes up and down it   8 pulls the segments back up
//   $A the puffs that rise from the pipe             $C the blob of the chemical that falls, and splashes
//   $E a segment of the pipe                         $10 the arm: the pipe's mouth, which goes out over Sonic, and its parts
//   $12 Eggman                                       $14 a piece of the wreck, which falls
//   $16 the ship's flame                             $18 the light on the ship
// The ship is the first of them, and moves by two 16.16 numbers (the place it bobs about), and the others keep to it.
#include "Object/CPZBoss.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Object/BossCommon.h"
#include "Constants.h"

#include "EnginePalette.h"
#include "Level.h"
#include "LevelCollision.h"
#include "MathUtil.h"
#include "Object/Sonic.h"
#include "Sound.h"

#include "Macros.h"

#include "Resource/Animation/CPZBoss.h"
#include "Resource/Animation/CPZBoss1.h"
#include "Resource/Mappings/CPZBoss.h"
#include "Resource/Mappings/CPZBoss1.h"
#include "Resource/Mappings/CPZBoss2.h"
#include "Resource/Mappings/CPZBoss3.h"

#define ObjId_CPZBoss 0x5D

// Where the art is (the boss's list, the pre-beta's $28): the ship at $420, the light and the vapour at $418 (its boost), the boss at $500
#define ArtTile_Ship  0x420
#define ArtTile_Boost 0x418
#define ArtTile_Boss  0x500

typedef struct {
    int32_t x16;     // 0x30 (var_04): the ship's place, as 16.16 numbers
    int32_t y16;     // 0x38 (var_0C)
    int16_t timer;   // 0x2A
    int16_t count;   // 0x2C (var_00)
    int16_t out;     // 0x2E (var_02): how far the arm is out
    int16_t offset;  // 0x38 (var_0C): where a segment is on the pipe
    int16_t wreck;   // 0x3C (var_10)
    uint8_t flags1;  // 0x2D (var_01): the ship's side (bit 3)
    uint8_t flags;   // 0x2E (var_02): what the ship tells its parts (see Flag_*)
    uint8_t step;    // 0x30 (var_04): the part's counter
    uint8_t reach;   // 0x31 (var_05): how far down the pipe's mouth is
    uint8_t dips;    // 0x32 (Boss_Hit_2)
    uint8_t link;    // 0x34 (var_08): the slot of what it belongs to
    uint8_t flash;   // 0x3E (var_12): how long its hit flash lasts
    uint8_t phase;   // 0x3F (var_13): the angle of its bob
} Own_CPZBoss;

#define OWN(s) ((Own_CPZBoss *)(s)->own.u8)

// What the ship's flags say (var_02 of the ship): the arm and the pipe wait for each other by them
enum {
    Flag_Ready   = 0x01, // the ship is at the side: the pipe comes out
    Flag_Puff    = 0x02, // a puff has gone up: the arm's vapour takes one more step
    Flag_Out     = 0x04, // the arm is out
    Flag_Drop    = 0x08, // the blob is let fall
    Flag_Back    = 0x10, // the arm goes in again
    Flag_Wait    = 0x20, // the arm waits to let the blob fall
    Flag_Leaving = 0x40, // the ship is beaten and goes away
    Flag_Armed   = 0x80  // the arm's first parts have been made
};

enum {
    Routine_Init = 0, Routine_Ship = 2, Routine_PipeMaker = 4, Routine_PipeEnd = 6, Routine_Retract = 8, Routine_Puff = 0xA, Routine_Blob = 0xC, Routine_Segment = 0xE,
    Routine_Arm = 0x10, Routine_Eggman = 0x12, Routine_Piece = 0x14, Routine_Flame = 0x16, Routine_Light = 0x18
};

static Sst *Link(const Sst *s) {
    return &sst[OWN(s)->link];
}

static int16_t Spread(uint32_t random) { // (a part of the wreck's speed along: -2 to 1)
    return (int16_t)((int16_t)random >> 14);
}

// A part: a new object with the boss's id (after the object that makes it, or anywhere), its place at that object's and its slot in the part
static Sst *MakePart(Sst *maker, bool after, const uint8_t *mappings, uint16_t art, uint8_t priority) {
    Sst *part = after ? Sst_AllocateAfter(maker) : Sst_Allocate();
    if (part == NULL)
        return NULL;
    part->code = sst_object_codes[ObjId_CPZBoss];
    part->mappings = mappings;
    part->art_tile = art;
    part->render_flags = SST_RENDER_LEVEL;
    part->width_pixels = 0x20;
    part->priority = (uint16_t)(priority * 0x80);
    part->x_pos = maker->x_pos;
    part->y_pos = maker->y_pos;
    OWN(part)->link = (uint8_t)(maker - sst);
    return part;
}

static void CopyLook(Sst *part, const Sst *from) {
    part->render_flags = from->render_flags;
    part->status = from->status;
}

static void Follow(Sst *part, const Sst *from) { // (the place, the flips and the status of what it belongs to)
    part->x_pos.f.u = from->x_pos.f.u;
    part->y_pos.f.u = from->y_pos.f.u;
    CopyLook(part, from);
}

static void ShowAnimated(Sst *self) {
    Sst_Animate(self, Animation_CPZBoss);
    Sst_Draw(self);
}

// The ship's own place: its speed moves it (Offset_0x029398)
static void Ship_Move(Sst *self, Own_CPZBoss *own) {
    own->x16 += (int32_t)self->x_vel * 256;
    own->y16 += (int32_t)self->y_vel * 256;
}

// The ship's bob, its place and its hit (Offset_0x029318)
static void Ship_Place(Sst *self, Own_CPZBoss *own) {
    int16_t sin, cos;
    CalcSine(own->phase, &sin, &cos);
    self->y_pos.f.u = (int16_t)((sin >> 6) + (int16_t)(own->y16 >> 16));
    self->x_pos.f.u = (int16_t)(own->x16 >> 16);
    own->phase = (uint8_t)(own->phase + 2);
    if (self->routine_secondary >= 8)
        return;
    if (self->status & 0x80) { // the hits are out
        AddPoints(100);
        self->routine_secondary = 8;
        own->wreck = 0xB3;
        return;
    }
    if (self->collision_flags != 0)
        return;
    if (own->flash == 0) {
        own->flash = 0x40;
        QueueSound2(sfx_HitBoss);
    }
    dry_palette[1][1] = (dry_palette[1][1] == 0) ? 0x0EEE : 0;
    if (--own->flash == 0) {
        self->collision_flags = 0x0F;
        own->flags1 &= (uint8_t)~2;
    }
}

static void Ship_Side(Sst *self) { // (it looks at Sonic while it is not beaten)
    if (self->routine_secondary >= 8)
        return;
    if ((int16_t)(player->x_pos.f.u - self->x_pos.f.u) > 0)
        self->status |= SST_STATUS_X_FLIP;
    else
        self->status &= (uint8_t)~SST_STATUS_X_FLIP;
}

static void Ship_Run(Sst *self, Own_CPZBoss *own) {
    Ship_Side(self);
    switch (self->routine_secondary) {
    case 0: // comes down into the arena
        self->y_vel = 0x100;
        Ship_Move(self, own);
        if ((int16_t)(own->y16 >> 16) == 0x4C0) {
            self->y_vel = 0;
            self->routine_secondary += 2;
        }
        Ship_Place(self, own);
        break;
    case 2: { // goes to the side it is to be at, and stops there
        const int16_t target = (own->flags1 & 8) ? 0x2A50 : 0x2B30;
        const int16_t x = (int16_t)(own->x16 >> 16);
        int16_t distance = (int16_t)(target - x);
        if (distance < 0)
            distance = (int16_t)-distance;
        if (distance > 3) {
            self->x_vel = target > x ? 0x300 : -0x300;
            Ship_Move(self, own);
        } else if ((int16_t)(own->y16 >> 16) == 0x4C0) {
            self->x_vel = 0;
            self->y_vel = 0;
            self->routine_secondary += 2;
            own->flags1 ^= 8;
            own->flags |= Flag_Ready;
        }
        Ship_Place(self, own);
        break;
    }
    case 4: // waits for the pipe and the arm to be done
        if (!(own->flags & Flag_Ready))
            self->routine_secondary += 2;
        Ship_Place(self, own);
        break;
    case 6: { // follows Sonic (a little way off)
        const int16_t want = (int16_t)(player->x_pos.f.u + 0x4C);
        const int16_t x = (int16_t)(own->x16 >> 16);
        if (want > x) {
            own->x16 += 0x10000;
            if ((int16_t)(own->x16 >> 16) >= 0x2B70)
                own->x16 = (int32_t)((uint32_t)0x2B70 << 16 | ((uint32_t)own->x16 & 0xFFFF));
        } else if (want < x) {
            own->x16 -= 0x10000;
            if ((int16_t)(own->x16 >> 16) <= 0x2A28)
                own->x16 = (int32_t)((uint32_t)0x2A28 << 16 | ((uint32_t)own->x16 & 0xFFFF));
        }
        Ship_Place(self, own);
        break;
    }
    case 8: // beaten: it blows up
        if (--own->wreck >= 0) {
            Boss_Defeated(self);
        } else {
            self->status |= SST_STATUS_X_FLIP;
            self->status &= 0x7F;
            self->x_vel = 0;
            self->routine_secondary += 2;
            own->wreck = (int16_t)0xFFDA;
        }
        break;
    case 0xA: // falls a little and stops
        ++own->wreck;
        if (own->wreck == 0) {
            self->y_vel = 0;
        } else if (own->wreck < 0) {
            self->y_vel += 0x18;
        } else if ((uint16_t)own->wreck < 0x30) {
            self->y_vel -= 8;
        } else if (own->wreck == 0x30) {
            self->y_vel = 0;
        } else if ((uint16_t)own->wreck >= 0x38) {
            self->routine_secondary += 2;
        }
        Ship_Move(self, own);
        Ship_Place(self, own);
        break;
    case 0xC: // and goes away to the right
        own->flags |= Flag_Leaving;
        self->x_vel = 0x400;
        self->y_vel = (int16_t)0xFFC0;
        if (limit_right2 != 0x2CC0 + SCREEN_WIDEADD2) {
            limit_right1 = limit_right2 = (uint16_t)(limit_right2 + 2);
        } else if (!(self->render_flags & SST_RENDER_ON_SCREEN)) { // (out of sight: it is gone)
            boss_status = 1;
            Sst_Delete(self);
            return;
        }
        Ship_Move(self, own);
        Ship_Place(self, own);
        break;
    }
    Sst_Animate(self, Animation_CPZBoss1);
    self->render_flags = (uint8_t)((self->render_flags & 0xFC) | (self->status & 3));
    Sst_Draw(self);
}

// The part that follows the ship's place (the flame, and the one that Eggman's is made of)
static void Flame_Run(Sst *self) {
    Follow(self, Link(self));
    Sst_Animate(self, Animation_CPZBoss1);
    Sst_Draw(self);
}

// The light on the ship: it flashes. When the ship is beaten it is a vapour that the pre-beta never lets out (the routine that would do it is not reached), so there is none
static void Light_Run(Sst *self, Own_CPZBoss *own) {
    static const int8_t frames[4] = { 0, -1, 1, 0 };
    Sst *ship = Link(self);
    if (!(self->status & 0x80)) {
        self->x_pos = ship->x_pos;
        self->y_pos = ship->y_pos;
        CopyLook(self, ship);
        if ((int8_t)--self->anim_frame_timer < 0) {
            self->anim_frame_timer = 1;
            uint8_t d0 = (uint8_t)(own->timer + 1);
            if (d0 > 2)
                d0 = 0;
            self->mapping_frame = (uint8_t)frames[d0];
            own->timer = d0;
        }
        if (self->mapping_frame == 0xFF) {
            self->mapping_frame = 0;
            return;
        }
        Sst_Draw(self);
        return;
    }
    if (!(OWN(ship)->flags & Flag_Leaving))
        return;
    self->routine_secondary += 2;
    self->mappings = Mappings_CPZBoss3;
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Boost);
    self->mapping_frame = 0;
    self->anim_frame_timer = 5;
    self->x_pos.f.u = (int16_t)(ship->x_pos.f.u - 0x28);
    self->y_pos.f.u = (int16_t)(ship->y_pos.f.u + 4);
}

// A piece of the wreck, or a segment of the pipe that has come off, goes with a speed of its own: x_vel is a number of pixels, y_vel $FC80, and a delay before the first explosion
static void Wreck_Start(Sst *piece) {
    Own_CPZBoss *own = OWN(piece);
    const uint32_t random = RandomNumber();
    piece->routine = Routine_Piece;
    piece->x_vel = Spread(random);
    piece->y_vel = (int16_t)0xFC80;
    own->step = (uint8_t)((((random >> 16) & 0xFF) + 0x1E) & 0x7F);
}

static void Explosion_At(Sst *piece) {
    Sst *explosion = Sst_Spawn(ObjId_BossExplosionPiece, piece->x_pos.f.u, piece->y_pos.f.u);
    (void)explosion;
}

static void Piece_Run(Sst *self, Own_CPZBoss *own) {
    if (own->step != 0xF9) {
        own->step--;
        if ((int8_t)own->step > 0) {
            Sst_Draw(self);
            return;
        }
        Explosion_At(self);
        own->step = 0xF9;
        own->timer = 0x1E;
    }
    if (--own->timer >= 0) {
        Sst_Draw(self);
        return;
    }
    self->x_pos.f.u = (int16_t)(self->x_pos.f.u + self->x_vel);
    const int16_t speed = self->y_vel;
    self->y_vel = (int16_t)(self->y_vel + 0x38);
    self->y_pos.v += (uint32_t)((int32_t)speed * 256);
    if ((uint32_t)self->y_pos.v >= 0x05800000u) {
        Sst_Delete(self);
        return;
    }
    SstOnScreenTest(self);
}

// Eggman (routine $12): he follows the ship, and when it is beaten he and two more pieces come apart
static void Eggman_Run(Sst *self, Own_CPZBoss *own) {
    Sst *ship = Link(self);
    if (!(self->status & 0x80)) {
        self->x_pos = ship->x_pos;
        self->y_pos = ship->y_pos;
        CopyLook(self, ship);
        ShowAnimated(self);
        return;
    }
    own->step = 0x78;
    self->x_pos.f.u = ship->x_pos.f.u;
    self->y_pos.f.u = ship->y_pos.f.u;
    self->mapping_frame = 0x22;
    Wreck_Start(self);
    OWN(self)->step = 0x78;
    uint8_t frame = 0x23;
    for (int i = 0; i < 2; i++, frame++) {
        Sst *piece = MakePart(self, false, Mappings_CPZBoss, TILE_MAP(0, 1, 0, 0, ArtTile_Boss), 2);
        if (piece == NULL) {
            Sst_Delete(self);
            return;
        }
        piece->mapping_frame = frame;
        piece->routine = Routine_Piece;
        piece->status = self->status;
        piece->render_flags = self->render_flags;
        piece->x_pos.f.u = self->x_pos.f.u;
        piece->y_pos.f.u = self->y_pos.f.u;
        Wreck_Start(piece);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The pipe: twelve segments (8 apart, one made each frame), the end of it that comes down and goes up, and the puffs
// ---------------------------------------------------------------------------------------------------------------------------------------
static void Segment_Run(Sst *self, Own_CPZBoss *own) {
    Sst *parent = Link(self);
    Sst *ship = Link(parent);
    if (ship->status & 0x80) { // the boss is beaten: it comes off
        Wreck_Start(self);
        Sst_Draw(self);
        return;
    }
    self->x_pos.f.u = parent->x_pos.f.u;
    self->y_pos.f.u = parent->y_pos.f.u;
    if (self->routine_secondary == 4)
        self->y_pos.f.u += 0x18;
    if (self->status & 0x80) {
        Sst_Delete(self);
        return;
    }
    self->y_pos.f.u = (int16_t)(self->y_pos.f.u + own->offset);
    ShowAnimated(self);
}

// A segment of the pipe made by the maker (or the maker itself, the first)
static void PipeMaker_Segment(Sst *maker, Sst *segment, bool first) {
    Own_CPZBoss *own = OWN(maker);
    if (--own->count < 0) {
        maker->routine_secondary = 0;
        maker->routine = Routine_PipeEnd;
        Segment_Run(maker, own);
        return;
    }
    segment->mappings = Mappings_CPZBoss;
    segment->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_Boss);
    segment->render_flags = SST_RENDER_LEVEL;
    segment->width_pixels = 0x20;
    segment->priority = 5 * 0x80;
    segment->x_pos.f.u = maker->x_pos.f.u;
    segment->y_pos.f.u = maker->y_pos.f.u;
    const int16_t offset = (int16_t)(((0x0B - own->count) & 0xFFFF) << 3);
    OWN(segment)->offset = offset;
    segment->y_pos.f.u = (int16_t)(segment->y_pos.f.u + offset);
    segment->anim = 1;
    if (!first)
        segment->routine = Routine_Segment;
    Segment_Run(maker, own);
}

static void PipeMaker_Run(Sst *self, Own_CPZBoss *own) {
    Sst *ship = Link(self);
    switch (self->routine_secondary) {
    case 0:
        if (!(OWN(ship)->flags & Flag_Ready))
            return;
        self->x_pos.f.u = ship->x_pos.f.u;
        self->y_pos.f.u = (int16_t)(ship->y_pos.f.u + 0x18);
        own->count = 0x0C;
        self->routine_secondary += 2;
        PipeMaker_Segment(self, self, true);
        break;
    case 2: {
        Sst *segment = Sst_AllocateAfter(self);
        if (segment == NULL)
            return;
        segment->code = sst_object_codes[ObjId_CPZBoss];
        OWN(segment)->link = (uint8_t)(self - sst);
        PipeMaker_Segment(self, segment, false);
        break;
    }
    }
}

// The pipe is made: its end comes down, and the puffs come up (routine 6: the pipe's maker, once the twelve are made, makes the two and is a segment itself)
static void PipeEnd_Run(Sst *self, Own_CPZBoss *own) {
    switch (self->routine_secondary) {
    case 0: {
        Sst *end = Sst_AllocateAfter(self);
        if (end == NULL) {
            Segment_Run(self, own);
            return;
        }
        self->routine = Routine_Segment;
        end->code = sst_object_codes[ObjId_CPZBoss];
        end->routine = Routine_PipeEnd;
        end->routine_secondary = 2;
        end->mappings = Mappings_CPZBoss;
        end->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_Boss);
        end->render_flags = SST_RENDER_LEVEL;
        end->width_pixels = 0x20;
        end->priority = 4 * 0x80;
        OWN(end)->dips = 2;
        end->x_pos = self->x_pos;
        end->y_pos = self->y_pos;
        OWN(end)->reach = 0x58;
        end->y_pos.f.u = (int16_t)(end->y_pos.f.u + 0x58);
        end->anim = 2;
        OWN(end)->link = (uint8_t)(self - sst);
        OWN(end)->step = 0x12;
        Sst *puff = Sst_AllocateAfter(self);
        if (puff != NULL) {
            puff->code = sst_object_codes[ObjId_CPZBoss];
            puff->routine = Routine_Puff;
            OWN(puff)->link = OWN(self)->link;
        }
        Segment_Run(self, own);
        break;
    }
    case 2: { // the end goes up by 8 every $12 frames
        Sst *parent = Link(self);
        if (Link(parent)->status & 0x80) {
            Sst_Delete(self);
            return;
        }
        self->x_pos.f.u = parent->x_pos.f.u;
        self->y_pos.f.u = parent->y_pos.f.u;
        if (--own->step == 0) {
            own->step = 0x12;
            own->reach = (uint8_t)(own->reach - 8);
            const int8_t reach = (int8_t)own->reach;
            if (reach < 0) {
                own->step = 6;
                self->routine_secondary = 4;
                return;
            }
            if (reach == 0) {
                self->anim = 3;
                own->step = 0x0C;
            }
        }
        self->y_pos.f.u = (int16_t)(self->y_pos.f.u + own->reach);
        ShowAnimated(self);
        break;
    }
    case 4: { // it has been to the bottom: the pipe is pulled up (the pre-beta puts the end away after the first time, though it counts two)
        if (--own->step != 0)
            return;
        if (--own->dips != 0) {
            self->anim = 2;
            own->step = 0x12;
            self->routine_secondary = 2;
            own->reach = 0x58;
        }
        Sst *maker = Link(self);
        maker->routine = Routine_Retract;
        OWN(maker)->reach = 0x58;
        Sst_Delete(self);
        break;
    }
    }
}

// The pipe is pulled up: each frame the lowest segment is taken off (the pre-beta finds it by its Y among all the objects, and so does this)
static void Retract_Run(Sst *self, Own_CPZBoss *own) {
    Sst *target = NULL;
    if (own->wreck == 0) {
        const int16_t y = (int16_t)(own->reach + self->y_pos.f.u);
        for (int i = 0; i < SST_COUNT; i++) {
            if (sst[i].y_pos.f.u == y && sst[i].code == sst_object_codes[ObjId_CPZBoss]) {
                target = &sst[i];
                break;
            }
        }
        if (target == NULL) {
            Segment_Run(self, own);
            return;
        }
    } else {
        target = self;
    }
    target->status |= 0x80;
    own->reach = (uint8_t)(own->reach - 8);
    if (own->reach == 0)
        own->wreck = 0xFF;
    Segment_Run(self, own);
}

// The puffs: twelve of them, one at a time, from the pipe's mouth; each one is a step for the arm's vapour
static void Puff_Run(Sst *self, Own_CPZBoss *own) {
    Sst *ship = Link(self);
    if (self->status & 0x80) {
        Sst_Delete(self);
        return;
    }
    switch (self->routine_secondary) {
    case 0:
        self->routine_secondary += 2;
        self->mappings = Mappings_CPZBoss;
        self->art_tile = TILE_MAP(0, 3, 0, 0, ArtTile_Boss);
        self->render_flags = SST_RENDER_LEVEL;
        self->width_pixels = 0x20;
        self->priority = 4 * 0x80;
        self->x_pos.f.u = ship->x_pos.f.u;
        self->y_pos.f.u = ship->y_pos.f.u;
        own->step = 0x0F;
        self->anim = 4;
        // Fallthrough
    case 2:
        if (--own->step != 0) {
            self->x_pos.f.u = ship->x_pos.f.u;
            self->y_pos.f.u = ship->y_pos.f.u;
            CopyLook(self, ship);
            ShowAnimated(self);
            return;
        }
        self->anim = 5;
        own->step = 4;
        self->routine_secondary += 2;
        self->y_pos.f.u = (int16_t)(self->y_pos.f.u - 0x24);
        self->x_pos.f.u = (int16_t)(self->x_pos.f.u - 2);
        return;
    case 4:
        if (--own->step == 0) {
            self->routine_secondary = 0;
            OWN(ship)->flags |= Flag_Puff;
            own->wreck = (int16_t)(own->wreck + 1);
            if ((int8_t)own->wreck >= 0x0C)
                Sst_Delete(self);
            return;
        }
        self->x_pos.f.u = ship->x_pos.f.u;
        self->y_pos.f.u = (int16_t)(ship->y_pos.f.u - 0x24);
        self->x_pos.f.u = (int16_t)(self->x_pos.f.u - 2);
        if (self->render_flags & SST_RENDER_X_FLIP)
            self->x_pos.f.u = (int16_t)(self->x_pos.f.u + 4);
        ShowAnimated(self);
        return;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The arm (routine $10)
// ---------------------------------------------------------------------------------------------------------------------------------------
enum { ArmState_Make = 0, ArmState_Run = 2, ArmState_Mark = 4, ArmState_Vapour = 6, ArmState_Blob = 8, ArmState_Wreck = 0xA };

// The blob, and the drops that its splash makes (routine $C): it falls to the floor, splashes and takes the arm back
static void Blob_Finish(Sst *self) {
    Sst *ship = Link(Link(self));
    OWN(ship)->flags |= Flag_Out | Flag_Back;
    ship->routine_secondary = 2;
}

static void Blob_Run(Sst *self, Own_CPZBoss *own) {
    Sst *ship = Link(Link(self));
    switch (self->routine_secondary) {
    case 0:
        self->routine_secondary += 2;
        self->y_radius = 0x20;
        self->anim = 0x19;
        self->y_vel = 0;
        if (OWN(ship)->flags1 & 4) {
            OWN(ship)->flags1 &= (uint8_t)~4;
            self->routine_secondary = 6;
            own->timer = 9;
        }
        // Fallthrough
    case 2: {
        Sst_MoveSprite(self);
        const int16_t distance = SstFloorDist(self, self->x_pos.f.u);
        if (distance < 0) { // it has reached the floor
            self->y_pos.f.u = (int16_t)(self->y_pos.f.u + distance);
            Blob_Finish(self);
            self->routine_secondary += 2;
            self->subtype = 0;
            Sst_Draw(self);
            return;
        }
        if ((int16_t)self->y_pos.f.u >= 0x518) {
            Blob_Finish(self);
            Sst_Delete(self);
            return;
        }
        ShowAnimated(self);
        return;
    }
    case 4: // the splash
        if (self->subtype == 0) {
            self->y_pos.f.u = (int16_t)(self->y_pos.f.u + 0x18);
            self->x_pos.f.u = (int16_t)(self->x_pos.f.u + ((self->render_flags & SST_RENDER_X_FLIP) ? -0x0C : 0x0C));
            self->y_radius = 4;
            self->x_radius = 4;
            self->subtype++;
            self->mapping_frame = 9;
            self->y_vel = (int16_t)-(int16_t)((uint16_t)self->y_vel >> 1);
            int16_t speed = (int16_t)((int16_t)RandomNumber() >> 6);
            if (speed >= 0)
                speed = (int16_t)(speed + 0x200);
            self->x_vel = (int16_t)(speed - 0x100);
            self->collision_flags = 0;
            for (int i = 3; i >= 0; i--) {
                Sst *drop = MakePart(self, true, Mappings_CPZBoss, TILE_MAP(0, 3, 0, 0, ArtTile_Boss), 2);
                if (drop == NULL)
                    break;
                drop->y_radius = 4;
                drop->x_radius = 4;
                drop->mapping_frame = 9;
                drop->routine = Routine_Blob;
                drop->routine_secondary = 4;
                drop->subtype = 1;
                drop->y_vel = self->y_vel;
                drop->collision_flags = self->collision_flags;
                const uint32_t random = RandomNumber();
                int16_t side = (int16_t)((int16_t)random >> 6);
                if (side >= 0)
                    side = (int16_t)(side + 0x80);
                drop->x_vel = (int16_t)(side - 0x80);
                drop->y_vel = (int16_t)(drop->y_vel - (int16_t)((random >> 16) & 0x3FF));
            }
            Sst_Draw(self);
            return;
        }
        Sst_MoveSprite(self);
        if (SstFloorDist(self, self->x_pos.f.u) < 0)
            Sst_Delete(self);
        else
            SstOnScreenTest(self);
        return;
    case 6:
        if (--own->timer >= 0) {
            Sst_MoveSprite2(self);
            ShowAnimated(self);
            return;
        }
        self->priority = 2 * 0x80;
        self->mapping_frame = 0x25;
        self->x_pos.f.u = ship->x_pos.f.u;
        self->y_pos.f.u = ship->y_pos.f.u;
        self->routine_secondary += 2;
        self->anim_frame_timer = 8;
        // Fallthrough
    case 8:
        if ((int8_t)--self->anim_frame_timer < 0) {
            self->mapping_frame++;
            self->anim_frame_timer = 8;
            if (self->mapping_frame > 0x27) {
                Blob_Finish(self);
                Sst_Delete(self);
                return;
            }
            if (self->mapping_frame == 0x27)
                self->anim_frame_timer = (int8_t)(self->anim_frame_timer + 0x0C);
        }
        self->x_pos.f.u = ship->x_pos.f.u;
        self->y_pos.f.u = ship->y_pos.f.u;
        Sst_Draw(self);
        return;
    }
}

// Where the arm goes: out to Sonic by one a frame, with its look at each stretch, and in again (Offset_0x029D58)
static void Arm_Move(Sst *self, Own_CPZBoss *own, Sst *ship) {
    Own_CPZBoss *s = OWN(ship);
    int d0 = (s->flags & Flag_Back) ? 1 : -1;
    if (own->out == (int16_t)0xFFF0 && (s->flags & Flag_Back)) { // it is in again: the pipe starts over
        s->flags &= (uint8_t)~Flag_Back;
        s->flags &= (uint8_t)~Flag_Out;
        self->routine_secondary = 0;
        Sst *maker = MakePart(self, true, Mappings_CPZBoss, TILE_MAP(0, 1, 0, 0, ArtTile_Boss), 4);
        if (maker == NULL)
            return;
        OWN(maker)->link = own->link;
        maker->routine = Routine_PipeMaker;
        self->routine_secondary = 0;
        return;
    }
    const int16_t out = own->out;
    if (out >= (int16_t)0xFFD8) {
        self->anim = 6;
    } else if (out >= (int16_t)0xFFC0) {
        self->anim = 7;
    } else {
        self->anim = 8;
        if (out < (int16_t)0xFFA8) {
            own->out = (int16_t)(self->render_flags & SST_RENDER_X_FLIP ? 0x58 : 0xFFA8);
        } else if (out == (int16_t)0xFFA8 && !(s->flags & Flag_Back)) {
            return;
        }
    }
    own->out = (int16_t)(own->out + d0);
}

// Where it stops to let the blob fall: over Sonic (Offset_0x029C7C)
static void Arm_Choose(Sst *self, Own_CPZBoss *own, Sst *ship) {
    Own_CPZBoss *s = OWN(ship);
    if (s->flags & (Flag_Drop | Flag_Back))
        return;
    bool go = false;
    if (own->out >= (int16_t)0xFFEC) {
        if (!(s->flags1 & 2))
            return;
        s->flags1 &= (uint8_t)~2;
        s->flags1 |= 4;
        go = true;
    } else {
        if (own->out >= (int16_t)0xFFC0)
            return;
        int16_t d1 = (int16_t)(player->x_pos.f.u - 8);
        if (self->render_flags & SST_RENDER_X_FLIP) {
            d1 = (int16_t)(d1 + own->out - self->x_pos.f.u);
            if (d1 > 0)
                return;
            go = d1 >= (int16_t)0xFFE8;
        } else {
            d1 = (int16_t)(d1 - own->out - self->x_pos.f.u);
            if (d1 < 0 || d1 > 0x18)
                return;
            go = true;
        }
    }
    if (!go)
        return;
    s->flags |= Flag_Wait;
    s->flags &= (uint8_t)~Flag_Out;
    own->timer = 0x12;
    Sst *blob = MakePart(self, true, Mappings_CPZBoss, TILE_MAP(0, 1, 0, 0, ArtTile_Boss), 5);
    if (blob == NULL)
        return;
    blob->routine = Routine_Arm;
    blob->routine_secondary = ArmState_Blob;
    blob->anim = 0x0B;
    OWN(blob)->timer = 0x24;
}

static void Arm_Run(Sst *self, Own_CPZBoss *own) {
    Sst *parent = Link(self);
    switch (self->routine_secondary) {
    case ArmState_Make: {
        Own_CPZBoss *s = OWN(parent);
        if (!(s->flags & Flag_Armed)) {
            s->flags |= Flag_Armed;
            Sst *mark = MakePart(self, true, Mappings_CPZBoss, TILE_MAP(0, 1, 0, 0, ArtTile_Boss), 4);
            if (mark != NULL) {
                mark->routine = Routine_Arm;
                mark->routine_secondary = ArmState_Mark;
                mark->anim = 9;
            }
        }
        Sst *vapour = MakePart(self, true, Mappings_CPZBoss, TILE_MAP(0, 3, 0, 0, ArtTile_Boss), 4);
        if (vapour != NULL) {
            vapour->routine = Routine_Arm;
            vapour->routine_secondary = ArmState_Vapour;
        }
        self->routine_secondary += 2;
    }
    // Fallthrough
    case ArmState_Run: {
        self->x_pos.f.u = parent->x_pos.f.u;
        self->y_pos.f.u = (int16_t)(parent->y_pos.f.u - 0x38);
        Own_CPZBoss *s = OWN(parent);
        if (self->status & 0x80) {
            self->routine_secondary = ArmState_Wreck;
        } else if (s->flags & Flag_Out) {
            Arm_Move(self, own, parent);
            Arm_Choose(self, own, parent);
        } else if (s->flags & Flag_Wait) {
            if (--own->timer == 0) {
                s->flags &= (uint8_t)~Flag_Wait;
                s->flags |= Flag_Drop | Flag_Back;
            }
        }
        CopyLook(self, parent);
        int16_t out = own->out;
        if (self->render_flags & SST_RENDER_X_FLIP)
            out = (int16_t)-out;
        self->x_pos.f.u = (int16_t)(self->x_pos.f.u + out);
        ShowAnimated(self);
        return;
    }
    case ArmState_Mark: { // the light on the arm: it changes when the blob is let fall
        if (self->status & 0x80) {
            Sst_Delete(self);
            return;
        }
        Sst *ship = Link(parent);
        if ((OWN(ship)->flags & Flag_Wait) && self->anim == 9)
            self->anim = 0x0A;
        Follow(self, parent);
        ShowAnimated(self);
        return;
    }
    case ArmState_Vapour: { // the vapour in the arm: it fills up a step with each puff and then the arm goes out; it makes the blob fall when it is let
        if (self->status & 0x80) {
            Sst_Delete(self);
            return;
        }
        Sst *ship = Link(parent);
        Own_CPZBoss *s = OWN(ship);
        if (s->flags & Flag_Drop) {
            s->flags &= (uint8_t)~Flag_Drop;
            self->routine = Routine_Blob;
            self->routine_secondary = 0;
            self->collision_flags = 0x87;
        } else if (s->flags & Flag_Puff) {
            s->flags &= (uint8_t)~Flag_Puff;
            if (self->anim == 0)
                self->anim = 0x0B;
            self->anim++;
            if (self->anim >= 0x17) {
                s->flags &= (uint8_t)~Flag_Ready;
                s->flags |= Flag_Out;
            }
        } else if (self->anim == 0) {
            return;
        }
        Follow(self, parent);
        ShowAnimated(self);
        return;
    }
    case ArmState_Blob: // the drop of the arm: it waits to be taken away
        if (self->status & 0x80 || --own->timer == 0) {
            Sst_Delete(self);
            return;
        }
        Follow(self, parent);
        ShowAnimated(self);
        return;
    case ArmState_Wreck: { // the arm is beaten: its parts come off
        Own_CPZBoss *s = own;
        s->step = 0x1E;
        self->x_pos.f.u = parent->x_pos.f.u;
        self->y_pos.f.u = (int16_t)(parent->y_pos.f.u - 0x38);
        int16_t out = own->out;
        if (self->render_flags & SST_RENDER_X_FLIP)
            out = (int16_t)-out;
        self->x_pos.f.u = (int16_t)(self->x_pos.f.u + out);
        self->mapping_frame = 0x20;
        Wreck_Start(self);
        s->step = 0x1E;
        int pieces = 3;
        int16_t d0 = (int16_t)(own->out + 0x18);
        if (d0 >= 0) {
            pieces = 0;
        } else if ((d0 = (int16_t)(d0 + 0x18)) >= 0) {
            pieces = 1;
        } else if ((int16_t)(d0 + 0x18) >= 0) {
            pieces = 2;
        }
        static const int16_t distances[3] = { 0x18, 0x30, 0x48 };
        for (int i = pieces - 1; i >= 0; i--) {
            Sst *piece = MakePart(self, false, Mappings_CPZBoss, TILE_MAP(0, 1, 0, 0, ArtTile_Boss), 2);
            if (piece == NULL) {
                Sst_Delete(self);
                return;
            }
            piece->mapping_frame = 0x21;
            piece->render_flags = self->render_flags;
            piece->y_pos.f.u = (int16_t)(self->y_pos.f.u + 8);
            const int16_t d3 = (self->render_flags & SST_RENDER_X_FLIP) ? (int16_t)-distances[i] : distances[i];
            piece->x_pos.f.u = (int16_t)(self->x_pos.f.u + d3);
            Wreck_Start(piece);
        }
        Sst_Draw(self);
        return;
    }
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The boss
// ---------------------------------------------------------------------------------------------------------------------------------------
static void Init_Run(Sst *self, Own_CPZBoss *own) {
    self->mappings = Mappings_CPZBoss1;
    self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_Ship);
    self->render_flags |= SST_RENDER_LEVEL;
    self->width_pixels = 0x20;
    self->x_pos.f.u = 0x2B80;
    self->y_pos.f.u = 0x04B0;
    self->priority = 3 * 0x80;
    self->collision_flags = 0x0F;
    self->collision_property = 8;
    self->routine += 2;
    own->x16 = (int32_t)((uint32_t)0x2B80 << 16);
    own->y16 = (int32_t)((uint32_t)0x04B0 << 16);
    own->flags1 &= (uint8_t)~8;

    Sst *flame = MakePart(self, true, Mappings_CPZBoss1, TILE_MAP(0, 0, 0, 0, ArtTile_Ship), 3);
    if (flame != NULL) {
        own->link = (uint8_t)(flame - sst);
        flame->routine = Routine_Flame;
        flame->anim = 1;
        flame->render_flags = self->render_flags;
        if (!(self->subtype & 0x80)) {
            Sst *light = MakePart(self, true, Mappings_CPZBoss2, TILE_MAP(0, 0, 0, 0, ArtTile_Boost), 3);
            if (light != NULL) {
                self->anim_frame_timer = 1;
                light->routine = Routine_Light;
                light->render_flags = self->render_flags;
                Sst *eggman = MakePart(self, true, Mappings_CPZBoss, TILE_MAP(0, 1, 0, 0, ArtTile_Boss), 2);
                if (eggman != NULL)
                    eggman->routine = Routine_Eggman;
            }
        }
    }
    Sst *arm = MakePart(self, true, Mappings_CPZBoss, TILE_MAP(0, 1, 0, 0, ArtTile_Boss), 4);
    if (arm != NULL) {
        arm->y_pos.f.u = (int16_t)(arm->y_pos.f.u - 0x38);
        arm->x_pos.f.u = (int16_t)(arm->x_pos.f.u - 0x10);
        OWN(arm)->out = (int16_t)0xFFF0;
        arm->routine = Routine_Arm;
        arm->anim = 6;
    }
    Sst *pipe = MakePart(self, true, Mappings_CPZBoss, TILE_MAP(0, 1, 0, 0, ArtTile_Boss), 4);
    if (pipe != NULL)
        pipe->routine = Routine_PipeMaker;
}

void Obj_CPZBoss(Sst *self) {
    Own_CPZBoss *own = OWN(self);
    switch (self->routine) {
    case Routine_Init:
        Init_Run(self, own);
        break;
    case Routine_Ship:
        Ship_Run(self, own);
        break;
    case Routine_PipeMaker:
        PipeMaker_Run(self, own);
        break;
    case Routine_PipeEnd:
        PipeEnd_Run(self, own);
        break;
    case Routine_Retract:
        Retract_Run(self, own);
        break;
    case Routine_Puff:
        Puff_Run(self, own);
        break;
    case Routine_Blob:
        Blob_Run(self, own);
        break;
    case Routine_Segment:
        Segment_Run(self, own);
        break;
    case Routine_Arm:
        Arm_Run(self, own);
        break;
    case Routine_Eggman:
        Eggman_Run(self, own);
        break;
    case Routine_Piece:
        Piece_Run(self, own);
        break;
    case Routine_Flame:
        Flame_Run(self);
        break;
    case Routine_Light:
        Light_Run(self, own);
        break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The events at the end of the second act (the pre-beta's DynResize_CPz): the camera stops at the arena, then Sonic goes in, the music changes, and ninety frames on the boss comes
// ---------------------------------------------------------------------------------------------------------------------------------------
void CPZBoss_Events(void) {
    static uint8_t delay;
    switch (dle_routine) {
    case 0:
        if (scrpos_x.f.u < 0x2680 - SCREEN_WIDEADD2)
            break;
        limit_left2 = scrpos_x.f.u;
        limit_btm1 = 0x450 - SCREEN_TALLADD;
        dle_routine += 2;
        break;
    case 2:
        if (scrpos_x.f.u < 0x2A20 - SCREEN_WIDEADD2)
            break;
        limit_left2 = 0x2A20 - SCREEN_WIDEADD2;
        limit_right1 = limit_right2 = 0x2A20 + SCREEN_WIDEADD2;
        dle_routine += 2;
        QueueSound1(bgm_Fade);
        delay = 0;
        boss_flag = 1;
        AddPLC(PlcId_CPZBoss);
        break;
    case 4:
        if (scrpos_y.f.u >= 0x448 - SCREEN_TALLADD)
            limit_top2 = 0x448 - SCREEN_TALLADD;
        if (++delay < 0x5A)
            break;
        {
            Sst *boss = Sst_Allocate();
            if (boss != NULL)
                boss->code = sst_object_codes[ObjId_CPZBoss];
        }
        dle_routine += 2;
        QueueSound1(bgm_Boss);
        break;
    case 6:
        if (boss_status)
            GotThroughAct();
        break;
    }
}
